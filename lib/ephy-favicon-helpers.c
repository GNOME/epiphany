/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2012 Igalia S.L.
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

#include "config.h"
#include "ephy-favicon-helpers.h"

#include <gdk/gdk.h>
#include <glib.h>

GdkPixbuf *
ephy_pixbuf_get_from_surface_scaled (cairo_surface_t *surface,
                                     int              width,
                                     int              height)
{
  GdkPixbuf *pixbuf;
  int favicon_width;
  int favicon_height;

  /* Treat NULL surface cleanly. */
  if (!surface)
    return NULL;

  favicon_width = cairo_image_surface_get_width (surface);
  favicon_height = cairo_image_surface_get_height (surface);
  pixbuf = gdk_pixbuf_get_from_surface (surface, 0, 0, favicon_width, favicon_height);

  /* A size of (0, 0) means the original size of the favicon. */
  if (width && height && (favicon_width != width || favicon_height != height)) {
    GdkPixbuf *scaled_pixbuf = gdk_pixbuf_scale_simple (pixbuf, width, height, GDK_INTERP_BILINEAR);
    g_object_unref (pixbuf);
    pixbuf = scaled_pixbuf;
  }

  return pixbuf;
}
