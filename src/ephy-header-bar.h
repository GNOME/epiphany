/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2012 Igalia S.L.
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
#include "ephy-title-widget.h"
#include "ephy-adaptive-mode.h"
#include "ephy-window.h"

G_BEGIN_DECLS

#define EPHY_TYPE_HEADER_BAR (ephy_header_bar_get_type())

G_DECLARE_FINAL_TYPE (EphyHeaderBar, ephy_header_bar, EPHY, HEADER_BAR, AdwBin)

GtkWidget          *ephy_header_bar_new                            (EphyWindow    *window);
EphyTitleWidget    *ephy_header_bar_get_title_widget               (EphyHeaderBar *header_bar);
GtkWidget          *ephy_header_bar_get_page_menu_button           (EphyHeaderBar *header_bar);
EphyWindow         *ephy_header_bar_get_window                     (EphyHeaderBar *header_bar);
void                ephy_header_bar_set_reader_mode_state          (EphyHeaderBar *header_bar,
                                                                    EphyWebView   *view);
EphyActionBarStart *ephy_header_bar_get_action_bar_start           (EphyHeaderBar *header_bar);
EphyActionBarEnd   *ephy_header_bar_get_action_bar_end             (EphyHeaderBar *header_bar);
void                ephy_header_bar_set_adaptive_mode              (EphyHeaderBar    *header_bar,
                                                                    EphyAdaptiveMode  adaptive_mode);

G_END_DECLS
