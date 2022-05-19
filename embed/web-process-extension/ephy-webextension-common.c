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

static char *
js_getmessage (const char *message,
               gpointer    user_data)
{
  JsonObject *translations = user_data;
  g_autoptr (JsonObject) translation = NULL;

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

  return g_strdup_printf ("ephy-webextension://%s/%s", guid, path);
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
ephy_webextension_install_common_apis (JSCContext *js_context,
                                       const char *guid,
                                       JsonObject *translations)
{
  g_autoptr (JSCValue) result = NULL;
  g_autoptr (JSCValue) js_browser = NULL;
  g_autoptr (JSCValue) js_i18n = NULL;
  g_autoptr (JSCValue) js_extension = NULL;
  g_autoptr (JSCValue) js_function = NULL;
  g_autoptr (JSCValue) js_object = NULL;

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
}
