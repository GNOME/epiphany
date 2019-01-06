/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2002 Jorn Baayen
 *  Copyright © 2003, 2004 Marco Pesenti Gritti
 *  Copyright © 2004, 2005, 2006 Christian Persch
 *  Copyright © 2012 Igalia S.L.
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
#include "ephy-file-helpers.h"

#include "ephy-debug.h"
#include "ephy-flatpak-utils.h"
#include "ephy-prefs.h"
#include "ephy-profile-utils.h"
#include "ephy-settings.h"
#include "ephy-string.h"
#include "ephy-web-app-utils.h"

#include <errno.h>
#include <gdk/gdk.h>
#include <gio/gdesktopappinfo.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <libxml/xmlreader.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * SECTION:ephy-file-helpers
 * @short_description: miscellaneous file related utility functions
 *
 * File related functions, including functions to launch, browse or move files
 * atomically.
 */

#define DELAY_MAX_TICKS 64
#define INITIAL_TICKS   2

typedef enum {
  EPHY_PROFILE_DIR_UNKNOWN,
  EPHY_PROFILE_DIR_DEFAULT,
  EPHY_PROFILE_DIR_WEB_APP,
  EPHY_PROFILE_DIR_TEST
} EphyProfileDirType;

static GHashTable *files;
static GHashTable *mime_table;

static gboolean keep_directory;
static char *profile_dir_global;
static char *cache_dir;
static char *config_dir;
static char *tmp_dir;
static GList *del_on_exit;
static EphyProfileDirType profile_dir_type;

GQuark ephy_file_helpers_error_quark;

/**
 * ephy_file_tmp_dir:
 *
 * Returns the name of the temp dir for the running Epiphany instance.
 *
 * Returns: the name of the temp dir, this string belongs to Epiphany.
 **/
const char *
ephy_file_tmp_dir (void)
{
  if (tmp_dir == NULL) {
    char *partial_name;
    char *full_name;

    partial_name = g_strconcat ("epiphany-", g_get_user_name (),
                                "-XXXXXX", NULL);
    full_name = g_build_filename (g_get_tmp_dir (), partial_name,
                                  NULL);
    tmp_dir = mkdtemp (full_name);
    g_free (partial_name);

    if (tmp_dir == NULL) {
      g_free (full_name);
    }
  }

  return tmp_dir;
}

static char *
ephy_file_download_dir (void)
{
  const char *xdg_download_dir;

  xdg_download_dir = g_get_user_special_dir (G_USER_DIRECTORY_DOWNLOAD);
  if (xdg_download_dir != NULL)
    return g_strdup (xdg_download_dir);

  /* If we don't have XDG user dirs info, return an educated guess. */
  return g_build_filename (g_get_home_dir (), _("Downloads"), NULL);
}

/**
 * ephy_file_get_downloads_dir:
 *
 * Returns a proper downloads destination by checking the
 * EPHY_PREFS_STATE_DOWNLOAD_DIR GSettings key and following this logic:
 *
 *  - An absolute path: considered user-set, use this value directly.
 *
 *  - "Desktop" keyword in GSettings: the directory returned by
 *    ephy_file_desktop_dir().
 *
 *  - "Downloads" keyword in GSettings, or any other value: the XDG
 *  downloads directory, or ~/Downloads.
 *
 * Returns: a newly-allocated string containing the path to the downloads dir.
 **/
char *
ephy_file_get_downloads_dir (void)
{
  char *download_dir;

  download_dir = g_settings_get_string (EPHY_SETTINGS_STATE,
                                        EPHY_PREFS_STATE_DOWNLOAD_DIR);

  if (g_strcmp0 (download_dir, "Desktop") == 0) {
    g_free (download_dir);
    return ephy_file_desktop_dir ();
  }

  if (g_strcmp0 (download_dir, "Downloads") == 0 ||
      !g_path_is_absolute (download_dir) ||
      ephy_is_running_inside_flatpak ()) {
    g_free (download_dir);
    return ephy_file_download_dir ();
  }

  return download_dir;
}

/**
 * ephy_file_desktop_dir:
 *
 * Gets the XDG desktop dir path or a default homedir/Desktop alternative.
 *
 * Returns: a newly-allocated string containing the desktop dir path.
 **/
char *
ephy_file_desktop_dir (void)
{
  const char *xdg_desktop_dir;

  xdg_desktop_dir = g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP);
  if (xdg_desktop_dir != NULL)
    return g_strdup (xdg_desktop_dir);

  /* If we don't have XDG user dirs info, return an educated guess. */
  return g_build_filename (g_get_home_dir (), _("Desktop"), NULL);
}

/**
 * ephy_file_tmp_filename:
 * @base: the base name of the temp file to create, containing "XXXXXX"
 * @extension: an optional extension for @base or %NULL
 *
 * Gets a usable temp filename with g_mkstemp() using @base as the name
 * with an optional @extension. @base should contain "XXXXXX" in it.
 *
 * Notice that this does not create the file. It only gets a valid
 * filename.
 *
 * Returns: a newly-allocated string containing the name of the temp
 * file name or %NULL.
 **/
char *
ephy_file_tmp_filename (const char *base,
                        const char *extension)
{
  int fd;
  char *name = g_strdup (base);

  fd = g_mkstemp (name);

  if (fd != -1) {
    unlink (name);
    close (fd);
  } else {
    g_free (name);

    return NULL;
  }

  if (extension) {
    char *tmp;
    tmp = g_strconcat (name, ".",
                       extension, NULL);
    g_free (name);
    name = tmp;
  }

  return name;
}

/**
 * ephy_profile_dir:
 *
 * Gets Epiphany's configuration directory, usually .local/share/epiphany
 * under user's homedir.
 *
 * Returns: the full path to Epiphany's configuration directory
 **/
const char *
ephy_profile_dir (void)
{
  return profile_dir_global;
}

/**
 * ephy_config_dir:
 *
 * Gets Epiphany's configuration directory, usually .config/epiphany
 * under user's homedir.
 *
 * Returns: the full path to Epiphany's configuration directory
 **/
const char *
ephy_config_dir (void)
{
  return config_dir;
}

/**
 * ephy_cache_dir:
 *
 * Gets Epiphany's cache directory, usually .cache/epiphany
 * under user's homedir.
 *
 * Returns: the full path to Epiphany's cache directory
 **/
const char *
ephy_cache_dir (void)
{
  return cache_dir;
}


/**
 * ephy_profile_dir_is_default:
 *
 * Returns whether the profile directory in use is the default one, found in
 * ~/.local/share
 *
 * Returns: %TRUE if it is the default profile dir, %FALSE for others
 **/
gboolean
ephy_profile_dir_is_default (void)
{
  return profile_dir_type == EPHY_PROFILE_DIR_DEFAULT || profile_dir_type == EPHY_PROFILE_DIR_TEST;
}

/**
 * ephy_profile_dir_is_web_application:
 *
 * Returns whether the profile directory in use is a web appplication one.
 *
 * Returns: %TRUE if it is a web application profile dir, %FALSE for others
 */
gboolean
ephy_profile_dir_is_web_application (void)
{
  return profile_dir_type == EPHY_PROFILE_DIR_WEB_APP;
}

/**
 * ephy_default_profile_dir:
 *
 * Get the path to the default profile directory found in ~/.local/share
 *
 * Returns: a new allocated string, free with g_free() when done.
 */
char *
ephy_default_profile_dir (void)
{
  return profile_dir_type == EPHY_PROFILE_DIR_TEST ?
    g_strdup (ephy_profile_dir ()) :
    g_build_filename (g_get_user_data_dir (), "epiphany", NULL);
}

/**
 * ephy_default_cache_dir:
 *
 * Get the path to the default cache directory found in ~/.cache
 *
 * Returns: a new allocated string, free with g_free() when done.
 */
char *
ephy_default_cache_dir (void)
{
  return profile_dir_type == EPHY_PROFILE_DIR_TEST ?
    g_build_filename (ephy_profile_dir (), "cache", NULL) :
    g_build_filename (g_get_user_cache_dir (), "epiphany", NULL);
}

/**
 * ephy_default_config_dir:
 *
 * Get the path to the default config directory found in ~/.config
 *
 * Returns: a new allocated string, free with g_free() when done.
 */
char *
ephy_default_config_dir (void)
{
  return profile_dir_type == EPHY_PROFILE_DIR_TEST ?
    g_build_filename (ephy_profile_dir (), "config", NULL) :
    g_build_filename (g_get_user_config_dir (), "epiphany", NULL);
}

/**
 * ephy_file_helpers_init:
 * @profile_dir: directory to use as Epiphany's profile
 * @flags: the %EphyFileHelpersFlags for this session
 * @error: an optional #GError
 *
 * Initializes Epiphany file helper functions, sets @profile_dir as Epiphany's
 * profile dir and whether the running session will be private.
 *
 * Returns: %FALSE if the profile dir couldn't be created or accessed
 **/
gboolean
ephy_file_helpers_init (const char          *profile_dir,
                        EphyFileHelpersFlags flags,
                        GError             **error)
{
  gboolean ret = TRUE;
  gboolean private_profile;
  gboolean steal_data_from_profile;

  ephy_file_helpers_error_quark = g_quark_from_static_string ("ephy-file-helpers-error");

  files = g_hash_table_new_full (g_str_hash,
                                 g_str_equal,
                                 (GDestroyNotify)g_free,
                                 (GDestroyNotify)g_free);

  keep_directory = flags & EPHY_FILE_HELPERS_KEEP_DIR;
  private_profile = (flags & EPHY_FILE_HELPERS_PRIVATE_PROFILE || flags & EPHY_FILE_HELPERS_TESTING_MODE);
  steal_data_from_profile = flags & EPHY_FILE_HELPERS_STEAL_DATA;

  if (profile_dir != NULL && !steal_data_from_profile) {
    if (g_path_is_absolute (profile_dir)) {
      profile_dir_global = g_strdup (profile_dir);
    } else {
      GFile *file = g_file_new_for_path (profile_dir);
      profile_dir_global = g_file_get_path (file);
      g_object_unref (file);
    }

    g_autofree char *app_file = g_build_filename (profile_dir, ".app", NULL);
    if (g_file_test (app_file, G_FILE_TEST_EXISTS)) {
      const char *app_name = ephy_web_application_get_program_name_from_profile_directory (profile_dir_global);
      cache_dir = g_build_filename (g_get_user_cache_dir (), app_name, NULL);
      config_dir = g_build_filename (g_get_user_config_dir (), app_name, NULL);
      profile_dir_type = EPHY_PROFILE_DIR_WEB_APP;
    } else {
      cache_dir = g_build_filename (profile_dir_global, "cache", NULL);
      config_dir = g_build_filename (profile_dir_global, "config", NULL);
    }
  } else if (private_profile) {
    if (ephy_file_tmp_dir () == NULL) {
      g_set_error (error,
                   EPHY_FILE_HELPERS_ERROR_QUARK,
                   0,
                   _("Could not create a temporary directory in “%s”."),
                   g_get_tmp_dir ());
      return FALSE;
    }

    profile_dir_global = g_build_filename (ephy_file_tmp_dir (),
                                           "epiphany",
                                           NULL);
    cache_dir = g_build_filename (profile_dir_global, "cache", NULL);
    config_dir = g_build_filename (profile_dir_global, "config", NULL);
    if (flags & EPHY_FILE_HELPERS_TESTING_MODE)
      profile_dir_type = EPHY_PROFILE_DIR_TEST;
  }

  if (profile_dir_global == NULL) {
    profile_dir_type = EPHY_PROFILE_DIR_DEFAULT;
    profile_dir_global = ephy_default_profile_dir ();
  }

  if (cache_dir == NULL)
    cache_dir = ephy_default_cache_dir ();

  if (config_dir == NULL)
    config_dir = ephy_default_config_dir ();

  if (flags & EPHY_FILE_HELPERS_ENSURE_EXISTS) {
    ret = ephy_ensure_dir_exists (ephy_profile_dir (), error);
    ephy_ensure_dir_exists (ephy_cache_dir (), NULL);
    ephy_ensure_dir_exists (ephy_config_dir (), NULL);
  }

  if (steal_data_from_profile && profile_dir) {
    guint i;
    const char *files_to_copy[] = { EPHY_HISTORY_FILE, EPHY_BOOKMARKS_FILE };

    for (i = 0; i < G_N_ELEMENTS (files_to_copy); i++) {
      char *filename;
      GError *err = NULL;
      GFile *source, *destination;

      filename = g_build_filename (profile_dir,
                                   files_to_copy[i],
                                   NULL);
      source = g_file_new_for_path (filename);
      g_free (filename);

      filename = g_build_filename (profile_dir_global,
                                   files_to_copy[i],
                                   NULL);
      destination = g_file_new_for_path (filename);
      g_free (filename);

      g_file_copy (source, destination,
                   G_FILE_COPY_OVERWRITE,
                   NULL, NULL, NULL, &err);
      if (err) {
        printf ("Error stealing file %s from profile: %s\n", files_to_copy[i], err->message);
        g_error_free (err);
      }

      g_object_unref (source);
      g_object_unref (destination);
    }
  }

  return ret;
}

static void
delete_files (GList *l)
{
  for (; l != NULL; l = l->next) {
    unlink (l->data);
  }
}

/**
 * ephy_file_helpers_shutdown:
 *
 * Cleans file helpers information, corresponds to ephy_file_helpers_init().
 **/
void
ephy_file_helpers_shutdown (void)
{
  g_hash_table_destroy (files);

  del_on_exit = g_list_reverse (del_on_exit);
  delete_files (del_on_exit);
  g_list_foreach (del_on_exit, (GFunc)g_free, NULL);
  g_list_free (del_on_exit);
  del_on_exit = NULL;

  if (mime_table != NULL) {
    LOG ("Destroying mime type hashtable");
    g_hash_table_destroy (mime_table);
    mime_table = NULL;
  }

  g_clear_pointer (&profile_dir_global, g_free);
  g_clear_pointer (&cache_dir, g_free);
  g_clear_pointer (&config_dir, g_free);

  if (tmp_dir != NULL) {
    if (!keep_directory) {
      /* recursively delete the contents and the
       * directory */
      LOG ("shutdown: delete tmp_dir %s", tmp_dir);
      ephy_file_delete_dir_recursively (tmp_dir, NULL);
    }

    g_free (tmp_dir);
    tmp_dir = NULL;
  }
}

/**
 * ephy_ensure_dir_exists:
 * @dir: path to a directory
 * @error: an optional GError to fill or %NULL
 *
 * Checks if @dir exists and is a directory, if it it exists and it's not a
 * directory %FALSE is returned. If @dir doesn't exist and can't be created
 * then %FALSE is returned.
 *
 * Returns: %TRUE if @dir exists and is a directory
 **/
gboolean
ephy_ensure_dir_exists (const char *dir,
                        GError    **error)
{
  if (g_file_test (dir, G_FILE_TEST_EXISTS) &&
      !g_file_test (dir, G_FILE_TEST_IS_DIR)) {
    g_set_error (error,
                 EPHY_FILE_HELPERS_ERROR_QUARK,
                 0,
                 _("The file “%s” exists. Please move it out of the way."),
                 dir);

    return FALSE;
  }

  if (!g_file_test (dir, G_FILE_TEST_EXISTS)) {
    if (g_mkdir_with_parents (dir, 488) == 0) {
      if (dir == ephy_profile_dir ()) {
        /* We need to set the .migrated file to the
         * current profile migration version,
         * otherwise the next time the browser runs
         * things might go awry. */
        ephy_profile_utils_set_migration_version (EPHY_PROFILE_MIGRATION_VERSION);
      }
    } else {
      g_set_error (error,
                   EPHY_FILE_HELPERS_ERROR_QUARK,
                   0,
                   _("Failed to create directory “%s”."),
                   dir);

      return FALSE;
    }
  }

  return TRUE;
}

static void
ephy_find_file_recursive (const char *path,
                          const char *fname,
                          GSList    **list,
                          gint        depth,
                          gint        maxdepth)
{
  GDir *dir;
  const gchar *file;

  dir = g_dir_open (path, 0, NULL);
  if (dir != NULL) {
    while ((file = g_dir_read_name (dir))) {
      if (depth < maxdepth) {
        char *new_path = g_build_filename (path, file, NULL);
        ephy_find_file_recursive (new_path, fname, list,
                                  depth + 1, maxdepth);
        g_free (new_path);
      }
      if (strcmp (file, fname) == 0) {
        char *new_path = g_build_filename (path, file, NULL);
        *list = g_slist_prepend (*list, new_path);
      }
    }

    g_dir_close (dir);
  }
}

/**
 * ephy_file_find:
 * @path: path to search for @fname
 * @fname: filename to search for
 * @maxdepth: maximum directory depth when searching @path
 *
 * Searchs for @fname in @path with a maximum depth of @maxdepth.
 *
 * Returns: a GSList of matches
 **/
GSList *
ephy_file_find (const char *path,
                const char *fname,
                gint        maxdepth)
{
  GSList *ret = NULL;
  ephy_find_file_recursive (path, fname, &ret, 0, maxdepth);
  return ret;
}

/**
 * ephy_file_delete_on_exit:
 * @file: a #GFile
 *
 * Schedules @file to be deleted when Epiphany exits. This function currently
 * does nothing.
 **/
void
ephy_file_delete_on_exit (GFile *file)
{
  /* does nothing now */
}

/**
 * ephy_file_launch_application:
 * @app: the application to launch
 * @list: files to pass to @app
 * @user_time: user time to prevent focus stealing
 * @widget: a relevant widget from where to get the #GdkScreen and #GdkDisplay
 *
 * Launches @app to open @files. If @widget is set the screen and display from
 * it will be used to launch the application, otherwise the defaults will be
 * used.
 *
 * Returns: %TRUE if g_app_info_launch() succeeded
 **/
gboolean
ephy_file_launch_application (GAppInfo  *app,
                              GList     *list,
                              guint32    user_time,
                              GtkWidget *widget)
{
  GdkAppLaunchContext *context;
  GdkDisplay *display;
  GdkScreen *screen;
  gboolean res;

  if (widget) {
    display = gtk_widget_get_display (widget);
    screen = gtk_widget_get_screen (widget);
  } else {
    display = gdk_display_get_default ();
    screen = gdk_screen_get_default ();
  }

  context = gdk_display_get_app_launch_context (display);
  gdk_app_launch_context_set_screen (context, screen);
  gdk_app_launch_context_set_timestamp (context, user_time);

  res = g_app_info_launch (app, list,
                           G_APP_LAUNCH_CONTEXT (context), NULL);
  g_object_unref (context);

  return res;
}

/**
 * ephy_file_launch_desktop_file:
 * @filename: the path to the .desktop file
 * @parameter: path to an optional parameter file to pass to the application
 * @user_time: user time to prevent focus stealing
 * @widget: an optional widget for ephy_file_launch_application()
 *
 * Calls ephy_file_launch_application() for the application described by the
 * .desktop file @filename. Can pass @parameter as optional file arguments.
 *
 * Returns: %TRUE if the application launch was successful
 **/
gboolean
ephy_file_launch_desktop_file (const char *filename,
                               const char *parameter,
                               guint32     user_time,
                               GtkWidget  *widget)
{
  GDesktopAppInfo *app;
  GFile *file = NULL;
  GList *list = NULL;
  gboolean ret;

  app = g_desktop_app_info_new (filename);
  if (parameter) {
    file = g_file_new_for_path (parameter);
    list = g_list_append (list, file);
  }

  ret = ephy_file_launch_application (G_APP_INFO (app), list, user_time, widget);
  g_list_free (list);
  if (file)
    g_object_unref (file);
  return ret;
}

GAppInfo *
ephy_file_launcher_get_app_info_for_file (GFile      *file,
                                          const char *mime_type)
{
  GAppInfo *app = NULL;

  g_assert (file || mime_type);

  if (mime_type != NULL) {
    app = g_app_info_get_default_for_type (mime_type,
                                           FALSE);
  } else {
    GFileInfo *file_info;
    char *type;

    /* Sniff mime type and check if it's safe to open */
    file_info = g_file_query_info (file,
                                   G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                   0, NULL, NULL);
    if (file_info == NULL) {
      return FALSE;
    }
    type = g_strdup (g_file_info_get_content_type (file_info));

    g_object_unref (file_info);

    if (type != NULL && type[0] != '\0') {
      app = g_app_info_get_default_for_type (type, FALSE);
    }
    g_free (type);
  }

  return app;
}

/**
 * ephy_file_launch_handler:
 * @mime_type: the mime type of @file or %NULL
 * @file: a #GFile to pass as argument
 * @user_time: user time to prevent focus stealing
 *
 * Launches @file with its default handler application, if @mime_type is %NULL
 * then @file will be queried for its type.
 *
 * Returns: %TRUE on success
 **/
gboolean
ephy_file_launch_handler (const char *mime_type,
                          GFile      *file,
                          guint32     user_time)
{
  GAppInfo *app = NULL;
  gboolean ret = FALSE;

  g_assert (file != NULL);

  app = ephy_file_launcher_get_app_info_for_file (file, mime_type);

  if (app != NULL) {
    GList *list = NULL;

    list = g_list_append (list, file);
    ret = ephy_file_launch_application (app, list, user_time, NULL);
    g_list_free (list);
  }

  return ret;
}

gboolean
ephy_file_open_uri_in_default_browser (const char *uri,
                                       guint32     timestamp,
                                       GdkScreen  *screen)
{
  GdkAppLaunchContext *context;
  GAppInfo *appinfo;
  GList uris;
  gboolean retval = TRUE;
  GError *error = NULL;

  context = gdk_display_get_app_launch_context (screen ? gdk_screen_get_display (screen) : gdk_display_get_default ());
  gdk_app_launch_context_set_screen (context, screen);
  gdk_app_launch_context_set_timestamp (context, timestamp);

  appinfo = g_app_info_get_default_for_type ("x-scheme-handler/http", TRUE);
  uris.data = (gpointer)uri;
  uris.next = uris.prev = NULL;

  if (!g_app_info_launch_uris (appinfo, &uris, G_APP_LAUNCH_CONTEXT (context), &error)) {
    g_warning ("Failed to launch %s: %s", uri, error->message);
    g_error_free (error);
    retval = FALSE;
  }

  g_object_unref (context);
  g_object_unref (appinfo);

  return retval;
}

/**
 * ephy_file_browse_to:
 * @file: a #GFile
 * @user_time: user_time to prevent focus stealing
 *
 * Launches the default application for browsing directories, with @file's
 * parent directory as its target. Passes @user_time to
 * ephy_file_launch_handler() to prevent focus stealing.
 *
 * Returns: %TRUE if the launch succeeded
 **/
gboolean
ephy_file_browse_to (GFile  *file,
                     guint32 user_time)
{
  return ephy_file_launch_handler ("inode/directory", file, user_time);
}

/**
 * ephy_file_delete_dir_recursively:
 * @directory: directory to remove
 * @error: location to set any #GError
 *
 * Remove @path and its contents. Like calling rm -rf @path.
 *
 * Returns: %TRUE if delete succeeded
 **/
gboolean
ephy_file_delete_dir_recursively (const char *directory, GError **error)
{
  GDir *dir;
  const char *file_name;
  gboolean failed = FALSE;

  dir = g_dir_open (directory, 0, error);
  if (!dir)
    return FALSE;

  file_name = g_dir_read_name (dir);
  while (file_name && !failed) {
    char *file_path;

    file_path = g_build_filename (directory, file_name, NULL);
    if (g_file_test (file_path, G_FILE_TEST_IS_DIR)) {
      failed = !ephy_file_delete_dir_recursively (file_path, error);
    } else {
      int result = g_unlink (file_path);

      if (result == -1) {
        int errsv = errno;

        g_set_error (error, G_IO_ERROR,
                     g_io_error_from_errno (errsv),
                     "Error removing file %s: %s",
                     file_path, g_strerror (errsv));
        failed = TRUE;
      }
    }
    g_free (file_path);
    file_name = g_dir_read_name (dir);
  }
  g_dir_close (dir);

  if (!failed) {
    int result = g_rmdir (directory);

    if (result == -1) {
      int errsv = errno;

      g_set_error (error, G_IO_ERROR,
                   g_io_error_from_errno (errsv),
                   "Error removing directory %s: %s",
                   directory, g_strerror (errsv));
      failed = TRUE;
    }
  }

  return !failed;
}

/**
 * ephy_file_delete_uri
 * @uri: URI of the file to be deleted
 *
 * Remove the given URI.
 */
void
ephy_file_delete_uri (const char *uri)
{
  GFile *file;
  gboolean ret;

  g_assert (uri);

  file = g_file_new_for_uri (uri);

  ret = g_file_delete (file, NULL, NULL);

  if (ret == TRUE) {
    LOG ("Deleted file at URI '%s'", uri);
  } else {
    LOG ("Couldn't file at URI '%s'", uri);
  }
  g_object_unref (file);
}

/**
 * ephy_file_move_uri
 * @source_uri: URI of the file to be moved
 * @dest_uri: URI of the destination
 *
 * Move from source_uri to dest_uri, overwriting if necessary.
 *
 * Returns: %TRUE on successful move, %FALSE otherwise.
 */
gboolean
ephy_file_move_uri (const char *source_uri, const char *dest_uri)
{
  GFile *src;
  GFile *dest;
  gboolean ret;

  g_assert (source_uri && dest_uri);

  src = g_file_new_for_uri (source_uri);
  dest = g_file_new_for_uri (dest_uri);

  ret = g_file_move (src, dest, G_FILE_COPY_OVERWRITE | G_FILE_COPY_ALL_METADATA,
                     NULL, NULL, NULL, NULL);

  if (ret == TRUE) {
    LOG ("Moved file '%s' to '%s'", source_uri, dest_uri);
  } else {
    LOG ("Couldn't move file '%s' to '%s'", source_uri, dest_uri);
  }
  g_object_unref (src);
  g_object_unref (dest);
  return ret;
}

/**
 * ephy_file_create_data_uri_for_filename:
 * @filename: the filename of a local path
 * @mime_type: the MIME type of the filename, or %NULL
 *
 * Create a data uri using the contents of @filename.
 * If @mime_type is %NULL, the %G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE
 * attribute of @filename will be used.
 *
 * Returns: a new allocated string containg the data uri, or %NULL if the
 *   data uri could not be created
 */
char *
ephy_file_create_data_uri_for_filename (const char *filename,
                                        const char *mime_type)
{
  gchar *data;
  gsize data_length;
  gchar *base64;
  gchar *uri = NULL;
  GFileInfo *file_info = NULL;

  g_assert (filename != NULL);

  if (!g_file_get_contents (filename, &data, &data_length, NULL))
    return NULL;

  base64 = g_base64_encode ((const guchar *)data, data_length);
  g_free (data);

  if (!mime_type) {
    GFile *file;

    file = g_file_new_for_path (filename);
    file_info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                   G_FILE_QUERY_INFO_NONE, NULL, NULL);
    if (file_info)
      mime_type = g_file_info_get_content_type (file_info);

    g_object_unref (file);
  }

  if (mime_type)
    uri = g_strdup_printf ("data:%s;charset=utf8;base64,%s", mime_type, base64);
  g_free (base64);

  if (file_info)
    g_object_unref (file_info);

  return uri;
}

/**
 * ephy_sanitize_filename:
 * @filename: a filename
 *
 * Sanitize @filename to make sure it's a valid filename. If the
 * filename contains directory separators, they will be converted to
 * underscores, so that they are not interpreted as a path by the
 * filesystem.
 *
 * Note that it modifies string in place. The return value is to allow nesting.
 *
 * Returns: the sanitized filename
 */
char *
ephy_sanitize_filename (char *filename)
{
  g_assert (filename != NULL);

  return g_strdelimit (filename, G_DIR_SEPARATOR_S, '_');
}

void
ephy_open_default_instance_window (void)
{
  GError *error = NULL;

  g_spawn_command_line_async ("epiphany", &error);

  if (error) {
    g_warning ("Couldn't open default instance: %s", error->message);
    g_error_free (error);
  }
}

void
ephy_open_incognito_window (const char *uri)
{
  char *command;
  GError *error = NULL;

  command = g_strdup_printf ("epiphany --incognito-mode --profile %s ", ephy_profile_dir ());

  if (uri) {
    char *str = g_strconcat (command, uri, NULL);
    g_free (command);
    command = str;
  }

  g_spawn_command_line_async (command, &error);

  if (error) {
    g_warning ("Couldn't open link in incognito window: %s", error->message);
    g_error_free (error);
  }

  g_free (command);
}

gboolean
ephy_file_launch_via_uri_handler (const char *uri)
{
  GdkDisplay *display;
  GdkAppLaunchContext *context;
  GError *error = NULL;

  display = gdk_display_get_default ();
  context = gdk_display_get_app_launch_context (display);

  g_app_info_launch_default_for_uri (uri, G_APP_LAUNCH_CONTEXT (context), &error);

  if (error != NULL) {
    g_warning ("Failed to launch handler for URI %s: %s", uri, error->message);
    g_error_free (error);
    return FALSE;
  }

  return TRUE;
}

gboolean
ephy_file_launch_file_via_uri_handler (GFile *file)
{
  const char *uri;

  uri = g_file_get_uri (file);
  return ephy_file_launch_via_uri_handler (uri);
}
