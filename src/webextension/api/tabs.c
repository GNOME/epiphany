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

#include "config.h"

#include "ephy-embed-utils.h"
#include "ephy-shell.h"
#include "ephy-window.h"
#include "ephy-reader-handler.h"

#include "api-utils.h"
#include "tabs.h"

/* Matches Firefox. */
static const int WINDOW_ID_CURRENT = -2;

static WebKitWebView *
get_web_view_for_tab_id (EphyShell   *shell,
                         gint32       tab_id,
                         EphyWindow **window_out)
{
  GList *windows;

  if (window_out)
    *window_out = NULL;

  if (tab_id < 0)
    return NULL;

  windows = gtk_application_get_windows (GTK_APPLICATION (shell));

  for (GList *win_list = windows; win_list; win_list = g_list_next (win_list)) {
    EphyWindow *window = EPHY_WINDOW (win_list->data);
    EphyTabView *tab_view = ephy_window_get_tab_view (window);

    for (int i = 0; i < ephy_tab_view_get_n_pages (tab_view); i++) {
      EphyWebView *web_view = ephy_embed_get_web_view (EPHY_EMBED (ephy_tab_view_get_nth_page (tab_view, i)));

      if (ephy_web_view_get_uid (web_view) == (guint64)tab_id) {
        if (window_out)
          *window_out = window;
        return WEBKIT_WEB_VIEW (web_view);
      }
    }
  }

  g_debug ("Failed to find tab with id %d", tab_id);
  return NULL;
}

static void
add_web_view_to_json (EphyWebExtension *extension,
                      JsonBuilder      *builder,
                      EphyWindow       *window,
                      EphyWebView      *web_view)
{
  EphyTabView *tab_view = ephy_window_get_tab_view (window);
  GtkWidget *page = gtk_widget_get_parent (gtk_widget_get_parent (GTK_WIDGET (web_view)));
  gboolean is_active = ephy_tab_view_get_current_page (tab_view) == page;
  WebKitFaviconDatabase *favicon_db = webkit_web_context_get_favicon_database (webkit_web_view_get_context (WEBKIT_WEB_VIEW (web_view)));
  const char *favicon_uri = webkit_favicon_database_get_favicon_uri (favicon_db, ephy_web_view_get_address (web_view));
  gboolean has_tab_permission = ephy_web_extension_has_tab_or_host_permission (extension, web_view, TRUE);

  json_builder_begin_object (builder);
  if (has_tab_permission) {
    json_builder_set_member_name (builder, "url");
    json_builder_add_string_value (builder, ephy_web_view_get_address (web_view));
    json_builder_set_member_name (builder, "title");
    json_builder_add_string_value (builder, webkit_web_view_get_title (WEBKIT_WEB_VIEW (web_view)));
    if (favicon_uri) {
      json_builder_set_member_name (builder, "favIconUrl");
      json_builder_add_string_value (builder, favicon_uri);
    }
  }
  json_builder_set_member_name (builder, "id");
  json_builder_add_int_value (builder, ephy_web_view_get_uid (web_view));
  json_builder_set_member_name (builder, "windowId");
  json_builder_add_int_value (builder, ephy_window_get_uid (window));
  json_builder_set_member_name (builder, "active");
  json_builder_add_boolean_value (builder, is_active);
  json_builder_set_member_name (builder, "highlighted");
  json_builder_add_boolean_value (builder, is_active);
  json_builder_set_member_name (builder, "hidden");
  json_builder_add_boolean_value (builder, FALSE);
  json_builder_set_member_name (builder, "incognito");
  json_builder_add_boolean_value (builder, ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) == EPHY_EMBED_SHELL_MODE_INCOGNITO);
  json_builder_set_member_name (builder, "isInReaderMode");
  json_builder_add_boolean_value (builder, ephy_web_view_get_reader_mode_state (web_view));
  json_builder_set_member_name (builder, "isArticle");
  json_builder_add_boolean_value (builder, ephy_web_view_is_reader_mode_available (web_view));
  json_builder_set_member_name (builder, "pinned");
  json_builder_add_boolean_value (builder, ephy_tab_view_get_is_pinned (tab_view, page));
  json_builder_set_member_name (builder, "index");
  json_builder_add_int_value (builder, ephy_tab_view_get_page_index (tab_view, page));
  json_builder_set_member_name (builder, "status");
  json_builder_add_string_value (builder, ephy_web_view_is_loading (web_view) ? "loading" : "complete");
  json_builder_set_member_name (builder, "mutedInfo");
  json_builder_begin_object (builder);
  json_builder_set_member_name (builder, "muted");
  json_builder_add_boolean_value (builder, webkit_web_view_get_is_muted (WEBKIT_WEB_VIEW (web_view)));
  json_builder_end_object (builder);
  json_builder_end_object (builder);
}

void
ephy_web_extension_api_tabs_add_tab_to_json (EphyWebExtension *extension,
                                             JsonBuilder      *builder,
                                             EphyWindow       *window,
                                             EphyWebView      *web_view)
{
  add_web_view_to_json (extension, builder, window, web_view);
}

JsonNode *
ephy_web_extension_api_tabs_create_tab_object (EphyWebExtension *extension,
                                               EphyWebView      *web_view)
{
  g_autoptr (JsonBuilder) builder = json_builder_new ();
  add_web_view_to_json (extension,
                        builder,
                        EPHY_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (web_view))),
                        web_view);
  return json_builder_get_root (builder);
}

static EphyWindow *
get_window_by_id (EphyShell *shell,
                  gint64     window_id)
{
  if (window_id >= 0) {
    GList *windows = gtk_application_get_windows (GTK_APPLICATION (shell));
    for (GList *win_list = windows; win_list; win_list = g_list_next (win_list)) {
      EphyWindow *window = EPHY_WINDOW (win_list->data);

      if ((guint64)window_id == ephy_window_get_uid (window))
        return window;
    }
  }

  return EPHY_WINDOW (gtk_application_get_active_window (GTK_APPLICATION (shell)));
}

static void
tabs_handler_query (EphyWebExtensionSender *sender,
                    char                   *name,
                    JSCValue               *args,
                    GTask                  *task)
{
  g_autoptr (JsonBuilder) builder = json_builder_new ();
  g_autoptr (JsonNode) root = NULL;
  EphyShell *shell = ephy_shell_get_default ();
  g_autoptr (JSCValue) value = jsc_value_object_get_property_at_index (args, 0);
  GList *windows;
  EphyWindow *active_window;
  ApiTriStateValue current_window;
  ApiTriStateValue active;
  gint32 window_id;
  gint32 tab_index;

  if (!jsc_value_is_object (value)) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.query(): Missing query object.");
    return;
  }

  active = api_utils_get_tri_state_value_property (value, "active");
  current_window = api_utils_get_tri_state_value_property (value, "currentWindow");
  window_id = api_utils_get_int32_property (value, "windowId", -1);
  tab_index = api_utils_get_int32_property (value, "index", -1);

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

    if (current_window == API_VALUE_TRUE && window != active_window)
      continue;
    else if (current_window == API_VALUE_FALSE && window == active_window)
      continue;

    tab_view = ephy_window_get_tab_view (window);
    active_web_view = ephy_embed_get_web_view (EPHY_EMBED (ephy_tab_view_get_selected_page (tab_view)));
    for (int i = 0; i < ephy_tab_view_get_n_pages (tab_view); i++) {
      EphyWebView *web_view;

      if (tab_index != -1 && tab_index != i)
        continue;

      web_view = ephy_embed_get_web_view (EPHY_EMBED (ephy_tab_view_get_nth_page (tab_view, i)));
      if (active == API_VALUE_TRUE && web_view != active_web_view)
        continue;
      else if (active == API_VALUE_FALSE && web_view == active_web_view)
        continue;

      add_web_view_to_json (sender->extension, builder, window, web_view);
    }
  }

  json_builder_end_array (builder);
  root = json_builder_get_root (builder);
  g_task_return_pointer (task, json_to_string (root, FALSE), g_free);
}

static void
tabs_handler_insert_css (EphyWebExtensionSender *sender,
                         char                   *name,
                         JSCValue               *args,
                         GTask                  *task)
{
  EphyShell *shell = ephy_shell_get_default ();
  WebKitUserContentManager *ucm;
  WebKitUserStyleSheet *css = NULL;
  g_autoptr (JSCValue) code = NULL;
  g_autoptr (JSCValue) obj = NULL;
  g_autoptr (JSCValue) tab_id_value = NULL;
  WebKitWebView *target_web_view;

  /* This takes an optional first argument so it's either:
   * [tabId:int, details:obj], or [details:obj] */
  tab_id_value = jsc_value_object_get_property_at_index (args, 0);
  if (jsc_value_is_number (tab_id_value)) {
    obj = jsc_value_object_get_property_at_index (args, 1);
  } else {
    obj = g_steal_pointer (&tab_id_value);
  }

  if (!jsc_value_is_object (obj)) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "Invalid Arguments");
    return;
  }

  if (!tab_id_value)
    target_web_view = WEBKIT_WEB_VIEW (ephy_shell_get_active_web_view (shell));
  else
    target_web_view = get_web_view_for_tab_id (shell, jsc_value_to_int32 (tab_id_value), NULL);

  if (!target_web_view) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "Invalid Arguments");
    return;
  }

  if (!ephy_web_extension_has_host_or_active_permission (sender->extension, EPHY_WEB_VIEW (target_web_view), TRUE)) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_PERMISSION_DENIED, "Permission Denied");
    return;
  }

  ucm = webkit_web_view_get_user_content_manager (target_web_view);

  code = jsc_value_object_get_property (obj, "code");
  css = ephy_web_extension_add_custom_css (sender->extension, jsc_value_to_string (code));

  if (css)
    webkit_user_content_manager_add_style_sheet (ucm, css);

  g_task_return_pointer (task, NULL, NULL);
}

static void
tabs_handler_remove_css (EphyWebExtensionSender *sender,
                         char                   *name,
                         JSCValue               *args,
                         GTask                  *task)
{
  EphyShell *shell = ephy_shell_get_default ();
  JSCValue *code;
  WebKitUserStyleSheet *css = NULL;
  WebKitUserContentManager *ucm;
  g_autoptr (JSCValue) obj = NULL;
  g_autoptr (JSCValue) tab_id_value = NULL;
  WebKitWebView *target_web_view;

  /* This takes an optional first argument so it's either:
   * [tabId:int, details:obj], or [details:obj] */
  tab_id_value = jsc_value_object_get_property_at_index (args, 0);
  if (jsc_value_is_number (tab_id_value)) {
    obj = jsc_value_object_get_property_at_index (args, 1);
  } else {
    obj = g_steal_pointer (&tab_id_value);
  }

  if (!jsc_value_is_object (obj)) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "Invalid Arguments");
    return;
  }

  if (!tab_id_value)
    target_web_view = WEBKIT_WEB_VIEW (ephy_shell_get_active_web_view (shell));
  else
    target_web_view = get_web_view_for_tab_id (shell, jsc_value_to_int32 (tab_id_value), NULL);

  if (!target_web_view) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "Invalid Arguments");
    return;
  }

  if (!ephy_web_extension_has_host_or_active_permission (sender->extension, EPHY_WEB_VIEW (target_web_view), TRUE)) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_PERMISSION_DENIED, "Permission Denied");
    return;
  }

  ucm = webkit_web_view_get_user_content_manager (target_web_view);

  code = jsc_value_object_get_property (obj, "code");
  css = ephy_web_extension_get_custom_css (sender->extension, jsc_value_to_string (code));
  if (css)
    webkit_user_content_manager_remove_style_sheet (ucm, css);

  g_task_return_pointer (task, NULL, NULL);
}

static void
tabs_handler_get (EphyWebExtensionSender *sender,
                  char                   *name,
                  JSCValue               *args,
                  GTask                  *task)
{
  EphyShell *shell = ephy_shell_get_default ();
  g_autoptr (JsonBuilder) builder = json_builder_new ();
  g_autoptr (JsonNode) root = NULL;
  g_autoptr (JSCValue) tab_id_value = NULL;
  EphyWebView *target_web_view;
  EphyWindow *parent_window;

  tab_id_value = jsc_value_object_get_property_at_index (args, 0);
  if (!jsc_value_is_number (tab_id_value)) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "Invalid Arguments");
    return;
  }

  target_web_view = EPHY_WEB_VIEW (get_web_view_for_tab_id (shell, jsc_value_to_int32 (args), &parent_window));
  if (!target_web_view) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "Invalid Arguments");
    return;
  }

  add_web_view_to_json (sender->extension, builder, parent_window, target_web_view);
  root = json_builder_get_root (builder);

  g_task_return_pointer (task, json_to_string (root, FALSE), g_free);
}

static void
on_execute_script_ready (GObject      *source,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  g_autoptr (WebKitJavascriptResult) js_result = NULL;
  g_autoptr (GError) error = NULL;
  GTask *task = user_data;

  js_result = webkit_web_view_run_javascript_in_world_finish (WEBKIT_WEB_VIEW (source),
                                                              result,
                                                              &error);

  if (error) {
    g_task_return_error (task, g_steal_pointer (&error));
    return;
  }

  g_task_return_pointer (task, jsc_value_to_json (webkit_javascript_result_get_js_value (js_result), 0), g_free);
}

static void
tabs_handler_execute_script (EphyWebExtensionSender *sender,
                             char                   *name,
                             JSCValue               *args,
                             GTask                  *task)
{
  g_autoptr (JSCValue) code_value = NULL;
  g_autoptr (JSCValue) file_value = NULL;
  g_autoptr (JSCValue) obj = NULL;
  g_autoptr (JSCValue) tab_id_value = NULL;
  g_autoptr (GError) error = NULL;
  g_autofree char *code = NULL;
  EphyShell *shell = ephy_shell_get_default ();
  WebKitWebView *target_web_view;

  /* This takes an optional first argument so it's either:
   * [tabId:int, details:obj], or [details:obj] */
  tab_id_value = jsc_value_object_get_property_at_index (args, 0);
  if (jsc_value_is_number (tab_id_value)) {
    obj = jsc_value_object_get_property_at_index (args, 1);
  } else {
    obj = g_steal_pointer (&tab_id_value);
  }

  if (!jsc_value_is_object (obj)) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "Invalid Arguments");
    return;
  }

  file_value = jsc_value_object_get_property (obj, "file");
  code_value = jsc_value_object_get_property (obj, "code");

  if (jsc_value_is_string (code_value))
    code = jsc_value_to_string (code_value);
  else if (jsc_value_is_string (file_value)) {
    g_autofree char *resource_path = jsc_value_to_string (file_value);
    code = ephy_web_extension_get_resource_as_string (sender->extension, resource_path[0] == '/' ? resource_path + 1 : resource_path);
  } else {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "Invalid Arguments");
    return;
  }

  if (!tab_id_value)
    target_web_view = WEBKIT_WEB_VIEW (ephy_shell_get_active_web_view (shell));
  else
    target_web_view = get_web_view_for_tab_id (shell, jsc_value_to_int32 (tab_id_value), NULL);

  if (code && target_web_view) {
    if (!ephy_web_extension_has_host_or_active_permission (sender->extension, EPHY_WEB_VIEW (target_web_view), TRUE)) {
      g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_PERMISSION_DENIED, "Permission Denied");
      return;
    }

    webkit_web_view_run_javascript_in_world (target_web_view,
                                             code,
                                             ephy_web_extension_get_guid (sender->extension),
                                             NULL,
                                             on_execute_script_ready,
                                             task);
  }
}

static void
tabs_handler_send_message (EphyWebExtensionSender *sender,
                           char                   *name,
                           JSCValue               *args,
                           GTask                  *task)
{
  EphyWebExtensionManager *manager = ephy_web_extension_manager_get_default ();
  g_autoptr (JSCValue) tab_id_value = NULL;
  g_autoptr (JSCValue) message_value = NULL;
  g_autofree char *serialized_message = NULL;
  EphyShell *shell = ephy_shell_get_default ();
  WebKitWebView *target_web_view;

  tab_id_value = jsc_value_object_get_property_at_index (args, 0);
  if (!jsc_value_is_number (tab_id_value)) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.sendMessage(): Invalid tabId");
    return;
  }

  message_value = jsc_value_object_get_property_at_index (args, 1);
  if (jsc_value_is_undefined (message_value)) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.sendMessage(): Message argument missing");
    return;
  }

  target_web_view = get_web_view_for_tab_id (shell, jsc_value_to_int32 (tab_id_value), NULL);
  if (!target_web_view) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.sendMessage(): Failed to find tabId");
    return;
  }


  if (!ephy_web_extension_has_host_or_active_permission (sender->extension, EPHY_WEB_VIEW (target_web_view), TRUE)) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_PERMISSION_DENIED, "tabs.sendMessage(): Permission Denied");
    return;
  }

  serialized_message = jsc_value_to_json (message_value, 0);
  ephy_web_extension_manager_emit_in_tab_with_reply (manager,
                                                     sender->extension,
                                                     "runtime.onMessage",
                                                     serialized_message,
                                                     target_web_view,
                                                     ephy_web_extension_create_sender_object (sender),
                                                     task);
}

static char *
resolve_to_absolute_url (EphyWebExtension *web_extension,
                         char             *url)
{
  char *absolute_url;

  if (!url || *url != '/')
    return url;

  absolute_url = g_strconcat ("ephy-webextension://", ephy_web_extension_get_guid (web_extension), url, NULL);
  g_free (url);
  return absolute_url;
}

gboolean
ephy_web_extension_api_tabs_url_is_unprivileged (const char *url)
{
  const char *scheme;
  /* https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/tabs/create */
  const char * const forbidden_schemes[] = {
    "data",
    "javascript",
    "chrome",
    "file",
    "about", /* ephy_embed_utils_url_is_empty() allows safe about URIs. */
  };

  if (!url)
    return TRUE;

  if (ephy_embed_utils_url_is_empty (url))
    return TRUE;

  scheme = g_uri_peek_scheme (url);

  for (guint i = 0; i < G_N_ELEMENTS (forbidden_schemes); i++) {
    if (g_strcmp0 (scheme, forbidden_schemes[i]) == 0)
      return FALSE;
  }

  return TRUE;
}

static void
tabs_handler_create (EphyWebExtensionSender *sender,
                     char                   *name,
                     JSCValue               *args,
                     GTask                  *task)
{
  EphyShell *shell = ephy_shell_get_default ();
  EphyEmbed *embed;
  EphyWindow *parent_window;
  EphyWebView *new_web_view;
  g_autoptr (JSCValue) create_properties = NULL;
  g_autofree char *url = NULL;
  EphyNewTabFlags new_tab_flags = 0;
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) root = NULL;

  create_properties = jsc_value_object_get_property_at_index (args, 0);
  if (!jsc_value_is_object (create_properties)) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.create(): First argument is not an object");
    return;
  }

  url = resolve_to_absolute_url (sender->extension, api_utils_get_string_property (create_properties, "url", NULL));
  if (!ephy_web_extension_api_tabs_url_is_unprivileged (url)) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.create(): URL '%s' is not allowed", url);
    return;
  }

  if (api_utils_get_boolean_property (create_properties, "active", FALSE))
    new_tab_flags |= EPHY_NEW_TAB_JUMP;

  parent_window = get_window_by_id (shell, api_utils_get_int32_property (create_properties, "windowId", -1));
  embed = ephy_shell_new_tab (shell, parent_window, NULL, new_tab_flags);
  new_web_view = ephy_embed_get_web_view (embed);

  if (url && api_utils_get_boolean_property (create_properties, "openInReaderMode", FALSE)) {
    char *reader_url = g_strconcat (EPHY_READER_SCHEME, ":", url, NULL);
    g_free (url);
    url = reader_url;
  }

  if (url)
    ephy_web_view_load_url (new_web_view, url);
  else
    ephy_web_view_load_new_tab_page (new_web_view);

  builder = json_builder_new ();
  add_web_view_to_json (sender->extension, builder, parent_window, new_web_view);
  root = json_builder_get_root (builder);
  g_task_return_pointer (task, json_to_string (root, FALSE), g_free);
}

static void
tabs_handler_update (EphyWebExtensionSender *sender,
                     char                   *name,
                     JSCValue               *args,
                     GTask                  *task)
{
  EphyShell *shell = ephy_shell_get_default ();
  g_autoptr (JSCValue) update_properties = NULL;
  g_autoptr (JSCValue) tab_id_value = NULL;
  g_autofree char *new_url = NULL;
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) root = NULL;
  WebKitWebView *target_web_view;
  EphyWindow *parent_window;
  int tab_id = -1;
  int muted;

  /* First arg is optional tabId, second updateProperties. */
  update_properties = jsc_value_object_get_property_at_index (args, 1);
  if (jsc_value_is_undefined (update_properties)) {
    g_object_unref (update_properties);
    update_properties = jsc_value_object_get_property_at_index (args, 0);
  } else {
    tab_id_value = jsc_value_object_get_property_at_index (args, 0);
    tab_id = jsc_value_to_int32 (tab_id_value);
  }

  if (!jsc_value_is_object (update_properties)) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.update(): Missing updateProperties.");
    return;
  }

  if (tab_id >= 0)
    target_web_view = get_web_view_for_tab_id (shell, tab_id, &parent_window);
  else {
    target_web_view = WEBKIT_WEB_VIEW (ephy_shell_get_active_web_view (shell));
    parent_window = EPHY_WINDOW (gtk_application_get_active_window (GTK_APPLICATION (shell)));
  }

  if (!target_web_view) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.update(): Failed to find tabId %d.", tab_id);
    return;
  }

  new_url = resolve_to_absolute_url (sender->extension, api_utils_get_string_property (update_properties, "url", NULL));
  if (!ephy_web_extension_api_tabs_url_is_unprivileged (new_url)) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.update(): URL '%s' is not allowed", new_url);
    return;
  }

  muted = api_utils_get_tri_state_value_property (update_properties, "muted");
  if (muted != API_VALUE_UNSET)
    webkit_web_view_set_is_muted (target_web_view, muted);

  if (new_url)
    webkit_web_view_load_uri (target_web_view, new_url);

  builder = json_builder_new ();
  add_web_view_to_json (sender->extension, builder, parent_window, EPHY_WEB_VIEW (target_web_view));
  root = json_builder_get_root (builder);
  g_task_return_pointer (task, json_to_string (root, FALSE), g_free);
}

static void
close_tab_id (EphyShell *shell,
              int        tab_id)
{
  EphyWindow *parent;
  EphyTabView *tab_view;
  GtkWidget *web_view;

  web_view = GTK_WIDGET (get_web_view_for_tab_id (shell, tab_id, &parent));
  if (!web_view)
    return;

  tab_view = ephy_window_get_tab_view (parent);
  ephy_tab_view_close (tab_view, gtk_widget_get_parent (gtk_widget_get_parent (web_view)));
}

static void
tabs_handler_remove (EphyWebExtensionSender *sender,
                     char                   *name,
                     JSCValue               *args,
                     GTask                  *task)
{
  EphyShell *shell = ephy_shell_get_default ();
  g_autoptr (JSCValue) tab_ids = NULL;

  /* FIXME: Epiphany should use webkit_web_view_try_close() and this should wait until after that. */

  tab_ids = jsc_value_object_get_property_at_index (args, 0);

  if (jsc_value_is_number (tab_ids)) {
    close_tab_id (shell, jsc_value_to_int32 (tab_ids));
    g_task_return_pointer (task, NULL, NULL);
    return;
  }

  if (jsc_value_is_array (tab_ids)) {
    g_autoptr (JSCValue) value = jsc_value_object_get_property_at_index (tab_ids, 0);
    for (guint i = 1; !jsc_value_is_undefined (value); i++) {
      if (jsc_value_is_number (value))
        close_tab_id (shell, jsc_value_to_int32 (value));

      g_object_unref (value);
      value = jsc_value_object_get_property_at_index (tab_ids, i);
    }

    g_task_return_pointer (task, NULL, NULL);
  }

  g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.remove(): First argument is not a number or array.");
}

static void
tabs_handler_set_zoom (EphyWebExtensionSender *sender,
                       char                   *name,
                       JSCValue               *args,
                       GTask                  *task)
{
  EphyShell *shell = ephy_shell_get_default ();
  g_autoptr (JSCValue) zoom_level_value = NULL;
  g_autoptr (JSCValue) tab_id_value = NULL;
  WebKitWebView *target_web_view;
  int tab_id = -1;
  double zoom_level;

  /* First arg is optional tabId, second zoomFactor. */
  zoom_level_value = jsc_value_object_get_property_at_index (args, 1);
  if (jsc_value_is_undefined (zoom_level_value)) {
    g_object_unref (zoom_level_value);
    zoom_level_value = jsc_value_object_get_property_at_index (args, 0);
  } else {
    tab_id_value = jsc_value_object_get_property_at_index (args, 0);
    tab_id = jsc_value_to_int32 (tab_id_value);
  }

  if (!jsc_value_is_number (zoom_level_value)) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.setZoom(): Missing zoomFactor.");
    return;
  }

  zoom_level = jsc_value_to_double (zoom_level_value);
  if (zoom_level < 0.3 || zoom_level > 5) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.setZoom(): zoomFactor must be between 0.3 and 5.0.");
    return;
  }

  if (tab_id >= 0)
    target_web_view = get_web_view_for_tab_id (shell, tab_id, NULL);
  else
    target_web_view = WEBKIT_WEB_VIEW (ephy_shell_get_active_web_view (shell));

  if (!target_web_view) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.setZoom(): Failed to find tabId %d.", tab_id);
    return;
  }

  webkit_web_view_set_zoom_level (target_web_view, jsc_value_to_double (zoom_level_value));
  g_task_return_pointer (task, NULL, NULL);
}

static void
tabs_handler_get_zoom (EphyWebExtensionSender *sender,
                       char                   *name,
                       JSCValue               *args,
                       GTask                  *task)
{
  EphyShell *shell = ephy_shell_get_default ();
  g_autoptr (JSCValue) tab_id_value = NULL;
  WebKitWebView *target_web_view;
  int tab_id = -1;

  tab_id_value = jsc_value_object_get_property_at_index (args, 0);
  if (jsc_value_is_number (tab_id_value))
    tab_id = jsc_value_to_int32 (tab_id_value);

  if (tab_id >= 0)
    target_web_view = get_web_view_for_tab_id (shell, tab_id, NULL);
  else
    target_web_view = WEBKIT_WEB_VIEW (ephy_shell_get_active_web_view (shell));

  if (!target_web_view) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.getZoom(): Failed to find tabId %d.", tab_id);
    return;
  }

  g_task_return_pointer (task, g_strdup_printf ("%f", webkit_web_view_get_zoom_level (target_web_view)), g_free);
}

static void
tabs_handler_reload (EphyWebExtensionSender *sender,
                     char                   *name,
                     JSCValue               *args,
                     GTask                  *task)
{
  EphyShell *shell = ephy_shell_get_default ();
  g_autoptr (JSCValue) tab_id_value = NULL;
  WebKitWebView *target_web_view;
  int tab_id = -1;

  tab_id_value = jsc_value_object_get_property_at_index (args, 0);
  if (jsc_value_is_number (tab_id_value))
    tab_id = jsc_value_to_int32 (tab_id_value);

  if (tab_id >= 0)
    target_web_view = get_web_view_for_tab_id (shell, tab_id, NULL);
  else
    target_web_view = WEBKIT_WEB_VIEW (ephy_shell_get_active_web_view (shell));

  if (!target_web_view) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.reload(): Failed to find tabId %d.", tab_id);
    return;
  }

  webkit_web_view_reload (WEBKIT_WEB_VIEW (target_web_view));

  g_task_return_pointer (task, NULL, NULL);
}

static EphyWebExtensionAsyncApiHandler tab_async_handlers[] = {
  {"executeScript", tabs_handler_execute_script},
  {"sendMessage", tabs_handler_send_message},
  {"create", tabs_handler_create},
  {"query", tabs_handler_query},
  {"insertCSS", tabs_handler_insert_css},
  {"remove", tabs_handler_remove},
  {"removeCSS", tabs_handler_remove_css},
  {"get", tabs_handler_get},
  {"getZoom", tabs_handler_get_zoom},
  {"setZoom", tabs_handler_set_zoom},
  {"update", tabs_handler_update},
  {"reload", tabs_handler_reload},
};

void
ephy_web_extension_api_tabs_handler (EphyWebExtensionSender *sender,
                                     char                   *name,
                                     JSCValue               *args,
                                     GTask                  *task)
{
  for (guint idx = 0; idx < G_N_ELEMENTS (tab_async_handlers); idx++) {
    EphyWebExtensionAsyncApiHandler handler = tab_async_handlers[idx];

    if (g_strcmp0 (handler.name, name) == 0) {
      handler.execute (sender, name, args, task);
      return;
    }
  }

  g_warning ("%s(): '%s' not implemented by Epiphany!", __FUNCTION__, name);
  g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_NOT_IMPLEMENTED, "Not Implemented");
}
