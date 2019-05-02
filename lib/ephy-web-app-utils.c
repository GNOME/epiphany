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
#include <libsoup/soup.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

/* Web Apps are installed in the default config dir of the user.
 * Every app has its own profile directory. To create a web app
 * an id needs to be generated using the given name as
 * <normalized-name>-checksum.
 *
 * The id is used to uniquely identify the app.
 *  - Program name: epiphany-<id>
 *  - Profile directory: <program-name>
 *  - Desktop file: <profile-dir>/<program-name>.desktop
 *
 * System web applications have a profile dir without a desktop file.
 */

#define EPHY_WEB_APP_PROGRAM_NAME_PREFIX "epiphany-"

char *
ephy_web_application_get_app_id_from_name (const char *name)
{
  char *normal_id;
  char *checksum;
  char *id;

  normal_id = g_utf8_strdown (name, -1);
  g_strdelimit (normal_id, " ", '-');
  g_strdelimit (normal_id, G_DIR_SEPARATOR_S, '-');

  checksum = g_compute_checksum_for_string (G_CHECKSUM_SHA1, name, -1);
  id = g_strdup_printf ("%s-%s", normal_id, checksum);

  g_free (normal_id);
  g_free (checksum);

  return id;
}

static char *
get_encoded_path (const char *path)
{
  char *encoded;
  GError *error = NULL;

  encoded = g_filename_from_utf8 (path, -1, NULL, NULL, &error);
  if (error) {
    g_warning ("%s", error->message);
    g_error_free (error);
    return NULL;
  }

  return encoded;
}

static char *
get_app_profile_directory_name (const char *id)
{
  char *profile_dir;
  char *encoded;

  profile_dir = g_strconcat (EPHY_WEB_APP_PROGRAM_NAME_PREFIX, id, NULL);
  encoded = get_encoded_path (profile_dir);
  g_free (profile_dir);

  return encoded;
}

static char *
get_app_desktop_filename (const char *id)
{
  char *filename;
  char *encoded;

  filename = g_strconcat (EPHY_WEB_APP_PROGRAM_NAME_PREFIX, id, ".desktop", NULL);
  encoded = get_encoded_path (filename);
  g_free (filename);

  return encoded;
}

const char *
ephy_web_application_get_program_name_from_profile_directory (const char *profile_dir)
{
  const char *name;

  // Just get the basename
  name = strrchr (profile_dir, G_DIR_SEPARATOR);
  if (name == NULL) {
    g_warning ("Profile directoroy %s is not a valid path", profile_dir);
    return NULL;
  }

  name++; // Strip '/'

  // Legacy web app support
  if (g_str_has_prefix (name, "app-"))
    name += strlen ("app-");

  if (!g_str_has_prefix (name, EPHY_WEB_APP_PROGRAM_NAME_PREFIX)) {
    g_warning ("Profile directory %s does not begin with required web app prefix %s", profile_dir, EPHY_WEB_APP_PROGRAM_NAME_PREFIX);
    return NULL;
  }

  return name;
}

static const char *
get_app_id_from_program_name (const char *name)
{
  if (!g_str_has_prefix (name, EPHY_WEB_APP_PROGRAM_NAME_PREFIX)) {
    g_warning ("Program name %s does not begin with required prefix %s", name, EPHY_WEB_APP_PROGRAM_NAME_PREFIX);
    return NULL;
  }

  return name + strlen (EPHY_WEB_APP_PROGRAM_NAME_PREFIX);
}

static const char *
get_app_id_from_profile_directory (const char *profile_dir)
{
  const char *program_name;

  program_name = ephy_web_application_get_program_name_from_profile_directory (profile_dir);
  return program_name ? get_app_id_from_program_name (program_name) : NULL;
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
  g_autoptr(GFile) launcher = NULL;
  g_autoptr(GError) error = NULL;

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
  GKeyFile *file = NULL;
  char *exec_string;
  char *data = NULL;
  char *filename, *apps_path, *desktop_file_path = NULL;
  char *link_path;
  char *wm_class;
  GFile *link;
  GError *error = NULL;

  g_assert (profile_dir);

  filename = get_app_desktop_filename (id);
  if (!filename)
    return NULL;

  file = g_key_file_new ();
  g_key_file_set_value (file, "Desktop Entry", "Name", name);
  exec_string = g_strdup_printf ("epiphany --application-mode --profile=\"%s\" %s",
                                 profile_dir,
                                 address);
  g_key_file_set_value (file, "Desktop Entry", "Exec", exec_string);
  g_free (exec_string);
  g_key_file_set_value (file, "Desktop Entry", "StartupNotify", "true");
  g_key_file_set_value (file, "Desktop Entry", "Terminal", "false");
  g_key_file_set_value (file, "Desktop Entry", "Type", "Application");
  g_key_file_set_value (file, "Desktop Entry", "Categories", "Network;GNOME;GTK;");

  if (icon) {
    GOutputStream *stream;
    char *path;
    GFile *image;

    path = g_build_filename (profile_dir, EPHY_WEB_APP_ICON_NAME, NULL);
    image = g_file_new_for_path (path);

    stream = (GOutputStream *)g_file_create (image, 0, NULL, NULL);
    gdk_pixbuf_save_to_stream (icon, stream, "png", NULL, NULL, NULL);
    g_key_file_set_value (file, "Desktop Entry", "Icon", path);

    g_object_unref (stream);
    g_object_unref (image);
    g_free (path);
  }

  wm_class = g_strconcat (EPHY_WEB_APP_PROGRAM_NAME_PREFIX, id, NULL);
  g_key_file_set_value (file, "Desktop Entry", "StartupWMClass", wm_class);
  g_free (wm_class);
  data = g_key_file_to_data (file, NULL, NULL);

  desktop_file_path = g_build_filename (profile_dir, filename, NULL);

  if (!g_file_set_contents (desktop_file_path, data, -1, NULL)) {
    g_free (desktop_file_path);
    desktop_file_path = NULL;
  }

  /* Create a symlink in XDG_DATA_DIR/applications for the Shell to
   * pick up this application. */
  apps_path = g_build_filename (g_get_user_data_dir (), "applications", NULL);
  if (ephy_ensure_dir_exists (apps_path, &error)) {
    link_path = g_build_filename (apps_path, filename, NULL);
    link = g_file_new_for_path (link_path);
    g_free (link_path);
    g_file_make_symbolic_link (link, desktop_file_path, NULL, NULL);
    g_object_unref (link);
  } else {
    g_warning ("Error creating application symlink: %s", error->message);
    g_error_free (error);
  }

  g_free (apps_path);
  g_free (filename);
  g_free (data);
  g_key_file_free (file);

  return desktop_file_path;
}

/**
 * ephy_web_application_create:
 * @id: the identifier for the new web application
 * @address: the address of the new web application
 * @name: the name for the new web application
 * @icon: the icon for the new web application
 *
 * Creates a new Web Application for @address.
 *
 * Returns: (transfer-full): the path to the desktop file representing the new application
 **/
char *
ephy_web_application_create (const char *id,
                             const char *address,
                             const char *name,
                             GdkPixbuf  *icon)
{
  char *profile_dir;
  char *desktop_file_path = NULL;

  /* If there's already a WebApp profile for the contents of this
   * view, do nothing. */
  profile_dir = ephy_web_application_get_profile_directory (id);
  if (g_file_test (profile_dir, G_FILE_TEST_IS_DIR)) {
    g_warning ("Profile directory %s already exists", profile_dir);
    goto out;
  }

  /* Create the profile directory, populate it. */
  if (g_mkdir_with_parents (profile_dir, 488) == -1) {
    g_warning ("Failed to create directory %s", profile_dir);
    goto out;
  }

  /* Skip migration for new web apps. */
  ephy_profile_utils_set_migration_version_for_profile_dir (EPHY_PROFILE_MIGRATION_VERSION, profile_dir);

  /* Create an .app file. */
  g_autofree char *app_file = g_build_filename (profile_dir, ".app", NULL);
  int fd = g_open (app_file, O_WRONLY|O_CREAT|O_TRUNC, 0644);
  if (fd < 0) {
    LOG ("Failed to create .app file: %s", g_strerror (errno));
    goto out;
  } else {
    close (fd);
  }

  /* Create the deskop file. */
  desktop_file_path = create_desktop_file (id, name, address, profile_dir, icon);
  if (desktop_file_path)
    ephy_web_application_initialize_settings (profile_dir);

 out:
  if (profile_dir)
    g_free (profile_dir);

  return desktop_file_path;
}

char *
ephy_web_application_ensure_for_app_info (GAppInfo *app_info)
{
  char *id;
  char *profile_dir;

  id = ephy_web_application_get_app_id_from_name (g_app_info_get_name (app_info));
  profile_dir = ephy_web_application_get_profile_directory (id);
  g_free (id);

  if (g_mkdir (profile_dir, 488) == -1) {
    if (errno == EEXIST)
      return profile_dir;

    g_free (profile_dir);
    return NULL;
  }

  return profile_dir;
}

void
ephy_web_application_setup_from_profile_directory (const char *profile_directory)
{
  const char *program_name;
  const char *id;
  char *app_icon;
  char *desktop_basename;
  char *desktop_filename;
  GDesktopAppInfo *desktop_info;

  g_assert (profile_directory != NULL);

  program_name = ephy_web_application_get_program_name_from_profile_directory (profile_directory);
  if (!program_name)
    exit (1);

  g_set_prgname (program_name);

  id = get_app_id_from_program_name (program_name);
  if (!id)
    exit (1);

  /* Get display name from desktop file */
  desktop_basename = get_app_desktop_filename (id);
  desktop_filename = g_build_filename (profile_directory, desktop_basename, NULL);
  desktop_info = g_desktop_app_info_new_from_filename (desktop_filename);
  if (!desktop_info) {
    g_warning ("Required desktop file not present at %s", desktop_filename);
    exit (1);
  }
  g_set_application_name (g_app_info_get_name (G_APP_INFO (desktop_info)));

  app_icon = g_build_filename (profile_directory, EPHY_WEB_APP_ICON_NAME, NULL);
  gtk_window_set_default_icon_from_file (app_icon, NULL);

  /* We need to re-set this because we have already parsed the
   * options, which inits GTK+ and sets this as a side effect.
   */
  gdk_set_program_class (program_name);

  g_free (app_icon);
  g_free (desktop_basename);
  g_free (desktop_filename);
  g_object_unref (desktop_info);
}

void
ephy_web_application_setup_from_desktop_file (GDesktopAppInfo *desktop_info)
{
  GAppInfo *app_info;
  const char *wm_class;
  GIcon *icon;

  g_assert (G_IS_DESKTOP_APP_INFO (desktop_info));

  app_info = G_APP_INFO (desktop_info);
  g_set_prgname (g_app_info_get_name (app_info));
  g_set_application_name (g_app_info_get_display_name (app_info));

  icon = g_app_info_get_icon (app_info);
  if (G_IS_FILE_ICON (icon)) {
    GFile *file = g_file_icon_get_file (G_FILE_ICON (icon));
    char *path = file ? g_file_get_path (file) : NULL;

    if (path) {
      gtk_window_set_default_icon_from_file (path, NULL);
      g_free (path);
    }
    g_clear_object (&file);
  } else if (G_IS_THEMED_ICON (icon)) {
    const char * const *names = g_themed_icon_get_names (G_THEMED_ICON (icon));
    if (names)
      gtk_window_set_default_icon_name (names[0]);
  }
  g_clear_object (&icon);

  /* We need to re-set this because we have already parsed the
   * options, which inits GTK+ and sets this as a side effect.
   */
  wm_class = g_desktop_app_info_get_startup_wm_class (desktop_info);
  if (wm_class)
    gdk_set_program_class (wm_class);
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
  EphyWebApplication *app;
  char *desktop_file_path;
  const char *id;
  GDesktopAppInfo *desktop_info;
  const char *exec;
  int argc;
  char **argv;
  GFile *file;
  GFileInfo *file_info;
  guint64 created;
  GDate *date;

  id = get_app_id_from_profile_directory (profile_dir);
  if (!id)
    return NULL;

  app = g_new0 (EphyWebApplication, 1);
  app->id = g_strdup (id);

  app->desktop_file = get_app_desktop_filename (id);
  desktop_file_path = g_build_filename (profile_dir, app->desktop_file, NULL);
  desktop_info = g_desktop_app_info_new_from_filename (desktop_file_path);
  if (!desktop_info) {
    ephy_web_application_free (app);
    g_free (desktop_file_path);
    return NULL;
  }

  app->name = g_strdup (g_app_info_get_name (G_APP_INFO (desktop_info)));
  app->icon_url = g_desktop_app_info_get_string (desktop_info, "Icon");
  exec = g_app_info_get_commandline (G_APP_INFO (desktop_info));
  if (g_shell_parse_argv (exec, &argc, &argv, NULL)) {
    app->url = g_strdup (argv[argc - 1]);
    g_strfreev (argv);
  }

  g_object_unref (desktop_info);

  file = g_file_new_for_path (desktop_file_path);

  /* FIXME: this should use TIME_CREATED but it does not seem to be working. */
  file_info = g_file_query_info (file, G_FILE_ATTRIBUTE_TIME_MODIFIED, 0, NULL, NULL);
  created = g_file_info_get_attribute_uint64 (file_info, G_FILE_ATTRIBUTE_TIME_MODIFIED);

  date = g_date_new ();
  g_date_set_time_t (date, (time_t)created);
  g_date_strftime (app->install_date, 127, "%x", date);

  g_date_free (date);
  g_object_unref (file);
  g_object_unref (file_info);
  g_free (desktop_file_path);

  return app;
}

static GList *
ephy_web_application_get_application_list_internal (gboolean only_legacy)
{
  GFileEnumerator *children = NULL;
  GFileInfo *info;
  GList *applications = NULL;
  g_autofree char *parent_directory_path = NULL;
  g_autoptr(GFile) parent_directory = NULL;

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

  info = g_file_enumerator_next_file (children, NULL, NULL);
  while (info) {
    const char *name;

    name = g_file_info_get_name (info);
    if ((only_legacy && g_str_has_prefix (name, "app-")) ||
        (!only_legacy && g_str_has_prefix (name, EPHY_WEB_APP_PROGRAM_NAME_PREFIX))) {
      EphyWebApplication *app;
      char *profile_dir;

      profile_dir = g_build_filename (parent_directory_path, name, NULL);
      app = ephy_web_application_for_profile_directory (profile_dir);
      if (app) {
        if (!only_legacy) {
          g_autofree char *app_file = g_build_filename (profile_dir, ".app", NULL);
          if (g_file_test (app_file, G_FILE_TEST_EXISTS))
            applications = g_list_prepend (applications, app);
          else
            g_object_unref (app);
        } else
          applications = g_list_prepend (applications, app);
      }

      g_free (profile_dir);
    }

    g_object_unref (info);

    info = g_file_enumerator_next_file (children, NULL, NULL);
  }

  g_object_unref (children);

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
  char *profile_dir;
  gboolean profile_exists;

  profile_dir = ephy_web_application_get_profile_directory (id);
  profile_exists = g_file_test (profile_dir, G_FILE_TEST_IS_DIR);
  g_free (profile_dir);

  return profile_exists;
}

void
ephy_web_application_initialize_settings (const char *profile_directory)
{
  GSettings *settings;
  GSettings *web_app_settings;
  char *name;
  char *path;

  name = g_path_get_basename (profile_directory);
  settings = g_settings_new_with_path (EPHY_PREFS_WEB_SCHEMA, "/org/gnome/epiphany/web/");

  path = g_build_path ("/", "/org/gnome/epiphany/web-apps/", name, "web/", NULL);
  web_app_settings = g_settings_new_with_path (EPHY_PREFS_WEB_SCHEMA, path);
  g_free (path);

  for (guint i = 0; i < G_N_ELEMENTS (ephy_prefs_web_schema); i++) {
    GVariant *value;

    value = g_settings_get_value (settings, ephy_prefs_web_schema[i]);
    g_settings_set_value (web_app_settings, ephy_prefs_web_schema[i], value);
    g_variant_unref (value);
  }

  g_object_unref (settings);
  g_object_unref (web_app_settings);

  settings = g_settings_new_with_path (EPHY_PREFS_STATE_SCHEMA, "/org/gnome/epiphany/state/");

  path = g_build_path ("/", "/org/gnome/epiphany/web-apps/", name, "state/", NULL);
  web_app_settings = g_settings_new_with_path (EPHY_PREFS_STATE_SCHEMA, path);
  g_free (path);

  for (guint i = 0; i < G_N_ELEMENTS (ephy_prefs_state_schema); i++) {
    GVariant *value;

    value = g_settings_get_value (settings, ephy_prefs_state_schema[i]);
    g_settings_set_value (web_app_settings, ephy_prefs_state_schema[i], value);
    g_variant_unref (value);
  }

  g_object_unref (settings);
  g_object_unref (web_app_settings);
  g_free (name);
}

static gboolean
urls_have_same_origin (const char *a_url,
                       const char *b_url)
{
  SoupURI *a_uri, *b_uri;
  gboolean retval = FALSE;

  a_uri = soup_uri_new (a_url);
  if (!a_uri)
    return retval;

  b_uri = soup_uri_new (b_url);
  if (b_uri) {
    retval = a_uri->host && b_uri->host && soup_uri_host_equal (a_uri, b_uri);
    soup_uri_free (b_uri);
  }

  soup_uri_free (a_uri);

  return retval;
}

gboolean
ephy_web_application_is_uri_allowed (const char *uri,
                                     const char *referrer)
{
  SoupURI *request_uri;
  char **urls;
  guint i;
  gboolean matched = FALSE;

  if (g_str_has_prefix (uri, "blob:") || g_str_has_prefix (uri, "data:"))
    return TRUE;

  if (urls_have_same_origin (uri, referrer))
    return TRUE;

  if (g_strcmp0 (uri, "about:blank") == 0)
    return TRUE;

  request_uri = soup_uri_new (uri);
  if (!request_uri)
    return FALSE;

  urls = g_settings_get_strv (EPHY_SETTINGS_WEB_APP, EPHY_PREFS_WEB_APP_ADDITIONAL_URLS);
  for (i = 0; urls[i] && !matched; i++) {
    if (!strstr (urls[i], "://")) {
      char *url = g_strdup_printf ("%s://%s", request_uri->scheme, urls[i]);

      matched = g_str_has_prefix (uri, url);
      g_free (url);
    } else {
      matched = g_str_has_prefix (uri, urls[i]);
    }
  }
  g_strfreev (urls);

  soup_uri_free (request_uri);

  return matched;
}

gboolean
ephy_web_application_save (EphyWebApplication *app)
{
  char *profile_dir;
  char *desktop_file_path;
  char *contents;
  gboolean saved = FALSE;
  GError *error = NULL;

  profile_dir = ephy_web_application_get_profile_directory (app->id);
  desktop_file_path = g_build_filename (profile_dir, app->desktop_file, NULL);
  if (g_file_get_contents (desktop_file_path, &contents, NULL, &error)) {
    GKeyFile *key;
    char *name;
    char *icon;
    char *exec;
    char **strings;
    guint exec_length;
    gboolean changed = FALSE;

    key = g_key_file_new ();
    g_key_file_load_from_data (key, contents, -1, 0, NULL);

    name = g_key_file_get_string (key, "Desktop Entry", "Name", NULL);
    if (g_strcmp0 (name, app->name) != 0) {
      changed = TRUE;
      g_key_file_set_string (key, "Desktop Entry", "Name", app->name);
    }
    g_free (name);

    icon = g_key_file_get_string (key, "Desktop Entry", "Icon", NULL);
    if (g_strcmp0 (icon, app->icon_url) != 0) {
      changed = TRUE;
      g_key_file_set_string (key, "Desktop Entry", "Icon", app->icon_url);
    }
    g_free (icon);

    exec = g_key_file_get_string (key, "Desktop Entry", "Exec", NULL);
    strings = g_strsplit (exec, " ", -1);
    g_free (exec);

    exec_length = g_strv_length (strings);
    if (g_strcmp0 (strings[exec_length - 1], app->url) != 0) {
      changed = TRUE;
      g_free (strings[exec_length - 1]);
      strings[exec_length - 1] = g_strdup (app->url);
      exec = g_strjoinv (" ", strings);
      g_key_file_set_string (key, "Desktop Entry", "Exec", exec);
      g_free (exec);
    }
    g_strfreev (strings);

    if (changed) {
      saved = g_key_file_save_to_file (key, desktop_file_path, &error);
      if (!saved) {
        g_warning ("Failed to save desktop file of web application: %s\n", error->message);
        g_error_free (error);
      }
    }
    g_free (contents);
    g_key_file_free (key);
  } else {
    g_warning ("Failed to load desktop file of web application: %s\n", error->message);
    g_error_free (error);
  }

  g_free (desktop_file_path);
  g_free (profile_dir);

  return saved;
}
