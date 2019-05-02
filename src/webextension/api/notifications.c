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

#include "ephy-notification.h"
#include "ephy-web-extension.h"

#include "notifications.h"

static char *
notifications_handler_create (EphyWebExtension *self,
                              char             *name,
                              JSCValue         *args)
{
  g_autofree char *title_str = NULL;
  g_autofree char *message_str = NULL;
  g_autoptr (JSCValue) title = NULL;
  g_autoptr (JSCValue) message = NULL;
  EphyNotification *notify;

  title = jsc_value_object_get_property (args, "title");
  title_str = jsc_value_to_string (title);

  message = jsc_value_object_get_property (args, "message");
  message_str = jsc_value_to_string (message);

  notify = ephy_notification_new (g_strdup (title_str), g_strdup (message_str));
  ephy_notification_show (notify);

  return NULL;
}

static EphyWebExtensionApiHandler notifications_handlers[] = {
  {"create", notifications_handler_create},
  {NULL, NULL},
};

char *
ephy_web_extension_api_notifications_handler (EphyWebExtension *self,
                                              char             *name,
                                              JSCValue         *args)
{
  guint idx;

  for (idx = 0; idx < G_N_ELEMENTS (notifications_handlers); idx++) {
    EphyWebExtensionApiHandler handler = notifications_handlers[idx];

    if (g_strcmp0 (handler.name, name) == 0)
      return handler.execute (self, name, args);
  }

  g_warning ("%s(): '%s' not implemented by Epiphany!", __FUNCTION__, name);

  return NULL;
}
