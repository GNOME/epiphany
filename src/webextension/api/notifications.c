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

#include "ephy-shell.h"
#include "ephy-web-extension.h"

#include "api-utils.h"
#include "notifications.h"

static char *
create_extension_notification_id (EphyWebExtension *web_extension,
                                  const char       *id)
{
  /* We need to namespace notification ids. */
  return g_strconcat (ephy_web_extension_get_guid (web_extension), ".", id, NULL);
}

static char *
notifications_handler_create (EphyWebExtensionSender  *sender,
                              char                    *name,
                              JSCValue                *args,
                              GError                 **error)
{
  g_autoptr (JSCValue) value = jsc_value_object_get_property_at_index (args, 0);
  g_autoptr (JSCValue) button_array = NULL;
  g_autofree char *id = NULL;
  g_autofree char *namespaced_id = NULL;
  g_autofree char *title = NULL;
  g_autofree char *message = NULL;
  g_autoptr (GNotification) notification = NULL;
  const char *extension_guid = ephy_web_extension_get_guid (sender->extension);

  /* We share the same "create" and "update" function here because our
   * implementation would be the same. The only difference is we require
   * an id if its for an update. */
  if (jsc_value_is_string (value)) {
    id = jsc_value_to_string (value);
    g_object_unref (value);
    value = jsc_value_object_get_property_at_index (args, 1);
  } else {
    if (strcmp (name, "update") == 0) {
      g_set_error_literal (error, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "notifications.update(): id not given");
      return NULL;
    }
    id = g_dbus_generate_guid ();
  }

  if (!jsc_value_is_object (value)) {
    g_set_error (error, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "notifications.%s(): notificationOptions not given", name);
    return NULL;
  }

  title = api_utils_get_string_property (value, "title", NULL);
  message = api_utils_get_string_property (value, "message", NULL);

  if (!title || !message) {
    g_set_error (error, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "notifications.%s(): title and message are required", name);
    return NULL;
  }

  notification = g_notification_new (title);
  g_notification_set_body (notification, message);
  g_notification_set_default_action_and_target (notification, "app.webextension-notification", "(ssi)", extension_guid, id, -1);

  button_array = jsc_value_object_get_property (value, "buttons");
  if (jsc_value_is_array (button_array)) {
    for (guint i = 0; i < 2; i++) {
      g_autoptr (JSCValue) button = jsc_value_object_get_property_at_index (button_array, i);
      g_autofree char *button_title = api_utils_get_string_property (button, "title", NULL);
      if (button_title)
        g_notification_add_button_with_target (notification, button_title, "app.webextension-notification", "(ssi)", extension_guid, id, i);
    }
  }

  namespaced_id = create_extension_notification_id (sender->extension, id);
  g_application_send_notification (G_APPLICATION (ephy_shell_get_default ()), namespaced_id, notification);

  return g_strdup_printf ("\"%s\"", id);
}

static char *
notifications_handler_clear (EphyWebExtensionSender  *sender,
                             char                    *name,
                             JSCValue                *args,
                             GError                 **error)
{
  g_autoptr (JSCValue) value = jsc_value_object_get_property_at_index (args, 0);
  g_autofree char *id = NULL;
  g_autofree char *namespaced_id = NULL;

  if (!jsc_value_is_string (value)) {
    g_set_error_literal (error, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "notifications.clear(): id not given");
    return NULL;
  }

  id = jsc_value_to_string (value);
  namespaced_id = create_extension_notification_id (sender->extension, id);

  g_application_withdraw_notification (G_APPLICATION (ephy_shell_get_default ()), namespaced_id);

  /* We don't actually know if a notification was withdrawn but lets assume it was. */
  return g_strdup ("true");
}

static char *
notifications_handler_get_all (EphyWebExtensionSender  *sender,
                               char                    *name,
                               JSCValue                *args,
                               GError                 **error)
{
  /* GNotification does not provide information on the state of notifications. */
  return g_strdup ("[]");
}

static EphyWebExtensionSyncApiHandler notifications_handlers[] = {
  {"create", notifications_handler_create},
  {"clear", notifications_handler_clear},
  {"getAll", notifications_handler_get_all},
  {"update", notifications_handler_create},
};

void
ephy_web_extension_api_notifications_handler (EphyWebExtensionSender *sender,
                                              char                   *name,
                                              JSCValue               *args,
                                              GTask                  *task)
{
  g_autoptr (GError) error = NULL;

  if (!ephy_web_extension_has_permission (sender->extension, "notifications")) {
    g_warning ("Extension %s tried to use notifications without permission.", ephy_web_extension_get_name (sender->extension));
    error = g_error_new_literal (WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_PERMISSION_DENIED, "Permission Denied");
    g_task_return_error (task, g_steal_pointer (&error));
    return;
  }

  for (guint idx = 0; idx < G_N_ELEMENTS (notifications_handlers); idx++) {
    EphyWebExtensionSyncApiHandler handler = notifications_handlers[idx];
    char *ret;

    if (g_strcmp0 (handler.name, name) == 0) {
      ret = handler.execute (sender, name, args, &error);

      if (error)
        g_task_return_error (task, g_steal_pointer (&error));
      else
        g_task_return_pointer (task, ret, g_free);

      return;
    }
  }

  g_warning ("%s(): '%s' not implemented by Epiphany!", __FUNCTION__, name);
  error = g_error_new_literal (WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_NOT_IMPLEMENTED, "Not Implemented");
  g_task_return_error (task, g_steal_pointer (&error));
}
