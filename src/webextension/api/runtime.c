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
runtime_handler_get_browser_info (EphyWebExtension  *self,
                                  char              *name,
                                  JSCValue          *args,
                                  WebKitWebView     *web_view,
                                  GError           **error)
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

  return json_to_string (root, FALSE);
}

static const char *
get_os (void)
{
  /* https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/runtime/PlatformOs */
#if defined(__linux__)
  return "linux";
#elif defined(_WIN32)
  return "win";
#elif defined(__OpenBSD__) || defined(__FreeBSD__)
  return "openbsd"; /* MDN documents same for both. */
#elif defined(__APPLE__)
  return "mac";
#else
  #warning "Unknown OS"
  return "unknown";
#endif
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

static char *
runtime_handler_get_platform_info (EphyWebExtension  *self,
                                   char              *name,
                                   JSCValue          *args,
                                   WebKitWebView     *web_view,
                                   GError           **error)
{
  g_autoptr (JsonBuilder) builder = json_builder_new ();
  g_autoptr (JsonNode) root = NULL;

  json_builder_begin_object (builder);
  json_builder_set_member_name (builder, "os");
  json_builder_add_string_value (builder, get_os ());
  json_builder_set_member_name (builder, "arch");
  json_builder_add_string_value (builder, get_arch ());
  json_builder_set_member_name (builder, "nacl_arch");
  json_builder_add_string_value (builder, get_arch ());
  json_builder_end_object (builder);

  root = json_builder_get_root (builder);

  return json_to_string (root, FALSE);
}

static gboolean
is_empty_object (JSCValue *value)
{
  if (jsc_value_is_object (value)) {
    g_auto (GStrv) keys = jsc_value_object_enumerate_properties (value);
    return keys == NULL;
  }

  return FALSE;
}

static void
runtime_handler_send_message (EphyWebExtension *self,
                              char             *name,
                              JSCValue         *args,
                              WebKitWebView    *web_view,
                              GTask            *task)
{
  EphyWebExtensionManager *manager = ephy_web_extension_manager_get_default ();
  g_autoptr (GError) error = NULL;
  g_autoptr (JSCValue) last_value = NULL;
  g_autoptr (JSCValue) message = NULL;
  g_autofree char *json = NULL;

  /* The arguments of this are so terrible Mozilla dedicates this to describing how to parse it:
   * https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/runtime/sendMessage#parameters */
  last_value = jsc_value_object_get_property_at_index (args, 2);
  if (!jsc_value_is_undefined (last_value)) {
    /* We don't actually support sending to external extensions yet. */
    error = g_error_new_literal (WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_NOT_IMPLEMENTED, "extensionId is not supported");
    g_task_return_error (task, g_steal_pointer (&error));
    return;
  }

  last_value = jsc_value_object_get_property_at_index (args, 1);
  if (jsc_value_is_undefined (last_value) || jsc_value_is_null (last_value) || is_empty_object (last_value)) {
    message = jsc_value_object_get_property_at_index (args, 0);
  } else {
    error = g_error_new_literal (WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_NOT_IMPLEMENTED, "extensionId is not supported");
    g_task_return_error (task, g_steal_pointer (&error));
    return;
  }

  json = jsc_value_to_json (message, 0);
  ephy_web_extension_manager_emit_in_extension_views_with_reply (manager, self, "runtime.onMessage",
                                                                 json,
                                                                 web_view,
                                                                 task);

  return;
}

static char *
runtime_handler_open_options_page (EphyWebExtension  *self,
                                   char              *name,
                                   JSCValue          *args,
                                   WebKitWebView     *web_view,
                                   GError           **error)
{
  const char *options_ui = ephy_web_extension_get_option_ui_page (self);
  EphyShell *shell = ephy_shell_get_default ();
  g_autofree char *title = NULL;
  g_autofree char *options_uri = NULL;
  GtkWidget *new_web_view;
  GtkWindow *new_window;

  if (!options_ui) {
    g_set_error_literal (error, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "Extension does not have an options page");
    return NULL;
  }

  title = g_strdup_printf (_("Options for %s"), ephy_web_extension_get_name (self));
  options_uri = g_strdup_printf ("ephy-webextension://%s/%s", ephy_web_extension_get_guid (self), options_ui);

  new_window = GTK_WINDOW (gtk_window_new (GTK_WINDOW_TOPLEVEL));
  gtk_window_set_transient_for (new_window, gtk_application_get_active_window (GTK_APPLICATION (shell)));
  gtk_window_set_destroy_with_parent (new_window, TRUE);
  gtk_window_set_title (new_window, title);
  gtk_window_set_position (new_window, GTK_WIN_POS_CENTER_ON_PARENT);

  new_web_view = ephy_web_extensions_manager_create_web_extensions_webview (self);
  gtk_container_add (GTK_CONTAINER (new_window), new_web_view);

  webkit_web_view_load_uri (WEBKIT_WEB_VIEW (new_web_view), options_uri);

  gtk_widget_show (new_web_view);
  gtk_window_present (new_window);

  return NULL;
}

static EphyWebExtensionSyncApiHandler runtime_sync_handlers[] = {
  {"getBrowserInfo", runtime_handler_get_browser_info},
  {"getPlatformInfo", runtime_handler_get_platform_info},
  {"openOptionsPage", runtime_handler_open_options_page},
};

static EphyWebExtensionAsyncApiHandler runtime_async_handlers[] = {
  {"sendMessage", runtime_handler_send_message},
};

void
ephy_web_extension_api_runtime_handler (EphyWebExtension *self,
                                        char             *name,
                                        JSCValue         *args,
                                        WebKitWebView    *web_view,
                                        GTask            *task)
{
  g_autoptr (GError) error = NULL;
  guint idx;

  for (idx = 0; idx < G_N_ELEMENTS (runtime_sync_handlers); idx++) {
    EphyWebExtensionSyncApiHandler handler = runtime_sync_handlers[idx];
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

  for (idx = 0; idx < G_N_ELEMENTS (runtime_async_handlers); idx++) {
    EphyWebExtensionAsyncApiHandler handler = runtime_async_handlers[idx];

    if (g_strcmp0 (handler.name, name) == 0) {
      handler.execute (self, name, args, web_view, task);
      return;
    }
  }

  g_warning ("%s(): '%s' not implemented by Epiphany!", __FUNCTION__, name);
  error = g_error_new_literal (WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_NOT_IMPLEMENTED, "Not Implemented");
  g_task_return_error (task, g_steal_pointer (&error));
}
