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
#include "ephy-web-process-extension.h"

#include <locale.h>
#include <json-glib/json-glib.h>
#include <webkit2/webkit-web-extension.h>
#include <JavaScriptCore/JavaScript.h>

static char *
js_getmessage (const char *message,
               gpointer    user_data)
{
  EphyWebProcessExtension *extension = EPHY_WEB_PROCESS_EXTENSION (user_data);
  GHashTable *translations = ephy_web_process_extension_get_translations (extension);
  JsonObject *translation = NULL;
  g_autoptr (JsonObject) name = NULL;
  GList *list = NULL;

  if (!extension)
    return g_strdup (message);

  list = g_hash_table_get_values (translations);
  if (list && list->data)
    translation = list->data;

  if (!translation) {
    return g_strdup (message);
  }

  name = json_object_get_object_member (translation, message);
  if (name) {
    const char *trans = json_object_get_string_member (name, "message");
    return g_strdup (trans);
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
  return g_strdup_printf ("ephy-webextension:///%s", path);
}

void
set_up_webextensions (EphyWebProcessExtension *extension,
                      WebKitWebPage           *page,
                      JSCContext              *js_context)
{
  g_autoptr (JSCValue) js_browser = NULL;
  g_autoptr (JSCValue) js_i18n = NULL;
  g_autoptr (JSCValue) js_extension = NULL;
  g_autoptr (JSCValue) js_function = NULL;
  g_autoptr (GBytes) bytes = NULL;
  g_autoptr (JSCValue) result = NULL;
  const char *data;
  gsize data_size;

  result = jsc_context_get_value (js_context, "browser");
  g_assert (jsc_value_is_undefined (result));

  js_browser = jsc_value_new_object (js_context, NULL, NULL);
  jsc_context_set_value (js_context, "browser", js_browser);

  /* i18n */
  js_i18n = jsc_value_new_object (js_context, NULL, NULL);
  jsc_value_object_set_property (js_browser, "i18n", js_i18n);

  js_function = jsc_value_new_function (js_context,
                                        "getUILanguage",
                                        G_CALLBACK (js_getuilanguage), extension, NULL,
                                        G_TYPE_STRING,
                                        0);
  jsc_value_object_set_property (js_i18n, "getUILanguage", js_function);
  g_clear_object (&js_function);

  js_function = jsc_value_new_function (js_context,
                                        "getMessage",
                                        G_CALLBACK (js_getmessage), extension, NULL,
                                        G_TYPE_STRING, 1,
                                        G_TYPE_STRING);
  jsc_value_object_set_property (js_i18n, "getMessage", js_function);
  g_clear_object (&js_function);

  /* extension */
  js_extension = jsc_value_new_object (js_context, NULL, NULL);
  jsc_value_object_set_property (js_browser, "extension", js_extension);

  js_function = jsc_value_new_function (js_context,
                                        "getURL",
                                        G_CALLBACK (js_geturl), extension, NULL,
                                        G_TYPE_STRING,
                                        1,
                                        G_TYPE_STRING);
  jsc_value_object_set_property (js_extension, "getURL", js_function);
  g_clear_object (&js_function);

  bytes = g_resources_lookup_data ("/org/gnome/epiphany-web-process-extension/js/webextensions.js", G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
  data = g_bytes_get_data (bytes, &data_size);
  result = jsc_context_evaluate_with_source_uri (js_context, data, data_size, "resource:///org/gnome/epiphany-web-process-extension/js/webextensions.js", 1);
  g_clear_object (&result);
}

void
webextensions_add_translation (EphyWebProcessExtension *extension,
                               const char              *name,
                               const char              *data,
                               guint64                  length)
{
  GHashTable *translations = ephy_web_process_extension_get_translations (extension);
  JsonParser *parser = NULL;
  JsonNode *root;
  JsonObject *root_object;
  g_autoptr (GError) error = NULL;

  g_hash_table_remove (translations, name);

  if (!data || strlen (data) == 0)
    return;

  parser = json_parser_new ();
  if (json_parser_load_from_data (parser, data, length, &error)) {
    root = json_parser_get_root (parser);
    g_assert (root);
    root_object = json_node_get_object (root);
    g_assert (root_object);

    g_hash_table_insert (translations, (char *)name, json_object_ref (root_object));
  } else {
    g_warning ("Could not read translation json data: %s. '%s'", error->message, data);
  }
}
