/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2010 Igalia S.L.
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#include "ephy-settings.h"

#include "ephy-debug.h"

#include <glib.h>
#include <gio/gio.h>

static GHashTable *settings = NULL;

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

  if (settings == NULL) {
    settings = g_hash_table_new_full (g_str_hash,
                                      g_str_equal, g_free,
                                      g_object_unref);
  }

  gsettings = g_hash_table_lookup (settings, schema);

  if (gsettings == NULL) {
    gsettings = g_settings_new (schema);

    if (gsettings == NULL)
      g_warning ("Invalid schema requested");
    else
      g_hash_table_insert (settings, g_strdup (schema), gsettings);
  }

  return gsettings;
}
