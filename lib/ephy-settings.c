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

#define G_SETTINGS_ENABLE_BACKEND
#include <gio/gsettingsbackend.h>

static GHashTable *settings = NULL;

static void
ephy_settings_init (void)
{
  const char *profile_directory;
  char *base_path;

  if (settings != NULL)
    return;

  profile_directory = ephy_profile_dir ();
  if (!profile_directory)
    g_error ("ephy-settings used before ephy_file_helpers_init");

  settings = g_hash_table_new_full (g_str_hash,
                                    g_str_equal, g_free,
                                    g_object_unref);

  if (ephy_profile_dir_is_web_application ()) {
    const char *web_app_name = ephy_web_application_get_program_name_from_profile_directory (profile_directory);
    base_path = g_build_path ("/", "/org/gnome/epiphany/web-apps/", web_app_name, NULL);
  } else {
    base_path = g_strdup ("/org/gnome/epiphany/");
  }

  for (guint i = 0; i < G_N_ELEMENTS (ephy_prefs_relocatable_schemas); i++) {
    char *path;

    if (!ephy_profile_dir_is_web_application () && ephy_prefs_relocatable_schemas[i].is_webapp_only)
      continue;

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
  if (gsettings)
    return gsettings;

  if (strcmp (schema, EPHY_PREFS_WEB_APP_SCHEMA) == 0) {
    /* EPHY_PREFS_WEB_APP_SCHEMA won't be added to the settings table if the
     * ephy_profile_dir_is_web_application() is FALSE. But we can still get
     * here in EPHY_EMBED_SHELL_MODE_APPLICATION if the profile dir is broken
     * such that its .app file is missing. This includes any web apps created by
     * Epiphany 3.30 or earlier that were migrated to 3.32 before 3.32.3 before
     * the main profile migration. This generally means anybody using Epiphany
     * only for web apps has wound up with broken web apps after the migration.
     *
     * We can only crash, but it's nicer to crash here rather than crash later.
     *
     * https://gitlab.gnome.org/GNOME/epiphany/issues/713
     */
    g_error ("Epiphany is trying to access web app settings outside web app"
             " mode. Your web app may be broken. If so, you must delete it and"
             " recreate. See epiphany#713.");
  }

  /* schema must not be relocatable, or g_settings_new() will crash. */
  for (guint i = 0; i < G_N_ELEMENTS (ephy_prefs_relocatable_schemas); i++)
    g_assert (strcmp (schema, ephy_prefs_relocatable_schemas[i].schema) != 0);

  gsettings = g_settings_new (schema);
  if (gsettings == NULL)
    g_warning ("Invalid schema %s requested", schema);
  else
    g_hash_table_insert (settings, g_strdup (schema), gsettings);

  return gsettings;
}

static void
on_settings_changed (GSettings *settings,
                     char      *key,
                     gpointer   user_data)
{
  g_autoptr (GVariant) value = g_settings_get_user_value (settings, key);
  if (value != NULL)
    g_settings_set_value (user_data, key, value);
  else
    g_settings_reset (user_data, key);
}

/*
 * This is to sync a host dconf settings schema with a keyfile based schema.
 * The reason this is done is to continue supporting dconf usage on the host
 * transparently.
 */
static void
sync_settings (GSettings *original,
               GSettings *new)
{
  g_autoptr (GSettingsSchema) schema = NULL;
  g_auto (GStrv) keys = NULL;

  g_object_get (original, "settings-schema", &schema, NULL);
  keys = g_settings_schema_list_keys (schema);

  for (size_t i = 0; keys[i] != NULL; ++i) {
    const char *key = keys[i];
    g_autoptr (GVariant) value = g_settings_get_user_value (original, key);

    if (value != NULL)
      g_settings_set_value (new, key, value);
  }

  g_signal_connect_object (original, "changed", G_CALLBACK (on_settings_changed), new, 0);
}

static char *
get_relocatable_path (const char *schema)
{
  g_autofree char *base_path = NULL;

  if (ephy_profile_dir_is_web_application ()) {
    const char *web_app_name = ephy_web_application_get_program_name_from_profile_directory (ephy_profile_dir ());
    base_path = g_build_path ("/", "/org/gnome/epiphany/web-apps/", web_app_name, NULL);
  } else
    base_path = g_strdup ("/org/gnome/epiphany/");

  for (size_t i = 0; i < G_N_ELEMENTS (ephy_prefs_relocatable_schemas); i++) {
    if (g_strcmp0 (ephy_prefs_relocatable_schemas[i].schema, schema) == 0)
      return g_build_path ("/", base_path, ephy_prefs_relocatable_schemas[i].path, NULL);
  }
  return NULL;
}

/**
 * ephy_settings_get_for_web_process_extension:
 *
 * Equivalent to ephy_settings_get() except it ensures that the
 * settings backend is always keyfile based instead of using DConf.
 *
 * This is required because the WebKitGTK sandbox will not grant
 * access to DConf but we do have access to our local directories.
 *
 * It is also assumed this will only be used as read-only.
 *
 * Returns: (transfer none): #GSettings
 */
GSettings *
ephy_settings_get_for_web_process_extension (const char *schema)
{
  GSettings *gsettings = NULL;
  g_autofree char *key_name = NULL;

  ephy_settings_init ();

  key_name = g_strdup_printf ("keyfile-%s", schema);
  gsettings = g_hash_table_lookup (settings, key_name);

  if (gsettings == NULL) {
    g_autoptr (GSettingsBackend) backend = NULL;
    g_autoptr (GSettings) web_gsettings = NULL;
    g_autofree char *keyfile_path = NULL;
    g_autofree char *path = NULL;

    gsettings = ephy_settings_get (schema);
    g_assert (gsettings != NULL);

    keyfile_path = g_build_filename (ephy_config_dir (), "web-extension-settings.ini", NULL);
    backend = g_keyfile_settings_backend_new (keyfile_path, "/", "/");

    path = get_relocatable_path (schema);
    if (path != NULL)
      web_gsettings = g_settings_new_with_backend_and_path (schema, backend, path);
    else
      web_gsettings = g_settings_new_with_backend (schema, backend);

    sync_settings (gsettings, web_gsettings);
    g_hash_table_insert (settings, g_steal_pointer (&key_name), web_gsettings);

    return g_steal_pointer (&web_gsettings);
  }

  return gsettings;
}
