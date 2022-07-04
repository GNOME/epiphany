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

static void
notifications_handler_create (EphyWebExtensionSender *sender,
                              const char             *method_name,
                              JsonArray              *args,
                              GTask                  *task)
{
  g_autofree char *id = NULL;
  g_autofree char *namespaced_id = NULL;
  JsonObject *notification_options;
  const char *title;
  const char *message;
  JsonArray *buttons;
  g_autoptr (GNotification) notification = NULL;
  const char *extension_guid = ephy_web_extension_get_guid (sender->extension);

  /* We share the same "create" and "update" function here because our
   * implementation would be the same. The only difference is we require
   * an id if its for an update. */
  id = g_strdup (ephy_json_array_get_string (args, 0));
  notification_options = ephy_json_array_get_object (args, id ? 1 : 0);

  if (!id) {
    if (strcmp (method_name, "update") == 0) {
      g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "notifications.update(): id not given");
      return;
    }
    id = g_dbus_generate_guid ();
  }

  if (!notification_options) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "notifications.%s(): notificationOptions not given", method_name);
    return;
  }

  title = ephy_json_object_get_string (notification_options, "title");
  message = ephy_json_object_get_string (notification_options, "message");

  if (!title || !message) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "notifications.%s(): title and message are required", method_name);
    return;
  }

  notification = g_notification_new (title);
  g_notification_set_body (notification, message);
  g_notification_set_default_action_and_target (notification, "app.webextension-notification", "(ssi)", extension_guid, id, -1);

  buttons = ephy_json_object_get_array (notification_options, "buttons");
  if (buttons) {
    /* Only max of 2 buttons are supported. */
    for (guint i = 0; i < 2; i++) {
      JsonObject *button = ephy_json_array_get_object (buttons, i);
      const char *button_title = button ? ephy_json_object_get_string (button, "title") : NULL;
      if (button_title)
        g_notification_add_button_with_target (notification, button_title, "app.webextension-notification", "(ssi)", extension_guid, id, i);
    }
  }

  namespaced_id = create_extension_notification_id (sender->extension, id);
  g_application_send_notification (G_APPLICATION (ephy_shell_get_default ()), namespaced_id, notification);

  g_task_return_pointer (task, g_strdup_printf ("\"%s\"", id), g_free);
}

static void
notifications_handler_clear (EphyWebExtensionSender *sender,
                             const char             *method_name,
                             JsonArray              *args,
                             GTask                  *task)
{
  const char *id = ephy_json_array_get_string (args, 0);
  g_autofree char *namespaced_id = NULL;

  if (!id) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "notifications.clear(): id not given");
    return;
  }

  namespaced_id = create_extension_notification_id (sender->extension, id);

  g_application_withdraw_notification (G_APPLICATION (ephy_shell_get_default ()), namespaced_id);

  /* We don't actually know if a notification was withdrawn but lets assume it was. */
  g_task_return_pointer (task, g_strdup ("true"), g_free);
}

static void
notifications_handler_get_all (EphyWebExtensionSender *sender,
                               const char             *method_name,
                               JsonArray              *args,
                               GTask                  *task)
{
  /* GNotification does not provide information on the state of notifications. */
  g_task_return_pointer (task, g_strdup ("[]"), g_free);
}

static EphyWebExtensionApiHandler notifications_handlers[] = {
  {"create", notifications_handler_create},
  {"clear", notifications_handler_clear},
  {"getAll", notifications_handler_get_all},
  {"update", notifications_handler_create},
};

void
ephy_web_extension_api_notifications_handler (EphyWebExtensionSender *sender,
                                              const char             *method_name,
                                              JsonArray              *args,
                                              GTask                  *task)
{
  if (!ephy_web_extension_has_permission (sender->extension, "notifications")) {
    g_warning ("Extension %s tried to use notifications without permission.", ephy_web_extension_get_name (sender->extension));
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_PERMISSION_DENIED, "Permission Denied");
    return;
  }

  for (guint idx = 0; idx < G_N_ELEMENTS (notifications_handlers); idx++) {
    EphyWebExtensionApiHandler handler = notifications_handlers[idx];

    if (g_strcmp0 (handler.name, method_name) == 0) {
      handler.execute (sender, method_name, args, task);
      return;
    }
  }

  g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_NOT_IMPLEMENTED, "Not Implemented");
}
