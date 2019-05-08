/* -*- Mode: C; tab-width: 2; indent-pages-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2019 Alexander Mikhaylenko <exalm7659@gmail.com>
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

#define EPHY_TYPE_PAGES_BUTTON (ephy_pages_button_get_type())

G_DECLARE_FINAL_TYPE (EphyPagesButton, ephy_pages_button, EPHY, PAGES_BUTTON, GtkButton)

EphyPagesButton *ephy_pages_button_new         (void);

int              ephy_pages_button_get_n_pages (EphyPagesButton *self);

void             ephy_pages_button_set_n_pages (EphyPagesButton *self,
                                                int              n_pages);

G_END_DECLS
