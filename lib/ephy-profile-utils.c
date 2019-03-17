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
#include "ephy-profile-utils.h"

#include "ephy-debug.h"
#include "ephy-file-helpers.h"

#include <string.h>

#define PROFILE_MIGRATION_FILE ".migrated"

int
ephy_profile_utils_get_migration_version_for_profile_dir (const char *profile_directory)
{
  char *migrated_file, *contents = NULL;
  gsize size;
  int result = 0;
  int latest = 0;

  migrated_file = g_build_filename (profile_directory,
                                    PROFILE_MIGRATION_FILE,
                                    NULL);

  if (g_file_test (migrated_file, G_FILE_TEST_EXISTS)) {
    g_file_get_contents (migrated_file, &contents, &size, NULL);

    if (contents != NULL)
      result = sscanf (contents, "%d", &latest);

    g_free (contents);

    if (result != 1)
      latest = 0;
  }

  g_free (migrated_file);

  return latest;
}

int
ephy_profile_utils_get_migration_version (void)
{
  return ephy_profile_utils_get_migration_version_for_profile_dir (ephy_profile_dir ());
}

gboolean
ephy_profile_utils_set_migration_version (int version)
{
  char *migrated_file, *contents;
  gboolean result = FALSE;

  migrated_file = g_build_filename (ephy_profile_dir (),
                                    PROFILE_MIGRATION_FILE,
                                    NULL);
  contents = g_strdup_printf ("%d", version);
  result = g_file_set_contents (migrated_file, contents, -1, NULL);

  if (result == FALSE)
    LOG ("Couldn't store migration version %d in %s (%s, %s)",
         version, migrated_file, ephy_profile_dir (), PROFILE_MIGRATION_FILE);

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
  const char *argv[8] = { PKGLIBEXECDIR "/" EPHY_PROFILE_MIGRATOR, "-v" };
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

#if DEVELOPER_MODE
  argv[0] = BUILD_ROOT "/src/" EPHY_PROFILE_MIGRATOR;
#else
  if (debug)
    argv[0] = BUILD_ROOT "/src/" EPHY_PROFILE_MIGRATOR;
#endif

  ret = g_spawn_sync (NULL, (char **)argv, envp, G_SPAWN_SEARCH_PATH,
                      NULL, NULL, NULL, NULL,
                      &status, &error);
  g_free (index);
  g_free (version);
  g_strfreev (envp);

  if (error) {
    g_warning ("Failed to run migrator: %s", error->message);
    g_error_free (error);
  }

  if (status != 0)
    ret = FALSE;

  return ret;
}
