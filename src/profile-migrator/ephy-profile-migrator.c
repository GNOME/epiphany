/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*
 *  Copyright © 2009 Xan López
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

#include "ephy-debug.h"
#include "ephy-file-helpers.h"
#include "ephy-flatpak-utils.h"
#include "ephy-profile-utils.h"
#include "ephy-settings.h"
#include "ephy-string.h"
#include "ephy-web-app-utils.h"

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <locale.h>

static int do_step_n = -1;
static int migration_version = -1;
static char *profile_dir = NULL;

typedef void (*EphyProfileMigrator) (void);

static void
migrate_pre_flatpak_webapps (void)
{
  /* Rename webapp folder, desktop file name and content from
   *  org.gnome.Epiphany.WebApp-${id}
   * to
   *  ${APP_ID}.WebApp_${hash}
   *
   * The app id sometimes has a Devel or Canary suffix, and we need the
   * GApplication ID to have the app id as a prefix for the dynamic launcher
   * portal to work. Also change the hyphen to an underscore for friendliness
   * with D-Bus. The ${id} sometimes has the app name so that is what
   * differentiates it from ${hash}. Finally, we also move the .desktop file
   * and the icon to the locations used by xdg-desktop-portal, which is what we
   * will use for creating the desktop files going forward.
   */
  g_autoptr (GFile) parent_directory = NULL;
  g_autoptr (GFileEnumerator) children = NULL;
  g_autoptr (GFileInfo) info = NULL;
  g_autoptr (GError) error = NULL;
  const char *parent_directory_path = g_get_user_data_dir ();
  g_autofree char *portal_desktop_dir = NULL;
  g_autofree char *portal_icon_dir = NULL;

  /* This migration is both not needed and not possible from within Flatpak. */
  if (ephy_is_running_inside_sandbox ())
    return;

  portal_desktop_dir = g_build_filename (g_get_user_data_dir (), "xdg-desktop-portal", "applications", NULL);
  portal_icon_dir = g_build_filename (g_get_user_data_dir (), "xdg-desktop-portal", "icons", "192x192", NULL);
  if (g_mkdir_with_parents (portal_desktop_dir, 0700) == -1)
    g_warning ("Failed to create portal desktop file dir: %s", g_strerror (errno));
  if (g_mkdir_with_parents (portal_icon_dir, 0700) == -1)
    g_warning ("Failed to create portal icon dir: %s", g_strerror (errno));

  parent_directory = g_file_new_for_path (parent_directory_path);
  children = g_file_enumerate_children (parent_directory,
                                        "standard::name",
                                        0, NULL, &error);
  if (!children) {
    g_warning ("Cannot enumerate profile directory: %s", error->message);
    return;
  }

  info = g_file_enumerator_next_file (children, NULL, &error);
  if (error) {
    g_warning ("Cannot enumerate profile directory: %s", error->message);
    return;
  }

  while (info) {
    const char *name;

    name = g_file_info_get_name (info);
    if (!g_str_has_prefix (name, "org.gnome.Epiphany.WebApp-"))
      goto next;

    /* the directory is either org.gnome.Epiphany.WebApp-name-checksum or
     * org.gnome.Epiphany.WebApp-checksum but unfortunately name can include a
     * hyphen
     */
    {
      g_autofree char *old_desktop_file_name = NULL;
      g_autofree char *new_desktop_file_name = NULL;
      g_autofree char *old_desktop_path_name = NULL;
      g_autofree char *new_desktop_path_name = NULL;
      g_autofree char *app_desktop_file_name = NULL;
      g_autofree char *old_cache_path = NULL;
      g_autofree char *new_cache_path = NULL;
      g_autofree char *old_config_path = NULL;
      g_autofree char *new_config_path = NULL;
      g_autofree char *old_data = NULL;
      g_autofree char *new_data = NULL;
      g_autofree char *icon = NULL;
      g_autofree char *new_icon = NULL;
      g_autofree char *tryexec = NULL;
      g_autofree char *relative_path = NULL;
      g_autoptr (GKeyFile) keyfile = NULL;
      g_autoptr (GFile) app_link_file = NULL;
      g_autoptr (GFile) old_desktop_file = NULL;
      const char *final_hyphen, *checksum;
      g_autofree char *new_name = NULL;

      final_hyphen = strrchr (name, '-');
      g_assert (final_hyphen);
      checksum = final_hyphen + 1;
      if (*checksum == '\0') {
        g_warning ("Web app ID %s is broken: should end with checksum, not hyphen", name);
        goto next;
      }
      new_name = g_strconcat (APPLICATION_ID, ".WebApp_", checksum, NULL);

      /* Rename profile directory */
      old_desktop_path_name = g_strconcat (parent_directory_path, G_DIR_SEPARATOR_S, name, NULL);
      new_desktop_path_name = g_strconcat (parent_directory_path, G_DIR_SEPARATOR_S, new_name, NULL);
      LOG ("migrate_profile_directories: moving '%s' to '%s'", old_desktop_path_name, new_desktop_path_name);
      if (g_rename (old_desktop_path_name, new_desktop_path_name) == -1) {
        g_warning ("Cannot rename web application directory from '%s' to '%s'", old_desktop_path_name, new_desktop_path_name);
        goto next;
      }

      /* Rename cache directory */
      old_cache_path = g_strconcat (g_get_user_cache_dir (), G_DIR_SEPARATOR_S, name, NULL);
      new_cache_path = g_strconcat (g_get_user_cache_dir (), G_DIR_SEPARATOR_S, new_name, NULL);
      if (g_file_test (old_cache_path, G_FILE_TEST_IS_DIR) &&
          g_rename (old_cache_path, new_cache_path) == -1) {
        g_warning ("Cannot rename web application cache directory from '%s' to '%s'", old_cache_path, new_cache_path);
      }

      /* Rename config directory */
      old_config_path = g_strconcat (g_get_user_config_dir (), G_DIR_SEPARATOR_S, name, NULL);
      new_config_path = g_strconcat (g_get_user_config_dir (), G_DIR_SEPARATOR_S, new_name, NULL);
      if (g_file_test (old_config_path, G_FILE_TEST_IS_DIR) &&
          g_rename (old_config_path, new_config_path) == -1) {
        g_warning ("Cannot rename web application config directory from '%s' to '%s'", old_config_path, new_config_path);
      }

      /* Create new desktop file */
      old_desktop_file_name = g_strconcat (parent_directory_path, G_DIR_SEPARATOR_S, new_name, G_DIR_SEPARATOR_S, name, ".desktop", NULL);
      new_desktop_file_name = g_strconcat (portal_desktop_dir, G_DIR_SEPARATOR_S, new_name, ".desktop", NULL);

      /* Fix paths in desktop file */
      if (!g_file_get_contents (old_desktop_file_name, &old_data, NULL, &error)) {
        g_warning ("Cannot read contents of '%s': %s", old_desktop_file_name, error->message);
        goto next;
      }
      new_data = ephy_string_find_and_replace ((const char *)old_data, name, new_name);
      keyfile = g_key_file_new ();
      if (!g_key_file_load_from_data (keyfile, new_data, -1,
                                      G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS,
                                      &error)) {
        g_warning ("Cannot load old desktop file %s as key file: %s", old_desktop_file_name, error->message);
        g_warning ("Key file contents:\n%s\n", (const char *)new_data);
        goto next;
      }
      /* The StartupWMClass will not always have been corrected by the
       * find-and-replace above, since it has the form
       * org.gnome.Epiphany.WebApp-<normalized_name>-<checksum> whereas the
       * gapplication id sometimes omits the <normalized_name> part
       */
      g_key_file_set_string (keyfile, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_STARTUP_WM_CLASS, new_name);

      /* Correct the Icon= key */
      icon = g_key_file_get_string (keyfile, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_ICON, &error);
      if (!icon) {
        g_warning ("Failed to get Icon key from %s: %s", old_desktop_file_name, error->message);
        g_clear_error (&error);
      } else if (g_str_has_suffix (icon, EPHY_WEB_APP_ICON_NAME)) {
        new_icon = g_strconcat (portal_icon_dir, G_DIR_SEPARATOR_S, new_name, ".png", NULL);
        if (g_rename (icon, new_icon) == -1) {
          g_warning ("Cannot rename icon from '%s' to '%s'", icon, new_icon);
          goto next;
        }
        LOG ("migrate_profile_directories: setting Icon to %s", new_icon);
        g_key_file_set_string (keyfile, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_ICON, new_icon);
      }

      tryexec = g_key_file_get_string (keyfile, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_TRY_EXEC, NULL);
      if (!tryexec)
        g_key_file_set_string (keyfile, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_TRY_EXEC, "epiphany");

      if (!g_key_file_save_to_file (keyfile, new_desktop_file_name, &error)) {
        g_warning ("Failed to save desktop file %s", error->message);
        goto next;
      }

      old_desktop_file = g_file_new_for_path (old_desktop_file_name);
      if (!g_file_delete (old_desktop_file, NULL, &error)) {
        g_warning ("Cannot delete old desktop file: %s", error->message);
        goto next;
      }

      /* Remove old symlink */
      app_desktop_file_name = g_strconcat (g_get_user_data_dir (), G_DIR_SEPARATOR_S, "applications",
                                           G_DIR_SEPARATOR_S, name, ".desktop", NULL);
      if (g_remove (app_desktop_file_name) == -1) {
        g_warning ("Cannot remove old desktop file symlink '%s'", app_desktop_file_name);
        goto next;
      }

      /* Create new symlink */
      g_free (app_desktop_file_name);
      app_desktop_file_name = g_strconcat (g_get_user_data_dir (), G_DIR_SEPARATOR_S, "applications",
                                           G_DIR_SEPARATOR_S, new_name, ".desktop", NULL);
      app_link_file = g_file_new_for_path (app_desktop_file_name);
      relative_path = g_strconcat ("..", G_DIR_SEPARATOR_S, "xdg-desktop-portal", G_DIR_SEPARATOR_S,
                                   "applications", G_DIR_SEPARATOR_S, new_name, ".desktop", NULL);

      if (!g_file_make_symbolic_link (app_link_file, relative_path, NULL, &error)) {
        g_warning ("Cannot create symlink to new desktop file %s: %s", new_desktop_file_name, error->message);
        goto next;
      }
    }

next:
    g_clear_error (&error);
    info = g_file_enumerator_next_file (children, NULL, &error);
    if (!info && error) {
      g_warning ("Cannot enumerate next file: %s", error->message);
      return;
    }
  }
}

static void
migrate_gsb_db (void)
{
  g_autofree char *threats_db = g_build_filename (ephy_default_cache_dir (), "gsb-threats.db", NULL);
  g_autofree char *threats_db_journal = g_build_filename (ephy_default_cache_dir (), "gsb-threats.db-journal", NULL);

  if (g_unlink (threats_db) == -1 && errno != ENOENT)
    g_warning ("Failed to delete %s: %s", threats_db, g_strerror (errno));

  if (g_unlink (threats_db_journal) == -1 && errno != ENOENT)
    g_warning ("Failed to delete %s: %s", threats_db_journal, g_strerror (errno));
}

static void
migrate_search_engines (void)
{
  g_settings_reset (EPHY_SETTINGS_MAIN, EPHY_PREFS_SEARCH_ENGINES);
}

[[maybe_unused]]
static void
migrate_nothing (void)
{
  /* Used to replace migrators that have been removed. Only remove migrators
   * that support particularly ancient versions of Epiphany.
   *
   * Note that you can only remove a migrator from the migrators struct if
   * all earlier migrators have also been removed and you bump
   * EPHY_MINIMUM_MIGRATION_VERSION. Otherwise, that would mess up tracking of
   * which migrators have been run.
   */
}

/* If adding anything here, you need to edit EPHY_PROFILE_MIGRATION_VERSION
 * in ephy-profile-utils.h. */
const int EPHY_MINIMUM_MIGRATION_VERSION = 37;
const EphyProfileMigrator migrators[] = {
  /* 37 */
  migrate_pre_flatpak_webapps,
  /* 38 */ migrate_gsb_db,
  /* 39 */ migrate_search_engines,
};

static gboolean
ephy_migrator (void)
{
  int previous;
  EphyProfileMigrator m;
  g_autofree char *legacy_migration_file = NULL;

  /* If there's no profile dir, there's nothing to migrate. */
  if (!g_file_test (ephy_profile_dir (), G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
    return TRUE;

  if (do_step_n != -1) {
    if (do_step_n > EPHY_PROFILE_MIGRATION_VERSION ||
        do_step_n < EPHY_MINIMUM_MIGRATION_VERSION) {
      g_printf ("Invalid migration step %d\n", do_step_n);
      return FALSE;
    }

    LOG ("Running only migrator: %d", do_step_n);
    m = migrators[do_step_n - EPHY_MINIMUM_MIGRATION_VERSION];
    m ();

    return TRUE;
  }

  /* The minimum previous migration version is exactly one less than the
   * smallest version we still support.
   */
  previous = MAX (ephy_profile_utils_get_migration_version (), EPHY_MINIMUM_MIGRATION_VERSION - 1);

  LOG ("Running migrators up to version %d, current migration version is %d.",
       EPHY_PROFILE_MIGRATION_VERSION, previous);

  for (int i = previous + 1; i <= EPHY_PROFILE_MIGRATION_VERSION; i++) {
    LOG ("Running migrator: %d of %d", i, EPHY_PROFILE_MIGRATION_VERSION);

    m = migrators[i - EPHY_MINIMUM_MIGRATION_VERSION];
    m ();
  }

  if (!ephy_profile_utils_set_migration_version (EPHY_PROFILE_MIGRATION_VERSION)) {
    LOG ("Failed to store the current migration version");
    return FALSE;
  }

  return TRUE;
}

static const GOptionEntry option_entries[] = {
  {
    "do-step", 'd', 0, G_OPTION_ARG_INT, &do_step_n,
    N_("Executes only the n-th migration step"), NULL
  },
  {
    "version", 'v', 0, G_OPTION_ARG_INT, &migration_version,
    N_("Specifies the required version for the migrator"), NULL
  },
  {
    "profile-dir", 'p', 0, G_OPTION_ARG_FILENAME, &profile_dir,
    N_("Specifies the profile where the migrator should run"), NULL
  },
  { NULL }
};

int
main (int   argc,
      char *argv[])
{
  GOptionContext *option_context;
  GOptionGroup *option_group;
  GError *error = NULL;
  EphyFileHelpersFlags file_helpers_flags = EPHY_FILE_HELPERS_NONE;

  setlocale (LC_ALL, "");
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  g_assert (EPHY_PROFILE_MIGRATION_VERSION - EPHY_MINIMUM_MIGRATION_VERSION + 1 == G_N_ELEMENTS (migrators));

  option_group = g_option_group_new ("ephy-profile-migrator",
                                     N_("Web profile migrator"),
                                     N_("Web profile migrator options"),
                                     NULL, NULL);

  g_option_group_set_translation_domain (option_group, GETTEXT_PACKAGE);
  g_option_group_add_entries (option_group, option_entries);

  option_context = g_option_context_new ("");
  g_option_context_set_main_group (option_context, option_group);

  if (!g_option_context_parse (option_context, &argc, &argv, &error)) {
    g_print ("Failed to parse arguments: %s\n", error->message);
    g_error_free (error);
    g_option_context_free (option_context);

    return 1;
  }

  g_option_context_free (option_context);

  if (migration_version != -1 && migration_version != EPHY_PROFILE_MIGRATION_VERSION) {
    g_print ("Version mismatch, version %d requested but our version is %d\n",
             migration_version, EPHY_PROFILE_MIGRATION_VERSION);

    return 1;
  }

  ephy_debug_init ();

  if (profile_dir)
    file_helpers_flags |= EPHY_FILE_HELPERS_PRIVATE_PROFILE;

  if (!ephy_file_helpers_init (profile_dir, file_helpers_flags, NULL)) {
    LOG ("Something wrong happened with ephy_file_helpers_init()");
    return -1;
  }

  return ephy_migrator () ? 0 : 1;
}
