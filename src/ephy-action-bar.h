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

#include <adwaita.h>

#include "ephy-action-bar-end.h"
#include "ephy-action-bar-start.h"
#include "ephy-adaptive-mode.h"
#include "ephy-window.h"

G_BEGIN_DECLS

#define EPHY_TYPE_ACTION_BAR (ephy_action_bar_get_type ())

G_DECLARE_FINAL_TYPE (EphyActionBar, ephy_action_bar, EPHY, ACTION_BAR, AdwBin);

EphyActionBar      *ephy_action_bar_new                  (EphyWindow *window);
EphyActionBarStart *ephy_action_bar_get_action_bar_start (EphyActionBar *action_bar);
EphyActionBarEnd   *ephy_action_bar_get_action_bar_end   (EphyActionBar *action_bar);

G_END_DECLS
