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

#include "ephy-adaptive-mode.h"

G_BEGIN_DECLS

#define EPHY_TYPE_ACTION_BAR_START (ephy_action_bar_start_get_type ())

G_DECLARE_FINAL_TYPE (EphyActionBarStart, ephy_action_bar_start, EPHY, ACTION_BAR_START, GtkBox);

EphyActionBarStart *ephy_action_bar_start_new                               (void);
GtkWidget          *ephy_action_bar_start_get_navigation_box                (EphyActionBarStart *action_bar_start);
void                ephy_action_bar_start_change_combined_stop_reload_state (EphyActionBarStart *action_bar_start,
                                                                             gboolean            loading);
GtkWidget          *ephy_action_bar_start_get_placeholder                   (EphyActionBarStart *action_bar_start);

void                ephy_action_bar_start_set_adaptive_mode                 (EphyActionBarStart *action_bar,
                                                                             EphyAdaptiveMode    adaptive_mode);

G_END_DECLS
