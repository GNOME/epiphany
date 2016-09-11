/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <glib.h>

G_BEGIN_DECLS

#define EPHY_PROFILE_MIGRATION_VERSION 10

#define EPHY_HISTORY_FILE       "ephy-history.db"
#define EPHY_BOOKMARKS_FILE     "ephy-bookmarks.xml"
#define EPHY_BOOKMARKS_FILE_RDF "bookmarks.rdf"

int ephy_profile_utils_get_migration_version (void);

gboolean ephy_profile_utils_set_migration_version (int version);

gboolean ephy_profile_utils_do_migration (const char *profile_directory, int test_to_run, gboolean debug);

G_END_DECLS
