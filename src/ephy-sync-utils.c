/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2016 Gabriel Ivascu <ivascu.gabriel59@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "ephy-sync-utils.h"

gchar *
ephy_sync_utils_build_json_string (const gchar *first_key,
                                   const gchar *first_value,
                                   ...)
{
  va_list args;
  gchar *json;
  gchar *key;
  gchar *value;
  gchar *tmp;

  json = g_strconcat ("{\"", first_key, "\": \"", first_value, "\"", NULL);
  va_start (args, first_value);

  while ((key = va_arg (args, gchar *)) != NULL) {
    value = va_arg (args, gchar *);
    tmp = json;
    json = g_strconcat (json, ", \"", key, "\": \"", value, "\"", NULL);
    g_free (tmp);
  }

  va_end (args);
  tmp = json;
  json = g_strconcat (json, "}", NULL);
  g_free (tmp);

  return json;
}

gchar *
ephy_sync_utils_create_bso_json (const gchar *id,
                                 const gchar *payload)
{
  return ephy_sync_utils_build_json_string ("id", id, "payload", payload, NULL);
}
