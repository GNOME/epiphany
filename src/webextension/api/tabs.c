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

/* Matches Firefox. */
static const int WINDOW_ID_CURRENT = -2;

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

typedef enum {
  TAB_QUERY_UNSET = -1,
  TAB_QUERY_DONT_MATCH = 0,
  TAB_QUERY_MATCH = 1,
} TabQuery;

static TabQuery
get_tab_query_property (JSCValue   *args,
                        const char *name)
{
  g_autoptr (JSCValue) value = jsc_value_object_get_property (args, name);

  if (!value || jsc_value_is_undefined (value))
    return TAB_QUERY_UNSET;

  return jsc_value_to_boolean (value);
}

static gint32
get_number_property (JSCValue   *args,
                     const char *name)
{
  g_autoptr (JSCValue) value = jsc_value_object_get_property (args, name);

  if (!value || jsc_value_is_undefined (value))
    return -1;

  return jsc_value_to_int32 (value);
}

static char *
tabs_handler_query (EphyWebExtension *self,
                    char             *name,
                    JSCValue         *args)
{
  g_autoptr (JsonBuilder) builder = json_builder_new ();
  g_autoptr (JsonNode) root = NULL;
  EphyShell *shell = ephy_shell_get_default ();
  GList *windows;
  EphyWindow *active_window;
  TabQuery current_window;
  TabQuery active;
  gint32 window_id;
  gint32 tab_index;

  active = get_tab_query_property (args, "active");
  current_window = get_tab_query_property (args, "currentWindow");
  window_id = get_number_property (args, "windowId");
  tab_index = get_number_property (args, "index");

  if (window_id == WINDOW_ID_CURRENT) {
    current_window = TRUE;
    window_id = -1;
  }

  active_window = EPHY_WINDOW (gtk_application_get_active_window (GTK_APPLICATION (shell)));

  windows = gtk_application_get_windows (GTK_APPLICATION (shell));

  json_builder_begin_array (builder);

  for (GList *win_list = windows; win_list; win_list = g_list_next (win_list)) {
    EphyWindow *window;
    EphyTabView *tab_view;
    EphyWebView *active_web_view;

    g_assert (EPHY_IS_WINDOW (win_list->data));

    window = EPHY_WINDOW (win_list->data);
    if (window_id != -1 && (guint64)window_id != ephy_window_get_uid (window))
      continue;

    if (current_window == TAB_QUERY_MATCH && window != active_window)
      continue;
    else if (current_window == TAB_QUERY_DONT_MATCH && window == active_window)
      continue;

    tab_view = ephy_window_get_tab_view (window);
    active_web_view = ephy_embed_get_web_view (EPHY_EMBED (ephy_tab_view_get_selected_page (tab_view)));
    for (int i = 0; i < ephy_tab_view_get_n_pages (tab_view); i++) {
      EphyWebView *web_view;

      if (tab_index != -1 && tab_index != i)
        continue;

      web_view = ephy_embed_get_web_view (EPHY_EMBED (ephy_tab_view_get_nth_page (tab_view, i)));
      if (active == TAB_QUERY_MATCH && web_view != active_web_view)
        continue;
      else if (active == TAB_QUERY_DONT_MATCH && web_view == active_web_view)
        continue;

      add_web_view_to_json (builder, web_view);
    }
  }

  json_builder_end_array (builder);
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

  /* This takes an optional first argument so it's either:
   * [tabId:str, details:obj], or [details:obj] */
  if (!jsc_value_is_array (args))
    return NULL;

  obj = jsc_value_object_get_property_at_index (args, 0);
  if (!jsc_value_is_object (obj)) {
    g_warning ("tabs.executeScript doesn't currently support tabId and will run in the activeTab!");
    g_object_unref (obj);
    obj = jsc_value_object_get_property_at_index (args, 1);
  }

  file_value = jsc_value_object_get_property (obj, "file");
  code_value = jsc_value_object_get_property (obj, "code");

  if (jsc_value_is_string (code_value))
    code = jsc_value_to_string (code_value);
  else if (jsc_value_is_string (file_value)) {
    g_autofree char *resource_path = jsc_value_to_string (file_value);
    code = ephy_web_extension_get_resource_as_string (self, resource_path[0] == '/' ? resource_path + 1 : resource_path);
  } else {
    g_warning ("tabs.executeScript called without usable code");
    return NULL;
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

static char *
tabs_handler_send_message (EphyWebExtension *self,
                           char             *name,
                           JSCValue         *args)
{
  g_autoptr (JSCValue) message_value = NULL;
  g_autofree char *serialized_message = NULL;
  g_autofree char *code = NULL;
  EphyShell *shell = ephy_shell_get_default ();

  if (!jsc_value_is_array (args))
    return NULL;

  message_value = jsc_value_object_get_property_at_index (args, 1);
  if (!message_value)
    return NULL;

  serialized_message = jsc_value_to_json (message_value, 0);
  code = g_strdup_printf ("runtimeSendMessage(JSON.parse('%s'));", serialized_message);

  g_warning ("tabs.sendMessage doesn't currently support tabId and will run in the activeTab!");

  webkit_web_view_run_javascript_in_world (WEBKIT_WEB_VIEW (ephy_shell_get_active_web_view (shell)),
                                           code,
                                           ephy_web_extension_get_guid (self),
                                           NULL,
                                           NULL,
                                           NULL);

  /* FIXME: Return message response. */
  return NULL;
}

static EphyWebExtensionApiHandler tabs_handlers[] = {
  {"query", tabs_handler_query},
  {"insertCSS", tabs_handler_insert_css},
  {"removeCSS", tabs_handler_remove_css},
  {"get", tabs_handler_get},
  {"executeScript", tabs_handler_execute_script},
  {"sendMessage", tabs_handler_send_message},
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
