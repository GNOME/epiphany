/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2015 Gustavo Noronha Silva <gns@gnome.org>
 *  Copyright © 2016 Igalia S.L.
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
#include "ephy-permissions-manager.h"

#include "ephy-file-helpers.h"
#include "ephy-string.h"

#define G_SETTINGS_ENABLE_BACKEND 1
#include <gio/gsettingsbackend.h>
#include <stdlib.h>
#include <string.h>
#include <webkit2/webkit2.h>

struct _EphyPermissionsManager
{
  GObject parent_instance;

  GHashTable *origins_mapping;
  GHashTable *settings_mapping;
};

G_DEFINE_TYPE (EphyPermissionsManager, ephy_permissions_manager, G_TYPE_OBJECT)

#define PERMISSIONS_FILENAME "permissions.ini"

static void
ephy_permissions_manager_init (EphyPermissionsManager *manager)
{
  manager->origins_mapping = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  manager->settings_mapping = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
}

static void
ephy_permissions_manager_dispose (GObject *object)
{
  EphyPermissionsManager *manager = EPHY_PERMISSIONS_MANAGER (object);

  g_clear_pointer (&manager->origins_mapping, g_hash_table_destroy);
  g_clear_pointer (&manager->settings_mapping, g_hash_table_destroy);

  G_OBJECT_CLASS (ephy_permissions_manager_parent_class)->dispose (object);
}

static void
ephy_permissions_manager_class_init (EphyPermissionsManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ephy_permissions_manager_dispose;
}

static GSettings *
ephy_permissions_manager_get_settings_for_origin (EphyPermissionsManager *manager,
                                                  const char             *origin)
{
  char *origin_path;
  char *trimmed_protocol;
  char *filename;
  GSettingsBackend* backend;
  GSettings *settings;
  WebKitSecurityOrigin *security_origin;
  char *pos;

  g_assert (origin != NULL);

  settings = g_hash_table_lookup (manager->origins_mapping, origin);
  if (settings)
    return settings;

  filename = g_build_filename (ephy_dot_dir (), PERMISSIONS_FILENAME, NULL);
  backend = g_keyfile_settings_backend_new (filename, "/", NULL);
  g_free (filename);

  /* Cannot contain consecutive slashes in GSettings path... */
  security_origin = webkit_security_origin_new_for_uri (origin);
  trimmed_protocol = g_strdup (webkit_security_origin_get_protocol (security_origin));
  pos = strchr (trimmed_protocol, '/');
  if (pos != NULL)
    *pos = '\0';

  origin_path = g_strdup_printf ("/org/gnome/epiphany/permissions/%s/%s/%u/",
                                 trimmed_protocol,
                                 webkit_security_origin_get_host (security_origin),
                                 webkit_security_origin_get_port (security_origin));

  settings = g_settings_new_with_backend_and_path ("org.gnome.Epiphany.Permissions", backend, origin_path);
  g_free (trimmed_protocol);
  g_free (origin_path);
  g_object_unref (backend);
  webkit_security_origin_unref (security_origin);

  /* Note that settings is owned only by the first hash table! */
  g_hash_table_insert (manager->origins_mapping, g_strdup (origin), settings);
  g_hash_table_insert (manager->settings_mapping, settings, g_strdup (origin));

  return settings;
}

EphyPermissionsManager *
ephy_permissions_manager_new (void)
{
  return EPHY_PERMISSIONS_MANAGER (g_object_new (EPHY_TYPE_PERMISSIONS_MANAGER, NULL));
}

static const char *
permission_type_to_string (EphyPermissionType type)
{
  switch (type) {
  case EPHY_PERMISSION_TYPE_SHOW_NOTIFICATIONS:
    return "notifications-permission";
  case EPHY_PERMISSION_TYPE_SAVE_PASSWORD:
    return "save-password-permission";
  case EPHY_PERMISSION_TYPE_ACCESS_LOCATION:
    return "geolocation-permission";
  case EPHY_PERMISSION_TYPE_ACCESS_MICROPHONE:
    return "audio-device-permission";
  case EPHY_PERMISSION_TYPE_ACCESS_WEBCAM:
    return "video-device-permission";
  default:
    g_assert_not_reached ();
  }
}

EphyPermission
ephy_permissions_manager_get_permission (EphyPermissionsManager *manager,
                                         EphyPermissionType      type,
                                         const char             *origin)
{
  GSettings *settings = ephy_permissions_manager_get_settings_for_origin (manager, origin);
  return g_settings_get_enum (settings, permission_type_to_string (type));
}

void
ephy_permissions_manager_set_permission (EphyPermissionsManager *manager,
                                         EphyPermissionType      type,
                                         const char             *origin,
                                         EphyPermission          permission)
{
  GSettings *settings = ephy_permissions_manager_get_settings_for_origin (manager, origin);
  g_settings_set_enum (settings, permission_type_to_string (type), permission);
}

static WebKitSecurityOrigin *
group_name_to_security_origin (const char *group)
{
  char **tokens;
  WebKitSecurityOrigin *origin = NULL;

  /* Should be of form org/gnome/epiphany/permissions/http/example.com/0 */
  tokens = g_strsplit (group, "/", -1);
  if (g_strv_length (tokens) == 7 && tokens[4] != NULL && tokens[5] != NULL && tokens[6] != NULL)
    origin = webkit_security_origin_new (tokens[4], tokens[5], atoi (tokens[6]));

  g_strfreev (tokens);

  return origin;
}

static WebKitSecurityOrigin *
origin_for_keyfile_key (GKeyFile           *file,
                        const char         *filename,
                        const char         *group,
                        const char         *key,
                        EphyPermissionType  type,
                        gboolean            permit)
{
  WebKitSecurityOrigin *origin = NULL;
  char *value;
  GError *error = NULL;

  if (strcmp (permission_type_to_string (type), key) == 0) {
    value = g_key_file_get_string (file, group, key, &error);
    if (error != NULL) {
      g_warning ("Error processing %s group %s key %s: %s",
                 filename, group, key, error->message);
      g_error_free (error);
      return NULL;
    }

    if ((permit && strcmp (value, "'allow'") == 0) ||
        (!permit && strcmp (value, "'deny'") == 0))
      origin = group_name_to_security_origin (group);

    g_free (value);
  }

  return origin;
}

static GList *
origins_for_keyfile_group (GKeyFile           *file,
                           const char         *filename,
                           const char         *group,
                           EphyPermissionType  type,
                           gboolean            permit)
{
  char **keys;
  gsize keys_length;
  GList *origins = NULL;
  WebKitSecurityOrigin *origin;
  GError *error = NULL;

  keys = g_key_file_get_keys (file, group, &keys_length, &error);
  if (error != NULL) {
    g_warning ("Error processing %s group %s: %s", filename, group, error->message);
    g_error_free (error);
    return NULL;
  }

  for (guint i = 0; i < keys_length; i++) {
    origin = origin_for_keyfile_key (file, filename, group, keys[i], type, permit);
    if (origin)
      origins = g_list_prepend (origins, origin);
  }

  g_strfreev (keys);

  return origins;
}

/* TODO: Consider caching this in memory. This gets called twice when
 * starting each web process. It could be dozens or hundreds of file
 * reads when starting the browser. It's silly to cache individual
 * settings but not this. */
static GList *
ephy_permissions_manager_get_matching_origins (EphyPermissionsManager *manager,
                                               EphyPermissionType      type,
                                               gboolean                permit)
{
  GKeyFile *file;
  char *filename;
  char **groups;
  gsize groups_length;
  GList *origins = NULL;
  GError *error = NULL;

  file = g_key_file_new ();
  filename = g_build_filename (ephy_dot_dir (), PERMISSIONS_FILENAME, NULL);

  g_key_file_load_from_file (file, filename, G_KEY_FILE_NONE, &error);
  if (error != NULL) {
    if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
      g_warning ("Error processing %s: %s", filename, error->message);
    g_error_free (error);
    return NULL;
  }

  groups = g_key_file_get_groups (file, &groups_length);
  for (guint i = 0; i < groups_length; i++)
    origins = g_list_concat (origins,
                             origins_for_keyfile_group (file, filename, groups[i], type, permit));

  g_key_file_unref (file);
  g_strfreev (groups);
  g_free (filename);

  return origins;
}

GList *
ephy_permissions_manager_get_permitted_origins (EphyPermissionsManager *manager,
                                                EphyPermissionType      type)
{
  return ephy_permissions_manager_get_matching_origins (manager, type, TRUE);
}

GList *
ephy_permissions_manager_get_denied_origins (EphyPermissionsManager *manager,
                                             EphyPermissionType      type)
{
  return ephy_permissions_manager_get_matching_origins (manager, type, FALSE);
}
