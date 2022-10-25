/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2019-2020 Jan-Michael Brummer <jan.brummer@tabos.org>
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

#include "ephy-debug.h"
#include "ephy-json-utils.h"
#include "ephy-window.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gio/gio.h>
#include <string.h>
#include <webkit/webkit.h>

G_BEGIN_DECLS

#define EPHY_TYPE_WEB_EXTENSION (ephy_web_extension_get_type ())

G_DECLARE_FINAL_TYPE (EphyWebExtension, ephy_web_extension, EPHY, WEB_EXTENSION, GObject)

/**
 * EphyWebExtensionSender:
 *
 * Represents the sender of a message or API call.
 * Which extension from which view in which frame.
 */
typedef struct {
  EphyWebExtension *extension;
  WebKitWebView *view;
  guint64 frame_id;
} EphyWebExtensionSender;

typedef void (*EphyApiExecuteFunc)(EphyWebExtensionSender *sender,
                                   const char             *method_name,
                                   JsonArray              *args,
                                   GTask                  *task);


extern GQuark web_extension_error_quark (void);
#define WEB_EXTENSION_ERROR web_extension_error_quark ()

typedef enum {
  WEB_EXTENSION_ERROR_INVALID_ARGUMENT = 1001,
  WEB_EXTENSION_ERROR_PERMISSION_DENIED = 1002,
  WEB_EXTENSION_ERROR_NOT_IMPLEMENTED = 1003,
  WEB_EXTENSION_ERROR_INVALID_MANIFEST = 1004,
  WEB_EXTENSION_ERROR_INVALID_XPI = 1005,
  WEB_EXTENSION_ERROR_INVALID_HOST = 1006,
} WebExtensionErrorCode;

typedef struct {
  char *name;
  char *description;
  char *accelerator;
  char *shortcut;
} WebExtensionCommand;

WebExtensionCommand *web_extension_command_copy (WebExtensionCommand *command);
void                 web_extension_command_free (WebExtensionCommand *command);

typedef struct {
  char *name;
  EphyApiExecuteFunc execute;
} EphyWebExtensionApiHandler;

GdkPixbuf             *ephy_web_extension_get_icon                        (EphyWebExtension *self,
                                                                           gint64            size);

const char            *ephy_web_extension_get_name                        (EphyWebExtension *self);

const char            *ephy_web_extension_get_short_name                  (EphyWebExtension *self);

const char            *ephy_web_extension_get_version                     (EphyWebExtension *self);

const char            *ephy_web_extension_get_description                 (EphyWebExtension *self);

const char            *ephy_web_extension_get_homepage_url                (EphyWebExtension *self);

const char            *ephy_web_extension_get_author                      (EphyWebExtension *self);

const char            *ephy_web_extension_get_content_security_policy     (EphyWebExtension *self);

void                   ephy_web_extension_load_async                      (GFile               *target,
                                                                           GFileInfo           *info,
                                                                           GCancellable        *cancellable,
                                                                           GAsyncReadyCallback  callback,
                                                                           gpointer             user_data);

EphyWebExtension      *ephy_web_extension_load_finished                   (GObject       *source_object,
                                                                           GAsyncResult  *result,
                                                                           GError       **error);

GdkPixbuf             *ephy_web_extension_load_pixbuf                     (EphyWebExtension *self,
                                                                           const char       *resource_path,
                                                                           int               size);

gboolean               ephy_web_extension_has_page_action                 (EphyWebExtension *self);

gboolean               ephy_web_extension_has_browser_action              (EphyWebExtension *self);

gboolean               ephy_web_extension_has_background_web_view         (EphyWebExtension *self);

void                   ephy_web_extension_remove                          (EphyWebExtension *self);

const char            *ephy_web_extension_get_manifest                    (EphyWebExtension *self);

const char            *ephy_web_extension_background_web_view_get_page    (EphyWebExtension *self);

GdkPixbuf             *ephy_web_extension_browser_action_get_icon         (EphyWebExtension *self,
                                                                           int               size);

const char            *ephy_web_extension_browser_action_get_tooltip      (EphyWebExtension *self);

const char            *ephy_web_extension_get_browser_popup               (EphyWebExtension *self);

GList                 *ephy_web_extension_get_content_scripts             (EphyWebExtension *self);

GList                 *ephy_web_extension_get_content_script_js           (EphyWebExtension *self,
                                                                           gpointer          content_script);

const char            *ephy_web_extension_get_base_location               (EphyWebExtension *self);

gconstpointer          ephy_web_extension_get_resource                    (EphyWebExtension *self,
                                                                           const char       *name,
                                                                           gsize            *length);

char                  *ephy_web_extension_get_resource_as_string          (EphyWebExtension *self,
                                                                           const char       *name);

WebKitUserStyleSheet  *ephy_web_extension_add_custom_css                  (EphyWebExtension *self,
                                                                           const char       *code);

WebKitUserStyleSheet  *ephy_web_extension_get_custom_css                  (EphyWebExtension *self,
                                                                           const char       *code);

GList                 *ephy_web_extension_get_custom_css_list             (EphyWebExtension *self);

WebKitUserStyleSheet  *ephy_web_extension_custom_css_style                (EphyWebExtension *self,
                                                                           gpointer          custom_css);

const char            *ephy_web_extension_get_option_ui_page              (EphyWebExtension *self);

const char            *ephy_web_extension_get_guid                        (EphyWebExtension *self);

GHashTable            *ephy_web_extension_get_commands                    (EphyWebExtension *self);

gboolean               ephy_web_extension_has_tab_or_host_permission      (EphyWebExtension *self,
                                                                           EphyWebView      *web_view,
                                                                           gboolean          is_user_interaction);

gboolean               ephy_web_extension_has_host_or_active_permission   (EphyWebExtension *self,
                                                                           EphyWebView      *web_view,
                                                                           gboolean          is_user_interaction);

gboolean               ephy_web_extension_has_permission                  (EphyWebExtension *self,
                                                                           const char       *permission);

gboolean               ephy_web_extension_has_host_permission             (EphyWebExtension *self,
                                                                           const char       *host);

const char * const    *ephy_web_extension_get_host_permissions            (EphyWebExtension *self);

JsonNode              *ephy_web_extension_get_local_storage               (EphyWebExtension *self);

void                   ephy_web_extension_save_local_storage              (EphyWebExtension *self);

void                   ephy_web_extension_clear_local_storage             (EphyWebExtension *self);

char                  *ephy_web_extension_create_sender_object            (EphyWebExtensionSender *sender);

gboolean               ephy_web_extension_rule_matches_uri                (const char       *rule,
                                                                           GUri             *uri);

gboolean               ephy_web_extension_has_web_accessible_resource     (EphyWebExtension *self,
                                                                           const char       *path);

char                   *ephy_web_extension_parse_command_key              (const char       *suggested_key);

G_END_DECLS

