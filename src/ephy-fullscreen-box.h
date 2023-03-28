/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2021 Purism SPC
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

#define EPHY_TYPE_FULLSCREEN_BOX (ephy_fullscreen_box_get_type())

G_DECLARE_FINAL_TYPE (EphyFullscreenBox, ephy_fullscreen_box, EPHY, FULLSCREEN_BOX, GtkWidget)

EphyFullscreenBox *ephy_fullscreen_box_new            (void);

gboolean           ephy_fullscreen_box_get_fullscreen (EphyFullscreenBox *self);
void               ephy_fullscreen_box_set_fullscreen (EphyFullscreenBox *self,
                                                       gboolean           fullscreen);

gboolean           ephy_fullscreen_box_get_autohide   (EphyFullscreenBox *self);
void               ephy_fullscreen_box_set_autohide   (EphyFullscreenBox *self,
                                                       gboolean           autohide);

GtkWidget         *ephy_fullscreen_box_get_content    (EphyFullscreenBox *self);
void               ephy_fullscreen_box_set_content    (EphyFullscreenBox *self,
                                                       GtkWidget         *content);

void               ephy_fullscreen_box_add_top_bar    (EphyFullscreenBox *self,
                                                       GtkWidget         *child);
void               ephy_fullscreen_box_add_bottom_bar (EphyFullscreenBox *self,
                                                       GtkWidget         *child);

G_END_DECLS
