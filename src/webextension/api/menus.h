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

#include "ephy-web-extension.h"

#include <json-glib/json-glib.h>
#include <webkit2/webkit2.h>

G_BEGIN_DECLS

void         ephy_web_extension_api_menus_handler                    (EphyWebExtensionSender *sender,
                                                                      const char             *method_name,
                                                                      JsonArray              *args,
                                                                      GTask                  *task);

WebKitContextMenuItem  *ephy_web_extension_api_menus_create_context_menu        (EphyWebExtension    *self,
                                                                                 WebKitWebView       *web_view,
                                                                                 WebKitContextMenu   *context_menu,
                                                                                 WebKitHitTestResult *hit_test_result,
                                                                                 GdkModifierType      modifiers,
                                                                                 gboolean             is_audio,
                                                                                 gboolean             is_video);

G_END_DECLS