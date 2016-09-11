/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2013, 2014 Yosef Or Boczko <yoseforb@gnome.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

#include "ephy-window.h"

G_BEGIN_DECLS

#define EPHY_TYPE_TITLE_BOX (ephy_title_box_get_type ())

G_DECLARE_FINAL_TYPE (EphyTitleBox, ephy_title_box, EPHY, TITLE_BOX, GtkBox)

EphyTitleBox       *ephy_title_box_new                  (void);

void                ephy_title_box_set_web_view         (EphyTitleBox         *title_box,
                                                         WebKitWebView        *web_view);

void                ephy_title_box_set_security_level   (EphyTitleBox         *title_box,
                                                         EphySecurityLevel     security_level);

void                ephy_title_box_set_address          (EphyTitleBox         *title_box,
                                                         const char           *address);

G_END_DECLS
