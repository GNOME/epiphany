/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2019 Christopher Davis <christopherdavis@gnome.org>
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

#define EPHY_TYPE_PAGES_VIEW (ephy_pages_view_get_type ())

G_DECLARE_FINAL_TYPE (EphyPagesView, ephy_pages_view, EPHY, PAGES_VIEW, GtkScrolledWindow)

EphyPagesView *ephy_pages_view_new               (void);

EphyNotebook  *ephy_pages_view_get_notebook      (EphyPagesView   *view);
void           ephy_pages_view_set_notebook      (EphyPagesView   *view,
                                                  EphyNotebook    *notebook);

void           ephy_pages_view_set_adaptive_mode (EphyPagesView   *self,
                                                  EphyAdaptiveMode adaptive_mode);

G_END_DECLS
