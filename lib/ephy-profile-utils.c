/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2009 Xan López
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "ephy-profile-utils.h"

#include "ephy-debug.h"
#include "ephy-file-helpers.h"

#define PROFILE_MIGRATION_FILE ".migrated"

int
ephy_profile_utils_get_migration_version ()
{
  char *migrated_file, *contents = NULL;
  gsize size;
  int result = 0;
  int latest = 0;

  migrated_file = g_build_filename (ephy_dot_dir (),
                                    PROFILE_MIGRATION_FILE,
                                    NULL);

  if (g_file_test (migrated_file, G_FILE_TEST_EXISTS)) {
    g_file_get_contents (migrated_file, &contents, &size, NULL);

    if (contents != NULL)
      result = sscanf(contents, "%d", &latest);

    g_free (contents);

    if (result != 1)
      latest = 0;
  } else if (ephy_dot_dir_is_default () == FALSE) {
    /* Since version 8, we need to migrate also profile directories
       other than the default one. Profiles in such directories work
       perfectly fine without going through the first 7 migration
       steps, so it is safe to assume that any non-default profile
       directory without a migration file can be migrated starting
       from the step 8. */
    latest = 7;
  }

  g_free (migrated_file);

  return latest;
}

gboolean
ephy_profile_utils_set_migration_version (int version)
{
  char *migrated_file, *contents;
  gboolean result = FALSE;

  migrated_file = g_build_filename (ephy_dot_dir (),
                                    PROFILE_MIGRATION_FILE,
                                    NULL);
  contents = g_strdup_printf ("%d", version);
  result = g_file_set_contents (migrated_file, contents, -1, NULL);

  if (result == FALSE)
    LOG ("Couldn't store migration version %d in %s (%s, %s)",
         version, migrated_file, ephy_dot_dir (), PROFILE_MIGRATION_FILE);

  g_free (contents);
  g_free (migrated_file);

  return result;
}

#define EPHY_PROFILE_MIGRATOR "ephy-profile-migrator"

gboolean
ephy_profile_utils_do_migration (const char *profile_directory, int test_to_run, gboolean debug)
{
  gboolean ret;
  GError *error = NULL;
  char *index = NULL, *version = NULL;
  int status;
  char *argv[6] = { EPHY_PROFILE_MIGRATOR, "-v" };
  int i = 2; /* index for argv, start filling at 2. */
  char **envp;

  envp = g_environ_setenv (g_get_environ (),
                           "EPHY_LOG_MODULES", "ephy-profile",
                           TRUE);

  argv[i++] = version = g_strdup_printf ("%d", EPHY_PROFILE_MIGRATION_VERSION);

  /* If we're not trying to run a migration step in a test and there
     is nothing to migrate, don't spawn the migrator at all. */
  if (test_to_run == -1 &&
      EPHY_PROFILE_MIGRATION_VERSION == ephy_profile_utils_get_migration_version ()) {
    g_strfreev (envp);
    return TRUE;
  }

  if (test_to_run != -1) {
    index = g_strdup_printf ("%d", test_to_run);

    argv[i++] = "-d";
    argv[i++] = index;
  }

  if (profile_directory != NULL) {
    argv[i++] = "-p";
    argv[i++] = (char *)profile_directory;
  }

  argv[i++] = NULL;

  if (debug)
    argv[0] = ABS_TOP_BUILD_DIR"/lib/"EPHY_PROFILE_MIGRATOR;

  ret = g_spawn_sync (NULL, argv, envp, G_SPAWN_SEARCH_PATH,
                      NULL, NULL, NULL, NULL,
                      &status, &error);
  g_free (index);
  g_free (version);
  g_strfreev (envp);
    
  if (error) {
    LOG ("Failed to run migrator: %s", error->message);
    g_error_free (error);
  }

  if (status != 0)
    ret = FALSE;

  return ret;
}
