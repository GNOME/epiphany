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
#include "ephy-prefs.h"

#include <errno.h>
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <libsoup/soup.h>
#include <stdlib.h>
#include <string.h>
#include <webkit2/webkit2.h>

#define EPHY_WEB_APP_DESKTOP_FILE_PREFIX "epiphany-"

/* This is necessary because of gnome-shell's guessing of a .desktop
   filename from WM_CLASS property. */
static char *
get_wm_class_from_app_title (const char *title)
{
  char *normal_title;
  char *wm_class;
  char *checksum;

  normal_title = g_utf8_strdown (title, -1);
  g_strdelimit (normal_title, " ", '-');
  g_strdelimit (normal_title, G_DIR_SEPARATOR_S, '-');
  checksum = g_compute_checksum_for_string (G_CHECKSUM_SHA1, title, -1);
  wm_class = g_strconcat (EPHY_WEB_APP_DESKTOP_FILE_PREFIX, normal_title, "-", checksum, NULL);

  g_free (checksum);
  g_free (normal_title);

  return wm_class;
}

/* Gets the proper .desktop filename from a WM_CLASS string,
   converting to the local charset when needed. */
static char *
desktop_filename_from_wm_class (char *wm_class)
{
  char *encoded;
  char *filename = NULL;
  GError *error = NULL;

  encoded = g_filename_from_utf8 (wm_class, -1, NULL, NULL, &error);
  if (error) {
    g_warning ("%s", error->message);
    g_error_free (error);
    return NULL;
  }
  filename = g_strconcat (encoded, ".desktop", NULL);
  g_free (encoded);

  return filename;
}

/**
 * ephy_web_application_get_profile_directory:
 * @name: the application name
 *
 * Gets the directory where the profile for @name is meant to be stored.
 *
 * Returns: (transfer full): A newly allocated string.
 **/
char *
ephy_web_application_get_profile_directory (const char *name)
{
  char *dot_dir, *app_dir, *wm_class, *profile_dir, *encoded;
  GError *error = NULL;

  wm_class = get_wm_class_from_app_title (name);
  encoded = g_filename_from_utf8 (wm_class, -1, NULL, NULL, &error);
  g_free (wm_class);

  if (error) {
    g_warning ("%s", error->message);
    g_error_free (error);
    return NULL;
  }

  dot_dir = !ephy_dot_dir_is_default () ? ephy_default_dot_dir () : NULL;
  app_dir = g_strconcat (EPHY_WEB_APP_PREFIX, encoded, NULL);
  profile_dir = g_build_filename (dot_dir ? dot_dir : ephy_dot_dir (), app_dir, NULL);
  g_free (encoded);
  g_free (app_dir);
  g_free (dot_dir);

  return profile_dir;
}

/**
 * ephy_web_application_delete:
 * @name: the name of the web application do delete
 *
 * Deletes all the data associated with a Web Application created by
 * Epiphany.
 *
 * Returns: %TRUE if the web app was succesfully deleted, %FALSE otherwise
 **/
gboolean
ephy_web_application_delete (const char *name)
{
  char *profile_dir = NULL;
  char *desktop_file = NULL, *desktop_path = NULL;
  char *wm_class;
  GFile *launcher = NULL;
  gboolean return_value = FALSE;

  g_assert (name);

  profile_dir = ephy_web_application_get_profile_directory (name);
  if (!profile_dir)
    goto out;

  /* If there's no profile dir for this app, it means it does not
   * exist. */
  if (!g_file_test (profile_dir, G_FILE_TEST_IS_DIR)) {
    g_warning ("No application with name '%s' is installed.\n", name);
    goto out;
  }

  if (!ephy_file_delete_dir_recursively (profile_dir, NULL))
    goto out;
  LOG ("Deleted application profile.\n");

  wm_class = get_wm_class_from_app_title (name);
  desktop_file = desktop_filename_from_wm_class (wm_class);
  g_free (wm_class);
  if (!desktop_file)
    goto out;

  desktop_path = g_build_filename (g_get_user_data_dir (), "applications", desktop_file, NULL);
  if (g_file_test (desktop_path, G_FILE_TEST_IS_SYMLINK)) {
    launcher = g_file_new_for_path (desktop_path);
    if (!g_file_delete (launcher, NULL, NULL))
      goto out;
    LOG ("Deleted application launcher.\n");
  }
  return_value = TRUE;

 out:

  g_free (profile_dir);

  if (launcher)
    g_object_unref (launcher);
  g_free (desktop_file);
  g_free (desktop_path);

  return return_value;
}

static char *
create_desktop_file (const char *address,
                     const char *profile_dir,
                     const char *title,
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

  wm_class = get_wm_class_from_app_title (title);
  filename = desktop_filename_from_wm_class (wm_class);

  if (!filename)
    goto out;

  file = g_key_file_new ();
  g_key_file_set_value (file, "Desktop Entry", "Name", title);
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

  g_key_file_set_value (file, "Desktop Entry", "StartupWMClass", wm_class);
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

 out:
  g_free (wm_class);
  g_free (data);
  g_key_file_free (file);

  return desktop_file_path;
}

/**
 * ephy_web_application_create:
 * @address: the address of the new web application
 * @name: the name for the new web application
 * @icon: the icon for the new web application
 *
 * Creates a new Web Application for @address.
 *
 * Returns: (transfer-full): the path to the desktop file representing the new application
 **/
char *
ephy_web_application_create (const char *address, const char *name, GdkPixbuf *icon)
{
  char *profile_dir = NULL;
  char *desktop_file_path = NULL;

  /* If there's already a WebApp profile for the contents of this
   * view, do nothing. */
  profile_dir = ephy_web_application_get_profile_directory (name);
  if (g_file_test (profile_dir, G_FILE_TEST_IS_DIR)) {
    LOG ("Profile directory %s already exists", profile_dir);
    goto out;
  }

  /* Create the profile directory, populate it. */
  if (g_mkdir (profile_dir, 488) == -1) {
    LOG ("Failed to create directory %s", profile_dir);
    goto out;
  }

  /* Create the deskop file. */
  desktop_file_path = create_desktop_file (address, profile_dir, name, icon);
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
  char *profile_dir;
  const char *cmd;
  char *address;

  profile_dir = ephy_web_application_get_profile_directory (g_app_info_get_name (app_info));
  if (g_mkdir (profile_dir, 488) == -1) {
    if (errno == EEXIST)
      return profile_dir;

    g_free (profile_dir);
    return NULL;
  }

  /* The address should be the last command line argument in the desktop file */
  cmd = g_app_info_get_commandline (app_info);
  if (!cmd) {
    g_free (profile_dir);
    return NULL;
  }

  address = g_strrstr (cmd, " ");
  if (!address) {
    g_free (profile_dir);
    return NULL;
  }

  address++;
  if (*address == '\0') {
    g_free (profile_dir);
    return NULL;
  }

  return profile_dir;
}

void
ephy_web_application_setup_from_profile_directory (const char *profile_directory)
{
  const char *app_name;
  char *app_icon;
  char *desktop_basename;
  char *desktop_filename;
  GDesktopAppInfo *desktop_info;

  g_assert (profile_directory != NULL);

  app_name = strstr (profile_directory, EPHY_WEB_APP_PREFIX);
  if (!app_name) {
    g_warning ("Profile directory %s does not begin with required web app prefix %s", profile_directory, EPHY_WEB_APP_PREFIX);
    exit (1);
  }

  /* Skip the 'app-' part */
  app_name += strlen (EPHY_WEB_APP_PREFIX);
  g_set_prgname (app_name);

  /* Get display name from desktop file */
  desktop_basename = g_strconcat (app_name, ".desktop", NULL);
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
  gdk_set_program_class (app_name);

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
  GFileEnumerator *children = NULL;
  GFileInfo *info;
  GList *applications = NULL;
  GFile *dot_dir;
  char *default_dot_dir;

  default_dot_dir = !ephy_dot_dir_is_default () ? ephy_default_dot_dir () : NULL;
  dot_dir = g_file_new_for_path (default_dot_dir ? default_dot_dir : ephy_dot_dir ());
  children = g_file_enumerate_children (dot_dir,
                                        "standard::name",
                                        0, NULL, NULL);
  g_object_unref (dot_dir);

  info = g_file_enumerator_next_file (children, NULL, NULL);
  while (info) {
    EphyWebApplication *app;
    const char *name;
    glong prefix_length = g_utf8_strlen (EPHY_WEB_APP_PREFIX, -1);

    name = g_file_info_get_name (info);
    if (g_str_has_prefix (name, EPHY_WEB_APP_PREFIX)) {
      char *profile_dir;
      guint64 created;
      GDate *date;
      char *desktop_file, *desktop_file_path;
      char *contents;
      GFileInfo *desktop_info;

      app = g_slice_new0 (EphyWebApplication);

      profile_dir = g_build_filename (default_dot_dir ? default_dot_dir : ephy_dot_dir (), name, NULL);
      app->icon_url = g_build_filename (profile_dir, EPHY_WEB_APP_ICON_NAME, NULL);

      desktop_file = g_strconcat (name + prefix_length, ".desktop", NULL);
      desktop_file_path = g_build_filename (profile_dir, desktop_file, NULL);
      app->desktop_file = g_strdup (desktop_file);

      if (g_file_get_contents (desktop_file_path, &contents, NULL, NULL)) {
        char *exec;
        char **strings;
        GKeyFile *key;
        int i;
        GFile *file;

        key = g_key_file_new ();
        g_key_file_load_from_data (key, contents, -1, 0, NULL);
        app->name = g_key_file_get_string (key, "Desktop Entry", "Name", NULL);
        exec = g_key_file_get_string (key, "Desktop Entry", "Exec", NULL);
        strings = g_strsplit (exec, " ", -1);

        for (i = 0; strings[i]; i++);
        app->url = g_strdup (strings[i - 1]);

        g_strfreev (strings);
        g_free (exec);
        g_key_file_free (key);

        file = g_file_new_for_path (desktop_file_path);

        /* FIXME: this should use TIME_CREATED but it does not seem to be working. */
        desktop_info = g_file_query_info (file, G_FILE_ATTRIBUTE_TIME_MODIFIED, 0, NULL, NULL);
        created = g_file_info_get_attribute_uint64 (desktop_info, G_FILE_ATTRIBUTE_TIME_MODIFIED);

        date = g_date_new ();
        g_date_set_time_t (date, (time_t)created);
        g_date_strftime (app->install_date, 127, "%x", date);

        g_date_free (date);
        g_object_unref (file);
        g_object_unref (desktop_info);

        applications = g_list_append (applications, app);
      }

      g_free (contents);
      g_free (desktop_file);
      g_free (profile_dir);
      g_free (desktop_file_path);
    }

    g_object_unref (info);

    info = g_file_enumerator_next_file (children, NULL, NULL);
  }

  g_object_unref (children);
  g_free (default_dot_dir);

  return applications;
}

static void
ephy_web_application_free (EphyWebApplication *app)
{
  g_free (app->name);
  g_free (app->icon_url);
  g_free (app->url);
  g_free (app->desktop_file);
  g_slice_free (EphyWebApplication, app);
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
  GList *p;

  for (p = list; p; p = p->next)
    ephy_web_application_free ((EphyWebApplication *)p->data);

  g_list_free (list);
}

/**
 * ephy_web_application_exists:
 * @name: the potential name of the web application
 *
 * Returns: whether an application with @name exists.
 **/
gboolean
ephy_web_application_exists (const char *name)
{
  char *profile_dir;
  gboolean profile_exists;

  profile_dir = ephy_web_application_get_profile_directory (name);
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
