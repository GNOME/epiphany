/*
 *  Copyright (C) 2003 Marco Pesenti Gritti
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef EPHY_BOOKMARKS_IMPORT_H
#define EPHY_BOOKMARKS_IMPORT_H

#include "ephy-bookmarks.h"

G_BEGIN_DECLS

#define MOZILLA_BOOKMARKS_DIR ".mozilla"
#define GALEON_BOOKMARKS_DIR ".galeon"
#define KDE_BOOKMARKS_DIR ".kde/share/apps/konqueror"

gboolean ephy_bookmarks_import         (EphyBookmarks *bookmarks,
					const char *filename);

gboolean ephy_bookmarks_import_mozilla (EphyBookmarks *bookmarks,
					const char *filename);

gboolean ephy_bookmarks_import_xbel    (EphyBookmarks *bookmarks,
					const char *filename);

G_END_DECLS

#endif
