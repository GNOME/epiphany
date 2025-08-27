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
  g_autofree char *migrated_file = NULL;
  g_autofree char *contents = NULL;
  gsize size;
  int result = 0;
  int latest = 0;

  migrated_file = g_build_filename (profile_directory,
                                    PROFILE_MIGRATION_FILE,
                                    NULL);

  if (g_file_test (migrated_file, G_FILE_TEST_EXISTS)) {
    g_file_get_contents (migrated_file, &contents, &size, NULL);

    if (contents)
      result = sscanf (contents, "%d", &latest);

    if (result != 1)
      latest = 0;
  }

  return latest;
}

int
ephy_profile_utils_get_migration_version (void)
{
  return ephy_profile_utils_get_migration_version_for_profile_dir (ephy_profile_dir ());
}

gboolean
ephy_profile_utils_set_migration_version_for_profile_dir (int         version,
                                                          const char *profile_directory)
{
  g_autofree char *migrated_file = NULL;
  g_autofree char *contents = NULL;
  gboolean result = FALSE;

  migrated_file = g_build_filename (profile_directory,
                                    PROFILE_MIGRATION_FILE,
                                    NULL);
  contents = g_strdup_printf ("%d", version);
  result = g_file_set_contents (migrated_file, contents, -1, NULL);

  if (!result)
    LOG ("Couldn't store migration version %d in %s", version, migrated_file);

  return result;
}

gboolean
ephy_profile_utils_set_migration_version (int version)
{
  return ephy_profile_utils_set_migration_version_for_profile_dir (version, ephy_profile_dir ());
}

#define EPHY_PROFILE_MIGRATOR "ephy-profile-migrator"

gboolean
ephy_profile_utils_do_migration (const char *profile_directory,
                                 int         test_to_run,
                                 gboolean    debug)
{
  g_autoptr (GError) error = NULL;
  g_autofree char *index = NULL;
  g_autofree char *version = NULL;
  int status;
  const char *argv[8] = { PKGLIBEXECDIR "/" EPHY_PROFILE_MIGRATOR, "-v" };
  int i = 2; /* index for argv, start filling at 2. */
  g_auto (GStrv) envp = NULL;

  envp = g_environ_setenv (g_get_environ (),
                           "EPHY_LOG_MODULES", "ephy-profile",
                           TRUE);

  version = g_strdup_printf ("%d", EPHY_PROFILE_MIGRATION_VERSION);
  argv[i++] = version;

  /* If we're not trying to run a migration step in a test and there
   *  is nothing to migrate, don't spawn the migrator at all. */
  if (test_to_run == -1 &&
      EPHY_PROFILE_MIGRATION_VERSION == ephy_profile_utils_get_migration_version ()) {
    return TRUE;
  }

  if (test_to_run != -1) {
    index = g_strdup_printf ("%d", test_to_run);

    argv[i++] = "-d";
    argv[i++] = index;
  }

  if (profile_directory) {
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

  g_spawn_sync (NULL, (char **)argv, envp, G_SPAWN_SEARCH_PATH,
                NULL, NULL, NULL, NULL,
                &status, &error);
  if (error) {
    g_warning ("Failed to run migrator: %s", error->message);
    return FALSE;
  }

  return status == 0;
}
