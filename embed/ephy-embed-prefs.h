/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*
 *  Copyright © 2008 Xan Lopez <xan@gnome.org>
 *  Copyright © 2009 Igalia S.L.
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

#include "ephy-embed.h"

#include <webkit/webkit.h>

#define USER_STYLESHEET_FILENAME	"user-stylesheet.css"
#define USER_JAVASCRIPT_FILENAME	"user-javascript.js"
#define FAVICON_SIZE 16

G_BEGIN_DECLS

WebKitSettings *ephy_embed_prefs_get_settings  (void);
void ephy_embed_prefs_set_cookie_accept_policy (WebKitCookieManager      *cookie_manager,
                                                const char               *settings_policy);
void ephy_embed_prefs_apply_user_style         (WebKitUserContentManager *ucm);
void ephy_embed_prefs_apply_user_javascript    (WebKitUserContentManager *ucm);

void ephy_embed_prefs_register_ucm             (WebKitUserContentManager *ucm);
void ephy_embed_prefs_unregister_ucm           (WebKitUserContentManager *ucm);


G_END_DECLS
