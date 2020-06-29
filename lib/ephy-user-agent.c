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

const char *
ephy_user_agent_get (void)
{
  static char *user_agent = NULL;
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
  user_agent = g_strdup_printf ("%s%s",
                                webkit_settings_get_user_agent (settings),
                                web_app ? " (Web App)" : "");
  g_object_unref (settings);

  return user_agent;
}
