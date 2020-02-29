/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2017 Gabriel Ivascu <gabrielivascu@gnome.org>
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
#include "ephy-user-agent.h"

#include "ephy-file-helpers.h"
#include "ephy-settings.h"

#include <webkit2/webkit2.h>

static gboolean mobile_user_agent = FALSE;
static char *user_agent = NULL;

void
ephy_user_agent_init_sync (void)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GDBusConnection) connection = NULL;
  g_autoptr (GVariant) var = NULL;
  g_autoptr (GVariant) v = NULL;
  const char *chassis;

  connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!connection) {
    g_warning ("Could not connect to system dbus: %s", error->message);
    return;
  }

  var = g_dbus_connection_call_sync (connection,
                                     "org.freedesktop.hostname1",
                                     "/org/freedesktop/hostname1",
                                     "org.freedesktop.DBus.Properties",
                                     "Get",
                                     g_variant_new ("(ss)",
                                                    "org.freedesktop.hostname1",
                                                    "Chassis"),
                                     NULL,
                                     G_DBUS_CALL_FLAGS_NONE,
                                     -1,
                                     NULL,
                                     &error);

  if (!var) {
    g_warning ("Could not access chassis property: %s", error->message);

    return;
  }

  g_variant_get (var, "(v)", &v);

  chassis = g_variant_get_string (v, NULL);

  mobile_user_agent = g_strcmp0 (chassis, "handset") == 0;

  g_clear_pointer (&user_agent, g_free);
}

const char *
ephy_user_agent_get (void)
{
  WebKitSettings *settings;
  gboolean web_app;

  if (user_agent)
    return user_agent;

  user_agent = g_settings_get_string (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_USER_AGENT);
  if (user_agent) {
    if (user_agent[0])
      return user_agent;
    g_free (user_agent);
  }

  web_app = ephy_profile_dir_is_web_application ();

  settings = webkit_settings_new ();
  user_agent = g_strdup_printf ("%s%s%s",
                                webkit_settings_get_user_agent (settings),
                                mobile_user_agent ? " Mobile" : "",
                                web_app ? " (Web App)" : "");
  g_object_unref (settings);

  return user_agent;
}
