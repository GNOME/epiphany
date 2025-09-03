/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#pragma once

#include <glib.h>

G_BEGIN_DECLS

#define EPHY_PROFILE_MIGRATION_VERSION 39
#define EPHY_INSECURE_PASSWORDS_MIGRATION_VERSION 11
#define EPHY_FIREFOX_SYNC_PASSWORDS_MIGRATION_VERSION 19
#define EPHY_TARGET_ORIGIN_MIGRATION_VERSION 21
#define EPHY_SYNC_DEVICE_ID_MIGRATION_VERSION 23
#define EPHY_PASSWORDS_TIMESTAMP_MIGRATION_VERSION 25

#define EPHY_BOOKMARKS_FILE     "bookmarks.gvdb"
#define EPHY_HISTORY_FILE       "ephy-history.db"

int ephy_profile_utils_get_migration_version (void);
int ephy_profile_utils_get_migration_version_for_profile_dir (const char *profile_directory);

gboolean ephy_profile_utils_set_migration_version (int version);
gboolean ephy_profile_utils_set_migration_version_for_profile_dir (int version, const char *profile_directory);

gboolean ephy_profile_utils_do_migration (const char *profile_directory, int test_to_run, gboolean debug);

G_END_DECLS
