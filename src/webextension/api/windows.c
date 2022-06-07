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

#include "ephy-shell.h"
#include "tabs.h"
#include "windows.h"

static EphyWindow *
get_window_for_id (int window_id)
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

  g_debug ("Failed to find window with id %d", window_id);
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
add_tabs_to_json (EphyWebExtension *self,
                  JsonBuilder      *builder,
                  EphyWindow       *window)
{
  EphyTabView *tab_view = ephy_window_get_tab_view (window);

  json_builder_begin_array (builder);

  for (int i = 0; i < ephy_tab_view_get_n_pages (tab_view); i++) {
    EphyWebView *web_view = ephy_embed_get_web_view (EPHY_EMBED (ephy_tab_view_get_nth_page (tab_view, i)));

    ephy_web_extension_api_tabs_add_tab_to_json (self, builder, window, web_view);
  }

  json_builder_end_array (builder);
}

static void
add_window_to_json (EphyWebExtension *self,
                    JsonBuilder      *builder,
                    EphyWindow       *window,
                    gboolean          populate_tabs)
{
  gboolean is_focused = EPHY_WINDOW (gtk_application_get_active_window (GTK_APPLICATION (ephy_shell_get_default ()))) == window;
  EphyTabView *tab_view = ephy_window_get_tab_view (window);
  EphyEmbed *embed = EPHY_EMBED (ephy_tab_view_get_selected_page (tab_view));
  EphyWebView *active_web_view = ephy_embed_get_web_view (embed);
  gboolean has_tab_permission = ephy_web_extension_has_tab_or_host_permission (self, active_web_view, TRUE);

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
    add_tabs_to_json (self, builder, window);
  }
  json_builder_end_object (builder);
}

static char *
windows_handler_get (EphyWebExtension  *self,
                     char              *name,
                     JSCValue          *args,
                     WebKitWebView     *web_view,
                     GError           **error)
{
  g_autoptr (JSCValue) window_id_value = jsc_value_object_get_property_at_index (args, 0);
  g_autoptr (JSCValue) get_info_value = jsc_value_object_get_property_at_index (args, 1);
  g_autoptr (JsonBuilder) builder = json_builder_new ();
  g_autoptr (JsonNode) root = NULL;
  EphyWindow *window;
  int window_id;
  gboolean populate_tabs = FALSE;

  if (!jsc_value_is_number (window_id_value)) {
    g_set_error_literal (error, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "window.get(): First argument is not a windowId");
    return NULL;
  }

  window_id = jsc_value_to_int32 (window_id_value);
  window = get_window_for_id (window_id);

  if (!window) {
    g_set_error_literal (error, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "window.get(): Failed to find window by id");
    return NULL;
  }

  if (jsc_value_is_object (get_info_value)) {
    g_autoptr (JSCValue) populate = jsc_value_object_get_property (get_info_value, "populate");
    populate_tabs = jsc_value_to_boolean (populate);
  }

  add_window_to_json (self, builder, window, populate_tabs);
  root = json_builder_get_root (builder);
  return json_to_string (root, FALSE);
}

static char *
windows_handler_get_current (EphyWebExtension  *self,
                             char              *name,
                             JSCValue          *args,
                             WebKitWebView     *web_view,
                             GError           **error)
{
  EphyWebExtensionManager *manager = ephy_web_extension_manager_get_default ();
  g_autoptr (JSCValue) get_info_value = jsc_value_object_get_property_at_index (args, 0);
  g_autoptr (JsonBuilder) builder = json_builder_new ();
  g_autoptr (JsonNode) root = NULL;
  gboolean populate_tabs = FALSE;
  EphyWindow *window;

  if (web_view == ephy_web_extension_manager_get_background_web_view (manager, self))
    window = EPHY_WINDOW (gtk_application_get_active_window (GTK_APPLICATION (ephy_shell_get_default ())));
  else
    window = EPHY_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (web_view)));

  if (jsc_value_is_object (get_info_value)) {
    g_autoptr (JSCValue) populate = jsc_value_object_get_property (get_info_value, "populate");
    populate_tabs = jsc_value_to_boolean (populate);
  }

  add_window_to_json (self, builder, window, populate_tabs);
  root = json_builder_get_root (builder);
  return json_to_string (root, FALSE);
}

static char *
windows_handler_get_last_focused (EphyWebExtension  *self,
                                  char              *name,
                                  JSCValue          *args,
                                  WebKitWebView     *web_view,
                                  GError           **error)
{
  g_autoptr (JSCValue) get_info_value = jsc_value_object_get_property_at_index (args, 0);
  g_autoptr (JsonBuilder) builder = json_builder_new ();
  g_autoptr (JsonNode) root = NULL;
  gboolean populate_tabs = FALSE;
  EphyWindow *window;

  window = EPHY_WINDOW (gtk_application_get_active_window (GTK_APPLICATION (ephy_shell_get_default ())));

  if (jsc_value_is_object (get_info_value)) {
    g_autoptr (JSCValue) populate = jsc_value_object_get_property (get_info_value, "populate");
    populate_tabs = jsc_value_to_boolean (populate);
  }

  add_window_to_json (self, builder, window, populate_tabs);
  root = json_builder_get_root (builder);
  return json_to_string (root, FALSE);
}

static EphyWebExtensionSyncApiHandler windows_handlers[] = {
  {"get", windows_handler_get},
  {"getCurrent", windows_handler_get_current},
  {"getLastFocused", windows_handler_get_last_focused},
};

void
ephy_web_extension_api_windows_handler (EphyWebExtension *self,
                                        char             *name,
                                        JSCValue         *args,
                                        WebKitWebView    *web_view,
                                        GTask            *task)
{
  g_autoptr (GError) error = NULL;
  guint idx;

  for (idx = 0; idx < G_N_ELEMENTS (windows_handlers); idx++) {
    EphyWebExtensionSyncApiHandler handler = windows_handlers[idx];
    char *ret;

    if (g_strcmp0 (handler.name, name) == 0) {
      ret = handler.execute (self, name, args, web_view, &error);

      if (error)
        g_task_return_error (task, g_steal_pointer (&error));
      else
        g_task_return_pointer (task, ret, g_free);

      return;
    }
  }

  g_warning ("%s(): '%s' not implemented by Epiphany!", __FUNCTION__, name);
  error = g_error_new_literal (WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_NOT_IMPLEMENTED, "Not Implemented");
  g_task_return_error (task, g_steal_pointer (&error));
}
