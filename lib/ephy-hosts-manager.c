/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2015 Gustavo Noronha Silva <gns@gnome.org>
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
#include "ephy-hosts-manager.h"

#include "ephy-file-helpers.h"
#include "ephy-string.h"

#define G_SETTINGS_ENABLE_BACKEND 1
#include <gio/gsettingsbackend.h>

struct _EphyHostsManager
{
  GObject parent_instance;

  GHashTable *hosts_mapping;
  GHashTable *settings_mapping;
};

G_DEFINE_TYPE (EphyHostsManager, ephy_hosts_manager, G_TYPE_OBJECT)

static void
ephy_hosts_manager_init (EphyHostsManager *manager)
{
  manager->hosts_mapping = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);
  manager->settings_mapping = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
}

static void
ephy_hosts_manager_dispose (GObject *object)
{
  EphyHostsManager *manager = EPHY_HOSTS_MANAGER (object);

  g_clear_pointer (&manager->hosts_mapping, g_hash_table_destroy);
  g_clear_pointer (&manager->settings_mapping, g_hash_table_destroy);

  G_OBJECT_CLASS (ephy_hosts_manager_parent_class)->dispose (object);
}

static void
ephy_hosts_manager_class_init (EphyHostsManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ephy_hosts_manager_dispose;

  /**
   * EphyHostsManager::setting-changed:
   * @host_manager: the #EphyHostsManager that received the signal
   * @host: the hostname for which the setting changed
   * @key: the name of the key that changed
   *
   * The ::setting-changed signal is emitted when the a setting changes for
   * one of the hosts managed by the manager. It can be used to represent the
   * change on the UI for instance.
   **/
  g_signal_new ("setting-changed",
                EPHY_TYPE_HOSTS_MANAGER,
                G_SIGNAL_RUN_FIRST,
                0, NULL, NULL, NULL,
                G_TYPE_NONE,
                2,
                G_TYPE_STRING,
                G_TYPE_STRING);
}

static void
setting_changed_cb (GSettings        *settings,
                    char             *key,
                    EphyHostsManager *manager)
{
  const char *host = g_hash_table_lookup (manager->settings_mapping, settings);
  g_signal_emit_by_name (manager, "setting-changed", host, key);
}

static GSettings *
ephy_hosts_manager_get_settings_for_address (EphyHostsManager *manager,
                                             const char       *address)
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

  g_signal_connect (settings, "changed",
                    G_CALLBACK (setting_changed_cb), manager);

  return settings;
}

EphyHostsManager *
ephy_hosts_manager_new (void)
{
  return EPHY_HOSTS_MANAGER (g_object_new (EPHY_TYPE_HOSTS_MANAGER, NULL));
}

static EphyHostPermission
ephy_hosts_manager_get_notifications_permission_for_address (EphyHostsManager *manager,
                                                             const char       *address)
{
  GSettings *settings = ephy_hosts_manager_get_settings_for_address (manager, address);
  return g_settings_get_enum (settings, "notifications-permission");
}

static void
ephy_hosts_manager_set_notifications_permission_for_address (EphyHostsManager   *manager,
                                                             const char         *address,
                                                             EphyHostPermission  permission)
{
  GSettings *settings = ephy_hosts_manager_get_settings_for_address (manager, address);
  g_settings_set_enum (settings, "notifications-permission", permission);
}

static EphyHostPermission
ephy_hosts_manager_get_save_password_permission_for_address (EphyHostsManager *manager,
                                                             const char       *address)
{
  GSettings *settings = ephy_hosts_manager_get_settings_for_address (manager, address);
  return g_settings_get_enum (settings, "save-password-permission");
}

static void
ephy_hosts_manager_set_save_password_permission_for_address (EphyHostsManager   *manager,
                                                             const char         *address,
                                                             EphyHostPermission  permission)
{
  GSettings *settings = ephy_hosts_manager_get_settings_for_address (manager, address);
  g_settings_set_enum (settings, "save-password-permission", permission);
}

EphyHostPermission
ephy_hosts_manager_get_permission_for_address (EphyHostsManager       *manager,
                                               EphyHostPermissionType  type,
                                               const char             *address)
{
  switch (type) {
  case EPHY_HOST_PERMISSION_TYPE_SHOW_NOTIFICATIONS:
    return ephy_hosts_manager_get_notifications_permission_for_address (manager, address);
  case EPHY_HOST_PERMISSION_TYPE_SAVE_PASSWORD:
    return ephy_hosts_manager_get_save_password_permission_for_address (manager, address);
  default:
    g_assert_not_reached ();
  }
}

void
ephy_hosts_manager_set_permission_for_address (EphyHostsManager       *manager,
                                               EphyHostPermissionType  type,
                                               const char             *address,
                                               EphyHostPermission      permission)
{
  switch (type) {
  case EPHY_HOST_PERMISSION_TYPE_SHOW_NOTIFICATIONS:
    ephy_hosts_manager_set_notifications_permission_for_address (manager, address, permission);
    break;
  case EPHY_HOST_PERMISSION_TYPE_SAVE_PASSWORD:
    ephy_hosts_manager_set_save_password_permission_for_address (manager, address, permission);
    break;
  default:
    g_assert_not_reached ();
  }
}
