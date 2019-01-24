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

#include <gtk/gtk.h>

#include "ephy-adaptive-mode.h"
#include "ephy-notebook.h"

G_BEGIN_DECLS

#define EPHY_TYPE_PAGES_POPOVER (ephy_pages_popover_get_type())

G_DECLARE_FINAL_TYPE (EphyPagesPopover, ephy_pages_popover, EPHY, PAGES_POPOVER, GtkPopover)

EphyPagesPopover *ephy_pages_popover_new (GtkWidget *relative_to);

EphyNotebook *ephy_pages_popover_get_notebook (EphyPagesPopover *popover);
void          ephy_pages_popover_set_notebook (EphyPagesPopover *popover,
                                               EphyNotebook     *notebook);

void ephy_pages_popover_set_adaptive_mode (EphyPagesPopover *self,
                                           EphyAdaptiveMode  adaptive_mode);

G_END_DECLS
