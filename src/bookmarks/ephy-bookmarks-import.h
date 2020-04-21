/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2016 Iulian-Gabriel Radu <iulian.radu67@gnome.org>
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

#include "ephy-bookmarks-manager.h"

G_BEGIN_DECLS

#define FIREFOX_PROFILES_DIR        ".mozilla/firefox"
#define FIREFOX_PROFILES_FILE       "profiles.ini"
#define FIREFOX_BOOKMARKS_FILE      "places.sqlite"

gboolean    ephy_bookmarks_import               (EphyBookmarksManager  *manager,
                                                 const char            *filename,
                                                 GError               **error);

gboolean    ephy_bookmarks_import_from_firefox  (EphyBookmarksManager  *manager,
                                                 const gchar           *profile,
                                                 GError               **error);

gboolean    ephy_bookmarks_import_from_html     (EphyBookmarksManager  *manager,
                                                 const char            *filename,
                                                 GError               **error);

gboolean    ephy_bookmarks_import_from_chrome   (EphyBookmarksManager  *manager,
                                                 const char            *filename,
                                                 GError               **error);

G_END_DECLS
