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

struct _EphyPermissionsManager
{
  GObject parent_instance;

  GHashTable *hosts_mapping;
  GHashTable *settings_mapping;
};

G_DEFINE_TYPE (EphyPermissionsManager, ephy_permissions_manager, G_TYPE_OBJECT)

static void
ephy_permissions_manager_init (EphyPermissionsManager *manager)
{
  manager->hosts_mapping = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);
  manager->settings_mapping = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
}

static void
ephy_permissions_manager_dispose (GObject *object)
{
  EphyPermissionsManager *manager = EPHY_PERMISSIONS_MANAGER (object);

  g_clear_pointer (&manager->hosts_mapping, g_hash_table_destroy);
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
ephy_permissions_manager_get_settings_for_address (EphyPermissionsManager *manager,
                                                   const char             *address)
{
  char *host = ephy_string_get_host_name (address);
  char *key_file = NULL;
  char *host_path = NULL;
  GSettingsBackend* backend = NULL;
  GSettings *settings;

  g_assert (host != NULL);

  settings = g_hash_table_lookup (manager->hosts_mapping, host);
  if (settings) {
    g_free (host);
    return settings;
  }

  key_file = g_strdup_printf ("%s" G_DIR_SEPARATOR_S "hosts.ini", ephy_dot_dir ());
  backend = g_keyfile_settings_backend_new (key_file, "/", "Hosts");
  g_free (key_file);

  host_path = g_strdup_printf ("/org/gnome/epiphany/hosts/%s/", host);

  settings = g_settings_new_with_backend_and_path ("org.gnome.Epiphany.host", backend, host_path);

  g_free (host_path);
  g_object_unref (backend);

  g_hash_table_insert (manager->hosts_mapping, host, settings);
  g_hash_table_insert (manager->settings_mapping, settings, host);

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
ephy_permissions_manager_get_permission_for_address (EphyPermissionsManager *manager,
                                                     EphyPermissionType      type,
                                                     const char             *address)
{
  GSettings *settings = ephy_permissions_manager_get_settings_for_address (manager, address);
  return g_settings_get_enum (settings, permission_type_to_string (type));
}

void
ephy_permissions_manager_set_permission_for_address (EphyPermissionsManager *manager,
                                                     EphyPermissionType      type,
                                                     const char             *address,
                                                     EphyPermission          permission)
{
  GSettings *settings = ephy_permissions_manager_get_settings_for_address (manager, address);
  g_settings_set_enum (settings, permission_type_to_string (type), permission);
}
