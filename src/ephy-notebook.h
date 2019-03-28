/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2002 Christophe Fergeau
 *  Copyright © 2003 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
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

#include <glib.h>
#include <gtk/gtk.h>

#include "ephy-adaptive-mode.h"
#include "ephy-embed.h"

G_BEGIN_DECLS

#define EPHY_TYPE_NOTEBOOK (ephy_notebook_get_type ())

G_DECLARE_FINAL_TYPE (EphyNotebook, ephy_notebook, EPHY, NOTEBOOK, GtkNotebook)

int             ephy_notebook_add_tab           (EphyNotebook *nb,
                                                 EphyEmbed *embed,
                                                 int position,
                                                 gboolean jump_to);

void            ephy_notebook_set_tabs_allowed  (EphyNotebook *nb,
                                                 gboolean tabs_allowed);

void            ephy_notebook_next_page         (EphyNotebook *notebook);

void            ephy_notebook_prev_page         (EphyNotebook *notebook);

GMenu          *ephy_notebook_get_pages_menu    (EphyNotebook *notebook);

void            ephy_notebook_set_adaptive_mode (EphyNotebook     *notebook,
                                                 EphyAdaptiveMode  adaptive_mode);

void           ephy_notebook_tab_set_pinned    (EphyNotebook *notebook,
                                                GtkWidget    *embed,
                                                gboolean      is_pinned);
gboolean       ephy_notebook_tab_is_pinned     (EphyNotebook *notebook,
                                                EphyEmbed    *embed);
G_END_DECLS
