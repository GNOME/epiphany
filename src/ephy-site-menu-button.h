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

G_BEGIN_DECLS

#define EPHY_TYPE_SITE_MENU_BUTTON (ephy_site_menu_button_get_type())

G_DECLARE_FINAL_TYPE (EphySiteMenuButton, ephy_site_menu_button, EPHY, SITE_MENU_BUTTON, GtkButton)

GtkWidget *ephy_site_menu_button_new  (void);

void       ephy_site_menu_button_set_zoom_level (EphySiteMenuButton *self,
                                                 char               *zoom_level);

void       ephy_site_menu_button_set_state (EphySiteMenuButton *self,
                                            unsigned int        state);

void       ephy_site_menu_button_append_description (EphySiteMenuButton *self,
                                                     const char         *section);

void       ephy_site_menu_button_clear_description (EphySiteMenuButton *self);

void       ephy_site_menu_button_update_bookmark_item (EphySiteMenuButton *self,
                                                       gboolean            has_bookmark);

gboolean   ephy_site_menu_button_is_animating (EphySiteMenuButton *self);

void       ephy_site_menu_button_set_do_animation (EphySiteMenuButton *self,
                                                   gboolean            do_animation);

void       ephy_site_menu_button_animate_reader_mode (EphySiteMenuButton *self);

void       ephy_site_menu_button_cancel_animation (EphySiteMenuButton *self);

G_END_DECLS
