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

#include "config.h"

#include "ephy-link.h"
#include "ephy-shell.h"

#include "tabs.h"
#include "windows.h"

EphyWindow *
ephy_web_extension_api_windows_get_window_for_id (gint64 window_id)
{
  GList *windows;

  if (window_id < 0)
    return NULL;

  windows = gtk_application_get_windows (GTK_APPLICATION (ephy_shell_get_default ()));

  for (GList *win_list = windows; win_list; win_list = g_list_next (win_list)) {
    EphyWindow *window = EPHY_WINDOW (win_list->data);

    if (ephy_window_get_uid (window) == (guint64)window_id)
      return window;
  }

  g_debug ("Failed to find window with id %ld", window_id);
  return NULL;
}

static const char *
get_window_state (EphyWindow *window)
{
  if (ephy_window_is_fullscreen (window))
    return "fullscreen";
  if (ephy_window_is_maximized (window))
    return "maximized";
  /* TODO: minimized */
  return "normal";
}

static void
add_tabs_to_json (EphyWebExtension *extension,
                  JsonBuilder      *builder,
                  EphyWindow       *window)
{
  EphyTabView *tab_view = ephy_window_get_tab_view (window);

  json_builder_begin_array (builder);

  for (int i = 0; i < ephy_tab_view_get_n_pages (tab_view); i++) {
    EphyWebView *web_view = ephy_embed_get_web_view (EPHY_EMBED (ephy_tab_view_get_nth_page (tab_view, i)));

    ephy_web_extension_api_tabs_add_tab_to_json (extension, builder, window, web_view);
  }

  json_builder_end_array (builder);
}

static void
add_window_to_json (EphyWebExtension *extension,
                    JsonBuilder      *builder,
                    EphyWindow       *window,
                    gboolean          populate_tabs)
{
  gboolean is_focused = EPHY_WINDOW (gtk_application_get_active_window (GTK_APPLICATION (ephy_shell_get_default ()))) == window;
  EphyTabView *tab_view = ephy_window_get_tab_view (window);
  EphyEmbed *embed = EPHY_EMBED (ephy_tab_view_get_selected_page (tab_view));
  EphyWebView *active_web_view = ephy_embed_get_web_view (embed);
  gboolean has_tab_permission = ephy_web_extension_has_tab_or_host_permission (extension, active_web_view, TRUE);

  json_builder_begin_object (builder);
  json_builder_set_member_name (builder, "id");
  json_builder_add_int_value (builder, ephy_window_get_uid (window));
  /* TODO: Get the Title if we have permissions for the active tab. */
  json_builder_set_member_name (builder, "focused");
  json_builder_add_boolean_value (builder, is_focused);
  json_builder_set_member_name (builder, "alwaysOnTop");
  json_builder_add_boolean_value (builder, FALSE);
  json_builder_set_member_name (builder, "type");
  json_builder_add_string_value (builder, "normal"); /* We don't show any non-normal windows. */
  json_builder_set_member_name (builder, "state");
  json_builder_add_string_value (builder, get_window_state (window));
  json_builder_set_member_name (builder, "incognito");
  json_builder_add_boolean_value (builder, ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) == EPHY_EMBED_SHELL_MODE_INCOGNITO);
  if (has_tab_permission) {
    json_builder_set_member_name (builder, "title");
    json_builder_add_string_value (builder, ephy_embed_get_title (embed));
  }
  if (populate_tabs) {
    json_builder_set_member_name (builder, "tabs");
    add_tabs_to_json (extension, builder, window);
  }
  json_builder_end_object (builder);
}

char *
ephy_web_extension_api_windows_create_window_json (EphyWebExtension *extension,
                                                   EphyWindow       *window)
{
  g_autoptr (JsonBuilder) builder = json_builder_new ();
  g_autoptr (JsonNode) root = NULL;
  add_window_to_json (extension,
                      builder,
                      window,
                      TRUE);
  root = json_builder_get_root (builder);
  return json_to_string (root, FALSE);
}


static void
windows_handler_get (EphyWebExtensionSender *sender,
                     const char             *method_name,
                     JsonArray              *args,
                     GTask                  *task)
{
  gint64 window_id = ephy_json_array_get_int (args, 0);
  JsonObject *get_info = ephy_json_array_get_object (args, 1);
  g_autoptr (JsonBuilder) builder = json_builder_new ();
  g_autoptr (JsonNode) root = NULL;
  EphyWindow *window;
  gboolean populate_tabs;

  if (window_id == -1) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "window.get(): First argument is not a windowId");
    return;
  }

  populate_tabs = get_info ? ephy_json_object_get_boolean (get_info, "populate", FALSE) : FALSE;
  window = ephy_web_extension_api_windows_get_window_for_id (window_id);
  if (!window) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "window.get(): Failed to find window by id");
    return;
  }

  add_window_to_json (sender->extension, builder, window, populate_tabs);
  root = json_builder_get_root (builder);
  g_task_return_pointer (task, json_to_string (root, FALSE), g_free);
}

static void
windows_handler_get_current (EphyWebExtensionSender *sender,
                             const char             *method_name,
                             JsonArray              *args,
                             GTask                  *task)
{
  EphyWebExtensionManager *manager = ephy_web_extension_manager_get_default ();
  JsonObject *get_info = ephy_json_array_get_object (args, 0);
  g_autoptr (JsonBuilder) builder = json_builder_new ();
  g_autoptr (JsonNode) root = NULL;
  gboolean populate_tabs = FALSE;
  EphyWindow *window;

  if (sender->view == ephy_web_extension_manager_get_background_web_view (manager, sender->extension))
    window = EPHY_WINDOW (gtk_application_get_active_window (GTK_APPLICATION (ephy_shell_get_default ())));
  else
    window = EPHY_WINDOW (gtk_widget_get_root (GTK_WIDGET (sender->view)));

  populate_tabs = get_info ? ephy_json_object_get_boolean (get_info, "populate", FALSE) : FALSE;

  add_window_to_json (sender->extension, builder, window, populate_tabs);
  root = json_builder_get_root (builder);
  g_task_return_pointer (task, json_to_string (root, FALSE), g_free);
}

static void
windows_handler_get_last_focused (EphyWebExtensionSender *sender,
                                  const char             *method_name,
                                  JsonArray              *args,
                                  GTask                  *task)
{
  JsonObject *get_info = ephy_json_array_get_object (args, 0);
  g_autoptr (JsonBuilder) builder = json_builder_new ();
  g_autoptr (JsonNode) root = NULL;
  gboolean populate_tabs;
  EphyWindow *window;

  window = EPHY_WINDOW (gtk_application_get_active_window (GTK_APPLICATION (ephy_shell_get_default ())));
  populate_tabs = get_info ? ephy_json_object_get_boolean (get_info, "populate", FALSE) : FALSE;

  add_window_to_json (sender->extension, builder, window, populate_tabs);
  root = json_builder_get_root (builder);
  g_task_return_pointer (task, json_to_string (root, FALSE), g_free);
}

static void
windows_handler_get_all (EphyWebExtensionSender *sender,
                         const char             *method_name,
                         JsonArray              *args,
                         GTask                  *task)
{
  JsonObject *get_info = ephy_json_array_get_object (args, 0);
  g_autoptr (JsonBuilder) builder = json_builder_new ();
  g_autoptr (JsonNode) root = NULL;
  gboolean populate_tabs;
  GList *windows;

  windows = gtk_application_get_windows (GTK_APPLICATION (ephy_shell_get_default ()));
  populate_tabs = get_info ? ephy_json_object_get_boolean (get_info, "populate", FALSE) : FALSE;

  json_builder_begin_array (builder);
  for (GList *win_list = windows; win_list; win_list = g_list_next (win_list)) {
    EphyWindow *window = EPHY_WINDOW (win_list->data);
    add_window_to_json (sender->extension, builder, window, populate_tabs);
  }
  json_builder_end_array (builder);

  root = json_builder_get_root (builder);
  g_task_return_pointer (task, json_to_string (root, FALSE), g_free);
}

/*
 * get_url_property:
 * @object: Object containing urls
 *
 * Returns: (transfer container): A #GPtrArray of urls, maybe empty or %NULL if invalid. Strings are owned by @object.
 */
static GPtrArray *
get_url_property (JsonObject *object)
{
  JsonNode *node = json_object_get_member (object, "url");
  GPtrArray *urls;

  if (!node)
    return NULL;

  if (ephy_json_node_to_string (node)) {
    const char *url = ephy_json_node_to_string (node);

    if (ephy_web_extension_api_tabs_url_is_unprivileged (url)) {
      urls = g_ptr_array_sized_new (1);
      g_ptr_array_add (urls, (char *)url);
      return urls;
    }

    return NULL;
  }

  if (JSON_NODE_HOLDS_ARRAY (node)) {
    JsonArray *array = json_node_get_array (node);
    urls = g_ptr_array_sized_new (json_array_get_length (array));

    for (guint i = 0; i < json_array_get_length (array); i++) {
      const char *url = ephy_json_array_get_string (array, i);

      if (!url)
        continue;

      if (ephy_web_extension_api_tabs_url_is_unprivileged (url))
        g_ptr_array_add (urls, (char *)url);
    }

    return urls;
  }

  g_debug ("Received invalid urls property");
  return NULL;
}

static void
windows_handler_create (EphyWebExtensionSender *sender,
                        const char             *method_name,
                        JsonArray              *args,
                        GTask                  *task)
{
  JsonObject *create_data = ephy_json_array_get_object (args, 0);
  g_autoptr (GPtrArray) urls = NULL;
  g_autoptr (JsonBuilder) builder = json_builder_new ();
  g_autoptr (JsonNode) root = NULL;
  EphyWindow *window;

  if (create_data)
    urls = get_url_property (create_data);

  window = ephy_window_new ();

  if (!urls || urls->len == 0)
    ephy_link_open (EPHY_LINK (window), NULL, NULL, EPHY_LINK_HOME_PAGE);
  else {
    for (guint i = 0; i < urls->len; i++)
      ephy_link_open (EPHY_LINK (window), g_ptr_array_index (urls, i), NULL, EPHY_LINK_NEW_TAB);
  }

  gtk_window_present (GTK_WINDOW (window));

  add_window_to_json (sender->extension, builder, window, TRUE);
  root = json_builder_get_root (builder);
  g_task_return_pointer (task, json_to_string (root, FALSE), g_free);
}

static void
windows_handler_remove (EphyWebExtensionSender *sender,
                        const char             *method_name,
                        JsonArray              *args,
                        GTask                  *task)
{
  gint64 window_id = ephy_json_array_get_int (args, 0);
  EphyWindow *window;

  if (window_id == -1) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "window.remove(): First argument is not a windowId");
    return;
  }

  window = ephy_web_extension_api_windows_get_window_for_id (window_id);

  if (!window) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "window.remove(): Failed to find window by id");
    return;
  }

  /* We could use `ephy_window_close()` here but it will do a blocking prompt to the user which I don't believe is expected. */
  gtk_window_destroy (GTK_WINDOW (window));
  g_task_return_pointer (task, NULL, NULL);
}

static EphyWebExtensionApiHandler windows_handlers[] = {
  {"get", windows_handler_get},
  {"getCurrent", windows_handler_get_current},
  {"getLastFocused", windows_handler_get_last_focused},
  {"getAll", windows_handler_get_all},
  {"create", windows_handler_create},
  {"remove", windows_handler_remove},
};

void
ephy_web_extension_api_windows_handler (EphyWebExtensionSender *sender,
                                        const char             *method_name,
                                        JsonArray              *args,
                                        GTask                  *task)
{
  for (guint idx = 0; idx < G_N_ELEMENTS (windows_handlers); idx++) {
    EphyWebExtensionApiHandler handler = windows_handlers[idx];

    if (g_strcmp0 (handler.name, method_name) == 0) {
      handler.execute (sender, method_name, args, task);
      return;
    }
  }

  g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_NOT_IMPLEMENTED, "windows.%s(): Not Implemented", method_name);
}
