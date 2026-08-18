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

#include "storage.h"

#include "ephy-embed-utils.h"
#include "ephy-shell.h"
#include "ephy-window.h"

static JsonNode *
get_storage_for_method (EphyWebExtension *extension,
                        const char       *method_name)
{
  if (g_str_has_prefix (method_name, "sync."))
    return ephy_web_extension_get_sync_storage (extension);

  return ephy_web_extension_get_local_storage (extension);
}

static void
save_storage_for_method (EphyWebExtension *extension,
                         const char       *method_name)
{
  if (g_str_has_prefix (method_name, "sync."))
    ephy_web_extension_save_sync_storage (extension);
  else
    ephy_web_extension_save_local_storage (extension);
}

static void
clear_storage_for_method (EphyWebExtension *extension,
                          const char       *method_name)
{
  if (g_str_has_prefix (method_name, "sync."))
    ephy_web_extension_clear_sync_storage (extension);
  else
    ephy_web_extension_clear_local_storage (extension);
}

static void
storage_handler_set (EphyWebExtensionSender *sender,
                     const char             *method_name,
                     JsonArray              *args,
                     GTask                  *task)
{
  JsonNode *storage = get_storage_for_method (sender->extension, method_name);
  JsonObject *storage_obj = json_node_get_object (storage);
  JsonObject *keys = ephy_json_array_get_object (args, 0);

  if (!keys) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "storage.%s(): Missing keys", method_name);
    return;
  }

  for (GList *l = json_object_get_members (keys); l; l = g_list_next (l)) {
    const char *member_name = l->data;
    json_object_set_member (storage_obj, member_name, json_node_ref (json_object_get_member (keys, member_name)));
  }

  /* FIXME: Implement storage.onChanged */
  /* FIXME: Async IO */
  save_storage_for_method (sender->extension, method_name);

  g_task_return_pointer (task, NULL, NULL);
}

static void
storage_handler_get (EphyWebExtensionSender *sender,
                     const char             *method_name,
                     JsonArray              *args,
                     GTask                  *task)
{
  JsonNode *storage = get_storage_for_method (sender->extension, method_name);
  JsonObject *storage_obj = json_node_get_object (storage);
  JsonNode *node = ephy_json_array_get_element (args, 0);
  g_autoptr (JsonBuilder) builder = NULL;

  if (!node) {
    g_task_return_pointer (task, json_to_string (storage, FALSE), g_free);
    return;
  }

  builder = json_builder_new ();
  json_builder_begin_object (builder);

  if (ephy_json_node_to_string (node)) {
    const char *key = ephy_json_node_to_string (node);
    JsonNode *member = json_object_get_member (storage_obj, key);
    if (member) {
      json_builder_set_member_name (builder, key);
      json_builder_add_value (builder, json_node_ref (member));
    }
    goto end_get;
  }

  if (JSON_NODE_HOLDS_ARRAY (node)) {
    JsonArray *array = json_node_get_array (node);
    for (guint i = 0; i < json_array_get_length (array); i++) {
      const char *key = ephy_json_array_get_string (array, i);
      JsonNode *member;

      if (!key)
        continue;

      member = json_object_get_member (storage_obj, key);
      if (member) {
        json_builder_set_member_name (builder, key);
        json_builder_add_value (builder, json_node_ref (member));
      }
    }
    goto end_get;
  }

  if (JSON_NODE_HOLDS_OBJECT (node)) {
    JsonObject *object = json_node_get_object (node);
    GList *members = json_object_get_members (object);
    for (GList *l = members; l; l = g_list_next (l)) {
      const char *member_name = l->data;
      JsonNode *member;

      /* Either we find the member or we return the value we were given, unless it was undefined. */
      if (!json_object_has_member (storage_obj, member_name))
        member = json_object_get_member (object, member_name);
      else
        member = json_object_get_member (storage_obj, member_name);

      if (member) {
        json_builder_set_member_name (builder, member_name);
        json_builder_add_value (builder, json_node_ref (member));
      }
    }
  }

end_get:
  json_builder_end_object (builder);
  g_task_return_pointer (task, json_to_string (json_builder_get_root (builder), FALSE), g_free);
}

static void
storage_handler_remove (EphyWebExtensionSender *sender,
                        const char             *method_name,
                        JsonArray              *args,
                        GTask                  *task)
{
  JsonNode *storage = get_storage_for_method (sender->extension, method_name);
  JsonObject *storage_obj = json_node_get_object (storage);
  JsonNode *node = ephy_json_array_get_element (args, 0);
  const char *string_value;

  if (!node)
    goto end_remove;

  if (JSON_NODE_HOLDS_ARRAY (node)) {
    JsonArray *array = json_node_get_array (node);
    for (guint i = 0; i < json_array_get_length (array); i++) {
      const char *name = ephy_json_array_get_string (array, i);
      if (name)
        json_object_remove_member (storage_obj, name);
    }
    goto end_remove;
  }

  string_value = ephy_json_node_to_string (node);
  if (string_value) {
    json_object_remove_member (storage_obj, string_value);
    goto end_remove;
  }

end_remove:
  save_storage_for_method (sender->extension, method_name);
  g_task_return_pointer (task, NULL, NULL);
}

static void
storage_handler_clear (EphyWebExtensionSender *sender,
                       const char             *method_name,
                       JsonArray              *args,
                       GTask                  *task)
{
  clear_storage_for_method (sender->extension, method_name);
  save_storage_for_method (sender->extension, method_name);
  g_task_return_pointer (task, NULL, NULL);
}

static EphyWebExtensionApiHandler storage_handlers[] = {
  {"local.set", storage_handler_set},
  {"local.get", storage_handler_get},
  {"local.remove", storage_handler_remove},
  {"local.clear", storage_handler_clear},
  {"sync.set", storage_handler_set},
  {"sync.get", storage_handler_get},
  {"sync.remove", storage_handler_remove},
  {"sync.clear", storage_handler_clear},
};

void
ephy_web_extension_api_storage_handler (EphyWebExtensionSender *sender,
                                        const char             *method_name,
                                        JsonArray              *args,
                                        GTask                  *task)
{
  if (!ephy_web_extension_has_permission (sender->extension, "storage")) {
    g_warning ("Extension %s tried to use storage without permission.", ephy_web_extension_get_name (sender->extension));
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_PERMISSION_DENIED, "storage: Permission Denied");
    return;
  }

  for (guint idx = 0; idx < G_N_ELEMENTS (storage_handlers); idx++) {
    EphyWebExtensionApiHandler handler = storage_handlers[idx];

    if (g_strcmp0 (handler.name, method_name) == 0) {
      handler.execute (sender, method_name, args, task);
      return;
    }
  }

  g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_NOT_IMPLEMENTED, "storage.%s(): Not Implemented", method_name);
}
