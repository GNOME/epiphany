/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2019-2022 Jan-Michael Brummer <jan.brummer@tabos.org>
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

G_BEGIN_DECLS

#include <glib.h>

#include "ephy-web-extension.h"

#define EPHY_TYPE_WEB_EXTENSION_MANAGER (ephy_web_extension_manager_get_type ())

G_DECLARE_FINAL_TYPE (EphyWebExtensionManager, ephy_web_extension_manager, EPHY, WEB_EXTENSION_MANAGER, GObject)

typedef void (*EphyWebExtensionForeachFunc) (EphyWebExtension *extension, gpointer user_data);

EphyWebExtensionManager *ephy_web_extension_manager_get_default                     (void);

GPtrArray              *ephy_web_extension_manager_get_web_extensions               (EphyWebExtensionManager *self);

void                    ephy_web_extension_manager_install_actions                  (EphyWebExtensionManager *self,
                                                                                     EphyWindow              *window);

void                    ephy_web_extension_manager_install                          (EphyWebExtensionManager *self,
                                                                                     GFile                   *file);

void                    ephy_web_extension_manager_uninstall                        (EphyWebExtensionManager *self,
                                                                                     EphyWebExtension        *web_extension);

void                    ephy_web_extension_manager_update_location_entry            (EphyWebExtensionManager *self,
                                                                                     EphyWindow              *window);

void                    ephy_web_extension_manager_add_web_extension_to_window      (EphyWebExtensionManager *self,
                                                                                     EphyWebExtension        *web_extension,
                                                                                     EphyWindow              *window);

void                    ephy_web_extension_manager_remove_web_extension_from_window (EphyWebExtensionManager *self,
                                                                                     EphyWebExtension        *web_extension,
                                                                                     EphyWindow              *window);

gboolean                ephy_web_extension_manager_is_active                        (EphyWebExtensionManager *self,
                                                                                     EphyWebExtension        *web_extension);

void                    ephy_web_extension_manager_set_active                       (EphyWebExtensionManager *self,
                                                                                     EphyWebExtension        *web_extension,
                                                                                     gboolean                 active);

void                    ephy_web_extension_manager_show_browser_action              (EphyWebExtensionManager *self,
                                                                                     EphyWebExtension        *web_extension);

GtkWidget               *ephy_web_extension_manager_get_page_action                 (EphyWebExtensionManager *self,
                                                                                     EphyWebExtension        *web_extension,
                                                                                     EphyWebView             *web_view);

GListStore              *ephy_web_extension_manager_get_browser_actions             (EphyWebExtensionManager *self);

WebKitWebView           *ephy_web_extension_manager_get_background_web_view         (EphyWebExtensionManager *self,
                                                                                     EphyWebExtension        *web_extension);
GtkWidget               *ephy_web_extension_manager_create_browser_popup           (EphyWebExtensionManager *self,
                                                                                    EphyWebExtension        *web_extension);
void                     ephy_web_extension_manager_handle_notifications_action     (EphyWebExtensionManager *self,
                                                                                     GVariant                *params);

void                     ephy_web_extension_manager_handle_context_menu_action      (EphyWebExtensionManager *self,
                                                                                     GVariant                *params);

void                     ephy_web_extension_manager_emit_in_extension_views         (EphyWebExtensionManager *self,
                                                                                     EphyWebExtension        *web_extension,
                                                                                     const char              *name,
                                                                                     const char              *json);

void                     ephy_web_extension_manager_emit_in_extension_views_with_reply
                                                                                    (EphyWebExtensionManager *self,
                                                                                     EphyWebExtension        *web_extension,
                                                                                     EphyWebExtensionSender  *sender,
                                                                                     const char              *name,
                                                                                     const char              *json,
                                                                                     GTask                   *reply_task);

void                     ephy_web_extension_manager_emit_in_tab_with_reply          (EphyWebExtensionManager *self,
                                                                                     EphyWebExtension        *web_extension,
                                                                                     const char              *name,
                                                                                     const char              *message_json,
                                                                                     WebKitWebView           *target_web_view,
                                                                                     const char              *sender_json,
                                                                                     GTask                   *reply_task);
void                     ephy_web_extension_manager_emit_in_background_view         (EphyWebExtensionManager *self,
                                                                                     EphyWebExtension        *web_extension,
                                                                                     const char              *name,
                                                                                     const char              *json);

GtkWidget                *ephy_web_extensions_manager_create_web_extensions_webview (EphyWebExtension        *web_extension);

void                      ephy_web_extension_manager_foreach_extension              (EphyWebExtensionManager     *self,
                                                                                     EphyWebExtensionForeachFunc  func,
                                                                                     gpointer                     user_data);
void                      ephy_web_extension_manager_browseraction_set_badge_text   (EphyWebExtensionManager *self,
                                                                                     EphyWebExtension        *web_extension,
                                                                                     const char              *text);
void                      ephy_web_extension_manager_browseraction_set_badge_background_color (EphyWebExtensionManager *self,
                                                                                               EphyWebExtension        *web_extension,
                                                                                               GdkRGBA                 *text);

void                      ephy_web_extension_manager_append_context_menu            (EphyWebExtensionManager *self,
                                                                                     WebKitWebView           *web_view,
                                                                                     WebKitContextMenu       *context_menu,
                                                                                     WebKitHitTestResult     *hit_test_result,
                                                                                     GdkModifierType          modifiers,
                                                                                     gboolean                 is_audio,
                                                                                     gboolean                 is_video);

void                      ephy_web_extension_manager_open_inspector                  (EphyWebExtensionManager *self,
                                                                                      EphyWebExtension        *web_extension);

G_END_DECLS
