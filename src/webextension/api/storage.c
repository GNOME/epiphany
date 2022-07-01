/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2019-2020 Jan-Michael Brummer <jan.brummer@tabos.org>
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

#include "ephy-embed-utils.h"
#include "ephy-shell.h"
#include "ephy-window.h"

#include "storage.h"

static JsonNode *
node_from_value_property (JSCValue   *object,
                          const char *property)
{
  g_autoptr (JSCValue) property_value = jsc_value_object_get_property (object, property);
  g_autofree char *value_json = jsc_value_to_json (property_value, 0);
  JsonNode *json = json_from_string (value_json, NULL);
  g_assert (json);
  return json;
}

static GStrv
strv_from_value (JSCValue *array)
{
  g_autoptr (GStrvBuilder) builder = g_strv_builder_new ();

  for (guint i = 0;; i++) {
    g_autoptr (JSCValue) value = jsc_value_object_get_property_at_index (array, i);
    g_autofree char *str = NULL;

    if (jsc_value_is_undefined (value))
      break;

    str = jsc_value_to_string (value);
    g_strv_builder_add (builder, str);
  }

  return g_strv_builder_end (builder);
}

static void
storage_handler_local_set (EphyWebExtensionSender *sender,
                           char                   *name,
                           JSCValue               *args,
                           GTask                  *task)
{
  JsonNode *local_storage = ephy_web_extension_get_local_storage (sender->extension);
  JsonObject *local_storage_obj = json_node_get_object (local_storage);
  g_auto (GStrv) keys = NULL;
  g_autoptr (JSCValue) value = jsc_value_object_get_property_at_index (args, 0);

  if (!jsc_value_is_object (value)) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "Invalid Arguments");
    return;
  }

  keys = jsc_value_object_enumerate_properties (value);

  for (guint i = 0; keys[i]; i++)
    json_object_set_member (local_storage_obj, keys[i], node_from_value_property (value, keys[i]));

  /* FIXME: Implement storage.onChanged */
  /* FIXME: Async IO */
  ephy_web_extension_save_local_storage (sender->extension);

  g_task_return_pointer (task, NULL, NULL);
}

static void
storage_handler_local_get (EphyWebExtensionSender *sender,
                           char                   *name,
                           JSCValue               *args,
                           GTask                  *task)
{
  JsonNode *local_storage = ephy_web_extension_get_local_storage (sender->extension);
  JsonObject *local_storage_obj = json_node_get_object (local_storage);
  g_autoptr (JsonBuilder) builder = NULL;
  g_auto (GStrv) keys = NULL;
  g_autoptr (JSCValue) value = jsc_value_object_get_property_at_index (args, 0);

  if (jsc_value_is_null (value)) {
    g_task_return_pointer (task, json_to_string (local_storage, FALSE), g_free);
    return;
  }

  builder = json_builder_new ();
  json_builder_begin_object (builder);

  if (jsc_value_is_string (value)) {
    g_autofree char *key = jsc_value_to_string (value);
    JsonNode *member = json_object_get_member (local_storage_obj, key);
    if (member) {
      json_builder_set_member_name (builder, key);
      json_builder_add_value (builder, member);
    }
    goto end_get;
  }

  if (jsc_value_is_array (value)) {
    keys = strv_from_value (value);
    for (guint i = 0; keys[i]; i++) {
      const char *key = keys[i];
      JsonNode *member = json_object_get_member (local_storage_obj, key);
      if (member) {
        json_builder_set_member_name (builder, key);
        json_builder_add_value (builder, member);
      }
    }
    goto end_get;
  }

  if (jsc_value_is_object (value)) {
    keys = jsc_value_object_enumerate_properties (value);
    for (guint i = 0; keys[i]; i++) {
      const char *key = keys[i];
      JsonNode *member = json_object_get_member (local_storage_obj, key);
      json_builder_set_member_name (builder, key);
      if (!member)
        member = node_from_value_property (value, key);
      json_builder_add_value (builder, member);
    }
    goto end_get;
  }

end_get:
  json_builder_end_object (builder);
  g_task_return_pointer (task, json_to_string (json_builder_get_root (builder), FALSE), g_free);
}

static void
storage_handler_local_remove (EphyWebExtensionSender *sender,
                              char                   *name,
                              JSCValue               *args,
                              GTask                  *task)
{
  JsonNode *local_storage = ephy_web_extension_get_local_storage (sender->extension);
  JsonObject *local_storage_obj = json_node_get_object (local_storage);
  g_autoptr (JSCValue) value = jsc_value_object_get_property_at_index (args, 0);

  if (jsc_value_is_string (value)) {
    g_autofree char *key = jsc_value_to_string (value);
    json_object_remove_member (local_storage_obj, key);
    goto end_remove;
  }

  if (jsc_value_is_array (value)) {
    g_auto (GStrv) keys = strv_from_value (value);
    for (guint i = 0; keys[i]; i++) {
      json_object_remove_member (local_storage_obj, keys[i]);
    }
    goto end_remove;
  }

end_remove:
  ephy_web_extension_save_local_storage (sender->extension);
  g_task_return_pointer (task, NULL, NULL);
}

static void
storage_handler_local_clear (EphyWebExtensionSender *sender,
                             char                   *name,
                             JSCValue               *args,
                             GTask                  *task)
{
  ephy_web_extension_clear_local_storage (sender->extension);
  ephy_web_extension_save_local_storage (sender->extension);
  g_task_return_pointer (task, NULL, NULL);
}

static EphyWebExtensionAsyncApiHandler storage_handlers[] = {
  {"local.set", storage_handler_local_set},
  {"local.get", storage_handler_local_get},
  {"local.remove", storage_handler_local_remove},
  {"local.clear", storage_handler_local_clear},
};

void
ephy_web_extension_api_storage_handler (EphyWebExtensionSender *sender,
                                        char                   *name,
                                        JSCValue               *args,
                                        GTask                  *task)
{
  if (!ephy_web_extension_has_permission (sender->extension, "storage")) {
    g_warning ("Extension %s tried to use storage without permission.", ephy_web_extension_get_name (sender->extension));
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_PERMISSION_DENIED, "storage: Permission Denied");
    return;
  }

  for (guint idx = 0; idx < G_N_ELEMENTS (storage_handlers); idx++) {
    EphyWebExtensionAsyncApiHandler handler = storage_handlers[idx];

    if (g_strcmp0 (handler.name, name) == 0) {
      handler.execute (sender, name, args, task);
      return;
    }
  }

  g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_NOT_IMPLEMENTED, "storage.%s(): Not Implemented", name);
}
