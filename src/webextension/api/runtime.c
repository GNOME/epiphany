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

#include "config.h"

#include "runtime.h"

#include "ephy-web-extension-manager.h"

#include "ephy-embed-utils.h"
#include "ephy-shell.h"

static char *
runtime_handler_get_browser_info (EphyWebExtension *self,
                                  char             *name,
                                  JSCValue         *args)
{
  g_autoptr (JsonBuilder) builder = json_builder_new ();
  g_autoptr (JsonNode) root = NULL;

  json_builder_begin_object (builder);
  json_builder_set_member_name (builder, "name");
  json_builder_add_string_value (builder, "GNOME Web (Epiphany)");
  json_builder_end_object (builder);

  root = json_builder_get_root (builder);

  return json_to_string (root, FALSE);
}

static char *
runtime_handler_send_message (EphyWebExtension *self,
                              char             *name,
                              JSCValue         *args)
{
  EphyShell *shell = ephy_shell_get_default ();
  EphyWebExtensionManager *manager = ephy_shell_get_web_extension_manager (shell);
  WebKitWebView *view = WEBKIT_WEB_VIEW (ephy_web_extension_manager_get_background_web_view (manager, self));
  g_autofree char *script = NULL;

  script = g_strdup_printf ("runtimeSendMessage(%s);", jsc_value_to_json (args, 2));
  webkit_web_view_run_javascript (view, script, NULL, NULL, NULL);

  return NULL;
}

static char *
runtime_handler_open_options_page (EphyWebExtension *self,
                                   char             *name,
                                   JSCValue         *args)
{
  const char *data = ephy_web_extension_get_option_ui_page (self);

  if (data) {
    EphyEmbed *embed;
    EphyShell *shell = ephy_shell_get_default ();
    WebKitWebView *web_view;
    GtkWindow *window = gtk_application_get_active_window (GTK_APPLICATION (shell));

    embed = ephy_shell_new_tab (shell,
                                EPHY_WINDOW (window),
                                NULL,
                                EPHY_NEW_TAB_JUMP);

    web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);
    webkit_web_view_load_html (web_view, data, NULL);
  }

  return NULL;
}

static char *
runtime_handler_set_uninstall_url (EphyWebExtension *self,
                                   char             *name,
                                   JSCValue         *args)
{
  return NULL;
}

static EphyWebExtensionApiHandler runtime_handlers[] = {
  {"getBrowserInfo", runtime_handler_get_browser_info},
  {"sendMessage", runtime_handler_send_message},
  {"openOptionsPage", runtime_handler_open_options_page},
  {"setUninstallURL", runtime_handler_set_uninstall_url},
  {NULL, NULL},
};

char *
ephy_web_extension_api_runtime_handler (EphyWebExtension *self,
                                        char             *name,
                                        JSCValue         *args)
{
  guint idx;

  for (idx = 0; idx < G_N_ELEMENTS (runtime_handlers); idx++) {
    EphyWebExtensionApiHandler handler = runtime_handlers[idx];

    if (g_strcmp0 (handler.name, name) == 0)
      return handler.execute (self, name, args);
  }

  g_warning ("%s(): '%s' not implemented by Epiphany!", __FUNCTION__, name);

  return NULL;
}
