/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2025 Jan-Michael Brummer <jan.brummer@tabos.org>
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
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_PAGE_MENU_BUTTON (ephy_page_menu_button_get_type())

G_DECLARE_FINAL_TYPE (EphyPageMenuButton, ephy_page_menu_button, EPHY, PAGE_MENU_BUTTON, AdwBin)

EphyPageMenuButton   *ephy_page_menu_button_new (void);

void                  ephy_page_menu_button_popup          (EphyPageMenuButton *self);
void                  ephy_page_menu_button_popdown        (EphyPageMenuButton *self);

G_END_DECLS
