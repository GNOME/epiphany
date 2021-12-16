/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2011 Igalia S.L.
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
#include "ephy-profile-utils.h"
#include "ephy-settings.h"

#include <errno.h>
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

/* Web apps are installed in the default data dir of the user. Every
 * app has its own profile directory. To create a web app, an ID needs
 * to be generated using the given name as <normalized-name>-checksum.
 * The ID is used to uniquely identify the app.
 *
 *  - Name: the user-visible pretty app name
 *  - Normalized name: lowercase name
 *  - Checksum: SHA-1 of name
 *  - ID: <normalized-name>-<checksum>
 *  - GApplication ID: see below
 *  - Profile directory: <gapplication-id>
 *  - Desktop file: <profile-dir>/<gapplication-id>.desktop
 *
 * Note that our ID and GApplication ID are different. Yes, this is confusing.
 *
 * For GApplication ID, there are two cases:
 *
 *  - If <id> is safe to use in a D-Bus identifier,
 *    then: GApplication ID is org.gnome.Epiphany.WebApp-<id>
 *  - otherwise: GAppliation ID is org.gnome.Epiphany.WebApp-<checksum>
 *
 * System web applications have a profile dir without a desktop file.
 */

#define EPHY_WEB_APP_GAPPLICATION_ID_PREFIX "org.gnome.Epiphany.WebApp-"

char *
ephy_web_application_get_app_id_from_name (const char *name)
{
  g_autofree char *normal_id = NULL;
  g_autofree char *checksum = NULL;

  /* FIXME: We should not use hyphens in app IDs. Sadly, changing this requires
   * a new migration.
   */
  normal_id = g_utf8_strdown (name, -1);
  g_strdelimit (normal_id, " ", '-');
  g_strdelimit (normal_id, G_DIR_SEPARATOR_S, '-');

  checksum = g_compute_checksum_for_string (G_CHECKSUM_SHA1, name, -1);
  return g_strdup_printf ("%s-%s", normal_id, checksum);
}

static char *
get_encoded_path (const char *path)
{
  g_autofree char *encoded = NULL;
  g_autoptr (GError) error = NULL;

  encoded = g_filename_from_utf8 (path, -1, NULL, NULL, &error);
  if (error) {
    g_warning ("%s", error->message);
    return NULL;
  }

  return g_steal_pointer (&encoded);
}

static const char *
get_app_id_from_gapplication_id (const char *name)
{
  if (!g_str_has_prefix (name, EPHY_WEB_APP_GAPPLICATION_ID_PREFIX)) {
    g_warning ("GApplication ID %s does not begin with required prefix %s", name, EPHY_WEB_APP_GAPPLICATION_ID_PREFIX);
    return NULL;
  }

  return name + strlen (EPHY_WEB_APP_GAPPLICATION_ID_PREFIX);
}

static char *
get_gapplication_id_from_id (const char *id)
{
  g_autofree char *gapplication_id = NULL;
  const char *final_hyphen;
  const char *checksum;

  /* FIXME: Ideally we would convert hyphens to underscores here, because
   * hyphens are not very friendly to D-Bus. However, changing this
   * would require a new profile migration, because the GApplication ID
   * must exactly match the desktop file basename.
   *
   * If we're willing to do another migration in the future, then we
   * should drop this path and always return just the prefix plus hash,
   * using underscores rather than hyphens.
   */
  gapplication_id = g_strconcat (EPHY_WEB_APP_GAPPLICATION_ID_PREFIX, id, NULL);
  if (g_application_id_is_valid (gapplication_id))
    return g_steal_pointer (&gapplication_id);

  /* Split ID into: <normalized-name>-<checksum> */
  final_hyphen = strrchr (id, '-');
  if (!final_hyphen) {
    g_warning ("Web app ID %s is broken: must contain a hyphen", id);
    return NULL;
  }
  checksum = final_hyphen + 1;

  if (*checksum == '\0') {
    g_warning ("Web app ID %s is broken: should end with checksum, not hyphen", id);
    return NULL;
  }

  /* We'll simply omit the <normalized-name> from the app ID, in order
   * to avoid problematic characters. Ideally we would use an underscore
   * here too, rather than a hyphen, but let's be consistent with
   * existing web apps.
   */
  g_clear_pointer (&gapplication_id, g_free);
  gapplication_id = g_strconcat (EPHY_WEB_APP_GAPPLICATION_ID_PREFIX, checksum, NULL);

  if (!g_application_id_is_valid (gapplication_id)) {
    g_warning ("Web app ID %s is broken: derived GApplication ID %s is not a valid app ID (is the final component alphanumeric?)", id, gapplication_id);
    return NULL;
  }

  return g_steal_pointer (&gapplication_id);
}

static char *
get_app_profile_directory_name (const char *id)
{
  g_autofree char *gapplication_id = NULL;

  gapplication_id = get_gapplication_id_from_id (id);
  if (!gapplication_id)
    g_error ("Failed to get GApplication ID from app ID %s", id);

  return get_encoded_path (gapplication_id);
}

static char *
get_app_desktop_filename (const char *id)
{
  g_autofree char *gapplication_id = NULL;
  g_autofree char *filename = NULL;

  /* Warning: the GApplication ID must exactly match the desktop file's
   * basename. Don't overthink this or stuff will break, e.g. GNotification.
   */
  gapplication_id = get_gapplication_id_from_id (id);
  if (!gapplication_id)
    g_error ("Failed to get GApplication ID from app ID %s", id);

  filename = g_strconcat (gapplication_id, ".desktop", NULL);
  return get_encoded_path (filename);
}

const char *
ephy_web_application_get_gapplication_id_from_profile_directory (const char *profile_dir)
{
  const char *name;

  /* Just get the basename */
  name = strrchr (profile_dir, G_DIR_SEPARATOR);
  if (name == NULL) {
    g_warning ("Profile directory %s is not a valid path", profile_dir);
    return NULL;
  }

  name++; /* Strip '/' */

  /* Legacy web app support */
  if (g_str_has_prefix (name, "app-"))
    name += strlen ("app-");

  if (!g_str_has_prefix (name, EPHY_WEB_APP_GAPPLICATION_ID_PREFIX)) {
    g_warning ("Profile directory %s does not begin with required web app prefix %s", profile_dir, EPHY_WEB_APP_GAPPLICATION_ID_PREFIX);
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
 * @id: the identifier of the web application do delete
 *
 * Deletes all the data associated with a Web Application created by
 * Epiphany.
 *
 * Returns: %TRUE if the web app was succesfully deleted, %FALSE otherwise
 **/
gboolean
ephy_web_application_delete (const char *id)
{
  g_autofree char *profile_dir = NULL;
  g_autofree char *cache_dir = NULL;
  g_autofree char *config_dir = NULL;
  g_autofree char *desktop_file = NULL;
  g_autofree char *desktop_path = NULL;
  g_autoptr (GFile) launcher = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (id);

  profile_dir = ephy_web_application_get_profile_directory (id);
  if (!profile_dir)
    return FALSE;

  /* If there's no profile dir for this app, it means it does not
   * exist. */
  if (!g_file_test (profile_dir, G_FILE_TEST_IS_DIR)) {
    g_warning ("No application with id '%s' is installed.\n", id);
    return FALSE;
  }

  if (!ephy_file_delete_dir_recursively (profile_dir, &error)) {
    g_warning ("Failed to recursively delete %s: %s", profile_dir, error->message);
    return FALSE;
  }
  LOG ("Deleted application profile.\n");

  cache_dir = ephy_web_application_get_cache_directory (id);
  if (g_file_test (cache_dir, G_FILE_TEST_IS_DIR)) {
    if (!ephy_file_delete_dir_recursively (cache_dir, &error)) {
      g_warning ("Failed to recursively delete %s: %s", cache_dir, error->message);
      return FALSE;
    }
    LOG ("Deleted application cache directory.\n");
  }

  config_dir = ephy_web_application_get_config_directory (id);
  if (g_file_test (config_dir, G_FILE_TEST_IS_DIR)) {
    if (!ephy_file_delete_dir_recursively (config_dir, &error)) {
      g_warning ("Failed to recursively delete %s: %s", config_dir, error->message);
      return FALSE;
    }
    LOG ("Deleted application config directory.\n");
  }

  desktop_file = get_app_desktop_filename (id);
  if (!desktop_file) {
    g_warning ("Failed to compute desktop filename for app %s", id);
    return FALSE;
  }

  desktop_path = g_build_filename (g_get_user_data_dir (), "applications", desktop_file, NULL);
  if (g_file_test (desktop_path, G_FILE_TEST_IS_SYMLINK)) {
    launcher = g_file_new_for_path (desktop_path);
    if (!g_file_delete (launcher, NULL, &error)) {
      g_warning ("Failed to delete %s: %s", desktop_path, error->message);
      return FALSE;
    }
    LOG ("Deleted application launcher.\n");
  }

  return TRUE;
}

static char *
create_desktop_file (const char *id,
                     const char *name,
                     const char *address,
                     const char *profile_dir,
                     GdkPixbuf  *icon)
{
  g_autofree char *filename = NULL;
  g_autoptr (GKeyFile) file = NULL;
  g_autofree char *exec_string = NULL;
  g_autofree char *wm_class = NULL;
  g_autofree char *data = NULL;
  g_autofree char *desktop_file_path = NULL;
  g_autofree char *apps_path = NULL;
  g_autofree char *link_path = NULL;
  g_autoptr (GFile) link = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (profile_dir);

  filename = get_app_desktop_filename (id);
  if (!filename)
    return NULL;

  file = g_key_file_new ();
  g_key_file_set_value (file, "Desktop Entry", "Name", name);
  exec_string = g_strdup_printf ("epiphany --application-mode \"--profile=%s\" %s",
                                 profile_dir,
                                 address);
  g_key_file_set_value (file, "Desktop Entry", "Exec", exec_string);
  g_key_file_set_value (file, "Desktop Entry", "StartupNotify", "true");
  g_key_file_set_value (file, "Desktop Entry", "Terminal", "false");
  g_key_file_set_value (file, "Desktop Entry", "Type", "Application");
  g_key_file_set_value (file, "Desktop Entry", "Categories", "GNOME;GTK;");

  if (icon) {
    g_autoptr (GOutputStream) stream = NULL;
    g_autofree char *path = NULL;
    g_autoptr (GFile) image = NULL;

    path = g_build_filename (profile_dir, EPHY_WEB_APP_ICON_NAME, NULL);
    image = g_file_new_for_path (path);

    stream = (GOutputStream *)g_file_create (image, 0, NULL, NULL);
    gdk_pixbuf_save_to_stream (icon, stream, "png", NULL, NULL, NULL);
    g_key_file_set_value (file, "Desktop Entry", "Icon", path);
  }

  wm_class = g_strconcat (EPHY_WEB_APP_GAPPLICATION_ID_PREFIX, id, NULL);
  g_key_file_set_value (file, "Desktop Entry", "StartupWMClass", wm_class);

  g_key_file_set_value (file, "Desktop Entry", "X-Purism-FormFactor", "Workstation;Mobile;");

  data = g_key_file_to_data (file, NULL, NULL);

  desktop_file_path = g_build_filename (profile_dir, filename, NULL);

  if (!g_file_set_contents (desktop_file_path, data, -1, NULL))
    g_clear_pointer (&desktop_file_path, g_free);

  /* Create a symlink in XDG_DATA_DIR/applications for the Shell to
   * pick up this application. */
  apps_path = g_build_filename (g_get_user_data_dir (), "applications", NULL);
  if (ephy_ensure_dir_exists (apps_path, &error)) {
    link_path = g_build_filename (apps_path, filename, NULL);
    link = g_file_new_for_path (link_path);
    g_file_make_symbolic_link (link, desktop_file_path, NULL, NULL);
  } else {
    g_warning ("Error creating application symlink: %s", error->message);
  }

  return g_steal_pointer (&desktop_file_path);
}

/**
 * ephy_web_application_create:
 * @id: the identifier for the new web application
 * @address: the address of the new web application
 * @name: the name for the new web application
 * @icon: the icon for the new web application
 * @options: the options for the new web application
 *
 * Creates a new Web Application for @address.
 *
 * Returns: (transfer-full): the path to the desktop file representing the new application
 **/
char *
ephy_web_application_create (const char                *id,
                             const char                *address,
                             const char                *name,
                             GdkPixbuf                 *icon,
                             EphyWebApplicationOptions  options)
{
  g_autofree char *app_file = NULL;
  g_autofree char *profile_dir = NULL;
  g_autofree char *desktop_file_path = NULL;
  int fd;

  /* If there's already a WebApp profile for the contents of this
   * view, do nothing. */
  profile_dir = ephy_web_application_get_profile_directory (id);
  if (g_file_test (profile_dir, G_FILE_TEST_IS_DIR)) {
    g_warning ("Profile directory %s already exists", profile_dir);
    return NULL;
  }

  /* Create the profile directory, populate it. */
  if (g_mkdir_with_parents (profile_dir, 488) == -1) {
    g_warning ("Failed to create directory %s", profile_dir);
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

  /* Create the deskop file. */
  desktop_file_path = create_desktop_file (id, name, address, profile_dir, icon);
  if (desktop_file_path)
    ephy_web_application_initialize_settings (profile_dir, options);

  return g_steal_pointer (&desktop_file_path);
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

  /* Create the deskop file. */
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

void
ephy_web_application_setup_from_profile_directory (const char *profile_directory)
{
  const char *gapplication_id;
  const char *id;
  g_autofree char *desktop_basename = NULL;
  g_autofree char *desktop_filename = NULL;
  g_autoptr (GDesktopAppInfo) desktop_info = NULL;

  g_assert (profile_directory != NULL);

  gapplication_id = ephy_web_application_get_gapplication_id_from_profile_directory (profile_directory);
  if (!gapplication_id)
    g_error ("Failed to get GApplication ID from profile directory %s", profile_directory);

  /* FIXME: is this actually necessary? Probably not? */
  g_set_prgname (gapplication_id);

  id = get_app_id_from_gapplication_id (gapplication_id);
  if (!id)
    g_error ("Failed to get app ID from GApplication ID %s", gapplication_id);

  /* Get display name from desktop file */
  desktop_basename = get_app_desktop_filename (id);
  desktop_filename = g_build_filename (profile_directory, desktop_basename, NULL);
  desktop_info = g_desktop_app_info_new_from_filename (desktop_filename);
  if (!desktop_info)
    g_error ("Required desktop file not present at %s", desktop_filename);
  g_set_application_name (g_app_info_get_name (G_APP_INFO (desktop_info)));
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
ephy_web_application_free (EphyWebApplication *app)
{
  g_free (app->id);
  g_free (app->name);
  g_free (app->icon_url);
  g_free (app->url);
  g_free (app->desktop_file);
  g_free (app);
}

EphyWebApplication *
ephy_web_application_for_profile_directory (const char *profile_dir)
{
  g_autoptr (EphyWebApplication) app = NULL;
  g_autofree char *desktop_file_path = NULL;
  const char *id;
  g_autoptr (GDesktopAppInfo) desktop_info = NULL;
  const char *exec;
  int argc;
  g_auto (GStrv) argv = NULL;
  g_autoptr (GFile) file = NULL;
  g_autoptr (GFileInfo) file_info = NULL;
  guint64 created;
  g_autoptr (GDate) date = NULL;

  id = get_app_id_from_profile_directory (profile_dir);
  if (!id)
    return NULL;

  app = g_new0 (EphyWebApplication, 1);
  app->id = g_strdup (id);

  app->desktop_file = get_app_desktop_filename (id);
  desktop_file_path = g_build_filename (profile_dir, app->desktop_file, NULL);
  desktop_info = g_desktop_app_info_new_from_filename (desktop_file_path);
  if (!desktop_info) {
    g_clear_pointer (&app, ephy_web_application_free); /* avoid a scan-build warning */
    return NULL;
  }

  app->name = g_strdup (g_app_info_get_name (G_APP_INFO (desktop_info)));
  app->icon_url = g_desktop_app_info_get_string (desktop_info, "Icon");
  exec = g_app_info_get_commandline (G_APP_INFO (desktop_info));
  if (g_shell_parse_argv (exec, &argc, &argv, NULL))
    app->url = g_strdup (argv[argc - 1]);

  file = g_file_new_for_path (desktop_file_path);

  /* FIXME: this should use TIME_CREATED but it does not seem to be working. */
  file_info = g_file_query_info (file, G_FILE_ATTRIBUTE_TIME_MODIFIED, 0, NULL, NULL);
  created = g_file_info_get_attribute_uint64 (file_info, G_FILE_ATTRIBUTE_TIME_MODIFIED);

  date = g_date_new ();
  g_date_set_time_t (date, (time_t)created);
  g_date_strftime (app->install_date, 127, "%x", date);

  return g_steal_pointer (&app);
}

static GList *
ephy_web_application_get_application_list_internal (gboolean only_legacy)
{
  g_autoptr (GFileEnumerator) children = NULL;
  GList *applications = NULL;
  g_autofree char *parent_directory_path = NULL;
  g_autoptr (GFile) parent_directory = NULL;

  if (only_legacy)
    parent_directory_path = g_build_filename (g_get_user_config_dir (), "epiphany", NULL);
  else
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
    if ((only_legacy && g_str_has_prefix (name, "app-")) ||
        (!only_legacy && g_str_has_prefix (name, EPHY_WEB_APP_GAPPLICATION_ID_PREFIX))) {
      g_autoptr (EphyWebApplication) app = NULL;
      g_autofree char *profile_dir = NULL;

      profile_dir = g_build_filename (parent_directory_path, name, NULL);
      app = ephy_web_application_for_profile_directory (profile_dir);
      if (app) {
        if (!only_legacy) {
          g_autofree char *app_file = g_build_filename (profile_dir, ".app", NULL);
          if (g_file_test (app_file, G_FILE_TEST_EXISTS))
            applications = g_list_prepend (applications, g_steal_pointer (&app));
        } else
          applications = g_list_prepend (applications, g_steal_pointer (&app));
      }
    }
  }

  return g_list_reverse (applications);
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
  return ephy_web_application_get_application_list_internal (FALSE);
}

/**
 * ephy_web_application_get_legacy_application_list:
 *
 * Gets a list of the currently installed web applications.
 * This is only used for the profile migrator as it gets
 * applications in the legacy directory.
 *
 * Free the returned GList with
 * ephy_web_application_free_application_list.
 *
 * Returns: (transfer-full): a #GList of #EphyWebApplication objects
 **/
GList *
ephy_web_application_get_legacy_application_list (void)
{
  return ephy_web_application_get_application_list_internal (TRUE);
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
urls_have_same_origin (const char *a_url,
                       const char *b_url)
{
  g_autoptr (GUri) a_uri = NULL;
  g_autoptr (GUri) b_uri = NULL;

  a_uri = g_uri_parse (a_url, G_URI_FLAGS_NONE, NULL);
  if (!a_uri || !g_uri_get_host (a_uri))
    return FALSE;

  b_uri = g_uri_parse (b_url, G_URI_FLAGS_NONE, NULL);
  if (!b_uri || !g_uri_get_host (b_uri))
    return FALSE;

  if (strcmp (g_uri_get_scheme (a_uri), g_uri_get_scheme (b_uri)) != 0)
    return FALSE;

  if (g_uri_get_port (a_uri) != g_uri_get_port (b_uri))
    return FALSE;

  return g_ascii_strcasecmp (g_uri_get_host (a_uri), g_uri_get_host (b_uri)) == 0;
}

gboolean
ephy_web_application_is_uri_allowed (const char *uri)
{
  g_autoptr (EphyWebApplication) webapp = ephy_web_application_for_profile_directory (ephy_profile_dir ());
  const char *scheme;
  g_auto (GStrv) urls = NULL;
  guint i;
  gboolean matched = FALSE;

  g_assert (webapp);

  if (g_str_has_prefix (uri, "blob:") || g_str_has_prefix (uri, "data:"))
    return TRUE;

  if (urls_have_same_origin (uri, webapp->url))
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

      matched = g_str_has_prefix (uri, url);
    } else {
      matched = g_str_has_prefix (uri, urls[i]);
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
  g_autofree char *profile_dir = NULL;
  g_autofree char *desktop_file_path = NULL;
  g_autofree char *contents = NULL;
  gboolean saved = FALSE;
  g_autoptr (GError) error = NULL;

  profile_dir = ephy_web_application_get_profile_directory (app->id);
  desktop_file_path = g_build_filename (profile_dir, app->desktop_file, NULL);
  if (g_file_get_contents (desktop_file_path, &contents, NULL, &error)) {
    g_autoptr (GKeyFile) key = NULL;
    g_autofree char *name = NULL;
    g_autofree char *icon = NULL;
    g_autofree char *exec = NULL;
    g_auto (GStrv) strings = NULL;
    guint exec_length;
    gboolean changed = FALSE;

    key = g_key_file_new ();
    g_key_file_load_from_data (key, contents, -1, 0, NULL);

    name = g_key_file_get_string (key, "Desktop Entry", "Name", NULL);
    if (g_strcmp0 (name, app->name) != 0) {
      changed = TRUE;
      g_key_file_set_string (key, "Desktop Entry", "Name", app->name);
    }

    icon = g_key_file_get_string (key, "Desktop Entry", "Icon", NULL);
    if (g_strcmp0 (icon, app->icon_url) != 0) {
      g_autoptr (GFile) new_icon = NULL;
      g_autoptr (GFile) old_icon = NULL;
      changed = TRUE;
      new_icon = g_file_new_for_path (app->icon_url);
      old_icon = g_file_new_for_path (icon);
      g_file_copy_async (new_icon, old_icon, G_FILE_COPY_OVERWRITE,
                         G_PRIORITY_DEFAULT, NULL, NULL, NULL,
                         (GAsyncReadyCallback)ephy_web_icon_copy_cb, NULL);
    }

    exec = g_key_file_get_string (key, "Desktop Entry", "Exec", NULL);
    strings = g_strsplit (exec, " ", -1);

    exec_length = g_strv_length (strings);
    if (g_strcmp0 (strings[exec_length - 1], app->url) != 0) {
      changed = TRUE;
      g_free (strings[exec_length - 1]);
      strings[exec_length - 1] = g_strdup (app->url);
      g_free (exec);
      exec = g_strjoinv (" ", strings);
      g_key_file_set_string (key, "Desktop Entry", "Exec", exec);
    }

    if (changed) {
      saved = g_key_file_save_to_file (key, desktop_file_path, &error);
      if (!saved)
        g_warning ("Failed to save desktop file of web application: %s\n", error->message);
    }
  } else {
    g_warning ("Failed to load desktop file of web application: %s\n", error->message);
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
