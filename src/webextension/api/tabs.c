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

#include "ephy-shell.h"
#include "ephy-window.h"

#include "tabs.h"

static void
add_web_view_to_json (JsonBuilder *builder,
                      EphyWebView *web_view)
{
  json_builder_begin_object (builder);
  json_builder_set_member_name (builder, "url");
  json_builder_add_string_value (builder, ephy_web_view_get_address (web_view));
  json_builder_set_member_name (builder, "id");
  json_builder_add_int_value (builder, ephy_web_view_get_uid (web_view));
  json_builder_end_object (builder);
}

static char *
tabs_handler_query (EphyWebExtension *self,
                    char             *name,
                    JSCValue         *args)
{
  g_autoptr (JsonBuilder) builder = json_builder_new ();
  g_autoptr (JsonNode) root = NULL;
  EphyShell *shell = ephy_shell_get_default ();
  GtkWindow *window;
  EphyTabView *tab_view;
  gboolean current_window = TRUE;
  gboolean active = TRUE;

  if (jsc_value_object_has_property (args, "active")) {
    g_autoptr (JSCValue) value = NULL;

    value = jsc_value_object_get_property (args, "active");
    active = jsc_value_to_boolean (value);
  }

  if (jsc_value_object_has_property (args, "currentWindow")) {
    g_autoptr (JSCValue) value = NULL;

    value = jsc_value_object_get_property (args, "currentWindow");
    current_window = jsc_value_to_boolean (value);
  }

  if (current_window) {
    window = gtk_application_get_active_window (GTK_APPLICATION (shell));
    tab_view = ephy_window_get_tab_view (EPHY_WINDOW (window));

    json_builder_begin_array (builder);

    if (active) {
      GtkWidget *page = ephy_tab_view_get_selected_page (tab_view);
      EphyWebView *tmp_webview = ephy_embed_get_web_view (EPHY_EMBED (page));

      add_web_view_to_json (builder, tmp_webview);
    } else {
      for (int i = 0; i < ephy_tab_view_get_n_pages (tab_view); i++) {
        GtkWidget *page = ephy_tab_view_get_nth_page (tab_view, i);
        EphyWebView *tmp_webview = ephy_embed_get_web_view (EPHY_EMBED (page));

        add_web_view_to_json (builder, tmp_webview);
      }
    }

    json_builder_end_array (builder);
  }

  root = json_builder_get_root (builder);

  return json_to_string (root, FALSE);
}

static char *
tabs_handler_insert_css (EphyWebExtension *self,
                         char             *name,
                         JSCValue         *args)
{
  EphyShell *shell = ephy_shell_get_default ();
  WebKitUserContentManager *ucm = webkit_web_view_get_user_content_manager (WEBKIT_WEB_VIEW (ephy_shell_get_active_web_view (shell)));
  WebKitUserStyleSheet *css = NULL;
  g_autoptr (JSCValue) code = NULL;

  code = jsc_value_object_get_property (args, "code");
  css = ephy_web_extension_add_custom_css (self, jsc_value_to_string (code));

  if (css)
    webkit_user_content_manager_add_style_sheet (ucm, css);

  return NULL;
}

static char *
tabs_handler_remove_css (EphyWebExtension *self,
                         char             *name,
                         JSCValue         *args)
{
  EphyShell *shell = ephy_shell_get_default ();
  JSCValue *code;
  WebKitUserStyleSheet *css = NULL;
  WebKitUserContentManager *ucm = webkit_web_view_get_user_content_manager (WEBKIT_WEB_VIEW (ephy_shell_get_active_web_view (shell)));

  code = jsc_value_object_get_property (args, "code");
  css = ephy_web_extension_get_custom_css (self, jsc_value_to_string (code));
  if (css)
    webkit_user_content_manager_remove_style_sheet (ucm, css);

  return NULL;
}

static char *
tabs_handler_get (EphyWebExtension *self,
                  char             *name,
                  JSCValue         *args)
{
  EphyShell *shell = ephy_shell_get_default ();
  g_autoptr (JsonBuilder) builder = json_builder_new ();
  g_autoptr (JsonNode) root = NULL;
  EphyWebView *tmp_webview = ephy_shell_get_active_web_view (shell);

  add_web_view_to_json (builder, tmp_webview);
  root = json_builder_get_root (builder);

  return json_to_string (root, FALSE);
}

static char *
tabs_handler_execute_script (EphyWebExtension *self,
                             char             *name,
                             JSCValue         *args)
{
  g_autoptr (JSCValue) code_value = NULL;
  g_autoptr (JSCValue) file_value = NULL;
  g_autoptr (JSCValue) obj = NULL;
  g_autofree char *code = NULL;
  EphyShell *shell = ephy_shell_get_default ();

  if (jsc_value_is_array (args)) {
    obj = jsc_value_object_get_property_at_index (args, 1);
  } else {
    obj = args;
  }

  file_value = jsc_value_object_get_property (obj, "file");
  code_value = jsc_value_object_get_property (obj, "code");

  if (code_value)
    code = jsc_value_to_string (code_value);
  else if (file_value) {
    g_autofree char *resource_path = jsc_value_to_string (code_value);
    code = g_strdup (ephy_web_extension_get_resource_as_string (self, resource_path));
  }

  if (code) {
    webkit_web_view_run_javascript_in_world (WEBKIT_WEB_VIEW (ephy_shell_get_active_web_view (shell)),
                                             code,
                                             ephy_web_extension_get_guid (self),
                                             NULL,
                                             NULL,
                                             NULL);
  }

  return NULL;
}

static EphyWebExtensionApiHandler tabs_handlers[] = {
  {"query", tabs_handler_query},
  {"insertCSS", tabs_handler_insert_css},
  {"removeCSS", tabs_handler_remove_css},
  {"get", tabs_handler_get},
  {"executeScript", tabs_handler_execute_script},
  {NULL, NULL},
};

char *
ephy_web_extension_api_tabs_handler (EphyWebExtension *self,
                                     char             *name,
                                     JSCValue         *args)
{
  guint idx;

  for (idx = 0; idx < G_N_ELEMENTS (tabs_handlers); idx++) {
    EphyWebExtensionApiHandler handler = tabs_handlers[idx];

    if (g_strcmp0 (handler.name, name) == 0)
      return handler.execute (self, name, args);
  }

  g_warning ("%s(): '%s' not implemented by Epiphany!", __FUNCTION__, name);

  return NULL;
}
