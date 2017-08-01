/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2017 Gabriel Ivascu <ivascu.gabriel59@gmail.com>
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

#include "ephy-settings.h"

#include <webkit2/webkit2.h>

static char *
webkit_pref_get_vendor_user_agent (void)
{
  GKeyFile *branding_keyfile;
  char *vendor_user_agent = NULL;

  branding_keyfile = g_key_file_new ();

  if (g_key_file_load_from_file (branding_keyfile, PKGDATADIR "/branding.conf",
                                 G_KEY_FILE_NONE, NULL)) {
    char *vendor;
    char *vendor_sub;
    char *vendor_comment;

    vendor = g_key_file_get_string (branding_keyfile,
                                    "User Agent", "Vendor", NULL);
    vendor_sub = g_key_file_get_string (branding_keyfile,
                                        "User Agent", "VendorSub", NULL);
    vendor_comment = g_key_file_get_string (branding_keyfile,
                                            "User Agent", "VendorComment", NULL);

    if (vendor) {
      vendor_user_agent = g_strconcat (vendor,
                                       vendor_sub ? "/" : "",
                                       vendor_sub ? vendor_sub : "",
                                       vendor_comment ? " (" : "",
                                       vendor_comment ? vendor_comment : "",
                                       vendor_comment ? ")" : "",
                                       NULL);
    }

    g_free (vendor);
    g_free (vendor_sub);
    g_free (vendor_comment);
  }

  g_key_file_free (branding_keyfile);

  return vendor_user_agent;
}

const char *
ephy_user_agent_get_internal (void)
{
  WebKitSettings *settings;
  const char *webkit_user_agent;
  char *vendor_user_agent;
  static char *user_agent = NULL;

  if (user_agent)
    return user_agent;

  user_agent = g_settings_get_string (EPHY_SETTINGS_WEB,
                                      EPHY_PREFS_WEB_USER_AGENT);
  if (user_agent && user_agent[0])
    return user_agent;

  settings = webkit_settings_new ();
  webkit_user_agent = webkit_settings_get_user_agent (settings);
  vendor_user_agent = webkit_pref_get_vendor_user_agent ();

  if (vendor_user_agent) {
    user_agent = g_strdup_printf ("%s %s Epiphany/%s",
                                  webkit_user_agent,
                                  vendor_user_agent,
                                  VERSION);
  } else {
    user_agent = g_strdup_printf ("%s Epiphany/%s",
                                  webkit_user_agent,
                                  VERSION);
  }

  g_free (vendor_user_agent);
  g_object_unref (settings);

  return user_agent;
}
