/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2017 Gabriel Ivascu <ivascu.gabriel59@gmail.com>
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

#include "ephy-embed-container.h"
#include "ephy-embed-shell.h"
#include "ephy-settings.h"
#include "ephy-sync-utils.h"
#include "ephy-synchronizable-manager.h"

struct _EphyOpenTabsManager {
  GObject parent_instance;

  /* A list of EphyOpenTabsRecord objects describing the open tabs
   * of other sync clients. This is updated at every sync. */
  GSList *remote_records;
};

static void ephy_synchronizable_manager_iface_init (EphySynchronizableManagerInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EphyOpenTabsManager, ephy_open_tabs_manager, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_SYNCHRONIZABLE_MANAGER,
                                                ephy_synchronizable_manager_iface_init))

static void
ephy_open_tabs_manager_dispose (GObject *object)
{
  EphyOpenTabsManager *self = EPHY_OPEN_TABS_MANAGER (object);

  g_slist_free_full (self->remote_records, g_object_unref);
  self->remote_records = NULL;

  G_OBJECT_CLASS (ephy_open_tabs_manager_parent_class)->dispose (object);
}

static void
ephy_open_tabs_manager_class_init (EphyOpenTabsManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ephy_open_tabs_manager_dispose;
}

static void
ephy_open_tabs_manager_init (EphyOpenTabsManager *self)
{
}

EphyOpenTabsManager *
ephy_open_tabs_manager_new (void)
{
  return EPHY_OPEN_TABS_MANAGER (g_object_new (EPHY_TYPE_OPEN_TABS_MANAGER, NULL));
}

EphyOpenTabsRecord *
ephy_open_tabs_manager_get_local_tabs (EphyOpenTabsManager *self)
{
  EphyOpenTabsRecord *local_tabs;
  WebKitFaviconDatabase *database;
  EphyEmbedShell *embed_shell;
  GList *windows;
  GList *tabs;
  char *favicon;
  char *id;
  char *name;
  const char *title;
  const char *url;

  g_return_val_if_fail (EPHY_IS_OPEN_TABS_MANAGER (self), NULL);

  embed_shell = ephy_embed_shell_get_default ();
  windows = gtk_application_get_windows (GTK_APPLICATION (embed_shell));
  database = webkit_web_context_get_favicon_database (ephy_embed_shell_get_web_context (embed_shell));
  id = ephy_sync_utils_get_device_id ();
  name = ephy_sync_utils_get_device_name ();
  local_tabs = ephy_open_tabs_record_new (id, name);

  for (GList *l = windows; l && l->data; l = l->next) {
    tabs = ephy_embed_container_get_children (l->data);

    for (GList *t = tabs; t && t->data; t = t->next) {
      title = ephy_embed_get_title (t->data);

      if (!g_strcmp0 (title, "Blank page") || !g_strcmp0 (title, "Most Visited"))
        continue;

      url = ephy_web_view_get_display_address (ephy_embed_get_web_view (t->data));
      favicon = webkit_favicon_database_get_favicon_uri (database, url);
      ephy_open_tabs_record_add_tab (local_tabs, title, url, favicon);

      g_free (favicon);
    }

    g_list_free (tabs);
  }

  g_free (id);
  g_free (name);

  return local_tabs;
}

GSList *
ephy_open_tabs_manager_get_remote_tabs (EphyOpenTabsManager *self)
{
  g_return_val_if_fail (EPHY_IS_OPEN_TABS_MANAGER (self), NULL);

  return self->remote_records;
}

void
ephy_open_tabs_manager_clear_cache (EphyOpenTabsManager *self)
{
  g_return_if_fail (EPHY_IS_OPEN_TABS_MANAGER (self));

  g_slist_free_full (self->remote_records, g_object_unref);
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

static double
synchronizable_manager_get_sync_time (EphySynchronizableManager *manager)
{
  return ephy_sync_utils_get_open_tabs_sync_time ();
}

static void
synchronizable_manager_set_sync_time (EphySynchronizableManager *manager,
                                      double                     sync_time)
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
                              GSList                                 *remotes_deleted,
                              GSList                                 *remotes_updated,
                              EphySynchronizableManagerMergeCallback  callback,
                              gpointer                                user_data)
{
  EphyOpenTabsManager *self = EPHY_OPEN_TABS_MANAGER (manager);
  EphyOpenTabsRecord *local_tabs;
  GSList *to_upload = NULL;
  char *id;

  id = ephy_sync_utils_get_device_id ();
  g_slist_free_full (self->remote_records, g_object_unref);
  self->remote_records = NULL;

  for (GSList *l = remotes_updated; l && l->data; l = l->next) {
    /* Exclude the record which describes the local open tabs. */
    if (!g_strcmp0 (id, ephy_open_tabs_record_get_id (l->data)))
      continue;

    self->remote_records = g_slist_prepend (self->remote_records, g_object_ref (l->data));
  }

  /* Only upload the local open tabs, we don't want to alter open tabs of
   * other clients. Also, overwrite any previous value by doing a force upload.
   */
  local_tabs = ephy_open_tabs_manager_get_local_tabs (self);
  to_upload = g_slist_prepend (to_upload, local_tabs);

  g_free (id);

  callback (to_upload, TRUE, user_data);
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
