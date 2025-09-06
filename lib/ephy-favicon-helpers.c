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

#include "ephy-pixbuf-utils.h"

#include <gdk/gdk.h>
#include <glib.h>

GIcon *
ephy_favicon_get_from_texture_scaled (GdkTexture *texture,
                                      int         width,
                                      int         height)
{
  g_autoptr (GdkPixbuf) pixbuf = NULL;
  int favicon_width;
  int favicon_height;

  if (!texture)
    return NULL;

  /* A size of (0, 0) means the original size of the favicon. */
  if (width == 0 && height == 0)
    return G_ICON (g_object_ref (texture));

  favicon_width = gdk_texture_get_width (texture);
  favicon_height = gdk_texture_get_height (texture);
  if (favicon_width == width && favicon_height == height)
    return G_ICON (g_object_ref (texture));

  pixbuf = ephy_texture_to_pixbuf (texture, TRUE);
  return G_ICON (gdk_pixbuf_scale_simple (pixbuf, width, height, GDK_INTERP_BILINEAR));
}
