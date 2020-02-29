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

#define CHASSIS_TYPE_PORTABLE   8
#define CHASSIS_TYPE_DETACHABLE 28
#define CHASSIS_TYPE_TABLET     32

const char *
ephy_user_agent_get (void)
{
  static char *user_agent = NULL;
  g_autoptr (GFile) sysfs_file = NULL;
  WebKitSettings *settings;
  gboolean mobile;
  gboolean web_app;

  if (user_agent)
    return user_agent;

  user_agent = g_settings_get_string (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_USER_AGENT);
  if (user_agent) {
    if (user_agent[0])
      return user_agent;
    g_free (user_agent);
  }

  sysfs_file = g_file_new_for_path ("/sys/devices/virtual/dmi/id/chassis_type");
  if (g_file_query_exists (sysfs_file, NULL)) {
    g_autoptr (GFileInfo) file_info = NULL;
    g_autofree char *data = NULL;
    g_autoptr (GFileInputStream) input_stream = NULL;
    goffset file_size;

    file_info = g_file_query_info (sysfs_file, G_FILE_ATTRIBUTE_STANDARD_SIZE, G_FILE_QUERY_INFO_NONE, NULL, NULL);
    file_size = g_file_info_get_size (file_info);

    if (file_size != 0) {
      int type;

      data = g_malloc0 (file_size + 1);
      input_stream = g_file_read (sysfs_file, NULL, NULL);

      g_input_stream_read_all (G_INPUT_STREAM (input_stream), data, file_size, NULL, NULL, NULL);

      type = atoi (data);

      if (type == CHASSIS_TYPE_TABLET || type == CHASSIS_TYPE_DETACHABLE || type == CHASSIS_TYPE_PORTABLE)
        mobile = TRUE;
    }
  } else {
    mobile = g_settings_get_boolean (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_MOBILE_USER_AGENT);
  }

  web_app = ephy_profile_dir_is_web_application ();

  settings = webkit_settings_new ();
  user_agent = g_strdup_printf ("%s%s Epiphany/605.1.15%s",
                                webkit_settings_get_user_agent (settings),
                                mobile ? " Mobile" : "",
                                web_app ? " (Web App)" : "");
  g_object_unref (settings);

  return user_agent;
}
