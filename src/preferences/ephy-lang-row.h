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

#include <handy.h>

G_BEGIN_DECLS

#define EPHY_TYPE_LANG_ROW (ephy_lang_row_get_type ())

G_DECLARE_FINAL_TYPE (EphyLangRow, ephy_lang_row, EPHY, LANG_ROW, GtkListBoxRow)

GtkWidget      *ephy_lang_row_new                     ();

void            ephy_lang_row_set_title               (EphyLangRow *self,
                                                       const char  *title);

const char     *ephy_lang_row_get_code                (EphyLangRow *self);
void            ephy_lang_row_set_code                (EphyLangRow *self,
                                                       const char  *code);

GtkWidget      *ephy_lang_row_get_drag_event_box      (EphyLangRow *self);

void            ephy_lang_row_set_delete_sensitive    (EphyLangRow *self,
                                                       gboolean     sensitive);

GtkWidget      *ephy_lang_row_get_dnd_top_revealer    (EphyLangRow *self);
GtkWidget      *ephy_lang_row_get_dnd_bottom_revealer (EphyLangRow *self);

G_END_DECLS
