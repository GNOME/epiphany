/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2000, 2001, 2002, 2003 Marco Pesenti Gritti
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

#include "ephy-window.h"
#include <adwaita.h>

G_BEGIN_DECLS

#define EPHY_TYPE_ENCODING_DIALOG (ephy_encoding_dialog_get_type ())
G_DECLARE_FINAL_TYPE (EphyEncodingDialog, ephy_encoding_dialog, EPHY, ENCODING_DIALOG, AdwWindow);

EphyEncodingDialog  *ephy_encoding_dialog_new  (EphyWindow *window);

G_END_DECLS
