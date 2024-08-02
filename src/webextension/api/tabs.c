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
#include "ephy-reader-handler.h"
#include "ephy-shell.h"
#include "ephy-window.h"

#include "api-utils.h"
#include "tabs.h"

/* Matches Firefox. */
static const int WINDOW_ID_CURRENT = -2;

static WebKitWebView *
get_web_view_for_tab_id (EphyShell   *shell,
                         gint64       tab_id,
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

  g_debug ("Failed to find tab with id %" G_GUINT64_FORMAT, tab_id);
  return NULL;
}

static void
add_web_view_to_json (EphyWebExtension *extension,
                      JsonBuilder      *builder,
                      EphyWindow       *window,
                      EphyWebView      *web_view)
{
  EphyTabView *tab_view = ephy_window_get_tab_view (window);
  GtkWidget *page = GTK_WIDGET (EPHY_GET_EMBED_FROM_EPHY_WEB_VIEW (web_view));
  gboolean is_active = ephy_tab_view_get_current_page (tab_view) == page;
  WebKitFaviconDatabase *favicon_db = ephy_embed_shell_get_favicon_database (ephy_embed_shell_get_default ());
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
                        EPHY_WINDOW (gtk_widget_get_root (GTK_WIDGET (web_view))),
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
                    const char             *method_name,
                    JsonArray              *args,
                    GTask                  *task)
{
  g_autoptr (JsonBuilder) builder = json_builder_new ();
  g_autoptr (JsonNode) root = NULL;
  EphyShell *shell = ephy_shell_get_default ();
  JsonObject *query = ephy_json_array_get_object (args, 0);
  GList *windows;
  EphyWindow *active_window;
  ApiTriStateValue current_window;
  ApiTriStateValue active;
  gint64 window_id;
  gint64 tab_index;

  if (!query) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.query(): Missing query object.");
    return;
  }

  active = ephy_json_object_get_boolean (query, "active", API_VALUE_UNSET);
  current_window = ephy_json_object_get_boolean (query, "currentWindow", API_VALUE_UNSET);
  window_id = ephy_json_object_get_int (query, "windowId");
  tab_index = ephy_json_object_get_int (query, "index");

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
                         const char             *method_name,
                         JsonArray              *args,
                         GTask                  *task)
{
  EphyShell *shell = ephy_shell_get_default ();
  WebKitUserContentManager *ucm;
  WebKitUserStyleSheet *css = NULL;
  const char *code;
  gint64 tab_id = -1;
  JsonObject *details;
  WebKitWebView *target_web_view;

  /* This takes an optional first argument so it's either:
   * [tabId:int, details:obj], or [details:obj] */
  details = ephy_json_array_get_object (args, 1);
  if (!details)
    details = ephy_json_array_get_object (args, 0);
  else
    tab_id = ephy_json_array_get_int (args, 0);

  if (!details) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.insertCSS(): Missing details");
    return;
  }

  if (tab_id == -1)
    target_web_view = WEBKIT_WEB_VIEW (ephy_shell_get_active_web_view (shell));
  else
    target_web_view = get_web_view_for_tab_id (shell, tab_id, NULL);

  if (!target_web_view) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.insertCSS(): Failed to find tabId");
    return;
  }

  if (!ephy_web_extension_has_host_or_active_permission (sender->extension, EPHY_WEB_VIEW (target_web_view), TRUE)) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_PERMISSION_DENIED, "tabs.insertCSS(): Permission Denied");
    return;
  }

  ucm = webkit_web_view_get_user_content_manager (target_web_view);

  /* FIXME: Handle file */
  if (ephy_json_object_get_string (details, "file")) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.insertCSS(): file is currently unsupported");
    return;
  }

  code = ephy_json_object_get_string (details, "code");
  if (!code) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.insertCSS(): Missing code");
    return;
  }

  if ((ephy_json_object_get_int (details, "frameId"))) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.insertCSS(): frameId is currently unsupported");
    return;
  }

  /* FIXME: Support allFrames and cssOrigin */
  css = ephy_web_extension_add_custom_css (sender->extension, code);
  webkit_user_content_manager_add_style_sheet (ucm, css);

  g_task_return_pointer (task, NULL, NULL);
}

static void
tabs_handler_remove_css (EphyWebExtensionSender *sender,
                         const char             *method_name,
                         JsonArray              *args,
                         GTask                  *task)
{
  EphyShell *shell = ephy_shell_get_default ();
  gint64 tab_id = -1;
  const char *code;
  JsonObject *details;
  WebKitUserStyleSheet *css = NULL;
  WebKitUserContentManager *ucm;
  WebKitWebView *target_web_view;

  /* This takes an optional first argument so it's either:
   * [tabId:int, details:obj], or [details:obj] */
  details = ephy_json_array_get_object (args, 1);
  if (!details)
    details = ephy_json_array_get_object (args, 0);
  else
    tab_id = ephy_json_array_get_int (args, 0);

  if (!details) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.removeCSS(): Missing details");
    return;
  }

  if (tab_id == -1)
    target_web_view = WEBKIT_WEB_VIEW (ephy_shell_get_active_web_view (shell));
  else
    target_web_view = get_web_view_for_tab_id (shell, tab_id, NULL);

  if (!target_web_view) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.removeCSS(): Failed to find tabId");
    return;
  }

  if (!ephy_web_extension_has_host_or_active_permission (sender->extension, EPHY_WEB_VIEW (target_web_view), TRUE)) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_PERMISSION_DENIED, "tabs.removeCSS(): Permission Denied");
    return;
  }

  ucm = webkit_web_view_get_user_content_manager (target_web_view);

  if (!(code = ephy_json_object_get_string (details, "code"))) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.removeCSS(): Missing code (file is unsupported)");
    return;
  }

  css = ephy_web_extension_get_custom_css (sender->extension, code);
  if (css)
    webkit_user_content_manager_remove_style_sheet (ucm, css);

  g_task_return_pointer (task, NULL, NULL);
}

static void
tabs_handler_get (EphyWebExtensionSender *sender,
                  const char             *method_name,
                  JsonArray              *args,
                  GTask                  *task)
{
  EphyShell *shell = ephy_shell_get_default ();
  g_autoptr (JsonBuilder) builder = json_builder_new ();
  g_autoptr (JsonNode) root = NULL;
  gint64 tab_id = ephy_json_array_get_int (args, 0);
  EphyWebView *target_web_view;
  EphyWindow *parent_window;

  if (tab_id == -1) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.get(): Missing tabId");
    return;
  }

  target_web_view = EPHY_WEB_VIEW (get_web_view_for_tab_id (shell, tab_id, &parent_window));
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
  g_autoptr (JSCValue) value = NULL;
  g_autoptr (GError) error = NULL;
  GTask *task = user_data;

  value = webkit_web_view_evaluate_javascript_finish (WEBKIT_WEB_VIEW (source),
                                                      result,
                                                      &error);

  if (error) {
    g_task_return_error (task, g_steal_pointer (&error));
    return;
  }

  g_task_return_pointer (task, jsc_value_to_json (value, 0), g_free);
}

static void
tabs_handler_execute_script (EphyWebExtensionSender *sender,
                             const char             *method_name,
                             JsonArray              *args,
                             GTask                  *task)
{
  g_autofree char *code = NULL;
  const char *file;
  JsonObject *details;
  gint64 tab_id = -1;
  EphyShell *shell = ephy_shell_get_default ();
  WebKitWebView *target_web_view;

  /* This takes an optional first argument so it's either:
   * [tabId:int, details:obj], or [details:obj] */
  details = ephy_json_array_get_object (args, 1);
  if (!details)
    details = ephy_json_array_get_object (args, 0);
  else
    tab_id = ephy_json_array_get_int (args, 0);

  if (!details) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.executeScript(): Missing details");
    return;
  }

  if ((file = ephy_json_object_get_string (details, "file")))
    code = ephy_web_extension_get_resource_as_string (sender->extension, file[0] == '/' ? file + 1 : file);
  else
    code = ephy_json_object_dup_string (details, "code");

  if (!code) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.executeScript(): Missing code");
    return;
  }

  if (tab_id == -1)
    target_web_view = WEBKIT_WEB_VIEW (ephy_shell_get_active_web_view (shell));
  else
    target_web_view = get_web_view_for_tab_id (shell, tab_id, NULL);

  if (!target_web_view) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.executeScript(): Failed to find tabId");
    return;
  }

  if (!ephy_web_extension_has_host_or_active_permission (sender->extension, EPHY_WEB_VIEW (target_web_view), TRUE)) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_PERMISSION_DENIED, "Permission Denied");
    return;
  }

  webkit_web_view_evaluate_javascript (target_web_view,
                                       code, -1,
                                       ephy_web_extension_get_guid (sender->extension),
                                       NULL,
                                       NULL,
                                       on_execute_script_ready,
                                       task);
}

static void
tabs_handler_send_message (EphyWebExtensionSender *sender,
                           const char             *method_name,
                           JsonArray              *args,
                           GTask                  *task)
{
  EphyWebExtensionManager *manager = ephy_web_extension_manager_get_default ();
  g_autofree char *serialized_message = NULL;
  EphyShell *shell = ephy_shell_get_default ();
  WebKitWebView *target_web_view;
  gint64 tab_id = ephy_json_array_get_int (args, 0);
  JsonNode *message = ephy_json_array_get_element (args, 1);

  if (tab_id == -1) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.sendMessage(): Invalid tabId");
    return;
  }

  if (!message) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.sendMessage(): Message argument missing");
    return;
  }

  target_web_view = get_web_view_for_tab_id (shell, tab_id, NULL);
  if (!target_web_view) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.sendMessage(): Failed to find tabId");
    return;
  }

  if (!ephy_web_extension_has_host_or_active_permission (sender->extension, EPHY_WEB_VIEW (target_web_view), TRUE)) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_PERMISSION_DENIED, "tabs.sendMessage(): Permission Denied");
    return;
  }

  serialized_message = json_to_string (message, FALSE);
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
                         const char       *url)
{
  if (!url || *url != '/')
    return g_strdup (url);

  return g_strconcat ("ephy-webextension://", ephy_web_extension_get_guid (web_extension), url, NULL);
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
                     const char             *method_name,
                     JsonArray              *args,
                     GTask                  *task)
{
  EphyShell *shell = ephy_shell_get_default ();
  JsonObject *create_properties = ephy_json_array_get_object (args, 0);
  EphyEmbed *embed;
  EphyWindow *parent_window;
  EphyWebView *new_web_view;
  g_autofree char *url = NULL;
  EphyNewTabFlags new_tab_flags = 0;
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) root = NULL;

  if (!create_properties) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.create(): First argument is not an object");
    return;
  }

  url = resolve_to_absolute_url (sender->extension, ephy_json_object_get_string (create_properties, "url"));
  if (!ephy_web_extension_api_tabs_url_is_unprivileged (url)) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.create(): URL '%s' is not allowed", url);
    return;
  }

  if (ephy_json_object_get_boolean (create_properties, "active", FALSE))
    new_tab_flags |= EPHY_NEW_TAB_JUMP;

  parent_window = get_window_by_id (shell, ephy_json_object_get_int (create_properties, "windowId"));
  embed = ephy_shell_new_tab (shell, parent_window, NULL, new_tab_flags);
  new_web_view = ephy_embed_get_web_view (embed);

  if (url && ephy_json_object_get_boolean (create_properties, "openInReaderMode", FALSE)) {
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
                     const char             *method_name,
                     JsonArray              *args,
                     GTask                  *task)
{
  EphyShell *shell = ephy_shell_get_default ();
  JsonObject *update_properties;
  gint64 tab_id = -1;
  const char *new_url;
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) root = NULL;
  WebKitWebView *target_web_view;
  EphyWindow *parent_window;
  ApiTriStateValue muted;

  /* First arg is optional tabId, second updateProperties. */
  update_properties = ephy_json_array_get_object (args, 1);
  if (!update_properties)
    update_properties = ephy_json_array_get_object (args, 0);
  else
    tab_id = ephy_json_array_get_int (args, 0);

  if (!update_properties) {
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
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.update(): Failed to find tabId %" G_GUINT64_FORMAT, tab_id);
    return;
  }

  new_url = resolve_to_absolute_url (sender->extension, ephy_json_object_get_string (update_properties, "url"));
  if (!ephy_web_extension_api_tabs_url_is_unprivileged (new_url)) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.update(): URL '%s' is not allowed", new_url);
    return;
  }

  muted = ephy_json_object_get_boolean (update_properties, "muted", API_VALUE_UNSET);
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
              gint64     tab_id)
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
                     const char             *method_name,
                     JsonArray              *args,
                     GTask                  *task)
{
  EphyShell *shell = ephy_shell_get_default ();
  JsonNode *tab_ids = ephy_json_array_get_element (args, 0);
  gint64 tab_id;

  /* FIXME: Epiphany should use webkit_web_view_try_close() and this should wait until after that. */

  if (JSON_NODE_HOLDS_ARRAY (tab_ids)) {
    JsonArray *array = json_node_get_array (tab_ids);

    for (guint i = 0; i < json_array_get_length (array); i++) {
      gint64 tab_id = ephy_json_array_get_int (array, i);
      if (tab_id != -1)
        close_tab_id (shell, tab_id);
    }

    g_task_return_pointer (task, NULL, NULL);
  }

  if ((tab_id = ephy_json_node_get_int (tab_ids)) != -1) {
    close_tab_id (shell, tab_id);
    g_task_return_pointer (task, NULL, NULL);
  }

  g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.remove(): First argument is not a number or array.");
}

static void
tabs_handler_set_zoom (EphyWebExtensionSender *sender,
                       const char             *method_name,
                       JsonArray              *args,
                       GTask                  *task)
{
  EphyShell *shell = ephy_shell_get_default ();
  WebKitWebView *target_web_view;
  gint64 tab_id = -1;
  double zoom_level;

  /* First arg is optional tabId, second zoomFactor. */
  zoom_level = ephy_json_array_get_double (args, 1);
  if (zoom_level == -1.0)
    zoom_level = ephy_json_array_get_double (args, 0);
  else
    tab_id = ephy_json_array_get_int (args, 0);

  if (zoom_level < 0.3 || zoom_level > 5) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.setZoom(): zoomFactor must be between 0.3 and 5.0.");
    return;
  }

  if (tab_id >= 0)
    target_web_view = get_web_view_for_tab_id (shell, tab_id, NULL);
  else
    target_web_view = WEBKIT_WEB_VIEW (ephy_shell_get_active_web_view (shell));

  if (!target_web_view) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.setZoom(): Failed to find tabId %" G_GUINT64_FORMAT, tab_id);
    return;
  }

  webkit_web_view_set_zoom_level (target_web_view, zoom_level);
  g_task_return_pointer (task, NULL, NULL);
}

static void
tabs_handler_get_zoom (EphyWebExtensionSender *sender,
                       const char             *method_name,
                       JsonArray              *args,
                       GTask                  *task)
{
  EphyShell *shell = ephy_shell_get_default ();
  gint64 tab_id = ephy_json_array_get_int (args, 0);
  WebKitWebView *target_web_view;

  if (tab_id >= 0)
    target_web_view = get_web_view_for_tab_id (shell, tab_id, NULL);
  else
    target_web_view = WEBKIT_WEB_VIEW (ephy_shell_get_active_web_view (shell));

  if (!target_web_view) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.getZoom(): Failed to find tabId %" G_GINT64_FORMAT, tab_id);
    return;
  }

  g_task_return_pointer (task, g_strdup_printf ("%f", webkit_web_view_get_zoom_level (target_web_view)), g_free);
}

static void
tabs_handler_reload (EphyWebExtensionSender *sender,
                     const char             *method_name,
                     JsonArray              *args,
                     GTask                  *task)
{
  EphyShell *shell = ephy_shell_get_default ();
  gint64 tab_id = ephy_json_array_get_int (args, 0);
  WebKitWebView *target_web_view;

  if (tab_id >= 0)
    target_web_view = get_web_view_for_tab_id (shell, tab_id, NULL);
  else
    target_web_view = WEBKIT_WEB_VIEW (ephy_shell_get_active_web_view (shell));

  if (!target_web_view) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.reload(): Failed to find tabId %" G_GINT64_FORMAT, tab_id);
    return;
  }

  webkit_web_view_reload (WEBKIT_WEB_VIEW (target_web_view));
  g_task_return_pointer (task, NULL, NULL);
}

static EphyWebExtensionApiHandler tab_async_handlers[] = {
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
                                     const char             *method_name,
                                     JsonArray              *args,
                                     GTask                  *task)
{
  for (guint idx = 0; idx < G_N_ELEMENTS (tab_async_handlers); idx++) {
    EphyWebExtensionApiHandler handler = tab_async_handlers[idx];

    if (g_strcmp0 (handler.name, method_name) == 0) {
      handler.execute (sender, method_name, args, task);
      return;
    }
  }

  g_warning ("%s(): '%s' not implemented by Epiphany!", __FUNCTION__, method_name);
  g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_NOT_IMPLEMENTED, "Not Implemented");
}
