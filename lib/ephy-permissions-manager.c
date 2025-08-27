/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2015 Gustavo Noronha Silva <gns@gnome.org>
 *  Copyright © 2016-2017 Igalia S.L.
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
#include <webkit/webkit.h>

struct _EphyPermissionsManager {
  GObject parent_instance;

  GHashTable *origins_mapping;
  GHashTable *settings_mapping;

  GHashTable *permission_type_permitted_origins;
  GHashTable *permission_type_denied_origins;

  GSettingsBackend *backend;
};

G_DEFINE_FINAL_TYPE (EphyPermissionsManager, ephy_permissions_manager, G_TYPE_OBJECT)

#define PERMISSIONS_FILENAME "permissions.ini"

static void
ephy_permissions_manager_init (EphyPermissionsManager *manager)
{
  g_autofree char *filename = NULL;

  manager->origins_mapping = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  manager->settings_mapping = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);

  /* We cannot use a key_destroy_func here because we need to be able to update
   * the GList keys without destroying the contents of the lists. */
  manager->permission_type_permitted_origins = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, NULL);
  manager->permission_type_denied_origins = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, NULL);

  filename = g_build_filename (ephy_profile_dir (), PERMISSIONS_FILENAME, NULL);
  manager->backend = g_keyfile_settings_backend_new (filename, "/", NULL);
}

static void
free_cached_origin_list (gpointer key,
                         gpointer value,
                         gpointer user_data)
{
  g_list_free_full ((GList *)value, (GDestroyNotify)webkit_security_origin_unref);
}

static void
ephy_permissions_manager_dispose (GObject *object)
{
  EphyPermissionsManager *manager = EPHY_PERMISSIONS_MANAGER (object);

  g_clear_pointer (&manager->origins_mapping, g_hash_table_destroy);
  g_clear_pointer (&manager->settings_mapping, g_hash_table_destroy);

  if (manager->permission_type_permitted_origins) {
    g_hash_table_foreach (manager->permission_type_permitted_origins, free_cached_origin_list, NULL);
    g_hash_table_destroy (manager->permission_type_permitted_origins);
    manager->permission_type_permitted_origins = NULL;
  }

  if (manager->permission_type_denied_origins) {
    g_hash_table_foreach (manager->permission_type_denied_origins, free_cached_origin_list, NULL);
    g_hash_table_destroy (manager->permission_type_denied_origins);
    manager->permission_type_denied_origins = NULL;
  }

  g_clear_object (&manager->backend);

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
  GSettings *settings;
  WebKitSecurityOrigin *security_origin;
  char *pos;

  g_assert (origin);

  settings = g_hash_table_lookup (manager->origins_mapping, origin);
  if (settings)
    return settings;

  /* Cannot contain consecutive slashes in GSettings path... */
  security_origin = webkit_security_origin_new_for_uri (origin);
  trimmed_protocol = g_strdup (webkit_security_origin_get_protocol (security_origin));
  pos = strchr (trimmed_protocol, '/');
  if (pos)
    *pos = '\0';

  origin_path = g_strdup_printf ("/org/gnome/epiphany/permissions/%s/%s/%u/",
                                 trimmed_protocol,
                                 webkit_security_origin_get_host (security_origin),
                                 webkit_security_origin_get_port (security_origin));

  settings = g_settings_new_with_backend_and_path ("org.gnome.Epiphany.permissions", manager->backend, origin_path);
  g_free (trimmed_protocol);
  g_free (origin_path);
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
  g_assert (ephy_permission_is_stored_by_permissions_manager (type));

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
    case EPHY_PERMISSION_TYPE_SHOW_ADS:
      return "advertisement-permission";
    case EPHY_PERMISSION_TYPE_AUTOPLAY_POLICY:
      return "autoplay-permission";
    case EPHY_PERMISSION_TYPE_ACCESS_DISPLAY:
      return "display-device-permission";
    default:
      g_assert_not_reached ();
  }
}

/* Some permissions do not get remembered by the permissions manager. Generally,
 * Epiphany prompts every time such permission is requested. But
 * ACCESS_WEBCAM_AND_MICROPHONE is special: only separate ACCESS_MICROPHONE and
 * ACCESS_WEBCAM permissions get stored.
 */
gboolean
ephy_permission_is_stored_by_permissions_manager (EphyPermissionType type)
{
  switch (type) {
    case EPHY_PERMISSION_TYPE_ACCESS_WEBCAM_AND_MICROPHONE:
    /* fallthrough */
    case EPHY_PERMISSION_TYPE_WEBSITE_DATA_ACCESS:
    /* fallthrough */
    case EPHY_PERMISSION_TYPE_CLIPBOARD:
      return FALSE;
    default:
      return TRUE;
  }
}

EphyPermission
ephy_permissions_manager_get_permission (EphyPermissionsManager *manager,
                                         EphyPermissionType      type,
                                         const char             *origin)
{
  GSettings *settings;

  g_assert (ephy_permission_is_stored_by_permissions_manager (type));

  settings = ephy_permissions_manager_get_settings_for_origin (manager, origin);
  return g_settings_get_enum (settings, permission_type_to_string (type));
}

static gint
webkit_security_origin_compare (WebKitSecurityOrigin *a,
                                WebKitSecurityOrigin *b)
{
  const char *protocol_a, *protocol_b;
  const char *host_a, *host_b;

  protocol_a = webkit_security_origin_get_protocol (a);
  protocol_b = webkit_security_origin_get_protocol (b);
  host_a = webkit_security_origin_get_host (a);
  host_b = webkit_security_origin_get_host (b);

  /* Security origins objects with NULL protocol or host do not represent the
   * same origin as others with NULL protocol or host. That is, they're not
   * equal. Therefore, they cannot be ordered, and must not be passed to this
   * compare function.
   */
  g_assert (protocol_a);
  g_assert (protocol_b);
  g_assert (host_a);
  g_assert (host_b);

  return strcmp (protocol_a, protocol_b) || strcmp (host_a, host_b) ||
         webkit_security_origin_get_port (b) - webkit_security_origin_get_port (a);
}

static void
maybe_add_origin_to_permission_type_cache (GHashTable           *permissions,
                                           EphyPermissionType    type,
                                           WebKitSecurityOrigin *origin)
{
  GList *origins;
  GList *l;

  /* Add origin to the appropriate permissions cache if (a) the cache already
   * exists, and (b) it does not already contain origin. */
  origins = g_hash_table_lookup (permissions, GINT_TO_POINTER (type));
  if (origins) {
    l = g_list_find_custom (origins, origin, (GCompareFunc)webkit_security_origin_compare);
    if (!l) {
      origins = g_list_prepend (origins, webkit_security_origin_ref (origin));
      g_hash_table_replace (permissions, GINT_TO_POINTER (type), origins);
    }
  }
}

static void
maybe_remove_origin_from_permission_type_cache (GHashTable           *permissions,
                                                EphyPermissionType    type,
                                                WebKitSecurityOrigin *origin)
{
  GList *origins;
  GList *l;

  /* Remove origin from the appropriate permissions cache if (a) the cache
   * exists, and (b) it contains origin. */
  origins = g_hash_table_lookup (permissions, GINT_TO_POINTER (type));
  if (origins) {
    l = g_list_find_custom (origins, origin, (GCompareFunc)webkit_security_origin_compare);
    if (l) {
      webkit_security_origin_unref (l->data);
      origins = g_list_remove_link (origins, l);
      g_hash_table_replace (permissions, GINT_TO_POINTER (type), origins);
    }
  }
}

void
ephy_permissions_manager_set_permission (EphyPermissionsManager *manager,
                                         EphyPermissionType      type,
                                         const char             *origin,
                                         EphyPermission          permission)
{
  WebKitSecurityOrigin *webkit_origin;
  GSettings *settings;

  g_assert (ephy_permission_is_stored_by_permissions_manager (type));

  webkit_origin = webkit_security_origin_new_for_uri (origin);
  if (!webkit_origin)
    return;

  settings = ephy_permissions_manager_get_settings_for_origin (manager, origin);
  g_settings_set_enum (settings, permission_type_to_string (type), permission);

  switch (permission) {
    case EPHY_PERMISSION_UNDECIDED:
      maybe_remove_origin_from_permission_type_cache (manager->permission_type_permitted_origins, type, webkit_origin);
      maybe_remove_origin_from_permission_type_cache (manager->permission_type_denied_origins, type, webkit_origin);
      break;
    case EPHY_PERMISSION_DENY:
      maybe_remove_origin_from_permission_type_cache (manager->permission_type_permitted_origins, type, webkit_origin);
      maybe_add_origin_to_permission_type_cache (manager->permission_type_denied_origins, type, webkit_origin);
      break;
    case EPHY_PERMISSION_PERMIT:
      maybe_add_origin_to_permission_type_cache (manager->permission_type_permitted_origins, type, webkit_origin);
      maybe_remove_origin_from_permission_type_cache (manager->permission_type_denied_origins, type, webkit_origin);
      break;
    default:
      g_assert_not_reached ();
  }

  webkit_security_origin_unref (webkit_origin);
}

static WebKitSecurityOrigin *
group_name_to_security_origin (const char *group)
{
  char **tokens;
  WebKitSecurityOrigin *origin = NULL;

  /* Should be of form org/gnome/epiphany/permissions/http/example.com/0 */
  tokens = g_strsplit (group, "/", -1);
  if (g_strv_length (tokens) == 7 && tokens[4] && tokens[5] && tokens[6])
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

  if (strcmp (permission_type_to_string (type), key) == 0) {
    g_autoptr (GError) error = NULL;
    g_autofree char *value = NULL;

    value = g_key_file_get_string (file, group, key, &error);
    if (error) {
      g_warning ("Error processing %s group %s key %s: %s",
                 filename, group, key, error->message);
      return NULL;
    }

    if ((permit && strcmp (value, "'allow'") == 0) ||
        (!permit && strcmp (value, "'deny'") == 0))
      origin = group_name_to_security_origin (group);
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
  g_auto (GStrv) keys = NULL;
  gsize keys_length;
  GList *origins = NULL;
  g_autoptr (GError) error = NULL;

  keys = g_key_file_get_keys (file, group, &keys_length, &error);
  if (error) {
    g_warning ("Error processing %s group %s: %s", filename, group, error->message);
    return NULL;
  }

  for (guint i = 0; i < keys_length; i++) {
    WebKitSecurityOrigin *origin;

    origin = origin_for_keyfile_key (file, filename, group, keys[i], type, permit);
    if (origin)
      origins = g_list_prepend (origins, origin);
  }

  return origins;
}

static GList *
ephy_permissions_manager_get_matching_origins (EphyPermissionsManager *manager,
                                               EphyPermissionType      type,
                                               gboolean                permit)
{
  GKeyFile *file;
  char *filename;
  char **groups = NULL;
  gsize groups_length;
  GList *origins = NULL;
  GError *error = NULL;

  /* Return results from cache, if they exist. */
  if (permit) {
    origins = g_hash_table_lookup (manager->permission_type_permitted_origins, GINT_TO_POINTER (type));
    if (origins)
      return origins;
  } else {
    origins = g_hash_table_lookup (manager->permission_type_denied_origins, GINT_TO_POINTER (type));
    if (origins)
      return origins;
  }

  /* Not cached. Load results from GSettings keyfile. Do it manually because the
   * GSettings API is not designed to be used for enumerating settings. */
  file = g_key_file_new ();
  filename = g_build_filename (ephy_profile_dir (), PERMISSIONS_FILENAME, NULL);

  g_key_file_load_from_file (file, filename, G_KEY_FILE_NONE, &error);
  if (error) {
    if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
      g_warning ("Error processing %s: %s", filename, error->message);
    g_error_free (error);
    goto out;
  }

  groups = g_key_file_get_groups (file, &groups_length);
  for (guint i = 0; i < groups_length; i++)
    origins = g_list_concat (origins,
                             origins_for_keyfile_group (file, filename, groups[i], type, permit));

  /* Cache the results. */
  if (origins) {
    g_hash_table_insert (permit ? manager->permission_type_permitted_origins
                                : manager->permission_type_denied_origins,
                         GINT_TO_POINTER (type),
                         origins);
  }

out:
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

static EphyPermission
js_permissions_manager_get_permission (EphyPermissionsManager *manager,
                                       EphyPermissionType      type,
                                       const char             *origin)
{
  return ephy_permissions_manager_get_permission (manager, type, origin);
}

void
ephy_permissions_manager_export_to_js_context (EphyPermissionsManager *manager,
                                               JSCContext             *js_context,
                                               JSCValue               *js_namespace)
{
  JSCClass *js_class;
  JSCValue *js_enum, *js_enum_value;
  JSCValue *js_permissions_manager;

  js_class = jsc_context_register_class (js_context, "PermissionsManager", NULL, NULL, NULL);
  jsc_class_add_method (js_class,
                        "permission",
                        G_CALLBACK (js_permissions_manager_get_permission), NULL, NULL,
                        G_TYPE_INT, 2,
                        G_TYPE_INT, G_TYPE_STRING);

  js_permissions_manager = jsc_value_new_object (js_context, manager, js_class);
  jsc_value_object_set_property (js_namespace, "permissionsManager", js_permissions_manager);
  g_object_unref (js_permissions_manager);

  js_enum = jsc_value_new_object (js_context, NULL, NULL);
  js_enum_value = jsc_value_new_number (js_context, EPHY_PERMISSION_UNDECIDED);
  jsc_value_object_set_property (js_enum, "UNDECIDED", js_enum_value);
  g_object_unref (js_enum_value);
  js_enum_value = jsc_value_new_number (js_context, EPHY_PERMISSION_DENY);
  jsc_value_object_set_property (js_enum, "DENY", js_enum_value);
  g_object_unref (js_enum_value);
  js_enum_value = jsc_value_new_number (js_context, EPHY_PERMISSION_PERMIT);
  jsc_value_object_set_property (js_enum, "PERMIT", js_enum_value);
  g_object_unref (js_enum_value);
  jsc_value_object_set_property (js_namespace, "Permission", js_enum);
  g_object_unref (js_enum);

  js_enum = jsc_value_new_object (js_context, NULL, NULL);
  js_enum_value = jsc_value_new_number (js_context, EPHY_PERMISSION_TYPE_SHOW_NOTIFICATIONS);
  jsc_value_object_set_property (js_enum, "SHOW_NOTIFICATIONS", js_enum_value);
  g_object_unref (js_enum_value);
  js_enum_value = jsc_value_new_number (js_context, EPHY_PERMISSION_TYPE_SAVE_PASSWORD);
  jsc_value_object_set_property (js_enum, "SAVE_PASSWORD", js_enum_value);
  g_object_unref (js_enum_value);
  js_enum_value = jsc_value_new_number (js_context, EPHY_PERMISSION_TYPE_ACCESS_LOCATION);
  jsc_value_object_set_property (js_enum, "ACCESS_LOCATION", js_enum_value);
  g_object_unref (js_enum_value);
  js_enum_value = jsc_value_new_number (js_context, EPHY_PERMISSION_TYPE_ACCESS_MICROPHONE);
  jsc_value_object_set_property (js_enum, "ACCESS_MICROPHONE", js_enum_value);
  g_object_unref (js_enum_value);
  js_enum_value = jsc_value_new_number (js_context, EPHY_PERMISSION_TYPE_ACCESS_WEBCAM);
  jsc_value_object_set_property (js_enum, "ACCESS_WEBCAM", js_enum_value);
  g_object_unref (js_enum_value);
  js_enum_value = jsc_value_new_number (js_context, EPHY_PERMISSION_TYPE_ACCESS_DISPLAY);
  jsc_value_object_set_property (js_enum, "ACCESS_DISPLAY", js_enum_value);
  g_object_unref (js_enum_value);
  js_enum_value = jsc_value_new_number (js_context, EPHY_PERMISSION_TYPE_SHOW_ADS);
  jsc_value_object_set_property (js_enum, "SHOW_ADS", js_enum_value);
  g_object_unref (js_enum_value);
  jsc_value_object_set_property (js_namespace, "PermissionType", js_enum);
  g_object_unref (js_enum);
}
