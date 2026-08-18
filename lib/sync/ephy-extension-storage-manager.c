/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2026 John Cardullo <john@jcarmedia.org>
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
#include "ephy-extension-storage-manager.h"

#include <gio/gio.h>

#include "ephy-file-helpers.h"
#include "ephy-sync-utils.h"
#include "ephy-synchronizable-manager.h"

struct _EphyExtensionStorageManager {
  GObject parent_instance;
};

static void ephy_synchronizable_manager_iface_init (EphySynchronizableManagerInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (EphyExtensionStorageManager, ephy_extension_storage_manager, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (EPHY_TYPE_SYNCHRONIZABLE_MANAGER,
                                                      ephy_synchronizable_manager_iface_init))

static void
ephy_extension_storage_manager_class_init (EphyExtensionStorageManagerClass *klass)
{
}

static void
ephy_extension_storage_manager_init (EphyExtensionStorageManager *self)
{
}

EphyExtensionStorageManager *
ephy_extension_storage_manager_new (void)
{
  return EPHY_EXTENSION_STORAGE_MANAGER (g_object_new (EPHY_TYPE_EXTENSION_STORAGE_MANAGER, NULL));
}

static const char *
synchronizable_manager_get_collection_name (EphySynchronizableManager *manager)
{
  return "extension-storage";
}

static GType
synchronizable_manager_get_synchronizable_type (EphySynchronizableManager *manager)
{
  return EPHY_TYPE_EXTENSION_STORAGE_RECORD;
}

static gboolean
synchronizable_manager_is_initial_sync (EphySynchronizableManager *manager)
{
  return ephy_sync_utils_get_extensions_sync_is_initial ();
}

static void
synchronizable_manager_set_is_initial_sync (EphySynchronizableManager *manager,
                                            gboolean                   is_initial)
{
  ephy_sync_utils_set_extensions_sync_is_initial (is_initial);
}

static gint64
synchronizable_manager_get_sync_time (EphySynchronizableManager *manager)
{
  return ephy_sync_utils_get_extensions_sync_time ();
}

static void
synchronizable_manager_set_sync_time (EphySynchronizableManager *manager,
                                      gint64                     sync_time)
{
  ephy_sync_utils_set_extensions_sync_time (sync_time);
}

static void
synchronizable_manager_add (EphySynchronizableManager *manager,
                            EphySynchronizable        *synchronizable)
{
}

static void
synchronizable_manager_remove (EphySynchronizableManager *manager,
                               EphySynchronizable        *synchronizable)
{
}

static void
synchronizable_manager_save (EphySynchronizableManager *manager,
                             EphySynchronizable        *synchronizable)
{
}

static void
synchronizable_manager_merge (EphySynchronizableManager              *manager,
                              gboolean                                is_initial,
                              GList                                  *remotes_deleted,
                              GList                                  *remotes_updated,
                              EphySynchronizableManagerMergeCallback  callback,
                              gpointer                                user_data)
{
  GPtrArray *to_upload;
  g_autofree char *web_extensions_dir = NULL;
  g_autoptr (GFile) dir = NULL;
  g_autoptr (GFileEnumerator) enumerator = NULL;

  /* First process incoming remote updates and save them locally. */
  for (GList *l = remotes_updated; l && l->data; l = l->next) {
    EphyExtensionStorageRecord *record = EPHY_EXTENSION_STORAGE_RECORD (l->data);
    const char *ext_id = ephy_extension_storage_record_get_extension_id (record);
    JsonNode *storage = ephy_extension_storage_record_get_storage (record);

    if (ext_id && storage) {
      g_autofree char *sync_storage_path = g_build_filename (ephy_config_dir (), "web_extensions",
                                                             ext_id, "sync_storage.json", NULL);
      g_autofree char *json = json_to_string (storage, TRUE);
      g_autoptr (GError) error = NULL;
      g_autofree char *parent_dir = g_path_get_dirname (sync_storage_path);

      g_mkdir_with_parents (parent_dir, 0700);

      if (!g_file_set_contents (sync_storage_path, json, -1, &error))
        g_warning ("Failed to write remote sync storage for %s: %s", ext_id, error->message);
    }
  }

  /* Now gather local sync_storage.json files to upload. */
  to_upload = g_ptr_array_new_with_free_func (g_object_unref);
  web_extensions_dir = g_build_filename (ephy_config_dir (), "web_extensions", NULL);
  dir = g_file_new_for_path (web_extensions_dir);

  enumerator = g_file_enumerate_children (dir,
                                          G_FILE_ATTRIBUTE_STANDARD_NAME,
                                          G_FILE_QUERY_INFO_NONE,
                                          NULL,
                                          NULL);
  if (enumerator) {
    GFileInfo *info;

    while ((info = g_file_enumerator_next_file (enumerator, NULL, NULL))) {
      const char *ext_id = g_file_info_get_name (info);
      g_autofree char *sync_storage_path = g_build_filename (web_extensions_dir, ext_id, "sync_storage.json", NULL);
      g_autofree char *contents = NULL;

      if (g_file_get_contents (sync_storage_path, &contents, NULL, NULL)) {
        g_autoptr (GError) local_error = NULL;
        JsonNode *node = json_from_string (contents, &local_error);

        if (!local_error && node) {
          EphyExtensionStorageRecord *record = ephy_extension_storage_record_new (ext_id, ext_id, node);
          g_ptr_array_add (to_upload, record);
          json_node_unref (node);
        }
      }

      g_object_unref (info);
    }
  }

  callback (to_upload, user_data);
}

static void
ephy_synchronizable_manager_iface_init (EphySynchronizableManagerInterface *iface)
{
  iface->get_collection_name = synchronizable_manager_get_collection_name;
  iface->get_synchronizable_type = synchronizable_manager_get_synchronizable_type;
  iface->is_initial_sync = synchronizable_manager_is_initial_sync;
  iface->set_is_initial_sync = synchronizable_manager_set_is_initial_sync;
  iface->get_sync_time = synchronizable_manager_get_sync_time;
  iface->set_sync_time = synchronizable_manager_set_sync_time;
  iface->add = synchronizable_manager_add;
  iface->remove = synchronizable_manager_remove;
  iface->save = synchronizable_manager_save;
  iface->merge = synchronizable_manager_merge;
}
