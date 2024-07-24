 /* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2020 Jan-Michael Brummer <jan.brummer@tabos.org>
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

#include <glib-object.h>
#include <jsc/jsc.h>
#include <json-glib/json-glib.h>
#include <webkit/webkit-web-process-extension.h>

G_BEGIN_DECLS

void ephy_webextension_install_common_apis (WebKitWebPage *page,
                                            WebKitFrame   *frame,
                                            JSCContext    *js_context,
                                            const char    *guid,
                                            JsonObject    *translations,
                                            const char    *manifest);

G_END_DECLS
