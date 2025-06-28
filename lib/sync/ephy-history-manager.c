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
#include "ephy-history-manager.h"

#include "ephy-settings.h"
#include "ephy-sync-utils.h"
#include "ephy-synchronizable-manager.h"

struct _EphyHistoryManager {
  GObject parent_instance;

  EphyHistoryService *service;
};

static void ephy_synchronizable_manager_iface_init (EphySynchronizableManagerInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (EphyHistoryManager, ephy_history_manager, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (EPHY_TYPE_SYNCHRONIZABLE_MANAGER,
                                                      ephy_synchronizable_manager_iface_init))

enum {
  PROP_0,
  PROP_HISTORY_SERVICE,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

enum {
  SYNCHRONIZABLE_DELETED,
  SYNCHRONIZABLE_MODIFIED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

typedef struct {
  EphyHistoryManager *manager;
  gboolean is_initial;
  GList *remotes_deleted;
  GList *remotes_updated;
  EphySynchronizableManagerMergeCallback callback;
  gpointer user_data;
} MergeHistoryAsyncData;

static MergeHistoryAsyncData *
merge_history_async_data_new (EphyHistoryManager                     *manager,
                              gboolean                                is_initial,
                              GList                                  *remotes_deleted,
                              GList                                  *remotes_updated,
                              EphySynchronizableManagerMergeCallback  callback,
                              gpointer                                user_data)
{
  MergeHistoryAsyncData *data;

  data = g_new (MergeHistoryAsyncData, 1);
  data->manager = g_object_ref (manager);
  data->is_initial = is_initial;
  data->remotes_deleted = remotes_deleted;
  data->remotes_updated = remotes_updated;
  data->callback = callback;
  data->user_data = user_data;

  return data;
}

static void
merge_history_async_data_free (MergeHistoryAsyncData *data)
{
  g_assert (data);

  g_object_unref (data->manager);
  g_free (data);
}

static void
url_visited_cb (EphyHistoryService *service,
                EphyHistoryURL     *url,
                EphyHistoryManager *self)
{
  EphyHistoryRecord *record;

  if (!url->sync_id)
    return;

  record = ephy_history_record_new (url->sync_id, url->title, url->url, url->last_visit_time);
  g_signal_emit (self, signals[SYNCHRONIZABLE_MODIFIED], 0, record, TRUE);
  g_object_unref (record);
}

static void
url_deleted_cb (EphyHistoryService *service,
                EphyHistoryURL     *url,
                EphyHistoryManager *self)
{
  EphyHistoryRecord *record;

  if (!url->sync_id)
    return;

  record = ephy_history_record_new (url->sync_id, url->title, url->url, url->last_visit_time);
  g_signal_emit (self, signals[SYNCHRONIZABLE_DELETED], 0, record);
  g_object_unref (record);
}

static void
ephy_history_manager_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  EphyHistoryManager *self = EPHY_HISTORY_MANAGER (object);

  switch (prop_id) {
    case PROP_HISTORY_SERVICE:
      if (self->service)
        g_object_unref (self->service);
      self->service = g_object_ref (g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_history_manager_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  EphyHistoryManager *self = EPHY_HISTORY_MANAGER (object);

  switch (prop_id) {
    case PROP_HISTORY_SERVICE:
      g_value_set_object (value, self->service);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_history_manager_dispose (GObject *object)
{
  EphyHistoryManager *self = EPHY_HISTORY_MANAGER (object);

  if (self->service) {
    g_signal_handlers_disconnect_by_func (self->service, url_visited_cb, self);
    g_signal_handlers_disconnect_by_func (self->service, url_deleted_cb, self);
  }

  g_clear_object (&self->service);

  G_OBJECT_CLASS (ephy_history_manager_parent_class)->dispose (object);
}

static void
ephy_history_manager_constructed (GObject *object)
{
  EphyHistoryManager *self = EPHY_HISTORY_MANAGER (object);

  G_OBJECT_CLASS (ephy_history_manager_parent_class)->constructed (object);

  g_signal_connect (self->service, "visit-url", G_CALLBACK (url_visited_cb), self);
  g_signal_connect (self->service, "url-deleted", G_CALLBACK (url_deleted_cb), self);
}

static void
ephy_history_manager_class_init (EphyHistoryManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = ephy_history_manager_set_property;
  object_class->get_property = ephy_history_manager_get_property;
  object_class->constructed = ephy_history_manager_constructed;
  object_class->dispose = ephy_history_manager_dispose;

  obj_properties[PROP_HISTORY_SERVICE] =
    g_param_spec_object ("history-service",
                         NULL, NULL,
                         EPHY_TYPE_HISTORY_SERVICE,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  signals[SYNCHRONIZABLE_DELETED] = g_signal_lookup ("synchronizable-deleted",
                                                     EPHY_TYPE_SYNCHRONIZABLE_MANAGER);

  signals[SYNCHRONIZABLE_MODIFIED] = g_signal_lookup ("synchronizable-modified",
                                                      EPHY_TYPE_SYNCHRONIZABLE_MANAGER);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);
}

static void
ephy_history_manager_init (EphyHistoryManager *self)
{
}

EphyHistoryManager *
ephy_history_manager_new (EphyHistoryService *service)
{
  return EPHY_HISTORY_MANAGER (g_object_new (EPHY_TYPE_HISTORY_MANAGER,
                                             "history-service", service,
                                             NULL));
}

static const char *
synchronizable_manager_get_collection_name (EphySynchronizableManager *manager)
{
  return ephy_sync_utils_sync_with_firefox () ? "history" : "ephy-history";
}

static GType
synchronizable_manager_get_synchronizable_type (EphySynchronizableManager *manager)
{
  return EPHY_TYPE_HISTORY_RECORD;
}

static gboolean
synchronizable_manager_is_initial_sync (EphySynchronizableManager *manager)
{
  return ephy_sync_utils_get_history_sync_is_initial ();
}

static void
synchronizable_manager_set_is_initial_sync (EphySynchronizableManager *manager,
                                            gboolean                   is_initial)
{
  ephy_sync_utils_set_history_sync_is_initial (is_initial);
}

static gint64
synchronizable_manager_get_sync_time (EphySynchronizableManager *manager)
{
  return ephy_sync_utils_get_history_sync_time ();
}

static void
synchronizable_manager_set_sync_time (EphySynchronizableManager *manager,
                                      gint64                     sync_time)
{
  ephy_sync_utils_set_history_sync_time (sync_time);
}

static void
synchronizable_manager_add (EphySynchronizableManager *manager,
                            EphySynchronizable        *synchronizable)
{
  EphyHistoryManager *self = EPHY_HISTORY_MANAGER (manager);
  EphyHistoryRecord *record = EPHY_HISTORY_RECORD (synchronizable);

  if (ephy_history_record_get_last_visit_time (record) > 0)
    ephy_history_service_visit_url (self->service,
                                    ephy_history_record_get_uri (record),
                                    ephy_history_record_get_id (record),
                                    ephy_history_record_get_last_visit_time (record),
                                    EPHY_PAGE_VISIT_LINK,
                                    FALSE);
}

static void
synchronizable_manager_remove (EphySynchronizableManager *manager,
                               EphySynchronizable        *synchronizable)
{
  EphyHistoryManager *self = EPHY_HISTORY_MANAGER (manager);
  EphyHistoryRecord *record = EPHY_HISTORY_RECORD (synchronizable);
  EphyHistoryURL *url;
  GList *to_delete = NULL;

  url = ephy_history_url_new (ephy_history_record_get_uri (record),
                              ephy_history_record_get_title (record),
                              0, 0,
                              ephy_history_record_get_last_visit_time (record));
  url->notify_delete = FALSE;
  to_delete = g_list_prepend (to_delete, url);
  ephy_history_service_delete_urls (self->service, to_delete, NULL, NULL, NULL);

  g_list_free_full (to_delete, (GDestroyNotify)ephy_history_url_free);
}

static void
synchronizable_manager_save (EphySynchronizableManager *manager,
                             EphySynchronizable        *synchronizable)
{
  /* No implementation.
   * We don't care about the server time modified of history records.
   */
}

static void
ephy_history_manager_handle_different_id_same_url (EphyHistoryManager *self,
                                                   EphyHistoryRecord  *local,
                                                   EphyHistoryRecord  *remote)
{
  gint64 local_last_visit_time;
  gint64 remote_last_visit_time;

  g_assert (EPHY_IS_HISTORY_MANAGER (self));
  g_assert (EPHY_HISTORY_RECORD (local));
  g_assert (EPHY_HISTORY_RECORD (remote));

  local_last_visit_time = ephy_history_record_get_last_visit_time (local);
  remote_last_visit_time = ephy_history_record_get_last_visit_time (remote);

  if (remote_last_visit_time > local_last_visit_time)
    ephy_history_service_visit_url (self->service,
                                    ephy_history_record_get_uri (local),
                                    ephy_history_record_get_id (local),
                                    local_last_visit_time,
                                    EPHY_PAGE_VISIT_LINK, FALSE);

  ephy_history_record_set_id (remote, ephy_history_record_get_id (local));
  ephy_history_record_add_visit_time (remote, local_last_visit_time);
}

static GPtrArray *
ephy_history_manager_handle_initial_merge (EphyHistoryManager *self,
                                           GHashTable         *records_ht_id,
                                           GHashTable         *records_ht_url,
                                           GList              *remote_records)
{
  EphyHistoryRecord *record;
  GHashTableIter iter;
  gpointer key, value;
  GPtrArray *to_upload;
  const char *remote_id;
  const char *remote_url;
  gint64 remote_last_visit_time;
  gint64 local_last_visit_time;

  g_assert (EPHY_IS_HISTORY_MANAGER (self));

  to_upload = g_ptr_array_new_with_free_func (g_object_unref);

  /* A history record is uniquely identified by its sync ID or by its URL. When
   * importing history records from server, we may encounter duplicates either
   * by ID or by URL. We start from the assumption that same ID means same URL
   * but same URL does not necessarily mean same ID. This is what our merge
   * logic is based on.
   */
  for (GList *l = remote_records; l && l->data; l = l->next) {
    remote_id = ephy_history_record_get_id (l->data);
    remote_url = ephy_history_record_get_uri (l->data);
    remote_last_visit_time = ephy_history_record_get_last_visit_time (l->data);

    /* Try find by ID. */
    record = g_hash_table_lookup (records_ht_id, remote_id);
    if (record) {
      /* Same ID, same URL. Update last visit time for the local record and add
       * the local last visit time to the remote one. */
      local_last_visit_time = ephy_history_record_get_last_visit_time (record);
      if (remote_last_visit_time > local_last_visit_time)
        ephy_history_service_visit_url (self->service, remote_url,
                                        remote_id, remote_last_visit_time,
                                        EPHY_PAGE_VISIT_LINK, FALSE);

      if (ephy_history_record_add_visit_time (l->data, local_last_visit_time))
        g_ptr_array_add (to_upload, g_object_ref (l->data));

      g_hash_table_remove (records_ht_id, remote_id);
    } else {
      /* Try find by URL. */
      record = g_hash_table_lookup (records_ht_url, remote_url);
      if (record) {
        /* Different ID, same URL. Keep local ID. */
        g_signal_emit (self, signals[SYNCHRONIZABLE_DELETED], 0, l->data);
        ephy_history_manager_handle_different_id_same_url (self, record, l->data);
        g_ptr_array_add (to_upload, g_object_ref (l->data));
        g_hash_table_remove (records_ht_id, ephy_history_record_get_id (record));
      } else {
        /* Different ID, different URL. This is a new record. */
        if (remote_last_visit_time > 0)
          ephy_history_service_visit_url (self->service, remote_url,
                                          remote_id, remote_last_visit_time,
                                          EPHY_PAGE_VISIT_LINK, FALSE);
      }
    }
  }

  /* Set the remaining local records to be uploaded to server. */
  g_hash_table_iter_init (&iter, records_ht_id);
  while (g_hash_table_iter_next (&iter, &key, &value))
    g_ptr_array_add (to_upload, g_object_ref (value));

  return to_upload;
}

static GPtrArray *
ephy_history_manager_handle_regular_merge (EphyHistoryManager *self,
                                           GHashTable         *records_ht_id,
                                           GHashTable         *records_ht_url,
                                           GList              *deleted_records,
                                           GList              *updated_records)
{
  EphyHistoryRecord *record;
  GPtrArray *to_upload;
  const char *remote_id;
  const char *remote_url;
  gint64 remote_last_visit_time;
  gint64 local_last_visit_time;

  g_assert (EPHY_IS_HISTORY_MANAGER (self));

  to_upload = g_ptr_array_new_with_free_func (g_object_unref);

  for (GList *l = deleted_records; l && l->data; l = l->next) {
    remote_id = ephy_history_record_get_id (l->data);
    remote_url = ephy_history_record_get_uri (l->data);

    record = g_hash_table_lookup (records_ht_id, remote_id);
    if (record) {
      ephy_synchronizable_manager_remove (EPHY_SYNCHRONIZABLE_MANAGER (self),
                                          EPHY_SYNCHRONIZABLE (record));
      g_hash_table_remove (records_ht_id, remote_id);
      g_hash_table_remove (records_ht_url, remote_url);
    }
  }

  /* See comment in ephy_history_manager_handle_initial_merge. */
  for (GList *l = updated_records; l && l->data; l = l->next) {
    remote_id = ephy_history_record_get_id (l->data);
    remote_url = ephy_history_record_get_uri (l->data);
    remote_last_visit_time = ephy_history_record_get_last_visit_time (l->data);

    /* Try find by ID. */
    record = g_hash_table_lookup (records_ht_id, remote_id);
    if (record) {
      /* Same ID, same URL. Update last visit time for the local record. */
      local_last_visit_time = ephy_history_record_get_last_visit_time (record);

      /* Firefox offers the option to "forget about this site" which means that
       * the record is not deleted from server but only has its visit times
       * deleted. Having no visit times translates to a negative last visit time
       * in Epiphany. Since Epiphany does not support having a history record
       * with no visit time, we delete it for good from the local database.
       */
      if (remote_last_visit_time <= 0)
        ephy_synchronizable_manager_remove (EPHY_SYNCHRONIZABLE_MANAGER (self),
                                            EPHY_SYNCHRONIZABLE (record));
      else if (remote_last_visit_time > local_last_visit_time)
        ephy_history_service_visit_url (self->service, remote_url,
                                        remote_id, remote_last_visit_time,
                                        EPHY_PAGE_VISIT_LINK, FALSE);
    } else {
      /* Try find by URL. */
      record = g_hash_table_lookup (records_ht_url, remote_url);
      if (record) {
        /* Different ID, same URL. Keep local ID. */
        g_signal_emit (self, signals[SYNCHRONIZABLE_DELETED], 0, l->data);
        ephy_history_manager_handle_different_id_same_url (self, record, l->data);
        g_ptr_array_add (to_upload, g_object_ref (l->data));
      } else {
        /* Different ID, different URL. This is a new record. */
        if (remote_last_visit_time > 0)
          ephy_history_service_visit_url (self->service, remote_url,
                                          remote_id, remote_last_visit_time,
                                          EPHY_PAGE_VISIT_LINK, FALSE);
      }
    }
  }

  return to_upload;
}

static void
merge_history_cb (EphyHistoryService    *service,
                  gboolean               success,
                  GList                 *urls,
                  MergeHistoryAsyncData *data)
{
  GHashTable *records_ht_id = NULL;
  GHashTable *records_ht_url = NULL;
  GPtrArray *to_upload = NULL;

  if (!success) {
    g_warning ("Failed to retrieve URLs in history");
    goto out;
  }

  records_ht_id = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  records_ht_url = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

  for (GList *l = urls; l && l->data; l = l->next) {
    EphyHistoryURL *url = (EphyHistoryURL *)l->data;
    EphyHistoryRecord *record;

    /* Ignore migrated history, i.e. URLs with a NULL id. */
    if (!url->sync_id)
      continue;

    record = ephy_history_record_new (url->sync_id, url->title, url->url, url->last_visit_time);
    g_hash_table_insert (records_ht_id, g_strdup (url->sync_id), record);
    g_hash_table_insert (records_ht_url, g_strdup (url->url), g_object_ref (record));
  }

  if (data->is_initial)
    to_upload = ephy_history_manager_handle_initial_merge (data->manager,
                                                           records_ht_id,
                                                           records_ht_url,
                                                           data->remotes_updated);
  else
    to_upload = ephy_history_manager_handle_regular_merge (data->manager,
                                                           records_ht_id,
                                                           records_ht_url,
                                                           data->remotes_deleted,
                                                           data->remotes_updated);

out:
  data->callback (to_upload, data->user_data);

  if (records_ht_id)
    g_hash_table_unref (records_ht_id);
  if (records_ht_url)
    g_hash_table_unref (records_ht_url);
  merge_history_async_data_free (data);
}

static void
synchronizable_manager_merge (EphySynchronizableManager              *manager,
                              gboolean                                is_initial,
                              GList                                  *remotes_deleted,
                              GList                                  *remotes_updated,
                              EphySynchronizableManagerMergeCallback  callback,
                              gpointer                                user_data)
{
  EphyHistoryManager *self = EPHY_HISTORY_MANAGER (manager);

  ephy_history_service_find_urls (self->service, -1, -1, -1, 0, NULL,
                                  EPHY_HISTORY_SORT_MOST_RECENTLY_VISITED, NULL,
                                  (EphyHistoryJobCallback)merge_history_cb,
                                  merge_history_async_data_new (self,
                                                                is_initial,
                                                                remotes_deleted,
                                                                remotes_updated,
                                                                callback,
                                                                user_data));
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
