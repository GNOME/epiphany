/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include <time.h>

#include "ephy-embed-utils.h"
#include "ephy-json-utils.h"
#include "ephy-shell.h"
#include "ephy-window.h"

#include "storage.h"

typedef struct {
  EphyWebExtension *web_extension; /* Parent object */
  char *name;
  guint repeat_interval_ms;
  double scheduled_time;
  double repeat_interval_minutes;
  guint timeout_id;
} Alarm;

static void
alarm_destroy (Alarm *alarm)
{
  g_clear_handle_id (&alarm->timeout_id, g_source_remove);
  g_clear_pointer (&alarm->name, g_free);
  g_free (alarm);
}

static GHashTable *
get_alarms (EphyWebExtension *extension)
{
  GHashTable *alarms = g_object_get_data (G_OBJECT (extension), "alarms");
  if (alarms)
    return alarms;

  alarms = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, (GDestroyNotify)alarm_destroy);
  g_object_set_data_full (G_OBJECT (extension), "alarms", alarms, (GDestroyNotify)g_hash_table_destroy);
  return alarms;
}

static guint64
time_now_ms (void)
{
  struct timespec spec;
  clock_gettime (CLOCK_REALTIME, &spec);
  return (spec.tv_sec * 1000) + (spec.tv_nsec / 1.0e6);
}

static guint
timestamp_to_ms (double timestamp)
{
  guint64 now_ms = time_now_ms ();

  if (now_ms > timestamp)
    return 0;

  return (guint)(timestamp - now_ms);
}

static guint
minutes_to_ms (double minutes)
{
  return (guint)(minutes * 60000);
}

static JsonNode *
alarm_to_node (Alarm *alarm)
{
  JsonNode *node;
  JsonObject *obj;

  if (!alarm)
    return NULL;

  node = json_node_init_object (json_node_alloc (), json_object_new ());
  obj = json_node_get_object (node);
  json_object_set_string_member (obj, "name", alarm->name);
  json_object_set_double_member (obj, "scheduledTime", alarm->scheduled_time);
  if (alarm->repeat_interval_ms)
    json_object_set_double_member (obj, "periodInMinutes", alarm->repeat_interval_minutes);
  else
    json_object_set_null_member (obj, "periodInMinutes");

  return node;
}

static char *
alarm_to_json (Alarm *alarm)
{
  g_autoptr (JsonNode) node = alarm_to_node (alarm);
  return json_to_string (node, FALSE);
}

static void
emit_alarm (Alarm *alarm)
{
  EphyWebExtensionManager *manager = ephy_web_extension_manager_get_default ();
  g_autofree char *json = alarm_to_json (alarm);

  ephy_web_extension_manager_emit_in_extension_views (manager, alarm->web_extension, "alarms.onAlarm", json);
}

static gboolean
on_alarm_repeat (gpointer user_data)
{
  emit_alarm (user_data);

  return G_SOURCE_CONTINUE;
}

static void
on_alarm_start (gpointer user_data)
{
  GHashTable *alarms;
  Alarm *alarm = user_data;
  alarm->timeout_id = 0;

  /* Remove, but don't free, ourselves before we call the extension. */
  if (!alarm->repeat_interval_ms) {
    alarms = get_alarms (alarm->web_extension);
    g_hash_table_steal (alarms, alarm->name);
  }

  emit_alarm (alarm);

  if (alarm->repeat_interval_ms) {
    alarm->timeout_id = g_timeout_add (alarm->repeat_interval_ms, on_alarm_repeat, alarm);
    alarm->scheduled_time = (double)(time_now_ms () + alarm->repeat_interval_ms);
  } else {
    alarm_destroy (alarm);
  }
}

static void
alarms_handler_create (EphyWebExtensionSender *sender,
                       const char             *method_name,
                       JsonArray              *args,
                       GTask                  *task)
{
  const char *name;
  JsonObject *alarm_info;
  GHashTable *alarms = get_alarms (sender->extension);
  Alarm *alarm;
  double delay_in_minutes = 0.0;
  double period_in_minutes = 0.0;
  double when = 0.0;

  /* This takes two optional args, name:str, info:obj */
  name = ephy_json_array_get_string (args, 0);
  alarm_info = ephy_json_array_get_object (args, name ? 1 : 0);

  if (!name)
    name = "";

  if (alarm_info) {
    delay_in_minutes = ephy_json_object_get_double_with_default (alarm_info, "delayInMinutes", 0.0);
    period_in_minutes = ephy_json_object_get_double_with_default (alarm_info, "periodInMinutes", 0.0);
    when = ephy_json_object_get_double_with_default (alarm_info, "when", 0.0);
  }

  if (delay_in_minutes && when) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "alarms.create(): Both 'when' and 'delayInMinutes' cannot be set");
    return;
  }

  alarm = g_new0 (Alarm, 1);
  alarm->repeat_interval_ms = minutes_to_ms (period_in_minutes);
  alarm->web_extension = sender->extension;
  alarm->name = g_strdup (name);

  if (delay_in_minutes) {
    alarm->timeout_id = g_timeout_add_once (minutes_to_ms (delay_in_minutes), on_alarm_start, alarm);
    alarm->scheduled_time = (double)(time_now_ms () + minutes_to_ms (delay_in_minutes));
  } else if (when) {
    alarm->timeout_id = g_timeout_add_once (timestamp_to_ms (when), on_alarm_start, alarm);
    alarm->scheduled_time = when;
  } else {
    alarm->timeout_id = g_idle_add_once (on_alarm_start, alarm);
    alarm->scheduled_time = (double)time_now_ms ();
  }

  g_hash_table_replace (alarms, alarm->name, alarm);

  g_task_return_pointer (task, NULL, NULL);
}

static void
alarms_handler_clear (EphyWebExtensionSender *sender,
                      const char             *method_name,
                      JsonArray              *args,
                      GTask                  *task)
{
  GHashTable *alarms = get_alarms (sender->extension);
  const char *name = ephy_json_array_get_string_with_default (args, 0, "");

  if (g_hash_table_remove (alarms, name)) {
    g_task_return_pointer (task, g_strdup ("true"), g_free);
    return;
  }

  g_task_return_pointer (task, g_strdup ("false"), g_free);
}

static void
alarms_handler_clear_all (EphyWebExtensionSender *sender,
                          const char             *method_name,
                          JsonArray              *args,
                          GTask                  *task)
{
  GHashTable *alarms = get_alarms (sender->extension);

  if (g_hash_table_size (alarms) == 0) {
    g_task_return_pointer (task, g_strdup ("false"), g_free);
    return;
  }

  g_hash_table_remove_all (alarms);
  g_task_return_pointer (task, g_strdup ("true"), g_free);
}

static void
alarms_handler_get (EphyWebExtensionSender *sender,
                    const char             *method_name,
                    JsonArray              *args,
                    GTask                  *task)
{
  GHashTable *alarms = get_alarms (sender->extension);
  const char *name = ephy_json_array_get_string (args, 0);
  Alarm *alarm;

  if (!name)
    name = "";

  alarm = g_hash_table_lookup (alarms, name);
  if (!alarm) {
    g_task_return_pointer (task, NULL, NULL);
    return;
  }

  g_task_return_pointer (task, alarm_to_json (alarm), g_free);
}

static void
alarms_handler_get_all (EphyWebExtensionSender *sender,
                        const char             *method_name,
                        JsonArray              *args,
                        GTask                  *task)
{
  GHashTable *alarms = get_alarms (sender->extension);
  g_autoptr (JsonNode) node = json_node_init_array (json_node_alloc (), json_array_new ());
  JsonArray *array = json_node_get_array (node);
  GHashTableIter iter;
  Alarm *alarm;

  g_hash_table_iter_init (&iter, alarms);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&alarm))
    json_array_add_element (array, alarm_to_node (alarm));

  g_task_return_pointer (task, json_to_string (node, FALSE), g_free);
}

static EphyWebExtensionApiHandler alarms_handlers[] = {
  {"clear", alarms_handler_clear},
  {"clearAll", alarms_handler_clear_all},
  {"create", alarms_handler_create},
  {"get", alarms_handler_get},
  {"getAll", alarms_handler_get_all},
};

void
ephy_web_extension_api_alarms_handler (EphyWebExtensionSender *sender,
                                       const char             *method_name,
                                       JsonArray              *args,
                                       GTask                  *task)
{
  if (!ephy_web_extension_has_permission (sender->extension, "alarms")) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_PERMISSION_DENIED, "alarms: Permission Denied");
    return;
  }

  for (guint idx = 0; idx < G_N_ELEMENTS (alarms_handlers); idx++) {
    EphyWebExtensionApiHandler handler = alarms_handlers[idx];

    if (g_strcmp0 (handler.name, method_name) == 0) {
      handler.execute (sender, method_name, args, task);
      return;
    }
  }

  g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_NOT_IMPLEMENTED, "alarms.%s(): Not Implemented", method_name);
}
