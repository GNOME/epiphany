/* vim: set sw=2 ts=2 sts=2 et: */
/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Copyright (C) 2016 Iulian-Gabriel Radu <iulian.radu67@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _EPHY_BOOKMARK_PROPERTIES_GRID_H
#define _EPHY_BOOKMARK_PROPERTIES_GRID_H

#include "ephy-bookmark.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_BOOKMARK_PROPERTIES_GRID (ephy_bookmark_properties_grid_get_type ())

G_DECLARE_FINAL_TYPE (EphyBookmarkPropertiesGrid, ephy_bookmark_properties_grid, EPHY, BOOKMARK_PROPERTIES_GRID, GtkGrid)

typedef enum {
  EPHY_BOOKMARK_PROPERTIES_GRID_TYPE_DIALOG,
  EPHY_BOOKMARK_PROPERTIES_GRID_TYPE_POPOVER
} EphyBookmarkPropertiesGridType;

GtkWidget *ephy_bookmark_properties_grid_new                (EphyBookmark *bookmark,
                                                             EphyBookmarkPropertiesGridType type);

GtkWidget *ephy_bookmark_properties_grid_get_add_tag_button (EphyBookmarkPropertiesGrid *self);

G_END_DECLS

#endif /* _EPHY_BOOKMARK_PROPERTIES_GRID_H */
