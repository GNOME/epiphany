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
#include <libportal/portal-helpers.h>
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

static char *profile_dir_global;
static char *cache_dir;
static char *config_dir;
static char *tmp_dir;
static EphyProfileDirType profile_dir_type;

static XdpPortal *global_portal;

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
  if (!tmp_dir) {
    char *partial_name;
    char *full_name;

    partial_name = g_strconcat ("epiphany-", g_get_user_name (),
                                "-XXXXXX", NULL);
    full_name = g_build_filename (g_get_tmp_dir (), partial_name,
                                  NULL);
    tmp_dir = mkdtemp (full_name);
    g_free (partial_name);

    if (!tmp_dir)
      g_free (full_name);
  }

  return tmp_dir;
}

static char *
ephy_file_download_dir (void)
{
  const char *xdg_download_dir;

  xdg_download_dir = g_get_user_special_dir (G_USER_DIRECTORY_DOWNLOAD);
  if (xdg_download_dir)
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
 *  - Under sandbox, always use the XDG downloads directory
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
  g_autofree char *download_dir = g_settings_get_string (EPHY_SETTINGS_STATE,
                                                         EPHY_PREFS_STATE_DOWNLOAD_DIR);

  if (ephy_is_running_inside_sandbox ())
    return ephy_file_download_dir ();

  if (g_strcmp0 (download_dir, "Desktop") == 0)
    return ephy_file_desktop_dir ();

  if (g_strcmp0 (download_dir, "Downloads") == 0 ||
      !g_path_is_absolute (download_dir))
    return ephy_file_download_dir ();

  return g_steal_pointer (&download_dir);
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
  if (xdg_desktop_dir)
    return g_strdup (xdg_desktop_dir);

  /* If we don't have XDG user dirs info, return an educated guess. */
  return g_build_filename (g_get_home_dir (), _("Desktop"), NULL);
}

char *
ephy_file_get_display_name (GFile *file)
{
  g_autofree char *path = NULL;
  g_autoptr (GFileInfo) info = NULL;
  g_autoptr (GError) error = NULL;

  path = g_file_get_path (file);

  if (!g_strcmp0 (path, g_get_home_dir ()))
    return g_strdup (_("Home"));

  if (!g_strcmp0 (path, ephy_file_desktop_dir ()))
    return g_strdup (_("Desktop"));

  if (!g_strcmp0 (path, ephy_file_download_dir ()))
    return g_strdup (_("Downloads"));

  info =
    g_file_query_info (file,
                       G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
                       G_FILE_QUERY_INFO_NONE,
                       NULL,
                       &error);

  if (error) {
    g_warning ("Failed to query display name for %s: %s", path, error->message);

    return g_file_get_basename (file);
  }

  return g_strdup (g_file_info_get_display_name (info));
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
 * Returns whether the profile directory in use is a web application one.
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
ephy_file_helpers_init (const char            *profile_dir,
                        EphyFileHelpersFlags   flags,
                        GError               **error)
{
  gboolean ret = TRUE;
  gboolean private_profile;
  gboolean steal_data_from_profile;
  g_autofree char *app_file = NULL;

  ephy_file_helpers_error_quark = g_quark_from_static_string ("ephy-file-helpers-error");

  files = g_hash_table_new_full (g_str_hash,
                                 g_str_equal,
                                 (GDestroyNotify)g_free,
                                 (GDestroyNotify)g_free);

  private_profile = (flags & EPHY_FILE_HELPERS_PRIVATE_PROFILE || flags & EPHY_FILE_HELPERS_TESTING_MODE);
  steal_data_from_profile = flags & EPHY_FILE_HELPERS_STEAL_DATA;

  if (profile_dir && !steal_data_from_profile) {
    if (g_path_is_absolute (profile_dir)) {
      profile_dir_global = g_strdup (profile_dir);
    } else {
      GFile *file = g_file_new_for_path (profile_dir);
      profile_dir_global = g_file_get_path (file);
      g_object_unref (file);
    }

    app_file = g_build_filename (profile_dir, ".app", NULL);
    if (g_file_test (app_file, G_FILE_TEST_EXISTS)) {
      const char *app_name = ephy_web_application_get_gapplication_id_from_profile_directory (profile_dir_global);
      cache_dir = g_build_filename (g_get_user_cache_dir (), app_name, NULL);
      config_dir = g_build_filename (g_get_user_config_dir (), app_name, NULL);
      profile_dir_type = EPHY_PROFILE_DIR_WEB_APP;
    } else {
      cache_dir = g_build_filename (profile_dir_global, "cache", NULL);
      config_dir = g_build_filename (profile_dir_global, "config", NULL);
    }
  } else if (private_profile) {
    if (!ephy_file_tmp_dir ()) {
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

  if (!profile_dir_global) {
    profile_dir_type = EPHY_PROFILE_DIR_DEFAULT;
    profile_dir_global = ephy_default_profile_dir ();
  }

  if (!cache_dir)
    cache_dir = ephy_default_cache_dir ();

  if (!config_dir)
    config_dir = ephy_default_config_dir ();

  if (flags & EPHY_FILE_HELPERS_ENSURE_EXISTS) {
    ret = ephy_ensure_dir_exists (ephy_profile_dir (), error);
    ephy_ensure_dir_exists (ephy_cache_dir (), NULL);
    ephy_ensure_dir_exists (ephy_config_dir (), NULL);
    ephy_ensure_dir_exists (ephy_file_tmp_dir (), NULL);
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

  global_portal = xdp_portal_new ();

  return ret;
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

  if (mime_table) {
    LOG ("Destroying mime type hashtable");
    g_hash_table_destroy (mime_table);
    mime_table = NULL;
  }

  g_clear_pointer (&profile_dir_global, g_free);
  g_clear_pointer (&cache_dir, g_free);
  g_clear_pointer (&config_dir, g_free);

  if (tmp_dir) {
    LOG ("shutdown: delete tmp_dir %s", tmp_dir);
    ephy_file_delete_dir_recursively (tmp_dir, NULL);
    g_clear_pointer (&tmp_dir, g_free);
  }

  g_clear_object (&global_portal);
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
ephy_ensure_dir_exists (const char  *dir,
                        GError     **error)
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

static gboolean
launch_application (GAppInfo   *app,
                    GList      *files,
                    GdkDisplay *display)
{
  g_autoptr (GdkAppLaunchContext) context = NULL;
  g_autoptr (GError) error = NULL;
  gboolean res;

  if (!display) {
    GApplication *application = g_application_get_default ();
    GtkWindow *window = gtk_application_get_active_window (GTK_APPLICATION (application));

    if (window)
      display = gtk_widget_get_display (GTK_WIDGET (window));
    if (!display)
      display = gdk_display_get_default ();
  }

  context = gdk_display_get_app_launch_context (display);

  res = g_app_info_launch (app, files,
                           G_APP_LAUNCH_CONTEXT (context), &error);
  if (!res)
    g_warning ("Failed to launch %s: %s", g_app_info_get_display_name (app), error->message);

  return res;
}

/**
 * ephy_file_launch_uri_handler:
 * @file: a #GFile to pass as argument
 *
 * If @mime_type is %NULL, launches @file with its default handler application.
 * Otherwise, launches @file will the default handler for @mime_type.
 *
 * Returns: %TRUE on success
 **/
gboolean
ephy_file_launch_uri_handler (GFile                        *file,
                              const char                   *mime_type,
                              GdkDisplay                   *display,
                              EphyFileLaunchUriHandlerType  type)
{
  g_autoptr (GAppInfo) app = NULL;
  g_autoptr (GList) list = NULL;
  g_autoptr (GError) error = NULL;
  gboolean ret = FALSE;

  g_assert (file);

  /* Launch via URI handler only under sandbox or when user interaction is
   * required, because this way loses focus stealing prevention. There's no
   * other way to open a file when sandboxed, and focus stealing prevention
   * becomes the responsibility of the portal in this case anyway.
   */
  if (ephy_is_running_inside_sandbox ()) {
    g_autofree char *uri = g_file_get_uri (file);
    EphyOpenUriFlags openUriFlags = EPHY_OPEN_URI_FLAGS_NONE;

    if (type == EPHY_FILE_LAUNCH_URI_HANDLER_DIRECTORY)
      ephy_open_directory_via_flatpak_portal (uri, openUriFlags);
    else
      ephy_open_uri_via_flatpak_portal (uri, openUriFlags);
    return TRUE;
  }

  if (mime_type)
    app = g_app_info_get_default_for_type (mime_type, TRUE);
  if (!app)
    app = g_file_query_default_handler (file, NULL, &error);

  if (!app) {
    g_autofree char *path = g_file_get_path (file);
    g_warning ("No available application to open %s: %s", path, error->message);
    return FALSE;
  }

  list = g_list_append (list, file);
  ret = launch_application (app, list, display);

  return ret;
}

gboolean
ephy_file_open_uri_in_default_browser (const char *uri,
                                       GdkDisplay *display)
{
  g_autoptr (GFile) file = g_file_new_for_uri (uri);
  return ephy_file_launch_uri_handler (file, "x-scheme-handler/http", display, EPHY_FILE_LAUNCH_URI_HANDLER_FILE);
}

/**
 * ephy_file_browse_to:
 * @file: a #GFile
 * @display: #GdkDisplay to use for launch context
 *
 * Launches the default application for browsing directories to point to
 * @file. E.g. nautilus will jump to @file within its directory and
 * select it.
 *
 * Returns: %TRUE if the launch succeeded
 **/
gboolean
ephy_file_browse_to (GFile      *file,
                     GdkDisplay *display)
{
  return ephy_file_launch_uri_handler (file, "inode/directory", display, EPHY_FILE_LAUNCH_URI_HANDLER_DIRECTORY);
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
ephy_file_delete_dir_recursively (const char  *directory,
                                  GError     **error)
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
                     _("Error removing file %s: %s"),
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
                   _("Error removing directory %s: %s"),
                   directory, g_strerror (errsv));
      failed = TRUE;
    }
  }

  return !failed;
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
  g_assert (filename);

  return g_strdelimit (filename, G_DIR_SEPARATOR_S, '_');
}

void
ephy_open_default_instance_window (void)
{
  GError *error = NULL;

  g_spawn_command_line_async ("epiphany --new-window", &error);

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

void
ephy_copy_directory (const char *source,
                     const char *target)
{
  g_autoptr (GError) error = NULL;
  GFileType type;
  g_autoptr (GFile) src_file = g_file_new_for_path (source);
  g_autoptr (GFile) dest_file = g_file_new_for_path (target);

  type = g_file_query_file_type (src_file, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL);

  if (type == G_FILE_TYPE_DIRECTORY) {
    g_autoptr (GFileEnumerator) enumerator = NULL;

    if (!g_file_make_directory_with_parents (dest_file, NULL, &error)) {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS)) {
        g_warning ("Could not create target directory for webextension: %s", error->message);
        return;
      }

      g_error_free (error);
    }

    if (!g_file_copy_attributes (src_file, dest_file, G_FILE_COPY_NONE, NULL, &error)) {
      g_warning ("Could not copy file attributes for webextension: %s", error->message);
      return;
    }

    enumerator = g_file_enumerate_children (src_file, G_FILE_ATTRIBUTE_STANDARD_NAME, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, &error);
    if (!enumerator) {
      g_warning ("Could not create file enumberator for webextensions: %s", error->message);
      return;
    }

    for (;;) {
      g_autoptr (GFileInfo) info = g_file_enumerator_next_file (enumerator, NULL, NULL);
      if (!info)
        break;
      ephy_copy_directory (
        g_build_filename (source, g_file_info_get_name (info), NULL),
        g_build_filename (target, g_file_info_get_name (info), NULL));
    }
  } else if (type == G_FILE_TYPE_REGULAR) {
    if (!g_file_copy (src_file, dest_file, G_FILE_COPY_NONE, NULL, NULL, NULL, &error)) {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS)) {
        g_warning ("Could not copy file for webextensions: %s", error->message);
        return;
      }
    }
  } else {
    g_warning ("Copying the file type of %s isn't supported.", source);
  }
}

XdpPortal *
ephy_get_portal (void)
{
  return global_portal;
}
