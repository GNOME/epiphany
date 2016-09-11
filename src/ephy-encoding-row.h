/*
 *  Copyright © 2015 Arnaud Bonatti
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include "ephy-encoding.h"
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_ENCODING_ROW (ephy_encoding_row_get_type ())
G_DECLARE_FINAL_TYPE (EphyEncodingRow, ephy_encoding_row, EPHY, ENCODING_ROW, GtkGrid);

EphyEncodingRow *ephy_encoding_row_new          (EphyEncoding    *encoding);

EphyEncoding    *ephy_encoding_row_get_encoding (EphyEncodingRow *row);

void             ephy_encoding_row_set_selected (EphyEncodingRow *row,
                                                 gboolean         selected);

G_END_DECLS
