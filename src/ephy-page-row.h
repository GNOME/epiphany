/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2019 Purism SPC
 *  Copyright © 2019 Adrien Plazas <kekun.plazas@laposte.net>
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
#include "ephy-adaptive-mode.h"
#include "ephy-tab-view.h"

G_BEGIN_DECLS

#define EPHY_TYPE_PAGE_ROW (ephy_page_row_get_type())

G_DECLARE_FINAL_TYPE (EphyPageRow, ephy_page_row, EPHY, PAGE_ROW, GtkListBoxRow)

EphyPageRow *ephy_page_row_new (EphyTabView *view,
                                HdyTabPage  *page);

void ephy_page_row_set_adaptive_mode (EphyPageRow      *self,
                                      EphyAdaptiveMode  adaptive_mode);

HdyTabPage *ephy_page_row_get_page (EphyPageRow *self);

G_END_DECLS
