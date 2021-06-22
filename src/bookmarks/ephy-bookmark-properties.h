/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2016 Iulian-Gabriel Radu <iulian.radu67@gmail.com>
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

#include "ephy-bookmark.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_BOOKMARK_PROPERTIES (ephy_bookmark_properties_get_type ())

G_DECLARE_FINAL_TYPE (EphyBookmarkProperties, ephy_bookmark_properties, EPHY, BOOKMARK_PROPERTIES, GtkBox)

typedef enum {
  EPHY_BOOKMARK_PROPERTIES_TYPE_DIALOG,
  EPHY_BOOKMARK_PROPERTIES_TYPE_POPOVER
} EphyBookmarkPropertiesType;

GtkWidget *ephy_bookmark_properties_new                (EphyBookmark                   *bookmark,
                                                        EphyBookmarkPropertiesType  type,
                                                        GtkWidget                      *parent);

GtkWidget *ephy_bookmark_properties_get_add_tag_button (EphyBookmarkProperties *self);

G_END_DECLS
