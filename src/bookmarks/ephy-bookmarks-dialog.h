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

#include <adwaita.h>

G_BEGIN_DECLS

#define EPHY_TYPE_BOOKMARKS_DIALOG (ephy_bookmarks_dialog_get_type())

G_DECLARE_FINAL_TYPE (EphyBookmarksDialog, ephy_bookmarks_dialog, EPHY, BOOKMARKS_DIALOG, AdwBin)

void ephy_bookmarks_dialog_set_is_editing (EphyBookmarksDialog *self,
                                           gboolean             is_editing);

GtkWidget *ephy_bookmarks_dialog_new      (void);

void ephy_bookmarks_dialog_focus (EphyBookmarksDialog *self);

void ephy_bookmarks_dialog_clear_search (EphyBookmarksDialog *self);

G_END_DECLS
