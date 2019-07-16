/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

#include "ephy-debug.h"
#include "ephy-embed-prefs.h"
#include "ephy-file-helpers.h"
#include "ephy-profile-utils.h"

#include <errno.h>
#include <gtk/gtk.h>

static char *
create_test_profile_dir (void)
{
  char *tmpdir;
  GError *error = NULL;

  tmpdir = g_dir_make_tmp ("ephy-migration-test-XXXXXX", &error);
  g_assert_no_error (error);

  return tmpdir;
}

static void
delete_test_profile_dir (const char *tmpdir)
{
  gboolean ret;
  GError *error = NULL;

  ret = ephy_file_delete_dir_recursively (tmpdir, &error);
  g_assert_true (ret);
  g_assert_no_error (error);
}

static void
test_do_migration_simple (void)
{
  gboolean ret;
  char *tmpdir;

  tmpdir = create_test_profile_dir ();
  ret = ephy_profile_utils_do_migration (tmpdir, -1, TRUE);
  g_assert_true (ret);

  delete_test_profile_dir (tmpdir);
  g_free (tmpdir);
}

static void
test_do_migration_invalid (void)
{
  gboolean ret;
  char *tmpdir;

  tmpdir = create_test_profile_dir ();
  ret = ephy_profile_utils_do_migration (tmpdir, EPHY_PROFILE_MIGRATION_VERSION + 1, TRUE);
  g_assert_false (ret);

  delete_test_profile_dir (tmpdir);
  g_free (tmpdir);
}

int
main (int   argc,
      char *argv[])
{
  int ret;
  gtk_test_init (&argc, &argv);

  ephy_debug_init ();

  if (!ephy_file_helpers_init (NULL,
                               EPHY_FILE_HELPERS_TESTING_MODE | EPHY_FILE_HELPERS_ENSURE_EXISTS,
                               NULL)) {
    g_debug ("Something wrong happened with ephy_file_helpers_init()");
    return -1;
  }

  g_test_add_func ("/lib/ephy-profile-utils/do_migration_simple",
                   test_do_migration_simple);
  g_test_add_func ("/lib/ephy-profile-utils/do_migration_invalid",
                   test_do_migration_invalid);

  ret = g_test_run ();

  ephy_file_helpers_shutdown ();

  return ret;
}
