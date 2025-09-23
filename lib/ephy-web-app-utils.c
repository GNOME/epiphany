/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2011 Igalia S.L.
 *  Copyright © 2022 Matthew Leeds
 *
 *  This file is part of Epiphany.
 *
 *  Epiphany is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Epiphany is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Epiphany.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "ephy-web-app-utils.h"

#include "ephy-debug.h"
#include "ephy-file-helpers.h"
#include "ephy-flatpak-utils.h"
#include "ephy-profile-utils.h"
#include "ephy-settings.h"
#include "ephy-uri-helpers.h"

#include <errno.h>
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <libportal-gtk4/portal-gtk4.h>
#include <libsoup/soup.h>
#include <glib/gi18n.h>
#include <gio/gunixoutputstream.h>

/* Web apps are installed in the default data dir of the user. Every
 * app has its own profile directory. To create a web app, an ID needs
 * to be generated using the checksum of the given name.
 * The ID is used to uniquely identify the app.
 *
 *  - Name: the user-visible pretty app name
 *  - Normalized name: lowercase name
 *  - ID: SHA-1 of name
 *  - Epiphany App ID: org.gnome.Epiphany or org.gnome.Epiphany.Devel or org.gnome.Epiphany.Canary
 *  - GApplication ID: <epiphany-app-id>.WebApp_<id>
 *  - Profile directory: <user-data-dir>/<gapplication-id>
 *  - Desktop file: <user-data-dir>/xdg-desktop-portal/applications/<gapplication-id>.desktop
 *  - Icon: <user-data-dir>/xdg-desktop-portal/icon/<gapplication-id>.png
 *  - Sym link: <user-data-dir>/applications/<gapplication-id>.desktop
 *
 * Note that our ID and GApplication ID are different. Yes, this is confusing.
 *
 * System web applications have a profile dir without a desktop file.
 */

GQuark webapp_error_quark (void);
G_DEFINE_QUARK (webapp - error - quark, webapp_error)
#define WEBAPP_ERROR webapp_error_quark ()

typedef enum {
  WEBAPP_ERROR_FAILED = 1001,
  WEBAPP_ERROR_EXISTS = 1002
} WebappErrorCode;

char *
ephy_web_application_get_app_id_from_name (const char *name)
{
  return g_compute_checksum_for_string (G_CHECKSUM_SHA1, name, -1);
}

static const char *
get_app_id_from_gapplication_id (const char *name)
{
  if (!g_str_has_prefix (name, EPHY_WEB_APP_GAPPLICATION_ID_PREFIX)) {
    g_warning ("GApplication ID %s does not begin with required prefix %s",
               name, EPHY_WEB_APP_GAPPLICATION_ID_PREFIX);
    return NULL;
  }

  return name + strlen (EPHY_WEB_APP_GAPPLICATION_ID_PREFIX);
}

static char *
get_gapplication_id_from_id (const char *id)
{
  g_autofree char *gapplication_id = NULL;

  gapplication_id = g_strconcat (EPHY_WEB_APP_GAPPLICATION_ID_PREFIX, id, NULL);
  if (!g_application_id_is_valid (gapplication_id))
    g_error ("Failed to get GApplication ID from app ID %s", id);

  return g_steal_pointer (&gapplication_id);
}

static char *
get_app_profile_directory_name (const char *id)
{
  return get_gapplication_id_from_id (id);
}

static char *
get_app_desktop_filename (const char *id)
{
  g_autofree char *gapplication_id = NULL;

  /* Warning: the GApplication ID must exactly match the desktop file's
   * basename. Don't overthink this or stuff will break, e.g. GNotification.
   */
  gapplication_id = get_gapplication_id_from_id (id);

  return g_strconcat (gapplication_id, ".desktop", NULL);
}

const char *
ephy_web_application_get_gapplication_id_from_profile_directory (const char *profile_dir)
{
  const char *name;

  /* Just get the basename */
  name = strrchr (profile_dir, G_DIR_SEPARATOR);
  if (!name) {
    g_warning ("Profile directory %s is not a valid path", profile_dir);
    return NULL;
  }

  name++; /* Strip '/' */

  /* Legacy web app support */
  if (g_str_has_prefix (name, "app-"))
    name += strlen ("app-");

  if (!g_str_has_prefix (name, EPHY_WEB_APP_GAPPLICATION_ID_PREFIX)) {
    g_warning ("Profile directory %s does not begin with required web app prefix %s",
               profile_dir, EPHY_WEB_APP_GAPPLICATION_ID_PREFIX);
    return NULL;
  }

  return name;
}

static const char *
get_app_id_from_profile_directory (const char *profile_dir)
{
  const char *gapplication_id;

  gapplication_id = ephy_web_application_get_gapplication_id_from_profile_directory (profile_dir);
  return gapplication_id ? get_app_id_from_gapplication_id (gapplication_id) : NULL;
}

static char *
ephy_web_application_get_directory_under (const char *id,
                                          const char *path)
{
  g_autofree char *app_dir = NULL;

  app_dir = get_app_profile_directory_name (id);
  if (!app_dir)
    return NULL;

  return g_build_filename (path, app_dir, NULL);
}

/**
 * ephy_web_application_get_profile_directory:
 * @id: the application identifier
 *
 * Gets the directory where the profile for @id is meant to be stored.
 *
 * Returns: (transfer full): A newly allocated string.
 **/
char *
ephy_web_application_get_profile_directory (const char *id)
{
  return ephy_web_application_get_directory_under (id, g_get_user_data_dir ());
}

static char *
ephy_web_application_get_cache_directory (const char *id)
{
  return ephy_web_application_get_directory_under (id, g_get_user_cache_dir ());
}

static char *
ephy_web_application_get_config_directory (const char *id)
{
  return ephy_web_application_get_directory_under (id, g_get_user_config_dir ());
}

/**
 * ephy_web_application_delete:
 * @id: the identifier of the web application to delete
 * @out_app_found: return location for a #EphyWebAppFound. This will be set to
 *   %EPHY_WEB_APP_NOT_FOUND if deleting the app failed due to it not being
 *   installed, and %EPHY_WEB_APP_FOUND otherwise.
 *
 * Deletes all the data associated with a Web Application created by
 * Epiphany.
 *
 * Returns: %TRUE if the web app was successfully deleted, %FALSE otherwise
 **/
gboolean
ephy_web_application_delete (const char      *id,
                             EphyWebAppFound *out_app_found)
{
  g_autofree char *profile_dir = NULL;
  g_autofree char *cache_dir = NULL;
  g_autofree char *config_dir = NULL;
  g_autofree char *desktop_file = NULL;
  g_autoptr (GError) error = NULL;
  XdpPortal *portal = ephy_get_portal ();

  g_assert (id);

  if (out_app_found)
    *out_app_found = EPHY_WEB_APP_FOUND;

  profile_dir = ephy_web_application_get_profile_directory (id);
  if (!profile_dir)
    return FALSE;

  /* If there's no profile dir for this app, it means it does not
   * exist. */
  if (!g_file_test (profile_dir, G_FILE_TEST_IS_DIR)) {
    g_warning ("No application with id '%s' is installed.", id);
    if (out_app_found)
      *out_app_found = EPHY_WEB_APP_NOT_FOUND;
    return FALSE;
  }

  if (!ephy_file_delete_dir_recursively (profile_dir, &error)) {
    g_warning ("Failed to recursively delete %s: %s", profile_dir, error->message);
    return FALSE;
  }
  LOG ("Deleted application profile.");

  cache_dir = ephy_web_application_get_cache_directory (id);
  if (g_file_test (cache_dir, G_FILE_TEST_IS_DIR)) {
    if (!ephy_file_delete_dir_recursively (cache_dir, &error)) {
      g_warning ("Failed to recursively delete %s: %s", cache_dir, error->message);
      return FALSE;
    }
    LOG ("Deleted application cache directory.");
  }

  config_dir = ephy_web_application_get_config_directory (id);
  if (g_file_test (config_dir, G_FILE_TEST_IS_DIR)) {
    if (!ephy_file_delete_dir_recursively (config_dir, &error)) {
      g_warning ("Failed to recursively delete %s: %s", config_dir, error->message);
      return FALSE;
    }
    LOG ("Deleted application config directory.");
  }

  desktop_file = get_app_desktop_filename (id);
  if (!desktop_file) {
    g_warning ("Failed to compute desktop filename for app %s", id);
    return FALSE;
  }

  if (!xdp_portal_dynamic_launcher_uninstall (portal, desktop_file, &error)) {
    g_warning ("Failed to uninstall desktop file using portal: %s", error->message);
    return FALSE;
  }

  LOG ("Deleted application launcher %s.", desktop_file);

  return TRUE;
}

/**
 * ephy_web_application_delete_by_desktop_file_id:
 * @desktop_file_id: the .desktop file name for the web app to be deleted, with
 *   the extension
 * @out_app_found: return location for a #EphyWebAppFound. This will be set to
 *   %EPHY_WEB_APP_NOT_FOUND if deleting the app failed due to it not being
 *   installed, and %EPHY_WEB_APP_FOUND otherwise.
 *
 * Deletes all the data associated with a Web Application created by
 * Epiphany.
 *
 * Returns: %TRUE if the web app was successfully deleted, %FALSE otherwise
 **/
gboolean
ephy_web_application_delete_by_desktop_file_id (const char      *desktop_file_id,
                                                EphyWebAppFound *out_app_found)
{
  const char *id;
  g_autofree char *gapp_id = NULL;

  g_assert (desktop_file_id);

  gapp_id = g_strdup (desktop_file_id);
  if (g_str_has_suffix (desktop_file_id, ".desktop"))
    gapp_id[strlen (desktop_file_id) - strlen (".desktop")] = '\0';

  id = get_app_id_from_gapplication_id (gapp_id);

  return ephy_web_application_delete (id, out_app_found);
}

static gboolean
create_desktop_file (const char  *id,
                     const char  *address,
                     const char  *profile_dir,
                     const char  *install_token,
                     GError     **error)
{
  g_autofree char *filename = NULL;
  g_autoptr (GKeyFile) file = NULL;
  g_autofree char *exec_string = NULL;
  g_autofree char *wm_class = NULL;
  g_autofree char *desktop_entry = NULL;
  XdpPortal *portal = ephy_get_portal ();

  g_assert (profile_dir);

  filename = get_app_desktop_filename (id);
  if (!filename) {
    g_set_error (error, WEBAPP_ERROR, WEBAPP_ERROR_FAILED,
                 _("Failed to get desktop filename for webapp id %s"), id);
    return FALSE;
  }

  file = g_key_file_new ();
  exec_string = g_strdup_printf ("epiphany --application-mode \"--profile=%s\" %s",
                                 profile_dir,
                                 address);
  g_key_file_set_value (file, "Desktop Entry", "Exec", exec_string);
  g_key_file_set_value (file, "Desktop Entry", "StartupNotify", "true");
  g_key_file_set_value (file, "Desktop Entry", "Terminal", "false");
  g_key_file_set_value (file, "Desktop Entry", "Type", "Application");
  g_key_file_set_value (file, "Desktop Entry", "Categories", "GNOME;GTK;");

  wm_class = g_strconcat (EPHY_WEB_APP_GAPPLICATION_ID_PREFIX, id, NULL);
  g_key_file_set_value (file, "Desktop Entry", "StartupWMClass", wm_class);

  g_key_file_set_value (file, "Desktop Entry", "X-Purism-FormFactor", "Workstation;Mobile;");

  desktop_entry = g_key_file_to_data (file, NULL, NULL);

  if (!xdp_portal_dynamic_launcher_install (portal, install_token, filename,
                                            desktop_entry, error)) {
    g_prefix_error (error, _("Failed to install desktop file %s: "), filename);
    ephy_file_delete_dir_recursively (profile_dir, NULL);
    return FALSE;
  }

  LOG ("Created application launcher %s.", filename);

  return TRUE;
}

/**
 * ephy_web_application_create:
 * @id: the identifier for the new web application
 * @address: the address of the new web application
 * @name: the name for the new web application
 * @install_token: the install token acquired via portal methods
 * @options: the options for the new web application
 * @error: return location for a GError pointer
 *
 * Creates a new Web Application for @address.
 *
 * Returns: %TRUE on success, %FALSE on failure with @error set
 **/
gboolean
ephy_web_application_create (const char                 *id,
                             const char                 *address,
                             const char                 *install_token,
                             EphyWebApplicationOptions   options,
                             GError                    **error)
{
  g_autofree char *app_file = NULL;
  g_autofree char *profile_dir = NULL;
  int fd;

  /* If there's already a WebApp profile for the contents of this
   * view, do nothing. */
  profile_dir = ephy_web_application_get_profile_directory (id);
  if (g_file_test (profile_dir, G_FILE_TEST_IS_DIR)) {
    g_set_error (error, WEBAPP_ERROR, WEBAPP_ERROR_EXISTS,
                 _("Profile directory %s already exists"), profile_dir);
    return FALSE;
  }

  /* Create the profile directory, populate it. */
  if (g_mkdir_with_parents (profile_dir, 488) == -1) {
    g_set_error (error, WEBAPP_ERROR, WEBAPP_ERROR_FAILED,
                 _("Failed to create directory %s"), profile_dir);
    return FALSE;
  }

  /* Skip migration for new web apps. */
  ephy_profile_utils_set_migration_version_for_profile_dir (EPHY_PROFILE_MIGRATION_VERSION, profile_dir);

  /* Create an .app file. */
  app_file = g_build_filename (profile_dir, ".app", NULL);
  fd = g_open (app_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) {
    g_set_error (error, WEBAPP_ERROR, WEBAPP_ERROR_FAILED,
                 _("Failed to create .app file: %s"), g_strerror (errno));
    return FALSE;
  }
  close (fd);

  /* Create the desktop file. */
  if (!create_desktop_file (id, address, profile_dir, install_token, error))
    return FALSE;

  ephy_web_application_initialize_settings (profile_dir, options);

  return TRUE;
}

char *
ephy_web_application_ensure_for_app_info (GAppInfo *app_info)
{
  g_autofree char *id = NULL;
  g_autofree char *profile_dir = NULL;
  g_autofree char *app_file = NULL;
  int fd;

  id = ephy_web_application_get_app_id_from_name (g_app_info_get_name (app_info));
  profile_dir = ephy_web_application_get_profile_directory (id);

  /* Create the profile directory, populate it. */
  if (g_mkdir (profile_dir, 488) == -1) {
    if (errno == EEXIST)
      return g_steal_pointer (&profile_dir);

    return NULL;
  }

  /* Skip migration for new web apps. */
  ephy_profile_utils_set_migration_version_for_profile_dir (EPHY_PROFILE_MIGRATION_VERSION, profile_dir);

  /* Create an .app file. */
  app_file = g_build_filename (profile_dir, ".app", NULL);
  fd = g_open (app_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) {
    g_warning ("Failed to create .app file: %s", g_strerror (errno));
    return NULL;
  }
  close (fd);

  /* Create the desktop file. */
  if (G_IS_DESKTOP_APP_INFO (app_info)) {
    const char *source_name = NULL;
    g_autofree char *dest_name = NULL;
    g_autofree char *desktop_basename = NULL;
    g_autoptr (GFile) source = NULL;
    g_autoptr (GFile) dest = NULL;
    g_autoptr (GError) error = NULL;

    source_name = g_desktop_app_info_get_filename (G_DESKTOP_APP_INFO (app_info));
    source = g_file_new_for_path (source_name);

    desktop_basename = get_app_desktop_filename (id);
    dest_name = g_build_filename (profile_dir, desktop_basename, NULL);
    dest = g_file_new_for_path (dest_name);

    g_file_copy (source, dest, G_FILE_COPY_NONE, NULL, NULL, NULL, &error);

    if (error)
      g_warning ("Couldn't copy desktop file: %s", error->message);

    ephy_web_application_initialize_settings (profile_dir, EPHY_WEB_APPLICATION_SYSTEM);
  }

  return g_steal_pointer (&profile_dir);
}

GKeyFile *
ephy_web_application_get_desktop_keyfile (const char  *id,
                                          GError     **error)
{
  XdpPortal *portal = ephy_get_portal ();
  g_autofree char *desktop_basename = NULL;
  g_autofree char *contents = NULL;
  g_autoptr (GKeyFile) key_file = NULL;

  desktop_basename = get_app_desktop_filename (id);
  contents = xdp_portal_dynamic_launcher_get_desktop_entry (portal, desktop_basename, error);
  if (!contents)
    return NULL;

  key_file = g_key_file_new ();
  if (!g_key_file_load_from_data (key_file, contents, -1, G_KEY_FILE_NONE, error))
    return NULL;

  return g_steal_pointer (&key_file);
}

void
ephy_web_application_setup_from_profile_directory (const char *profile_directory)
{
  const char *gapplication_id;
  const char *id;
  g_autoptr (GKeyFile) desktop_keyfile = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (profile_directory);

  gapplication_id = ephy_web_application_get_gapplication_id_from_profile_directory (profile_directory);
  if (!gapplication_id)
    g_error ("Failed to get GApplication ID from profile directory %s", profile_directory);

  /* FIXME: is this actually necessary? Probably not? */
  g_set_prgname (gapplication_id);

  id = get_app_id_from_gapplication_id (gapplication_id);
  if (!id)
    g_error ("Failed to get app ID from GApplication ID %s", gapplication_id);

  /* Get display name from desktop file */
  desktop_keyfile = ephy_web_application_get_desktop_keyfile (id, &error);
  if (!desktop_keyfile) {
    g_warning ("Required desktop file '%s' not available: %s", gapplication_id, error->message);
    g_clear_error (&error);
  } else {
    g_autofree char *name = NULL;
    name = g_key_file_get_string (desktop_keyfile, "Desktop Entry", "Name", NULL);
    if (!name)
      g_warning ("Missing name in desktop file '%s'", gapplication_id);
    else
      g_set_application_name (name);
  }
}

void
ephy_web_application_setup_from_desktop_file (GDesktopAppInfo *desktop_info)
{
  GAppInfo *app_info;

  g_assert (G_IS_DESKTOP_APP_INFO (desktop_info));

  app_info = G_APP_INFO (desktop_info);
  g_set_prgname (g_app_info_get_name (app_info));
  g_set_application_name (g_app_info_get_display_name (app_info));
}

void
ephy_web_application_launch (const char *id)
{
  XdpPortal *portal = ephy_get_portal ();
  g_autofree char *desktop_basename = NULL;
  g_autoptr (GError) error = NULL;

  desktop_basename = get_app_desktop_filename (id);
  if (!xdp_portal_dynamic_launcher_launch (portal, desktop_basename, NULL, &error))
    g_warning ("Failed to launch app '%s': %s", desktop_basename, error->message);
}

void
ephy_web_application_free (EphyWebApplication *app)
{
  g_free (app->id);
  g_free (app->name);
  g_free (app->icon_path);
  g_free (app->tmp_icon_path);
  g_free (app->url);
  g_free (app->desktop_file);
  g_free (app->desktop_path);
  g_free (app);
}

static char *
ephy_web_application_get_tmp_icon_path (const char  *desktop_path,
                                        GError     **error)
{
  XdpPortal *portal = ephy_get_portal ();
  g_autoptr (GVariant) icon_v = NULL;
  g_autofree char *icon_format = NULL;
  g_autofree char *desktop_basename = NULL;
  g_autofree char *tmp_file_name = NULL;
  g_autofree char *tmp_file_path = NULL;
  g_autoptr (GIcon) icon = NULL;
  g_autoptr (GOutputStream) stream = NULL;
  GBytes *bytes;
  gconstpointer bytes_data;
  gsize bytes_len;
  int fd;

  g_return_val_if_fail (desktop_path, NULL);

  /* This function is only useful inside the sandbox since the icon file on the
   * host is inaccessible in that case.
   */
  g_assert (ephy_is_running_inside_sandbox ());

  desktop_basename = g_path_get_basename (desktop_path);
  icon_v = xdp_portal_dynamic_launcher_get_icon (portal, desktop_basename, &icon_format, NULL, error);
  if (!icon_v)
    return NULL;

  tmp_file_name = ephy_file_tmp_filename (".ephy-webapp-icon-XXXXXX", icon_format);
  tmp_file_path = g_build_filename (ephy_file_tmp_dir (), tmp_file_name, NULL);

  icon = g_icon_deserialize (icon_v);
  if (!icon) {
    g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                         "Icon deserialization failed");
    return NULL;
  }

  if (!G_IS_BYTES_ICON (icon)) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                 "Unexpected icon type: %s", G_OBJECT_TYPE_NAME (icon));
    return NULL;
  }

  bytes = g_bytes_icon_get_bytes (G_BYTES_ICON (icon));
  fd = g_open (tmp_file_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (fd == -1) {
    g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errno),
                 "Failed to create tmpfile ‘%s’: %s",
                 tmp_file_path, g_strerror (errno));
    return NULL;
  }

  stream = g_unix_output_stream_new (fd, TRUE);
  /* Use write_all() instead of write_bytes() so we don't have to worry about
   * partial writes (https://gitlab.gnome.org/GNOME/glib/-/issues/570).
   */
  bytes_data = g_bytes_get_data (bytes, &bytes_len);
  if (!g_output_stream_write_all (stream, bytes_data, bytes_len, NULL, NULL, error))
    return NULL;

  if (!g_output_stream_close (stream, NULL, error))
    return NULL;

  return g_steal_pointer (&tmp_file_path);
}

EphyWebApplication *
ephy_web_application_for_profile_directory (const char            *profile_dir,
                                            EphyWebAppNeedTmpIcon  need_tmp_icon)
{
  g_autoptr (EphyWebApplication) app = NULL;
  const char *id;
  g_autoptr (GDesktopAppInfo) desktop_info = NULL;
  const char *exec;
  int argc;
  g_auto (GStrv) argv = NULL;
  g_autoptr (GFile) file = NULL;
  g_autoptr (GFileInfo) file_info = NULL;
  g_autoptr (GError) error = NULL;
  g_autoptr (GKeyFile) key_file = NULL;

  id = get_app_id_from_profile_directory (profile_dir);
  if (!id)
    return NULL;

  app = g_new0 (EphyWebApplication, 1);
  app->id = g_strdup (id);

  app->desktop_path = ephy_web_application_get_desktop_path (id);
  if (ephy_can_install_web_apps ()) {
    key_file = ephy_web_application_get_desktop_keyfile (id, &error);
    if (!key_file) {
      g_warning ("Failed to get desktop keyfile for id %s from portal: %s", id, error->message);
      g_clear_pointer (&app, ephy_web_application_free); /* avoid a scan-build warning */
      return NULL;
    }

    app->name = g_key_file_get_string (key_file, "Desktop Entry", "Name", NULL);
    app->icon_path = g_key_file_get_string (key_file, "Desktop Entry", "Icon", NULL);

    if (ephy_is_running_inside_sandbox () && need_tmp_icon == EPHY_WEB_APP_NEED_TMP_ICON) {
      app->tmp_icon_path = ephy_web_application_get_tmp_icon_path (app->desktop_path, &error);
      if (!app->tmp_icon_path)
        g_warning ("Failed to get tmp icon path for app %s: %s", app->id, error->message);
    }

    exec = g_key_file_get_string (key_file, "Desktop Entry", "Exec", NULL);
    if (g_shell_parse_argv (exec, &argc, &argv, NULL))
      app->url = g_strdup (argv[argc - 1]);

    file = g_file_new_for_path (profile_dir);

    file_info = g_file_query_info (file, G_FILE_ATTRIBUTE_TIME_CREATED, G_FILE_QUERY_INFO_NONE, NULL, NULL);
    app->install_date_uint64 = g_file_info_get_attribute_uint64 (file_info, G_FILE_ATTRIBUTE_TIME_CREATED);

    return g_steal_pointer (&app);
  }

  /* sandbox should take the code path above */
  if (ephy_is_running_inside_sandbox ()) {
    g_warning ("Epiphany is sandboxed but the DynamicLauncher portal is unavailable; can't use web app functionality");
    return NULL;
  }

  desktop_info = g_desktop_app_info_new_from_filename (app->desktop_path);
  if (!desktop_info) {
    g_clear_pointer (&app, ephy_web_application_free); /* avoid a scan-build warning */
    return NULL;
  }

  app->name = g_strdup (g_app_info_get_name (G_APP_INFO (desktop_info)));
  app->icon_path = g_desktop_app_info_get_string (desktop_info, "Icon");
  exec = g_app_info_get_commandline (G_APP_INFO (desktop_info));
  if (g_shell_parse_argv (exec, &argc, &argv, NULL))
    app->url = g_strdup (argv[argc - 1]);

  file = g_file_new_for_path (app->desktop_path);

  file_info = g_file_query_info (file, G_FILE_ATTRIBUTE_TIME_CREATED, 0, NULL, NULL);
  app->install_date_uint64 = g_file_info_get_attribute_uint64 (file_info, G_FILE_ATTRIBUTE_TIME_CREATED);

  return g_steal_pointer (&app);
}

/**
 * ephy_web_application_get_application_list:
 *
 * Gets a list of the currently installed web applications.
 * Free the returned GList with
 * ephy_web_application_free_application_list.
 *
 * Returns: (transfer-full): a #GList of #EphyWebApplication objects
 **/
GList *
ephy_web_application_get_application_list (void)
{
  g_autoptr (GFileEnumerator) children = NULL;
  GList *applications = NULL;
  g_autofree char *parent_directory_path = NULL;
  g_autoptr (GFile) parent_directory = NULL;

  parent_directory_path = g_strdup (g_get_user_data_dir ());

  parent_directory = g_file_new_for_path (parent_directory_path);
  children = g_file_enumerate_children (parent_directory,
                                        "standard::name",
                                        0, NULL, NULL);
  if (!children)
    return NULL;

  for (;;) {
    g_autoptr (GFileInfo) info = g_file_enumerator_next_file (children, NULL, NULL);
    const char *name;

    if (!info)
      break;

    name = g_file_info_get_name (info);
    if (g_str_has_prefix (name, EPHY_WEB_APP_GAPPLICATION_ID_PREFIX)) {
      g_autoptr (EphyWebApplication) app = NULL;
      g_autofree char *profile_dir = NULL;

      profile_dir = g_build_filename (parent_directory_path, name, NULL);
      app = ephy_web_application_for_profile_directory (profile_dir, EPHY_WEB_APP_NEED_TMP_ICON);
      if (app) {
        g_autofree char *app_file = g_build_filename (profile_dir, ".app", NULL);
        if (g_file_test (app_file, G_FILE_TEST_EXISTS))
          applications = g_list_prepend (applications, g_steal_pointer (&app));
      }
    }
  }

  return g_list_reverse (applications);
}

/**
 * ephy_web_application_free_application_list:
 * @list: an #EphyWebApplication GList
 *
 * Frees a @list as given by ephy_web_application_get_application_list.
 **/
void
ephy_web_application_free_application_list (GList *list)
{
  g_list_free_full (list, (GDestroyNotify)ephy_web_application_free);
}

/**
 * ephy_web_application_get_desktop_id_list:
 *
 * Gets a list of the currently installed web applications' .desktop filenames.
 * This is useful even though we don't have access to the actual .desktop files
 * when running under Flatpak, because we return it over D-Bus in the
 * WebAppProvider service.
 *
 * Returns: (transfer-full): a %NULL-terminated array of strings, which may be empty
 **/
char **
ephy_web_application_get_desktop_id_list (void)
{
  g_autoptr (GFileEnumerator) children = NULL;
  g_autoptr (GFile) parent_directory = NULL;
  g_autoptr (GPtrArray) desktop_file_ids = g_ptr_array_new_with_free_func (g_free);

  parent_directory = g_file_new_for_path (g_get_user_data_dir ());
  children = g_file_enumerate_children (parent_directory,
                                        "standard::name",
                                        0, NULL, NULL);
  if (!children)
    goto out;

  for (;;) {
    g_autoptr (GFileInfo) info = g_file_enumerator_next_file (children, NULL, NULL);
    const char *name;

    if (!info)
      break;

    name = g_file_info_get_name (info);
    if (g_str_has_prefix (name, EPHY_WEB_APP_GAPPLICATION_ID_PREFIX)) {
      g_autofree char *desktop_file_id = NULL;
      desktop_file_id = g_strconcat (name, ".desktop", NULL);
      g_ptr_array_add (desktop_file_ids, g_steal_pointer (&desktop_file_id));
    }
  }

out:
  g_ptr_array_add (desktop_file_ids, NULL);

  return (char **)g_ptr_array_free (g_steal_pointer (&desktop_file_ids), FALSE);
}

/**
 * ephy_web_application_exists:
 * @id: the potential identifier of the web application
 *
 * Returns: whether an application with @id exists.
 **/
gboolean
ephy_web_application_exists (const char *id)
{
  g_autofree char *profile_dir = NULL;

  profile_dir = ephy_web_application_get_profile_directory (id);
  return g_file_test (profile_dir, G_FILE_TEST_IS_DIR);
}

void
ephy_web_application_initialize_settings (const char                *profile_directory,
                                          EphyWebApplicationOptions  options)
{
  g_autoptr (GSettings) settings = NULL;
  g_autoptr (GSettings) web_app_settings = NULL;
  g_autofree char *name = NULL;
  g_autofree char *path = NULL;

  name = g_path_get_basename (profile_directory);
  settings = g_settings_new_with_path (EPHY_PREFS_WEB_SCHEMA, "/org/gnome/epiphany/web/");

  path = g_build_path ("/", "/org/gnome/epiphany/web-apps/", name, "web/", NULL);
  web_app_settings = g_settings_new_with_path (EPHY_PREFS_WEB_SCHEMA, path);

  for (guint i = 0; i < G_N_ELEMENTS (ephy_prefs_web_schema); i++) {
    g_autoptr (GVariant) value = NULL;

    value = g_settings_get_value (settings, ephy_prefs_web_schema[i]);
    g_settings_set_value (web_app_settings, ephy_prefs_web_schema[i], value);
  }

  g_clear_object (&settings);
  settings = g_settings_new_with_path (EPHY_PREFS_STATE_SCHEMA, "/org/gnome/epiphany/state/");

  g_clear_pointer (&path, g_free);
  path = g_build_path ("/", "/org/gnome/epiphany/web-apps/", name, "state/", NULL);
  g_clear_object (&web_app_settings);
  web_app_settings = g_settings_new_with_path (EPHY_PREFS_STATE_SCHEMA, path);

  for (guint i = 0; i < G_N_ELEMENTS (ephy_prefs_state_schema); i++) {
    g_autoptr (GVariant) value = NULL;

    value = g_settings_get_value (settings, ephy_prefs_state_schema[i]);
    g_settings_set_value (web_app_settings, ephy_prefs_state_schema[i], value);
  }

  if (options) {
    g_clear_pointer (&path, g_free);
    path = g_build_path ("/", "/org/gnome/epiphany/web-apps/", name, "webapp/", NULL);
    g_clear_object (&web_app_settings);
    web_app_settings = g_settings_new_with_path (EPHY_PREFS_WEB_APP_SCHEMA, path);

    if (options & EPHY_WEB_APPLICATION_MOBILE_CAPABLE)
      g_settings_set_boolean (web_app_settings, EPHY_PREFS_WEB_APP_SHOW_NAVIGATION_BUTTONS, TRUE);

    if (options & EPHY_WEB_APPLICATION_SYSTEM)
      g_settings_set_boolean (web_app_settings, EPHY_PREFS_WEB_APP_SYSTEM, TRUE);
  }
}

static gboolean
urls_have_same_base_domain (const char *a_url,
                            const char *b_url)
{
  g_autoptr (GUri) a_uri = NULL;
  g_autoptr (GUri) b_uri = NULL;
  g_autoptr (GError) error = NULL;
  g_autofree char *a_base = NULL;
  g_autofree char *b_base = NULL;

  a_uri = g_uri_parse (a_url, G_URI_FLAGS_PARSE_RELAXED, NULL);
  if (!a_uri || !g_uri_get_host (a_uri))
    return FALSE;

  b_uri = g_uri_parse (b_url, G_URI_FLAGS_PARSE_RELAXED, NULL);
  if (!b_uri || !g_uri_get_host (b_uri))
    return FALSE;

  if (strcmp (g_uri_get_scheme (a_uri), g_uri_get_scheme (b_uri)) != 0)
    return FALSE;

  if (g_uri_get_port (a_uri) != g_uri_get_port (b_uri))
    return FALSE;

  /* Compare base domains */
  a_base = ephy_uri_get_base_domain (g_uri_get_host (a_uri));
  b_base = ephy_uri_get_base_domain (g_uri_get_host (b_uri));

  if (!a_base || !b_base)
    return FALSE;

  return g_ascii_strcasecmp (a_base, b_base) == 0;
}

gboolean
ephy_web_application_is_uri_allowed (const char *uri)
{
  g_autoptr (EphyWebApplication) webapp = ephy_web_application_for_profile_directory (ephy_profile_dir (),
                                                                                      EPHY_WEB_APP_NO_TMP_ICON);
  const char *scheme;
  g_auto (GStrv) urls = NULL;
  guint i;
  gboolean matched = FALSE;

  g_assert (webapp);

  if (g_str_has_prefix (uri, "blob:") || g_str_has_prefix (uri, "data:"))
    return TRUE;

  if (urls_have_same_base_domain (uri, webapp->url))
    return TRUE;

  if (g_strcmp0 (uri, "about:blank") == 0)
    return TRUE;

  scheme = g_uri_peek_scheme (uri);
  if (!scheme)
    return FALSE;

  urls = g_settings_get_strv (EPHY_SETTINGS_WEB_APP, EPHY_PREFS_WEB_APP_ADDITIONAL_URLS);
  for (i = 0; urls[i] && !matched; i++) {
    if (!strstr (urls[i], "://")) {
      g_autofree char *url = NULL;

      url = g_strdup_printf ("%s://%s", scheme, urls[i]);

      matched = urls_have_same_base_domain (uri, url);
    } else {
      matched = urls_have_same_base_domain (uri, urls[i]);
    }
  }

  return matched;
}

static void
ephy_web_icon_copy_cb (GFile        *file,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  g_autoptr (GError) error = NULL;
  if (!g_file_copy_finish (file, result, &error))
    g_warning ("Failed to update web app icon: %s", error->message);
}

gboolean
ephy_web_application_save (EphyWebApplication *app)
{
  g_autoptr (GKeyFile) keyfile = NULL;
  g_autofree char *contents = NULL;
  g_autofree char *name = NULL;
  g_autofree char *icon = NULL;
  g_autofree char *exec = NULL;
  g_auto (GStrv) strings = NULL;
  guint exec_length;
  gboolean changed = FALSE;
  gboolean saved = FALSE;
  g_autoptr (GError) error = NULL;

  g_assert (!ephy_is_running_inside_sandbox ());

  if (!g_file_get_contents (app->desktop_path, &contents, NULL, &error)) {
    g_warning ("Failed to load desktop file of web application: %s", error->message);
    return FALSE;
  }

  keyfile = g_key_file_new ();
  g_key_file_load_from_data (keyfile, contents, -1, 0, NULL);

  name = g_key_file_get_string (keyfile, "Desktop Entry", "Name", NULL);
  if (g_strcmp0 (name, app->name) != 0) {
    changed = TRUE;
    g_key_file_set_string (keyfile, "Desktop Entry", "Name", app->name);
  }

  icon = g_key_file_get_string (keyfile, "Desktop Entry", "Icon", NULL);
  if (g_strcmp0 (icon, app->icon_path) != 0) {
    g_autoptr (GFile) new_icon = NULL;
    g_autoptr (GFile) old_icon = NULL;
    changed = TRUE;
    new_icon = g_file_new_for_path (app->icon_path);
    old_icon = g_file_new_for_path (icon);
    g_file_copy_async (new_icon, old_icon, G_FILE_COPY_OVERWRITE,
                       G_PRIORITY_DEFAULT, NULL, NULL, NULL,
                       (GAsyncReadyCallback)ephy_web_icon_copy_cb, NULL);
  }

  exec = g_key_file_get_string (keyfile, "Desktop Entry", "Exec", NULL);
  strings = g_strsplit (exec, " ", -1);

  exec_length = g_strv_length (strings);
  if (g_strcmp0 (strings[exec_length - 1], app->url) != 0) {
    changed = TRUE;
    g_free (strings[exec_length - 1]);
    strings[exec_length - 1] = g_strdup (app->url);
    g_free (exec);
    exec = g_strjoinv (" ", strings);
    g_key_file_set_string (keyfile, "Desktop Entry", "Exec", exec);
  }

  if (changed) {
    char *resolved_path = realpath (app->desktop_path, NULL);
    if (!resolved_path) {
      g_warning ("Failed to save web application %s: failed to resolve path %s: %s", app->name, app->desktop_path, g_strerror (errno));
      return FALSE;
    }

    saved = g_key_file_save_to_file (keyfile, resolved_path, &error);
    if (!saved)
      g_warning ("Failed to save web application %s desktop file %s: %s", app->name, resolved_path, error->message);
    free (resolved_path);
  }

  return saved;
}

gboolean
ephy_web_application_is_system (EphyWebApplication *app)
{
  GSettings *web_app_settings;
  g_autofree char *profile_directory = NULL;
  g_autofree char *name = NULL;
  g_autofree char *path = NULL;

  profile_directory = ephy_web_application_get_profile_directory (app->id);
  name = g_path_get_basename (profile_directory);

  path = g_build_path ("/", "/org/gnome/epiphany/web-apps/", name, "webapp/", NULL);
  web_app_settings = g_settings_new_with_path (EPHY_PREFS_WEB_APP_SCHEMA, path);

  return g_settings_get_boolean (web_app_settings, EPHY_PREFS_WEB_APP_SYSTEM);
}

char *
ephy_web_application_get_desktop_path (const char *id)
{
  g_autofree char *desktop_basename = NULL;

  desktop_basename = get_app_desktop_filename (id);

  /* Note: When running under Flatpak, this path is wrong since the
   * $XDG_DATA_HOME will be different than on the host. Under Flatpak we only
   * get the basename from this path and ignore the rest.
   */
  return g_build_filename (g_get_user_data_dir (), "applications", desktop_basename, NULL);
}
