/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2016 Gabriel Ivascu <ivascu.gabriel59@gmail.com>
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

#ifndef EPHY_SYNC_BOOKMARKS_H
#define EPHY_SYNC_BOOKMARKS_H

#include "ephy-sync-service.h"

#include <glib-object.h>

G_BEGIN_DECLS

void ephy_sync_bookmarks_create_storage_collection (void);

void ephy_sync_bookmarks_check_storage_collection  (void);

void ephy_sync_bookmarks_delete_storage_collection (void);

G_END_DECLS

#endif
