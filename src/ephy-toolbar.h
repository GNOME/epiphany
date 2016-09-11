/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2012 Igalia S.L.
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gtk/gtk.h>

#include "ephy-title-box.h"
#include "ephy-window.h"

G_BEGIN_DECLS

#define EPHY_TYPE_TOOLBAR (ephy_toolbar_get_type())

G_DECLARE_FINAL_TYPE (EphyToolbar, ephy_toolbar, EPHY, TOOLBAR, GtkHeaderBar)

GtkWidget    *ephy_toolbar_new                               (EphyWindow *window);

void          ephy_toolbar_change_combined_stop_reload_state (GSimpleAction *action,
                                                              GVariant      *state,
                                                              gpointer       user_data);

GtkWidget    *ephy_toolbar_get_location_entry                (EphyToolbar *toolbar);
EphyTitleBox *ephy_toolbar_get_title_box                     (EphyToolbar *toolbar);
GtkWidget    *ephy_toolbar_get_page_menu_button              (EphyToolbar *toolbar);
GtkWidget    *ephy_toolbar_get_new_tab_button                (EphyToolbar *toolbar);

G_END_DECLS
