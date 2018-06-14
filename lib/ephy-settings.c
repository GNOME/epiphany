/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2010 Igalia S.L.
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
#include "ephy-settings.h"

#include "ephy-debug.h"
#include "ephy-file-helpers.h"
#include "ephy-web-app-utils.h"

#include <glib.h>
#include <gio/gio.h>

static GHashTable *settings = NULL;

static void
ephy_settings_init (void)
{
  const char *profile_directory;
  const char *web_app_name;
  char *base_path;

  if (settings != NULL)
    return;

  profile_directory = ephy_dot_dir ();
  if (!profile_directory)
    g_error ("ephy-settings used before ephy_file_helpers_init");

  settings = g_hash_table_new_full (g_str_hash,
                                    g_str_equal, g_free,
                                    g_object_unref);

  web_app_name = strstr (profile_directory, EPHY_WEB_APP_PREFIX);
  if (web_app_name)
    base_path = g_build_path ("/", "/org/gnome/epiphany/web-apps/", web_app_name, NULL);
  else
    base_path = g_strdup ("/org/gnome/epiphany/");

  for (guint i = 0; i < G_N_ELEMENTS (ephy_prefs_relocatable_schemas); i++) {
    char *path;

    path = g_build_path ("/", base_path, ephy_prefs_relocatable_schemas[i].path, NULL);
    g_hash_table_insert (settings, g_strdup (ephy_prefs_relocatable_schemas[i].schema),
                         g_settings_new_with_path (ephy_prefs_relocatable_schemas[i].schema, path));
    g_free (path);
  }

  g_free (base_path);
}

void
ephy_settings_shutdown (void)
{
  if (settings != NULL) {
    g_hash_table_remove_all (settings);
    g_hash_table_unref (settings);
  }
}

GSettings *
ephy_settings_get (const char *schema)
{
  GSettings *gsettings = NULL;

  ephy_settings_init ();

  gsettings = g_hash_table_lookup (settings, schema);

  if (gsettings == NULL) {
    gsettings = g_settings_new (schema);
    if (gsettings == NULL)
      g_warning ("Invalid schema %s requested", schema);
    else
      g_hash_table_insert (settings, g_strdup (schema), gsettings);
  }

  return gsettings;
}


