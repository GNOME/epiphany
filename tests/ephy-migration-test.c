/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * ephy-migration-test.c
 *
 * Copyright © 2012 - Igalia S.L.
 *
 * Epiphany is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Epiphany is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Epiphany; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include "config.h"

#include "ephy-debug.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-private.h"
#include "ephy-file-helpers.h"
#include "ephy-private.h"
#include "ephy-profile-utils.h"

#include <gtk/gtk.h>

static void
test_do_migration_simple (void)
{
    gboolean ret;

    ret = ephy_profile_utils_do_migration (NULL, -1, TRUE);
    g_assert (ret);
}

static void
test_do_migration_invalid (void)
{
    gboolean ret;

    ret = ephy_profile_utils_do_migration (NULL, EPHY_PROFILE_MIGRATION_VERSION + 1, TRUE);
    g_assert (ret == FALSE);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv);

  ephy_debug_init ();

  if (!ephy_file_helpers_init (NULL,
                               EPHY_FILE_HELPERS_PRIVATE_PROFILE | EPHY_FILE_HELPERS_ENSURE_EXISTS,
                               NULL)) {
    g_debug ("Something wrong happened with ephy_file_helpers_init()");
    return -1;
  }

  g_test_add_func ("/lib/ephy-profile-utils/do_migration_simple",
                   test_do_migration_simple);
  g_test_add_func ("/lib/ephy-profile-utils/do_migration_invalid",
                   test_do_migration_invalid);

  return g_test_run ();
}
