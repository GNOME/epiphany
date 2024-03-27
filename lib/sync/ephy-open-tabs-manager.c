/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2017 Gabriel Ivascu <gabrielivascu@gnome.org>
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
#include "ephy-open-tabs-manager.h"

#include "ephy-settings.h"
#include "ephy-sync-utils.h"
#include "ephy-synchronizable-manager.h"

struct _EphyOpenTabsManager {
  GObject parent_instance;

  EphyTabsCatalog *catalog; /* unowned,
                             * because the EphyTabsCatalog is the EphyShell, which owns us. */

  /* A list of EphyOpenTabsRecord objects describing the open tabs
   * of other sync clients. This is updated at every sync. */
  GList *remote_records;
};

static void ephy_synchronizable_manager_iface_init (EphySynchronizableManagerInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (EphyOpenTabsManager, ephy_open_tabs_manager, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (EPHY_TYPE_SYNCHRONIZABLE_MANAGER,
                                                      ephy_synchronizable_manager_iface_init))

enum {
  PROP_0,
  PROP_TABS_CATALOG,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

static void
ephy_open_tabs_manager_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  EphyOpenTabsManager *self = EPHY_OPEN_TABS_MANAGER (object);

  switch (prop_id) {
    case PROP_TABS_CATALOG:
      self->catalog = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_open_tabs_manager_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  EphyOpenTabsManager *self = EPHY_OPEN_TABS_MANAGER (object);

  switch (prop_id) {
    case PROP_TABS_CATALOG:
      g_value_set_object (value, self->catalog);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_open_tabs_manager_finalize (GObject *object)
{
  EphyOpenTabsManager *self = EPHY_OPEN_TABS_MANAGER (object);

  g_list_free_full (self->remote_records, g_object_unref);

  G_OBJECT_CLASS (ephy_open_tabs_manager_parent_class)->finalize (object);
}

static void
ephy_open_tabs_manager_class_init (EphyOpenTabsManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = ephy_open_tabs_manager_set_property;
  object_class->get_property = ephy_open_tabs_manager_get_property;
  object_class->finalize = ephy_open_tabs_manager_finalize;

  obj_properties[PROP_TABS_CATALOG] =
    g_param_spec_object ("tabs-catalog",
                         NULL, NULL,
                         EPHY_TYPE_TABS_CATALOG,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);
}

static void
ephy_open_tabs_manager_init (EphyOpenTabsManager *self)
{
}

EphyOpenTabsManager *
ephy_open_tabs_manager_new (EphyTabsCatalog *catalog)
{
  return EPHY_OPEN_TABS_MANAGER (g_object_new (EPHY_TYPE_OPEN_TABS_MANAGER,
                                               "tabs-catalog", catalog,
                                               NULL));
}

EphyOpenTabsRecord *
ephy_open_tabs_manager_get_local_tabs (EphyOpenTabsManager *self)
{
  EphyOpenTabsRecord *local_tabs;
  EphyTabInfo *info;
  GList *tabs_info;
  char *device_bso_id;
  char *device_name;

  g_assert (EPHY_IS_OPEN_TABS_MANAGER (self));

  device_bso_id = ephy_sync_utils_get_device_bso_id ();
  device_name = ephy_sync_utils_get_device_name ();

  local_tabs = ephy_open_tabs_record_new (device_bso_id, device_name);
  tabs_info = ephy_tabs_catalog_get_tabs_info (self->catalog);

  for (GList *l = tabs_info; l && l->data; l = l->next) {
    info = (EphyTabInfo *)l->data;
    ephy_open_tabs_record_add_tab (local_tabs, info->title, info->url, info->favicon);
  }

  g_free (device_bso_id);
  g_free (device_name);
  g_list_free_full (tabs_info, (GDestroyNotify)ephy_tab_info_free);

  return local_tabs;
}

GList *
ephy_open_tabs_manager_get_remote_tabs (EphyOpenTabsManager *self)
{
  g_assert (EPHY_IS_OPEN_TABS_MANAGER (self));

  return self->remote_records;
}

void
ephy_open_tabs_manager_clear_cache (EphyOpenTabsManager *self)
{
  g_assert (EPHY_IS_OPEN_TABS_MANAGER (self));

  g_list_free_full (self->remote_records, g_object_unref);
  self->remote_records = NULL;
}

const char *
synchronizable_manager_get_collection_name (EphySynchronizableManager *manager)
{
  return ephy_sync_utils_sync_with_firefox () ? "tabs" : "ephy-tabs";
}

static GType
synchronizable_manager_get_synchronizable_type (EphySynchronizableManager *manager)
{
  return EPHY_TYPE_OPEN_TABS_RECORD;
}

static gboolean
synchronizable_manager_is_initial_sync (EphySynchronizableManager *manager)
{
  /* Initial sync will always be true.
   * We always want all records when syncing open tabs.
   */
  return TRUE;
}

static void
synchronizable_manager_set_is_initial_sync (EphySynchronizableManager *manager,
                                            gboolean                   is_initial)
{
  /* Initial sync will always be true. */
}

static gint64
synchronizable_manager_get_sync_time (EphySynchronizableManager *manager)
{
  return ephy_sync_utils_get_open_tabs_sync_time ();
}

static void
synchronizable_manager_set_sync_time (EphySynchronizableManager *manager,
                                      gint64                     sync_time)
{
  ephy_sync_utils_set_open_tabs_sync_time (sync_time);
}

static void
synchronizable_manager_add (EphySynchronizableManager *manager,
                            EphySynchronizable        *synchronizable)
{
  /* Every sync of open tabs is an initial sync so we don't need this. */
}

static void
synchronizable_manager_remove (EphySynchronizableManager *manager,
                               EphySynchronizable        *synchronizable)
{
  /* Every sync of open tabs is an initial sync so we don't need this. */
}

static void
synchronizable_manager_save (EphySynchronizableManager *manager,
                             EphySynchronizable        *synchronizable)
{
  /* No implementation.
   * We don't care about the server time modified of open tabs records.
   */
}

static void
synchronizable_manager_merge (EphySynchronizableManager              *manager,
                              gboolean                                is_initial,
                              GList                                  *remotes_deleted,
                              GList                                  *remotes_updated,
                              EphySynchronizableManagerMergeCallback  callback,
                              gpointer                                user_data)
{
  EphyOpenTabsManager *self = EPHY_OPEN_TABS_MANAGER (manager);
  GPtrArray *to_upload;
  char *device_bso_id;

  device_bso_id = ephy_sync_utils_get_device_bso_id ();
  g_list_free_full (self->remote_records, g_object_unref);
  self->remote_records = NULL;

  for (GList *l = remotes_updated; l && l->data; l = l->next) {
    /* Exclude the record which describes the local open tabs. */
    if (!g_strcmp0 (device_bso_id, ephy_open_tabs_record_get_id (l->data)))
      continue;

    self->remote_records = g_list_prepend (self->remote_records, g_object_ref (l->data));
  }

  /* Only upload the local open tabs, we don't want to alter open tabs of
   * other clients. Also, overwrite any previous value by doing a force upload.
   */
  to_upload = g_ptr_array_new_with_free_func (g_object_unref);
  g_ptr_array_add (to_upload, ephy_open_tabs_manager_get_local_tabs (self));

  g_free (device_bso_id);

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
