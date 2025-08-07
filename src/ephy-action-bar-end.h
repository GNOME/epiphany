/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2018 Purism SPC
 *  Copyright © 2018 Adrien Plazas <kekun.plazas@laposte.net>
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

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_ACTION_BAR_END (ephy_action_bar_end_get_type ())

G_DECLARE_FINAL_TYPE (EphyActionBarEnd, ephy_action_bar_end, EPHY, ACTION_BAR_END, GtkBox);

EphyActionBarEnd *ephy_action_bar_end_new                       (void);
void              ephy_action_bar_end_set_show_bookmarks_button (EphyActionBarEnd *action_bar_end,
                                                                 gboolean          show);
void              ephy_action_bar_end_show_downloads            (EphyActionBarEnd *action_bar_end);
GtkWidget        *ephy_action_bar_end_get_downloads_revealer    (EphyActionBarEnd *action_bar_end);

void              ephy_action_bar_end_set_browser_actions       (EphyActionBarEnd *action_bar_end,
                                                                 GListStore       *browser_actions);

G_END_DECLS
