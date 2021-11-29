/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2022 Purism SPC
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

#include <gtk/gtk.h>

#define EPHY_TYPE_DOWNLOADS_PAINTABLE (ephy_downloads_paintable_get_type())

G_DECLARE_FINAL_TYPE (EphyDownloadsPaintable, ephy_downloads_paintable, EPHY, DOWNLOADS_PAINTABLE, GObject)

GdkPaintable *ephy_downloads_paintable_new          (GtkWidget              *widget);

void          ephy_downloads_paintable_animate_done (EphyDownloadsPaintable *self);

G_END_DECLS
