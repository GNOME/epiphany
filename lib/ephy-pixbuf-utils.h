/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2024 Red Hat
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

#include <gdk/gdk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

GdkTexture *ephy_texture_new_for_pixbuf (GdkPixbuf *pixbuf);

GdkPixbuf *ephy_texture_to_pixbuf (GdkTexture *texture,
                                   gboolean has_alpha);

GdkPixbuf *ephy_get_pixbuf_from_surface (cairo_surface_t *surface,
                                         int              src_x,
                                         int              src_y,
                                         int              width,
                                         int              height);

