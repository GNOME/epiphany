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

#include "ephy-webextension-common.h"

#include <locale.h>

typedef struct {
  WebKitWebPage *page;
  WebKitFrame *frame;
  const char *guid;
} EphySendMessageData;

typedef struct {
  JSCValue *resolve_callback;
  JSCValue *reject_callback;
} EphyCallbackData;

static void
ephy_callback_data_free (EphyCallbackData *data)
{
  g_object_unref (data->reject_callback);
  g_object_unref (data->resolve_callback);
  g_free (data);
}

static EphyCallbackData *
ephy_callback_data_new (JSCValue *resolve_callback,
                        JSCValue *reject_callback)
{
  EphyCallbackData *data = g_new (EphyCallbackData, 1);
  data->resolve_callback = g_object_ref (resolve_callback);
  data->reject_callback = g_object_ref (reject_callback);
  return data;
}

static void
on_send_message_finish (WebKitWebPage *page,
                        GAsyncResult  *result,
                        gpointer       user_data)
{
  EphyCallbackData *callback_data = user_data;
  g_autoptr (GError) error = NULL;
  g_autoptr (WebKitUserMessage) response = NULL;
  g_autoptr (JSCValue) ret = NULL;

  response = webkit_web_page_send_message_to_view_finish (page, result, &error);

  if (error) {
    ret = jsc_value_function_call (callback_data->reject_callback, G_TYPE_STRING, error->message, G_TYPE_NONE);
  } else {
    const char *json = g_variant_get_string (webkit_user_message_get_parameters (response), NULL);
    g_autoptr (JSCValue) value = NULL;
    JSCContext *context = jsc_value_get_context (callback_data->resolve_callback);

    if (strcmp (json, "") == 0) {
      value = jsc_value_new_undefined (context);
      ret = jsc_value_function_call (callback_data->resolve_callback, JSC_TYPE_VALUE, value, G_TYPE_NONE);
    } else if (strcmp (webkit_user_message_get_name (response), "error") == 0) {
      ret = jsc_value_function_call (callback_data->reject_callback, G_TYPE_STRING, json, G_TYPE_NONE);
    } else {
      value = jsc_value_new_from_json (context, json);
      ret = jsc_value_function_call (callback_data->resolve_callback, JSC_TYPE_VALUE, value, G_TYPE_NONE);
    }
  }

  ephy_callback_data_free (callback_data);
  (void)ret;
}

static void
ephy_send_message (const char *function_name,
                   JSCValue   *function_args,
                   JSCValue   *resolve_callback,
                   JSCValue   *reject_callback,
                   gpointer    user_data)
{
  EphySendMessageData *send_message_data = user_data;
  WebKitUserMessage *message;
  g_autofree char *args_json = NULL;

  if (!jsc_value_is_function (reject_callback))
    return; /* Can't reject in this case. */

  if (!jsc_value_is_array (function_args) || !jsc_value_is_function (resolve_callback)) {
    g_autoptr (JSCValue) ret = jsc_value_function_call (reject_callback, G_TYPE_STRING, "ephy_send_message(): Invalid Arguments", G_TYPE_NONE);
    (void)ret;
    return;
  }

  args_json = jsc_value_to_json (function_args, 0);
  message = webkit_user_message_new (function_name,
                                     g_variant_new ("(sts)", send_message_data->guid, webkit_frame_get_id (send_message_data->frame), args_json));

  webkit_web_page_send_message_to_view (send_message_data->page, message, NULL,
                                        (GAsyncReadyCallback)on_send_message_finish,
                                        ephy_callback_data_new (resolve_callback, reject_callback));
}

static char *
js_getmessage (const char *message,
               gpointer    user_data)
{
  JsonObject *translations = user_data;
  JsonObject *translation = NULL;

  if (!translations)
    return g_strdup (message);

  translation = json_object_get_object_member (translations, message);
  if (translation) {
    /* FIXME: Implement placeholders: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Internationalization */
    const char *translated_message = json_object_get_string_member (translation, "message");
    return g_strdup (translated_message);
  }

  return g_strdup (message);
}

static char *
js_getuilanguage (void)
{
  char *locale = setlocale (LC_MESSAGES, NULL);

  if (locale) {
    locale[2] = '\0';

    return g_strdup (locale);
  }

  return g_strdup ("en");
}

static char *
js_geturl (const char *path,
           gpointer    user_data)
{
  const char *guid = user_data;

  return g_strdup_printf ("ephy-webextension://%s/%s", guid, path[0] == '/' ? path + 1 : path);
}

static JSCValue *
js_getmanifest (gpointer user_data)
{
  JSCValue *manifest_object = user_data;
  return g_object_ref (manifest_object);
}

static void
js_exception_handler (JSCContext   *context,
                      JSCException *exception)
{
  g_autoptr (JSCValue) js_console = NULL;
  g_autoptr (JSCValue) js_result = NULL;
  g_autofree char *report = NULL;

  js_console = jsc_context_get_value (context, "console");
  js_result = jsc_value_object_invoke_method (js_console, "error", JSC_TYPE_EXCEPTION, exception, G_TYPE_NONE);
  (void)js_result;
  report = jsc_exception_report (exception);
  g_warning ("%s", report);

  jsc_context_throw_exception (context, exception);
}

void
ephy_webextension_install_common_apis (WebKitWebPage *page,
                                       WebKitFrame   *frame,
                                       JSCContext    *js_context,
                                       const char    *guid,
                                       JsonObject    *translations,
                                       const char    *manifest)
{
  g_autoptr (JSCValue) js_browser = NULL;
  g_autoptr (JSCValue) js_i18n = NULL;
  g_autoptr (JSCValue) js_extension = NULL;
  g_autoptr (JSCValue) js_function = NULL;
  g_autoptr (JSCValue) manifest_object = NULL;
  EphySendMessageData *send_message_data;

  jsc_context_push_exception_handler (js_context, (JSCExceptionHandler)js_exception_handler, NULL, NULL);

  /* APIs available in content scripts: https://developer.chrome.com/docs/extensions/mv3/content_scripts/ */

  js_browser = jsc_context_get_value (js_context, "browser");
  g_assert (jsc_value_is_object (js_browser));

  /* i18n */
  js_i18n = jsc_value_new_object (js_context, NULL, NULL);
  jsc_value_object_set_property (js_browser, "i18n", js_i18n);

  js_function = jsc_value_new_function (js_context,
                                        "getUILanguage",
                                        G_CALLBACK (js_getuilanguage), NULL, NULL,
                                        G_TYPE_STRING,
                                        0);
  jsc_value_object_set_property (js_i18n, "getUILanguage", js_function);
  g_clear_object (&js_function);

  js_function = jsc_value_new_function (js_context,
                                        "getMessage",
                                        G_CALLBACK (js_getmessage),
                                        translations ? json_object_ref (translations) : NULL,
                                        translations ? (GDestroyNotify)json_object_unref : NULL,
                                        G_TYPE_STRING, 1,
                                        G_TYPE_STRING);
  jsc_value_object_set_property (js_i18n, "getMessage", js_function);
  g_clear_object (&js_function);

  /* extension */
  js_extension = jsc_value_new_object (js_context, NULL, NULL);
  jsc_value_object_set_property (js_browser, "extension", js_extension);

  js_function = jsc_value_new_function (js_context,
                                        "getURL",
                                        G_CALLBACK (js_geturl), g_strdup (guid), g_free,
                                        G_TYPE_STRING,
                                        1,
                                        G_TYPE_STRING);
  jsc_value_object_set_property (js_extension, "getURL", js_function);
  g_clear_object (&js_function);

  /* Manifest */
  manifest_object = jsc_value_new_from_json (js_context, manifest);
  js_function = jsc_value_new_function (js_context,
                                        NULL,
                                        G_CALLBACK (js_getmanifest), g_object_ref (manifest_object), g_object_unref,
                                        JSC_TYPE_VALUE,
                                        0);
  jsc_value_object_set_property (js_extension, "getManifest", js_function);
  g_clear_object (&js_function);

  /* global functions */
  send_message_data = g_new (EphySendMessageData, 1);
  send_message_data->page = page;
  send_message_data->frame = frame;
  send_message_data->guid = guid;
  js_function = jsc_value_new_function (js_context,
                                        NULL,
                                        G_CALLBACK (ephy_send_message),
                                        send_message_data, g_free,
                                        G_TYPE_NONE,
                                        4,
                                        G_TYPE_STRING,
                                        JSC_TYPE_VALUE,
                                        JSC_TYPE_VALUE,
                                        JSC_TYPE_VALUE);
  jsc_context_set_value (js_context, "ephy_send_message", js_function);
  g_clear_object (&js_function);
}
