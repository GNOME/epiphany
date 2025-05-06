/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2003 Christian Persch
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

#include <glib.h>
#include <glib/gi18n.h>

G_BEGIN_DECLS

#define ZOOM_MINIMAL  (0.30f)
#define ZOOM_MAXIMAL  (3.00f)
#define ZOOM_IN       (-1.0)
#define ZOOM_OUT      (-2.0)

float       ephy_zoom_get_changed_zoom_level (float level,
                                              int   steps);

int         ephy_zoom_get_index              (gdouble value);

gdouble     ephy_zoom_get_value              (int index);

G_END_DECLS
