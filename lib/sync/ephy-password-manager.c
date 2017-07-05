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
#include "ephy-password-manager.h"

#include "ephy-debug.h"
#include "ephy-settings.h"
#include "ephy-synchronizable-manager.h"
#include "ephy-uri-helpers.h"

#include <glib/gi18n.h>
#include <stdio.h>

const SecretSchema *
ephy_password_manager_get_password_schema (void)
{
  static const SecretSchema schema = {
    "org.epiphany.FormPassword", SECRET_SCHEMA_NONE,
    {
      { ID_KEY, SECRET_SCHEMA_ATTRIBUTE_STRING },
      { HOSTNAME_KEY, SECRET_SCHEMA_ATTRIBUTE_STRING },
      { USERNAME_FIELD_KEY, SECRET_SCHEMA_ATTRIBUTE_STRING },
      { PASSWORD_FIELD_KEY, SECRET_SCHEMA_ATTRIBUTE_STRING },
      { USERNAME_KEY, SECRET_SCHEMA_ATTRIBUTE_STRING },
      { SERVER_TIME_MODIFIED_KEY, SECRET_SCHEMA_ATTRIBUTE_STRING},
      { "NULL", 0 },
    }
  };
  return &schema;
}

struct _EphyPasswordManager {
  GObject parent_instance;

  GHashTable *cache;
};

static void ephy_synchronizable_manager_iface_init (EphySynchronizableManagerInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EphyPasswordManager, ephy_password_manager, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_SYNCHRONIZABLE_MANAGER,
                                                ephy_synchronizable_manager_iface_init))

typedef struct {
  EphyPasswordManagerQueryCallback callback;
  gpointer user_data;
} QueryAsyncData;

typedef struct {
  EphyPasswordManager *manager;
  char                *password;
} UpdatePasswordAsyncData;

typedef struct {
  EphyPasswordManager *manager;
  EphyPasswordRecord  *record;
} ReplaceRecordAsyncData;

typedef struct {
  EphyPasswordManager                    *manager;
  gboolean                                is_initial;
  GSList                                 *remotes_deleted;
  GSList                                 *remotes_updated;
  EphySynchronizableManagerMergeCallback  callback;
  gpointer                                user_data;
} MergePasswordsAsyncData;

static QueryAsyncData *
query_async_data_new (EphyPasswordManagerQueryCallback callback,
                      gpointer                         user_data)
{
  QueryAsyncData *data;

  data = g_slice_new (QueryAsyncData);
  data->callback = callback;
  data->user_data = user_data;

  return data;
}

static void
query_async_data_free (QueryAsyncData *data)
{
  g_assert (data);

  g_slice_free (QueryAsyncData, data);
}

static UpdatePasswordAsyncData *
update_password_async_data_new (EphyPasswordManager *manager,
                                const char          *password)
{
  UpdatePasswordAsyncData *data;

  data = g_slice_new (UpdatePasswordAsyncData);
  data->manager = g_object_ref (manager);
  data->password = g_strdup (password);

  return data;
}

static void
update_password_async_data_free (UpdatePasswordAsyncData *data)
{
  g_assert (data);

  g_object_unref (data->manager);
  g_free (data->password);
  g_slice_free (UpdatePasswordAsyncData, data);
}

static MergePasswordsAsyncData *
merge_passwords_async_data_new (EphyPasswordManager                    *manager,
                                gboolean                                is_initial,
                                GSList                                 *remotes_deleted,
                                GSList                                 *remotes_updated,
                                EphySynchronizableManagerMergeCallback  callback,
                                gpointer                                user_data)
{
  MergePasswordsAsyncData *data;

  data = g_slice_new (MergePasswordsAsyncData);
  data->manager = g_object_ref (manager);
  data->is_initial = is_initial;
  data->remotes_deleted = remotes_deleted;
  data->remotes_updated = remotes_updated;
  data->callback = callback;
  data->user_data = user_data;

  return data;
}

static void
merge_passwords_async_data_free (MergePasswordsAsyncData *data)
{
  g_assert (data);

  g_object_unref (data->manager);
  g_slice_free (MergePasswordsAsyncData, data);
}

static ReplaceRecordAsyncData *
replace_record_async_data_new (EphyPasswordManager *manager,
                               EphyPasswordRecord  *record)
{
  ReplaceRecordAsyncData *data;

  data = g_slice_new (ReplaceRecordAsyncData);
  data->manager = g_object_ref (manager);
  data->record = g_object_ref (record);

  return data;
}

static void
replace_record_async_data_free (ReplaceRecordAsyncData *data)
{
  g_assert (data);

  g_object_unref (data->manager);
  g_object_unref (data->record);
  g_slice_free (ReplaceRecordAsyncData, data);
}

static GHashTable *
get_attributes_table (const char *id,
                      const char *uri,
                      const char *username,
                      const char *username_field,
                      const char *password_field,
                      double      server_time_modified)
{
  GHashTable *attributes = secret_attributes_build (EPHY_FORM_PASSWORD_SCHEMA, NULL);

  if (id)
    g_hash_table_insert (attributes,
                         g_strdup (ID_KEY),
                         g_strdup (id));
  if (uri)
    g_hash_table_insert (attributes,
                         g_strdup (HOSTNAME_KEY),
                         ephy_uri_to_security_origin (uri));
  if (username)
    g_hash_table_insert (attributes,
                         g_strdup (USERNAME_KEY),
                         g_strdup (username));
  if (username_field)
    g_hash_table_insert (attributes,
                         g_strdup (USERNAME_FIELD_KEY),
                         g_strdup (username_field));
  if (password_field)
    g_hash_table_insert (attributes,
                         g_strdup (PASSWORD_FIELD_KEY),
                         g_strdup (password_field));
  if (server_time_modified >= 0)
    g_hash_table_insert (attributes,
                         g_strdup (SERVER_TIME_MODIFIED_KEY),
                         g_strdup_printf ("%.2lf", server_time_modified));

  return attributes;
}

static void
ephy_password_manager_cache_clear (EphyPasswordManager *self)
{
  GHashTableIter iter;
  gpointer key, value;

  g_assert (EPHY_IS_PASSWORD_MANAGER (self));
  g_assert (self->cache);

  g_hash_table_iter_init (&iter, self->cache);
  while (g_hash_table_iter_next (&iter, &key, &value))
    g_slist_free_full (value, g_free);
  g_hash_table_remove_all (self->cache);
}

static void
ephy_password_manager_cache_remove (EphyPasswordManager *self,
                                    const char          *hostname,
                                    const char          *username)
{
  GSList *usernames;
  GSList *new_usernames = NULL;

  g_assert (EPHY_IS_PASSWORD_MANAGER (self));
  g_assert (self->cache);
  g_assert (hostname);
  g_assert (username);

  usernames = g_hash_table_lookup (self->cache, hostname);
  if (usernames) {
    for (GSList *l = usernames; l && l->data; l = l->next) {
      if (g_strcmp0 (username, l->data))
        new_usernames = g_slist_prepend (new_usernames, g_strdup (l->data));
    }
    g_hash_table_replace (self->cache, g_strdup (hostname), new_usernames);
    g_slist_free_full (usernames, g_free);
  }
}

static void
ephy_password_manager_cache_add (EphyPasswordManager *self,
                                 const char          *hostname,
                                 const char          *username)
{
  GSList *usernames;

  g_assert (EPHY_IS_PASSWORD_MANAGER (self));
  g_assert (self->cache);
  g_assert (hostname);
  g_assert (username);

  usernames = g_hash_table_lookup (self->cache, hostname);
  for (GSList *l = usernames; l && l->data; l = l->next) {
    if (!g_strcmp0 (username, l->data))
      return;
  }
  usernames = g_slist_prepend (usernames, g_strdup (username));
  g_hash_table_replace (self->cache, g_strdup (hostname), usernames);
}

static void
populate_cache_cb (GSList   *records,
                   gpointer  user_data)
{
  EphyPasswordManager *self = EPHY_PASSWORD_MANAGER (user_data);

  for (GSList *l = records; l && l->data; l = l->next) {
    EphyPasswordRecord *record = EPHY_PASSWORD_RECORD (l->data);
    const char *hostname = ephy_password_record_get_hostname (record);
    const char *username = ephy_password_record_get_username (record);

    ephy_password_manager_cache_add (self, hostname, username);
  }

  g_slist_free_full (records, g_object_unref);
}

static void
ephy_password_manager_dispose (GObject *object)
{
  EphyPasswordManager *self = EPHY_PASSWORD_MANAGER (object);

  if (self->cache) {
    ephy_password_manager_cache_clear (self);
    g_clear_pointer (&self->cache, g_hash_table_unref);
  }

  G_OBJECT_CLASS (ephy_password_manager_parent_class)->dispose (object);
}

static void
ephy_password_manager_class_init (EphyPasswordManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ephy_password_manager_dispose;
}

static void
ephy_password_manager_init (EphyPasswordManager *self)
{
  LOG ("Loading usernames into internal cache...");
  self->cache = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  ephy_password_manager_query (self, NULL, NULL, NULL, NULL,
                               populate_cache_cb, self);
}

EphyPasswordManager *
ephy_password_manager_new (void)
{
  return EPHY_PASSWORD_MANAGER (g_object_new (EPHY_TYPE_PASSWORD_MANAGER, NULL));
}

GSList *
ephy_password_manager_get_cached_users_for_uri (EphyPasswordManager *self,
                                                const char          *uri)
{
  GSList *list;
  char *hostname;

  g_return_val_if_fail (EPHY_IS_PASSWORD_MANAGER (self), NULL);
  g_return_val_if_fail (uri, NULL);

  hostname = ephy_uri_to_security_origin (uri);
  list = g_hash_table_lookup (self->cache, hostname);
  g_free (hostname);

  return list;
}

static void
secret_service_store_cb (SecretService *service,
                         GAsyncResult  *result,
                         GTask         *task)
{
  GError *error = NULL;

  secret_service_store_finish (service, result, &error);
  if (error)
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, TRUE);

  g_object_unref (task);
}

static void
store_internal (const char          *password,
                GHashTable          *attributes,
                GAsyncReadyCallback  callback,
                gpointer             user_data)
{
  SecretValue *value;
  GTask *task;
  const char *hostname;
  const char *username;
  char *label;

  g_assert (password);
  g_assert (attributes);

  task = g_task_new (NULL, NULL, callback, user_data);
  value = secret_value_new (password, -1, "text/plain");
  hostname = g_hash_table_lookup (attributes, HOSTNAME_KEY);
  username = g_hash_table_lookup (attributes, USERNAME_KEY);

  if (username) {
    /* Translators: The first %s is the username and the second one is the
     * security origin where this is happening. Example: gnome@gmail.com and
     * https://mail.google.com. */
    label = g_strdup_printf (_("Password for %s in a form in %s"), username, hostname);
  } else {
    /* Translators: The %s is the security origin where this is happening.
     * Example: https://mail.google.com. */
    label = g_strdup_printf (_("Password in a form in %s"), hostname);
  }

  LOG ("Storing password record for (%s, %s, %s, %s)",
       hostname, username,
       (char *)g_hash_table_lookup (attributes, USERNAME_FIELD_KEY),
       (char *)g_hash_table_lookup (attributes, PASSWORD_FIELD_KEY));

  secret_service_store (NULL, EPHY_FORM_PASSWORD_SCHEMA,
                        attributes, NULL, label, value, NULL,
                        (GAsyncReadyCallback)secret_service_store_cb,
                        g_object_ref (task));

  g_free (label);
  secret_value_unref (value);
  g_object_unref (task);
}

static void
ephy_password_manger_store_record (EphyPasswordManager *self,
                                   EphyPasswordRecord  *record)
{
  GHashTable *attributes;

  g_assert (EPHY_IS_PASSWORD_MANAGER (self));
  g_assert (EPHY_IS_PASSWORD_RECORD (record));

  attributes = get_attributes_table (ephy_password_record_get_id (record),
                                     ephy_password_record_get_hostname (record),
                                     ephy_password_record_get_username (record),
                                     ephy_password_record_get_username_field (record),
                                     ephy_password_record_get_password_field (record),
                                     ephy_synchronizable_get_server_time_modified (EPHY_SYNCHRONIZABLE (record)));
  store_internal (ephy_password_record_get_password (record), attributes, NULL, NULL);
  ephy_password_manager_cache_add (self,
                                   ephy_password_record_get_hostname (record),
                                   ephy_password_record_get_username (record));

  g_hash_table_unref (attributes);
}

static void
update_password_cb (GSList   *records,
                    gpointer  user_data)
{
  UpdatePasswordAsyncData *data = (UpdatePasswordAsyncData *)user_data;
  EphyPasswordRecord *record;

  /* We only expect one matching record here. */
  g_assert (g_slist_length (records) == 1);

  record = EPHY_PASSWORD_RECORD (records->data);
  ephy_password_record_set_password (record, data->password);
  ephy_password_manger_store_record (data->manager, record);
  g_signal_emit_by_name (data->manager, "synchronizable-modified", record);

  g_slist_free_full (records, g_object_unref);
  update_password_async_data_free (data);
}

void
ephy_password_manager_save (EphyPasswordManager *self,
                            const char          *uri,
                            const char          *username,
                            const char          *password,
                            const char          *username_field,
                            const char          *password_field,
                            gboolean             is_new)
{
  EphyPasswordRecord *record;
  char *hostname;
  char *uuid;
  char *id;
  gint64 timestamp;

  g_return_if_fail (EPHY_IS_PASSWORD_MANAGER (self));
  g_return_if_fail (uri);
  g_return_if_fail (password);
  g_return_if_fail (!username_field || username);
  g_return_if_fail (!password_field || password);

  if (!is_new) {
    LOG ("Updating password for (%s, %s, %s, %s)",
         uri, username, username_field, password_field);
    ephy_password_manager_query (self, uri, username,
                                 username_field, password_field,
                                 update_password_cb,
                                 update_password_async_data_new (self, password));
    return;
  }

  uuid = g_uuid_string_random ();
  id = g_strdup_printf ("{%s}", uuid);
  timestamp = g_get_real_time () / 1000;
  hostname = ephy_uri_to_security_origin (uri);
  record = ephy_password_record_new (id, hostname,
                                     username, password,
                                     username_field, password_field,
                                     timestamp, timestamp);
  ephy_password_manger_store_record (self, record);
  g_signal_emit_by_name (self, "synchronizable-modified", record);

  g_free (hostname);
  g_free (uuid);
  g_free (id);
  g_object_unref (record);
}

static void
secret_service_search_cb (SecretService  *service,
                          GAsyncResult   *result,
                          QueryAsyncData *data)
{
  GList *matches = NULL;
  GSList *records = NULL;
  GError *error = NULL;

  matches = secret_service_search_finish (service, result, &error);
  if (error) {
    g_warning ("Failed to search secrets in password schema: %s", error->message);
    g_error_free (error);
    goto out;
  }

  for (GList *l = matches; l && l->data; l = l->next) {
    SecretItem *item = (SecretItem *)l->data;
    GHashTable *attributes = secret_item_get_attributes (item);
    SecretValue *value = secret_item_get_secret (item);
    const char *id = g_hash_table_lookup (attributes, ID_KEY);
    const char *hostname = g_hash_table_lookup (attributes, HOSTNAME_KEY);
    const char *username = g_hash_table_lookup (attributes, USERNAME_KEY);
    const char *username_field = g_hash_table_lookup (attributes, USERNAME_FIELD_KEY);
    const char *password_field = g_hash_table_lookup (attributes, PASSWORD_FIELD_KEY);
    const char *timestamp = g_hash_table_lookup (attributes, SERVER_TIME_MODIFIED_KEY);
    const char *password = secret_value_get (value, NULL);
    double server_time_modified;
    EphyPasswordRecord *record;

    LOG ("Found password record for (%s, %s, %s, %s)",
         hostname, username, username_field, password_field);

    record = ephy_password_record_new (id, hostname,
                                       username, password,
                                       username_field, password_field,
                                       secret_item_get_created (item) * 1000,
                                       secret_item_get_modified (item) * 1000);
    sscanf (timestamp, "%lf", &server_time_modified);
    ephy_synchronizable_set_server_time_modified (EPHY_SYNCHRONIZABLE (record),
                                                  server_time_modified);
    records = g_slist_prepend (records, record);

    secret_value_unref (value);
    g_hash_table_unref (attributes);
  }

out:
  if (data->callback)
    data->callback (records, data->user_data);
  query_async_data_free (data);
  g_list_free_full (matches, g_object_unref);
}

void
ephy_password_manager_query (EphyPasswordManager              *self,
                             const char                       *uri,
                             const char                       *username,
                             const char                       *username_field,
                             const char                       *password_field,
                             EphyPasswordManagerQueryCallback  callback,
                             gpointer                          user_data)
{
  QueryAsyncData *data;
  GHashTable *attributes;

  g_return_if_fail (EPHY_IS_PASSWORD_MANAGER (self));

  LOG ("Querying password records for (%s, %s, %s, %s)",
       uri, username, username_field, password_field);

  attributes = get_attributes_table (NULL, uri, username,
                                     username_field, password_field, -1);
  data = query_async_data_new (callback, user_data);

  secret_service_search (NULL,
                         EPHY_FORM_PASSWORD_SCHEMA,
                         attributes,
                         SECRET_SEARCH_ALL | SECRET_SEARCH_UNLOCK | SECRET_SEARCH_LOAD_SECRETS,
                         NULL,
                         (GAsyncReadyCallback)secret_service_search_cb,
                         data);

  g_hash_table_unref (attributes);
}

void
ephy_password_manager_store_raw (const char          *uri,
                                 const char          *username,
                                 const char          *password,
                                 const char          *username_field,
                                 const char          *password_field,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  GHashTable *attributes;

  g_return_if_fail (uri);
  g_return_if_fail (password);
  g_return_if_fail (!username_field || username);
  g_return_if_fail (!password_field || password);

  attributes = get_attributes_table (NULL, uri, username,
                                     username_field, password_field, -1);
  store_internal (password, attributes, callback, user_data);

  g_hash_table_unref (attributes);
}

gboolean
ephy_password_manager_store_finish (GAsyncResult  *result,
                                    GError       **error)
{
  g_return_val_if_fail (!error || !(*error), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, NULL), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
secret_service_clear_cb (SecretService *service,
                         GAsyncResult  *result,
                         gpointer       user_data)
{
  GError *error = NULL;

  secret_service_clear_finish (service, result, &error);
  if (error) {
    g_warning ("Failed to clear secrets from password schema: %s", error->message);
    g_error_free (error);
    return;
  }

  if (user_data) {
    ReplaceRecordAsyncData *data = (ReplaceRecordAsyncData *)user_data;
    ephy_password_manger_store_record (data->manager, data->record);
    replace_record_async_data_free (data);
  }
}

static void
ephy_password_manager_forget_record (EphyPasswordManager *self,
                                     EphyPasswordRecord  *record,
                                     EphyPasswordRecord  *replacement)
{
  GHashTable *attributes;

  g_assert (EPHY_IS_PASSWORD_MANAGER (self));
  g_assert (EPHY_IS_PASSWORD_RECORD (record));

  attributes = get_attributes_table (ephy_password_record_get_id (record),
                                     ephy_password_record_get_hostname (record),
                                     ephy_password_record_get_username (record),
                                     ephy_password_record_get_username_field (record),
                                     ephy_password_record_get_password_field (record),
                                     -1);

  LOG ("Forgetting password record for (%s, %s, %s, %s)",
       ephy_password_record_get_hostname (record),
       ephy_password_record_get_username (record),
       ephy_password_record_get_username_field (record),
       ephy_password_record_get_password_field (record));

  secret_service_clear (NULL, EPHY_FORM_PASSWORD_SCHEMA, attributes, NULL,
                        (GAsyncReadyCallback)secret_service_clear_cb,
                        replacement ? replace_record_async_data_new (self, replacement) : NULL);

  ephy_password_manager_cache_remove (self,
                                      ephy_password_record_get_hostname (record),
                                      ephy_password_record_get_username (record));
  g_hash_table_unref (attributes);
}

static void
forget_cb (GSList   *records,
           gpointer  user_data)
{
  EphyPasswordManager *self = EPHY_PASSWORD_MANAGER (user_data);
  EphyPasswordRecord *record;

  /* We only expect one matching record here. */
  g_assert (g_slist_length (records) == 1);

  record = EPHY_PASSWORD_RECORD (records->data);
  g_signal_emit_by_name (self, "synchronizable-deleted", record);
  ephy_password_manager_forget_record (self, record, NULL);

  g_slist_free_full (records, g_object_unref);
}

void
ephy_password_manager_forget (EphyPasswordManager *self,
                              const char          *hostname,
                              const char          *username,
                              const char          *username_field,
                              const char          *password_field)
{
  g_return_if_fail (EPHY_IS_PASSWORD_MANAGER (self));
  g_return_if_fail (hostname);
  g_return_if_fail (password_field);
  g_return_if_fail (!username_field || username);

  /* synchronizable-deleted signal needs an EphySynchronizable object,
   * therefore we need to obtain the password record first and then emit
   * the signal before clearing the password from the secret schema. */
  ephy_password_manager_query (self, hostname, username,
                               username_field, password_field,
                               forget_cb, self);
}

static void
forget_all_cb (GSList   *records,
               gpointer  user_data)
{
  EphyPasswordManager *self = EPHY_PASSWORD_MANAGER (user_data);
  GHashTable *attributes;

  attributes = secret_attributes_build (EPHY_FORM_PASSWORD_SCHEMA, NULL);
  secret_service_clear (NULL, EPHY_FORM_PASSWORD_SCHEMA, attributes, NULL,
                        (GAsyncReadyCallback)secret_service_clear_cb, NULL);

  for (GSList *l = records; l && l->data; l = l->next)
    g_signal_emit_by_name (self, "synchronizable-deleted", l->data);

  ephy_password_manager_cache_clear (self);

  g_hash_table_unref (attributes);
  g_slist_free_full (records, g_object_unref);
}

void
ephy_password_manager_forget_all (EphyPasswordManager *self)
{
  g_return_if_fail (EPHY_IS_PASSWORD_MANAGER (self));

  /* synchronizable-deleted signal needs an EphySynchronizable object, therefore
   * we need to obtain the password records first and emit the signal for each
   * one before clearing the secret schema. */
  ephy_password_manager_query (self, NULL, NULL, NULL, NULL,
                               forget_all_cb, self);
}

static const char *
synchronizable_manager_get_collection_name (EphySynchronizableManager *manager)
{
  gboolean sync_with_firefox = g_settings_get_boolean (EPHY_SETTINGS_SYNC,
                                                       EPHY_PREFS_SYNC_WITH_FIREFOX);

  return sync_with_firefox ? "passwords" : "ephy-passwords";
}

static GType
synchronizable_manager_get_synchronizable_type (EphySynchronizableManager *manager)
{
  return EPHY_TYPE_PASSWORD_RECORD;
}

static gboolean
synchronizable_manager_is_initial_sync (EphySynchronizableManager *manager)
{
  return g_settings_get_boolean (EPHY_SETTINGS_SYNC,
                                 EPHY_PREFS_SYNC_PASSWORDS_INITIAL);
}

static void
synchronizable_manager_set_is_initial_sync (EphySynchronizableManager *manager,
                                            gboolean                   is_initial)
{
  g_settings_set_boolean (EPHY_SETTINGS_SYNC,
                          EPHY_PREFS_SYNC_PASSWORDS_INITIAL,
                          is_initial);
}

static double
synchronizable_manager_get_sync_time (EphySynchronizableManager *manager)
{
  return g_settings_get_double (EPHY_SETTINGS_SYNC,
                                EPHY_PREFS_SYNC_PASSWORDS_TIME);
}

static void
synchronizable_manager_set_sync_time (EphySynchronizableManager *manager,
                                      double                     sync_time)
{
  g_settings_set_double (EPHY_SETTINGS_SYNC,
                         EPHY_PREFS_SYNC_PASSWORDS_TIME,
                         sync_time);
}

static void
synchronizable_manager_add (EphySynchronizableManager *manager,
                            EphySynchronizable        *synchronizable)
{
  EphyPasswordManager *self = EPHY_PASSWORD_MANAGER (manager);
  EphyPasswordRecord *record = EPHY_PASSWORD_RECORD (synchronizable);

  ephy_password_manger_store_record (self, record);
}

static void
synchronizable_manager_remove (EphySynchronizableManager *manager,
                               EphySynchronizable        *synchronizable)
{
  EphyPasswordManager *self = EPHY_PASSWORD_MANAGER (manager);
  EphyPasswordRecord *record = EPHY_PASSWORD_RECORD (synchronizable);

  ephy_password_manager_forget_record (self, record, NULL);
}

static void
replace_existing_cb (GSList   *records,
                     gpointer  user_data)
{
  ReplaceRecordAsyncData *data = (ReplaceRecordAsyncData *)user_data;

  /* We only expect one matching record here. */
  g_assert (g_slist_length (records) == 1);

  ephy_password_manager_forget_record (data->manager, records->data, data->record);

  replace_record_async_data_free (data);
}

static void
ephy_password_manager_replace_existing (EphyPasswordManager *self,
                                        EphyPasswordRecord  *record)
{
  g_assert (EPHY_IS_PASSWORD_MANAGER (self));
  g_assert (EPHY_IS_PASSWORD_RECORD (record));

  ephy_password_manager_query (self,
                               ephy_password_record_get_hostname (record),
                               ephy_password_record_get_username (record),
                               ephy_password_record_get_username_field (record),
                               ephy_password_record_get_password_field (record),
                               replace_existing_cb,
                               replace_record_async_data_new (self, record));
}

static void
synchronizable_manager_save (EphySynchronizableManager *manager,
                             EphySynchronizable        *synchronizable)
{
  EphyPasswordManager *self = EPHY_PASSWORD_MANAGER (manager);
  EphyPasswordRecord *record = EPHY_PASSWORD_RECORD (synchronizable);

  ephy_password_manager_replace_existing (self, record);
}

static EphyPasswordRecord *
get_record_by_id (GSList     *records,
                  const char *id)
{
  g_assert (id);

  for (GSList *l = records; l && l->data; l = l->next) {
    if (!g_strcmp0 (ephy_password_record_get_id (l->data), id))
      return l->data;
  }

  return NULL;
}

static EphyPasswordRecord *
get_record_by_parameters (GSList     *records,
                          const char *hostname,
                          const char *username,
                          const char *username_field,
                          const char *password_field)
{
  for (GSList *l = records; l && l->data; l = l->next) {
    if (!g_strcmp0 (ephy_password_record_get_username (l->data), username) &&
        !g_strcmp0 (ephy_password_record_get_hostname (l->data), hostname) &&
        !g_strcmp0 (ephy_password_record_get_username_field (l->data), username_field) &&
        !g_strcmp0 (ephy_password_record_get_password_field (l->data), password_field))
      return l->data;
  }

  return NULL;
}

static GSList *
ephy_password_manager_handle_initial_merge (EphyPasswordManager *self,
                                            GSList              *local_records,
                                            GSList              *remote_records)
{
  EphyPasswordRecord *record;
  GHashTable *dont_upload;
  GSList *to_upload = NULL;
  const char *remote_id;
  const char *remote_hostname;
  const char *remote_username;
  const char *remote_password;
  const char *remote_username_field;
  const char *remote_password_field;
  guint64 remote_timestamp;
  guint64 local_timestamp;
  double remote_server_time_modified;
  double local_server_time_modified;

  g_assert (EPHY_IS_PASSWORD_MANAGER (self));

  /* A saved password record is uniquely identified by its ID or by its tuple
   * of (hostname, username, username field, password field). When importing
   * password records from server, we may encounter duplicates either by ID
   * or by mentioned tuple. We start from the assumption that same ID means
   * same tuple but same tuple does not necessarily mean same ID. This is what
   * our merge logic is based on.
   */
  dont_upload = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  for (GSList *l = remote_records; l && l->data; l = l->next) {
    remote_id = ephy_password_record_get_id (l->data);
    remote_hostname = ephy_password_record_get_hostname (l->data);
    remote_username = ephy_password_record_get_username (l->data);
    remote_password = ephy_password_record_get_password (l->data);
    remote_username_field = ephy_password_record_get_username_field (l->data);
    remote_password_field = ephy_password_record_get_password_field (l->data);
    remote_timestamp = ephy_password_record_get_time_password_changed (l->data);
    remote_server_time_modified = ephy_synchronizable_get_server_time_modified (l->data);

    record = get_record_by_id (local_records, remote_id);
    if (record) {
      if (!g_strcmp0 (ephy_password_record_get_password (record), remote_password)) {
        /* Same id, same password. Nothing to do. */
        g_hash_table_add (dont_upload, g_strdup (remote_id));
      } else {
        /* Same id, different password. Keep the most recent modified. */
        local_timestamp = ephy_password_record_get_time_password_changed (record);
        if (local_timestamp > remote_timestamp) {
          /* Local record is newer. Keep it and upload it to server.
           * Also, must keep the most recent server time modified. */
          local_server_time_modified = ephy_synchronizable_get_server_time_modified (EPHY_SYNCHRONIZABLE (record));
          if (local_server_time_modified < remote_server_time_modified) {
            ephy_synchronizable_set_server_time_modified (EPHY_SYNCHRONIZABLE (record),
                                                          remote_server_time_modified);
            ephy_password_manager_replace_existing (self, record);
          }
        } else {
          /* Remote record is newer. Forget local record and store remote record. */
          ephy_password_manager_forget_record (self, record, l->data);
          g_hash_table_add (dont_upload, g_strdup (remote_id));
        }
      }
    } else {
      record = get_record_by_parameters (local_records,
                                         remote_hostname,
                                         remote_username,
                                         remote_username_field,
                                         remote_password_field);
      if (record) {
        /* Different id, same tuple. Keep the most recent modified. */
        local_timestamp = ephy_password_record_get_time_password_changed (record);
        if (local_timestamp > remote_timestamp) {
          /* Local record is newer. Keep it, upload it and delete remote record from server. */
          g_signal_emit_by_name (self, "synchronizable-deleted", l->data);
        } else {
          /* Remote record is newer. Forget local record and store remote record. */
          ephy_password_manager_forget_record (self, record, l->data);
          g_hash_table_add (dont_upload, g_strdup (remote_id));
        }
      } else {
        /* Different id, different tuple. This is a new record. */
        ephy_password_manger_store_record (self, l->data);
        g_hash_table_add (dont_upload, g_strdup (remote_id));
      }
    }
  }

  /* Set the remaining local records to be uploaded to server. */
  for (GSList *l = local_records; l && l->data; l = l->next) {
    record = EPHY_PASSWORD_RECORD (l->data);
    if (!g_hash_table_contains (dont_upload, ephy_password_record_get_id (record)))
      to_upload = g_slist_prepend (to_upload, g_object_ref (record));
  }

  g_hash_table_unref (dont_upload);

  return to_upload;
}

static GSList *
ephy_password_manager_handle_regular_merge (EphyPasswordManager *self,
                                            GSList              *local_records,
                                            GSList              *deleted_records,
                                            GSList              *updated_records)
{
  EphyPasswordRecord *record;
  GSList *to_upload = NULL;
  const char *remote_id;
  const char *remote_hostname;
  const char *remote_username;
  const char *remote_username_field;
  const char *remote_password_field;
  guint64 remote_timestamp;
  guint64 local_timestamp;

  g_assert (EPHY_IS_PASSWORD_MANAGER (self));

  for (GSList *l = deleted_records; l && l->data; l = l->next) {
    remote_id = ephy_password_record_get_id (l->data);
    record = get_record_by_id (local_records, remote_id);
    if (record)
      ephy_password_manager_forget_record (self, record, NULL);
  }

  /* See comment in ephy_password_manager_handle_initial_merge. */
  for (GSList *l = updated_records; l && l->data; l = l->next) {
    remote_id = ephy_password_record_get_id (l->data);
    remote_hostname = ephy_password_record_get_hostname (l->data);
    remote_username = ephy_password_record_get_username (l->data);
    remote_username_field = ephy_password_record_get_username_field (l->data);
    remote_password_field = ephy_password_record_get_password_field (l->data);
    remote_timestamp = ephy_password_record_get_time_password_changed (l->data);

    record = get_record_by_id (local_records, remote_id);
    if (record) {
      /* Same id. Overwrite local record. */
      ephy_password_manager_forget_record (self, record, l->data);
    } else {
      record = get_record_by_parameters (local_records,
                                         remote_hostname,
                                         remote_username,
                                         remote_username_field,
                                         remote_password_field);
      if (record) {
        /* Different id, same tuple. Keep the most recent modified. */
        local_timestamp = ephy_password_record_get_time_password_changed (record);
        if (local_timestamp > remote_timestamp) {
          /* Local record is newer. Keep it, upload it and delete remote record from server. */
          to_upload = g_slist_prepend (to_upload, g_object_ref (record));
          g_signal_emit_by_name (self, "synchronizable-deleted", l->data);
        } else {
          /* Remote record is newer. Forget local record and store remote record. */
          ephy_password_manager_forget_record (self, record, l->data);
        }
      } else {
        /* Different id, different tuple. This is a new record. */
        ephy_password_manger_store_record (self, l->data);
      }
    }
  }

  return to_upload;
}

static void
merge_cb (GSList   *records,
          gpointer  user_data)
{
  MergePasswordsAsyncData *data = (MergePasswordsAsyncData *)user_data;
  GSList *to_upload = NULL;

  if (data->is_initial)
    to_upload = ephy_password_manager_handle_initial_merge (data->manager,
                                                            records,
                                                            data->remotes_updated);
  else
    to_upload = ephy_password_manager_handle_regular_merge (data->manager,
                                                            records,
                                                            data->remotes_deleted,
                                                            data->remotes_updated);

  data->callback (to_upload, data->user_data);

  g_slist_free_full (records, g_object_unref);
  merge_passwords_async_data_free (data);
}

static void
synchronizable_manager_merge (EphySynchronizableManager              *manager,
                              gboolean                                is_initial,
                              GSList                                 *remotes_deleted,
                              GSList                                 *remotes_updated,
                              EphySynchronizableManagerMergeCallback  callback,
                              gpointer                                user_data)
{
  EphyPasswordManager *self = EPHY_PASSWORD_MANAGER (manager);

  ephy_password_manager_query (self, NULL, NULL, NULL, NULL,
                               merge_cb,
                               merge_passwords_async_data_new (self,
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
