/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2020 Andrei Lisita
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

#include <adwaita.h>

G_BEGIN_DECLS

#define EPHY_TYPE_LANG_ROW (ephy_lang_row_get_type ())

G_DECLARE_FINAL_TYPE (EphyLangRow, ephy_lang_row, EPHY, LANG_ROW, AdwActionRow)

GtkWidget      *ephy_lang_row_new                     ();

const char     *ephy_lang_row_get_code                (EphyLangRow *self);
void            ephy_lang_row_set_code                (EphyLangRow *self,
                                                       const char  *code);

void            ephy_lang_row_set_delete_sensitive    (EphyLangRow *self,
                                                       gboolean     sensitive);

void            ephy_lang_row_set_movable             (EphyLangRow *self,
                                                       gboolean     movable);

G_END_DECLS
