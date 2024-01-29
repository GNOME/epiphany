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

#include "ephy-embed-utils.h"
#include "ephy-web-extension-manager.h"
#include "ephy-shell.h"

#include "runtime.h"

static void
runtime_handler_get_browser_info (EphyWebExtensionSender *sender,
                                  const char             *method_name,
                                  JsonArray              *args,
                                  GTask                  *task)
{
  g_autoptr (JsonBuilder) builder = json_builder_new ();
  g_autoptr (JsonNode) root = NULL;

  json_builder_begin_object (builder);
  json_builder_set_member_name (builder, "name");
  json_builder_add_string_value (builder, "Epiphany");
  json_builder_set_member_name (builder, "version");
  json_builder_add_string_value (builder, EPHY_VERSION);
  json_builder_set_member_name (builder, "vendor");
  json_builder_add_string_value (builder, "GNOME");
  json_builder_end_object (builder);

  root = json_builder_get_root (builder);

  g_task_return_pointer (task, json_to_string (root, FALSE), g_free);
}

static const char *
get_arch (void)
{
  /* https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/runtime/PlatformArch */
#if defined (__x86_64__) || defined(_M_X64)
  return "x86-64";
#elif defined(__i386__) || defined(_M_IX86)
  return "x86-32";
#elif defined(__aarch64__) || defined(__arm__) || defined(_M_ARM64) || defined(_M_ARM)
  return "arm";
#else
  #warning "Unknown architecture"
  return "unknown";
#endif
}

static void
runtime_handler_get_platform_info (EphyWebExtensionSender *sender,
                                   const char             *method_name,
                                   JsonArray              *args,
                                   GTask                  *task)
{
  g_autoptr (JsonBuilder) builder = json_builder_new ();
  g_autoptr (JsonNode) root = NULL;

  json_builder_begin_object (builder);
  json_builder_set_member_name (builder, "os");
  /* Epiphany doesn't support Windows or macOS and separating out BSDs is most likely
   * to be used improperly anyway. */
  json_builder_add_string_value (builder, "linux");
  json_builder_set_member_name (builder, "arch");
  json_builder_add_string_value (builder, get_arch ());
  json_builder_set_member_name (builder, "nacl_arch");
  json_builder_add_string_value (builder, get_arch ());
  json_builder_end_object (builder);

  root = json_builder_get_root (builder);

  g_task_return_pointer (task, json_to_string (root, FALSE), g_free);
}

static gboolean
is_empty_object (JsonNode *node)
{
  g_assert (node);

  if (!JSON_NODE_HOLDS_OBJECT (node))
    return FALSE;

  return (json_object_get_size (json_node_get_object (node)) == 0);
}

static void
runtime_handler_send_message (EphyWebExtensionSender *sender,
                              const char             *method_name,
                              JsonArray              *args,
                              GTask                  *task)
{
  EphyWebExtensionManager *manager = ephy_web_extension_manager_get_default ();
  g_autofree char *json = NULL;
  JsonNode *last_arg;
  JsonNode *message;

  /* The arguments of this are so terrible Mozilla dedicates this to describing how to parse it:
   * https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/runtime/sendMessage#parameters */

  /* If 3 args were passed the first arg was an extensionId. */
  if (ephy_json_array_get_element (args, 2)) {
    /* We don't actually support sending to external extensions yet. */
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_NOT_IMPLEMENTED, "extensionId is not supported");
    return;
  }

  last_arg = ephy_json_array_get_element (args, 1);
  if (!last_arg || json_node_is_null (last_arg) || is_empty_object (last_arg))
    message = ephy_json_array_get_element (args, 0);
  else {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_NOT_IMPLEMENTED, "extensionId is not supported");
    return;
  }

  json = message ? json_to_string (message, FALSE) : g_strdup ("undefined");
  ephy_web_extension_manager_emit_in_extension_views_with_reply (manager, sender->extension,
                                                                 sender,
                                                                 "runtime.onMessage",
                                                                 json,
                                                                 task);

  return;
}

static void
runtime_handler_open_options_page (EphyWebExtensionSender *sender,
                                   const char             *method_name,
                                   JsonArray              *args,
                                   GTask                  *task)
{
  const char *options_ui = ephy_web_extension_get_option_ui_page (sender->extension);
  EphyShell *shell = ephy_shell_get_default ();
  g_autofree char *title = NULL;
  g_autofree char *options_uri = NULL;
  GtkWidget *new_web_view;
  GtkWindow *new_window;

  if (!options_ui) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "Extension does not have an options page");
    return;
  }

  title = g_strdup_printf (_("Options for %s"), ephy_web_extension_get_name (sender->extension));
  options_uri = g_strdup_printf ("ephy-webextension://%s/%s", ephy_web_extension_get_guid (sender->extension), options_ui);

  new_window = GTK_WINDOW (gtk_window_new ());
  gtk_window_set_transient_for (new_window, gtk_application_get_active_window (GTK_APPLICATION (shell)));
  gtk_window_set_destroy_with_parent (new_window, TRUE);
  gtk_window_set_title (new_window, title);

  new_web_view = ephy_web_extensions_manager_create_web_extensions_webview (sender->extension);
  gtk_window_set_child (GTK_WINDOW (new_window), new_web_view);

  webkit_web_view_load_uri (WEBKIT_WEB_VIEW (new_web_view), options_uri);

  gtk_window_present (new_window);

  g_task_return_pointer (task, NULL, NULL);
}

static EphyWebExtensionApiHandler runtime_async_handlers[] = {
  {"getBrowserInfo", runtime_handler_get_browser_info},
  {"getPlatformInfo", runtime_handler_get_platform_info},
  {"openOptionsPage", runtime_handler_open_options_page},
  {"sendMessage", runtime_handler_send_message},
};

void
ephy_web_extension_api_runtime_handler (EphyWebExtensionSender *sender,
                                        const char             *method_name,
                                        JsonArray              *args,
                                        GTask                  *task)
{
  for (guint idx = 0; idx < G_N_ELEMENTS (runtime_async_handlers); idx++) {
    EphyWebExtensionApiHandler handler = runtime_async_handlers[idx];

    if (g_strcmp0 (handler.name, method_name) == 0) {
      handler.execute (sender, method_name, args, task);
      return;
    }
  }

  g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_NOT_IMPLEMENTED, "Not Implemented");
}
