/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2020 Jan-Michael Brummer <jan.brummer@tabos.org>
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
#include "ephy-webextension-api.h"
#include "ephy-webextension-common.h"

#include <json-glib/json-glib.h>
#include <webkit2/webkit-web-extension.h>
#include <JavaScriptCore/JavaScript.h>

struct _EphyWebExtensionExtension {
  GObject parent_instance;

  WebKitWebExtension *extension;
  char *guid;
  gboolean initialized;

  JsonObject *translations;
  int counter;
};

G_DEFINE_TYPE (EphyWebExtensionExtension, ephy_web_extension_extension, G_TYPE_OBJECT)

static EphyWebExtensionExtension *extension = NULL;

static void
ephy_web_extension_page_user_message_received_cb (WebKitWebPage     *page,
                                                  WebKitUserMessage *message)
{
  const char *name = webkit_user_message_get_name (message);
  WebKitFrame *frame = webkit_web_page_get_main_frame (page);
  g_autoptr (JSCValue) value = NULL;

  if (strcmp (name, "executeScript") == 0) {
    GVariant *parameters;
    const char *guid;
    const char *path;
    const char *code;
    g_autofree char *uri = NULL;
    /* FIXME: This should run in content-script world of the target tab. */
    JSCContext *context = webkit_frame_get_js_context (frame);

    parameters = webkit_user_message_get_parameters (message);
    if (!parameters)
      return;

    g_variant_get (parameters, "(&s&s&s)", &guid, &path, &code);
    uri = g_strdup_printf ("ephy-webextension://%s/%s", guid, path);
    value = jsc_context_evaluate_with_source_uri (context, code, -1, uri, 1);
    g_clear_object (&value);
  } else if (strcmp (name, "sendMessage") == 0) {
    GVariant *parameters;
    const char *script;
    g_autofree char *uri = NULL;
    JSCContext *context = webkit_frame_get_js_context (frame);

    parameters = webkit_user_message_get_parameters (message);
    if (!parameters)
      return;

    g_variant_get (parameters, "(&s)", &script);
    value = jsc_context_evaluate (context, script, -1);
    g_clear_object (&value);
  }
}

static void
ephy_web_extension_extension_page_created_cb (EphyWebExtensionExtension *extension,
                                              WebKitWebPage             *web_page)
{
  g_autoptr (JSCContext) js_context = NULL;

  /* Enforce the creation of the script world global context in the main frame */
  js_context = webkit_frame_get_js_context_for_script_world (webkit_web_page_get_main_frame (web_page), webkit_script_world_get_default ());
  (void)js_context;

  g_signal_connect_swapped (web_page, "user-message-received",
                            G_CALLBACK (ephy_web_extension_page_user_message_received_cb),
                            web_page);
}

static void
ephy_web_extension_dispose (GObject *object)
{
  EphyWebExtensionExtension *extension = EPHY_WEB_EXTENSION_EXTENSION (object);

  g_clear_object (&extension->extension);
  g_clear_pointer (&extension->guid, g_free);

  g_clear_pointer (&extension->translations, json_object_unref);

  G_OBJECT_CLASS (ephy_web_extension_extension_parent_class)->dispose (object);
}

static void
ephy_web_extension_extension_class_init (EphyWebExtensionExtensionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ephy_web_extension_dispose;
}

static void
ephy_web_extension_extension_init (EphyWebExtensionExtension *extension)
{
}

static gpointer
ephy_web_extension_extension_create_instance (gpointer data)
{
  return g_object_new (EPHY_TYPE_WEB_EXTENSION_EXTENSION, NULL);
}

EphyWebExtensionExtension *
ephy_web_extension_extension_get (void)
{
  static GOnce once_init = G_ONCE_INIT;
  return EPHY_WEB_EXTENSION_EXTENSION (g_once (&once_init, ephy_web_extension_extension_create_instance, NULL));
}

typedef struct {
  JSCValue *resolve_callback;
  JSCValue *reject_callback;
} EphyMessageData;

static void
ephy_message_data_free (EphyMessageData *data)
{
  g_object_unref (data->reject_callback);
  g_object_unref (data->resolve_callback);
  g_free (data);
}

static EphyMessageData *
ephy_message_data_new (JSCValue *resolve_callback,
                       JSCValue *reject_callback)
{
  EphyMessageData *data = g_new (EphyMessageData, 1);
  data->resolve_callback = g_object_ref (resolve_callback);
  data->reject_callback = g_object_ref (reject_callback);
  return data;
}

static void
on_send_message_finish (WebKitWebExtension *extension,
                        GAsyncResult       *result,
                        gpointer            user_data)
{
  GTask *task = user_data;
  g_autoptr (GError) error = NULL;
  g_autoptr (WebKitUserMessage) response = NULL;
  GVariant *params;

  response = webkit_web_extension_send_message_to_context_finish (extension, result, &error);

  if (error) {
    g_task_return_error (task, g_steal_pointer (&error));
  } else if (strcmp (webkit_user_message_get_name (response), "error") == 0) {
    params = webkit_user_message_get_parameters (response);
    g_assert (params);
    error = g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED, "%s", g_variant_get_string (params, NULL));
    g_task_return_error (task, g_steal_pointer (&error));
  } else {
    params = webkit_user_message_get_parameters (response);
    g_assert (params);
    g_task_return_pointer (task, g_variant_dup_string (params, NULL), g_free);
  }

  g_object_unref (task);
}

static void
on_ephy_message_finish (EphyWebExtensionExtension *extension,
                        GAsyncResult              *result,
                        gpointer                   user_data)
{
  g_autoptr (GError) error = NULL;
  EphyMessageData *message_data = g_task_get_task_data (G_TASK (result));
  g_autofree char *json = g_task_propagate_pointer (G_TASK (result), &error);
  JSCContext *context = jsc_value_get_context (message_data->resolve_callback);
  g_autoptr (JSCValue) value = NULL;
  g_autoptr (JSCValue) ret = NULL;

  if (error) {
    ret = jsc_value_function_call (message_data->reject_callback, G_TYPE_STRING, error->message, G_TYPE_NONE);
  } else if (strcmp (json, "") == 0) {
    value = jsc_value_new_undefined (context);
    ret = jsc_value_function_call (message_data->resolve_callback, JSC_TYPE_VALUE, value, G_TYPE_NONE);
  } else {
    value = jsc_value_new_from_json (context, json);
    ret = jsc_value_function_call (message_data->resolve_callback, JSC_TYPE_VALUE, value, G_TYPE_NONE);
  }

  (void)ret;
}

static void
ephy_send_message (const char *function_name,
                   JSCValue   *function_args,
                   JSCValue   *resolve_callback,
                   JSCValue   *reject_callback,
                   gpointer    user_data)
{
  EphyWebExtensionExtension *extension = user_data;
  WebKitUserMessage *message;
  EphyMessageData *data;
  GTask *task;
  char *args_json;

  /* TODO: If function_args is list and last arg is callable, treat it as `chrome` API. */

  if (!jsc_value_is_function (reject_callback))
    return; /* Can't reject in this case. */

  if (!jsc_value_is_array (function_args) || !jsc_value_is_function (resolve_callback)) {
    g_autoptr (JSCValue) ret = jsc_value_function_call (reject_callback, G_TYPE_STRING, "Invalid Arguments", G_TYPE_NONE);
    return;
  }

  task = g_task_new (extension, NULL, (GAsyncReadyCallback)on_ephy_message_finish, NULL);
  data = ephy_message_data_new (resolve_callback, reject_callback);
  g_task_set_task_data (task, g_steal_pointer (&data), (GDestroyNotify)ephy_message_data_free);

  args_json = jsc_value_to_json (function_args, 0);
  message = webkit_user_message_new (function_name, g_variant_new_take_string (args_json));

  webkit_web_extension_send_message_to_context (extension->extension, message, NULL, (GAsyncReadyCallback)on_send_message_finish, task);
}

static void
window_object_cleared_cb (WebKitScriptWorld         *world,
                          WebKitWebPage             *page,
                          WebKitFrame               *frame,
                          EphyWebExtensionExtension *extension)
{
  g_autoptr (JSCContext) js_context = NULL;
  g_autoptr (JSCValue) js_browser = NULL;
  g_autoptr (JSCValue) js_i18n = NULL;
  g_autoptr (JSCValue) js_extension = NULL;
  g_autoptr (JSCValue) js_function = NULL;
  g_autoptr (GBytes) bytes = NULL;
  g_autoptr (JSCValue) result = NULL;
  const char *data;
  gsize data_size;

  js_context = webkit_frame_get_js_context_for_script_world (frame, world);

  js_browser = jsc_context_get_value (js_context, "browser");
  g_assert (!jsc_value_is_object (js_browser));

  js_function = jsc_value_new_function (js_context,
                                        "ephy_send_message",
                                        G_CALLBACK (ephy_send_message),
                                        extension, NULL,
                                        G_TYPE_NONE,
                                        4,
                                        G_TYPE_STRING,
                                        JSC_TYPE_VALUE,
                                        JSC_TYPE_VALUE,
                                        JSC_TYPE_VALUE);
  jsc_context_set_value (js_context, "ephy_send_message", js_function);
  g_clear_object (&js_function);

  bytes = g_resources_lookup_data ("/org/gnome/epiphany-web-extension/js/webextensions-common.js", G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
  data = g_bytes_get_data (bytes, &data_size);
  result = jsc_context_evaluate_with_source_uri (js_context, data, data_size, "resource:///org/gnome/epiphany-web-extension/js/webextensions-common.js", 1);
  g_bytes_unref (bytes);
  g_clear_object (&result);

  bytes = g_resources_lookup_data ("/org/gnome/epiphany-web-extension/js/webextensions.js", G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
  data = g_bytes_get_data (bytes, &data_size);
  result = jsc_context_evaluate_with_source_uri (js_context, data, data_size, "resource:///org/gnome/epiphany-web-extension/js/webextensions.js", 1);
  g_clear_object (&result);

  ephy_webextension_install_common_apis (js_context, extension->guid, extension->translations);
}

static void
ephy_web_extension_extension_update_translations (EphyWebExtensionExtension *extension,
                                                  const char                *data)
{
  g_autoptr (JsonParser) parser = NULL;
  JsonNode *root;
  JsonObject *root_object;
  g_autoptr (GError) error = NULL;

  g_clear_pointer (&extension->translations, json_object_unref);

  if (!data || !*data)
    return;

  parser = json_parser_new ();
  if (json_parser_load_from_data (parser, data, -1, &error)) {
    root = json_parser_get_root (parser);
    g_assert (root);
    root_object = json_node_get_object (root);
    g_assert (root_object);

    extension->translations = json_object_ref (root_object);
  } else {
    g_warning ("Could not read translation json data: %s. '%s'", error->message, data);
  }
}

void
ephy_web_extension_extension_initialize (EphyWebExtensionExtension *extension,
                                         WebKitWebExtension        *wk_extension,
                                         const char                *guid,
                                         const char                *translations)
{
  g_assert (EPHY_IS_WEB_EXTENSION_EXTENSION (extension));

  if (extension->initialized)
    return;

  g_assert (guid && *guid);

  extension->initialized = TRUE;
  extension->guid = g_strdup (guid);
  extension->extension = g_object_ref (wk_extension);

  g_signal_connect (webkit_script_world_get_default (),
                    "window-object-cleared",
                    G_CALLBACK (window_object_cleared_cb),
                    extension);

  ephy_web_extension_extension_update_translations (extension, translations);

  g_signal_connect_swapped (extension->extension, "page-created",
                            G_CALLBACK (ephy_web_extension_extension_page_created_cb),
                            extension);
}

G_MODULE_EXPORT void
webkit_web_extension_initialize_with_user_data (WebKitWebExtension *webkit_extension,
                                                GVariant           *user_data)
{
  const char *guid;
  const char *profile_dir;
  const char *webextension_translations;
  gboolean private_profile;
  gboolean should_remember_passwords;
  gboolean is_webextension;

  g_variant_get (user_data, "(&sm&sbbb&s)", &guid, &profile_dir, &should_remember_passwords, &private_profile, &is_webextension, &webextension_translations);

  if (!is_webextension)
    return;

  extension = ephy_web_extension_extension_get ();

  ephy_web_extension_extension_initialize (extension,
                                           webkit_extension,
                                           guid,
                                           webextension_translations);
}

static void __attribute__((destructor))
ephy_web_extension_shutdown (void)
{
  g_clear_object (&extension);
}
