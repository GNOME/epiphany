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

static gdouble
get_double_property (JSCValue   *object,
                     const char *name)
{
  g_autoptr (JSCValue) value = jsc_value_object_get_property (object, name);
  if (jsc_value_is_number (value))
    return jsc_value_to_double (value);
  return 0.0;
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

static gboolean
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

  return G_SOURCE_REMOVE;
}

static char *
alarms_handler_create (EphyWebExtension  *self,
                       char              *name,
                       JSCValue          *args,
                       const char        *context_guid,
                       GError           **error)
{
  g_autoptr (JSCValue) alarm_name = NULL;
  g_autoptr (JSCValue) alarm_info = NULL;
  GHashTable *alarms = get_alarms (self);
  Alarm *alarm;
  double delay_in_minutes = 0.0;
  double period_in_minutes = 0.0;
  double when = 0.0;
  g_autofree char *name_str = NULL;

  /* This takes two optional args, name:str, info:obj */
  alarm_name = jsc_value_object_get_property_at_index (args, 0);
  if (jsc_value_is_string (alarm_name)) {
    name_str = jsc_value_to_string (alarm_name);
    alarm_info = jsc_value_object_get_property_at_index (args, 1);
  } else {
    name_str = g_strdup ("");
    alarm_info = g_steal_pointer (&alarm_name);
  }

  if (jsc_value_is_object (alarm_info)) {
    delay_in_minutes = get_double_property (alarm_info, "delayInMinutes");
    period_in_minutes = get_double_property (alarm_info, "periodInMinutes");
    when = get_double_property (alarm_info, "when");
  }

  if (delay_in_minutes && when) {
    g_set_error (error, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "alarms.create(): Both 'when' and 'delayInMinutes' cannot be set");
    return NULL;
  }

  alarm = g_new0 (Alarm, 1);
  alarm->repeat_interval_ms = minutes_to_ms (period_in_minutes);
  alarm->web_extension = self;
  alarm->name = g_steal_pointer (&name_str);

  if (delay_in_minutes) {
    alarm->timeout_id = g_timeout_add (minutes_to_ms (delay_in_minutes), on_alarm_start, alarm);
    alarm->scheduled_time = (double)(time_now_ms () + minutes_to_ms (delay_in_minutes));
  } else if (when) {
    alarm->timeout_id = g_timeout_add (timestamp_to_ms (when), on_alarm_start, alarm);
    alarm->scheduled_time = when;
  } else {
    alarm->timeout_id = g_idle_add (on_alarm_start, alarm);
    alarm->scheduled_time = (double)time_now_ms ();
  }

  g_hash_table_replace (alarms, alarm->name, alarm);

  return NULL;
}

static char *
alarms_handler_clear (EphyWebExtension  *self,
                      char              *name,
                      JSCValue          *args,
                      const char        *context_guid,
                      GError           **error)
{
  GHashTable *alarms = get_alarms (self);
  g_autoptr (JSCValue) name_value = jsc_value_object_get_property_at_index (args, 0);
  g_autofree char *name_str = NULL;

  if (!jsc_value_is_string (name_value))
    name_str = g_strdup ("");
  else
    name_str = jsc_value_to_string (name_value);

  if (g_hash_table_remove (alarms, name_str))
    return g_strdup ("true");

  return g_strdup ("false");
}

static char *
alarms_handler_clear_all (EphyWebExtension  *self,
                          char              *name,
                          JSCValue          *args,
                          const char        *context_guid,
                          GError           **error)
{
  GHashTable *alarms = get_alarms (self);

  if (g_hash_table_size (alarms) == 0)
    return g_strdup ("false");

  g_hash_table_remove_all (alarms);
  return g_strdup ("true");
}

static char *
alarms_handler_get (EphyWebExtension  *self,
                    char              *name,
                    JSCValue          *args,
                    const char        *context_guid,
                    GError           **error)
{
  GHashTable *alarms = get_alarms (self);
  g_autoptr (JSCValue) name_value = jsc_value_object_get_property_at_index (args, 0);
  g_autofree char *name_str = NULL;
  Alarm *alarm;

  if (!jsc_value_is_string (name_value))
    name_str = g_strdup ("");
  else
    name_str = jsc_value_to_string (name_value);

  alarm = g_hash_table_lookup (alarms, name_str);
  return alarm_to_json (alarm);
}

static char *
alarms_handler_get_all (EphyWebExtension  *self,
                        char              *name,
                        JSCValue          *args,
                        const char        *context_guid,
                        GError           **error)
{
  GHashTable *alarms = get_alarms (self);
  g_autoptr (JsonNode) node = json_node_init_array (json_node_alloc (), json_array_new ());
  JsonArray *array = json_node_get_array (node);
  GHashTableIter iter;
  Alarm *alarm;

  g_hash_table_iter_init (&iter, alarms);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&alarm))
    json_array_add_element (array, alarm_to_node (alarm));

  return json_to_string (node, FALSE);
}

static EphyWebExtensionSyncApiHandler alarms_handlers[] = {
  {"clear", alarms_handler_clear},
  {"clearAll", alarms_handler_clear_all},
  {"create", alarms_handler_create},
  {"get", alarms_handler_get},
  {"getAll", alarms_handler_get_all},
};

void
ephy_web_extension_api_alarms_handler (EphyWebExtension *self,
                                       char             *name,
                                       JSCValue         *args,
                                       const char       *context_guid,
                                       GTask            *task)
{
  g_autoptr (GError) error = NULL;

  if (!ephy_web_extension_has_permission (self, "alarms")) {
    g_warning ("Extension %s tried to use alarms without permission.", ephy_web_extension_get_name (self));
    error = g_error_new_literal (WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_PERMISSION_DENIED, "alarms: Permission Denied");
    g_task_return_error (task, g_steal_pointer (&error));
    return;
  }

  for (guint idx = 0; idx < G_N_ELEMENTS (alarms_handlers); idx++) {
    EphyWebExtensionSyncApiHandler handler = alarms_handlers[idx];
    char *ret;

    if (g_strcmp0 (handler.name, name) == 0) {
      ret = handler.execute (self, name, args, context_guid, &error);

      if (error)
        g_task_return_error (task, g_steal_pointer (&error));
      else
        g_task_return_pointer (task, ret, g_free);

      return;
    }
  }

  error = g_error_new (WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_NOT_IMPLEMENTED, "alarms.%s(): Not Implemented", name);
  g_task_return_error (task, g_steal_pointer (&error));
}
