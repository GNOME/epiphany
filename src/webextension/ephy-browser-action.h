/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2022 Igalia S.L.
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

#include <gio/gio.h>

#include "ephy-web-extension.h"

G_BEGIN_DECLS

#define EPHY_TYPE_BROWSER_ACTION (ephy_browser_action_get_type())

G_DECLARE_FINAL_TYPE (EphyBrowserAction, ephy_browser_action, EPHY, BROWSER_ACTION, GObject)

EphyBrowserAction *ephy_browser_action_new        (EphyWebExtension *web_extension);
const char        *ephy_browser_action_get_title  (EphyBrowserAction *self);
GdkPixbuf         *ephy_browser_action_get_pixbuf (EphyBrowserAction *self, gint64 size);
EphyWebExtension  *ephy_browser_action_get_web_extension (EphyBrowserAction *self);
gboolean           ephy_browser_action_activate   (EphyBrowserAction *self);
void               ephy_browser_action_set_badge_text (EphyBrowserAction *self, const char *text);
const char        *ephy_browser_action_get_badge_text (EphyBrowserAction *self);
void               ephy_browser_action_set_badge_background_color (EphyBrowserAction *self, GdkRGBA *color);
GdkRGBA           *ephy_browser_action_get_badge_background_color (EphyBrowserAction *self);

G_END_DECLS
