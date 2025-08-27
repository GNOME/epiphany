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
#include "ephy-password-manager.h"

#include "ephy-debug.h"
#include "ephy-settings.h"
#include "ephy-sync-utils.h"
#include "ephy-synchronizable-manager.h"

#include <glib/gi18n.h>
#include <inttypes.h>
#include <stdio.h>

void
ephy_password_request_data_free (EphyPasswordRequestData *request_data)
{
  g_assert (request_data);

  g_free (request_data->origin);
  g_free (request_data->target_origin);
  g_free (request_data->username);
  g_free (request_data->password);
  g_free (request_data->usernameField);
  g_free (request_data->passwordField);

  g_free (request_data);
}

const SecretSchema *
ephy_password_manager_get_password_schema (void)
{
  static const SecretSchema schema = {
    "org.epiphany.FormPassword", SECRET_SCHEMA_NONE,
    {
      { ID_KEY, SECRET_SCHEMA_ATTRIBUTE_STRING },
      { ORIGIN_KEY, SECRET_SCHEMA_ATTRIBUTE_STRING },
      { TARGET_ORIGIN_KEY, SECRET_SCHEMA_ATTRIBUTE_STRING },
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

enum {
  SYNCHRONIZABLE_DELETED,
  SYNCHRONIZABLE_MODIFIED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void ephy_password_manager_forget_record (EphyPasswordManager *self,
                                                 EphyPasswordRecord  *record,
                                                 EphyPasswordRecord  *replacement,
                                                 GTask               *task);

static void ephy_synchronizable_manager_iface_init (EphySynchronizableManagerInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (EphyPasswordManager, ephy_password_manager, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (EPHY_TYPE_SYNCHRONIZABLE_MANAGER,
                                                      ephy_synchronizable_manager_iface_init))

typedef struct {
  EphyPasswordManagerQueryCallback callback;
  gpointer user_data;
  GList *records;
  guint n_matches;
} QueryAsyncData;

typedef struct {
  EphyPasswordManager *manager;
  char *username;
  char *password;
} UpdateCredentialsAsyncData;

typedef struct {
  EphyPasswordManager *manager;
  EphyPasswordRecord *record;
  GTask *task;
} ManageRecordAsyncData;

typedef struct {
  EphyPasswordManager *manager;
  gboolean is_initial;
  GList *remotes_deleted;
  GList *remotes_updated;
  EphySynchronizableManagerMergeCallback callback;
  gpointer user_data;
} MergePasswordsAsyncData;

static QueryAsyncData *
query_async_data_new (EphyPasswordManagerQueryCallback callback,
                      gpointer                         user_data)
{
  QueryAsyncData *data;

  data = g_new0 (QueryAsyncData, 1);
  data->callback = callback;
  data->user_data = user_data;

  return data;
}

static void
query_async_data_free (QueryAsyncData *data)
{
  g_assert (data);

  g_list_free_full (data->records, g_object_unref);
  g_free (data);
}

static UpdateCredentialsAsyncData *
update_credentials_async_data_new (EphyPasswordManager *manager,
                                   const char          *username,
                                   const char          *password)
{
  UpdateCredentialsAsyncData *data;

  data = g_new0 (UpdateCredentialsAsyncData, 1);
  data->manager = g_object_ref (manager);
  data->username = g_strdup (username);
  data->password = g_strdup (password);

  return data;
}

static void
update_credentials_async_data_free (UpdateCredentialsAsyncData *data)
{
  g_assert (data);

  g_object_unref (data->manager);
  g_clear_pointer (&data->username, g_free);
  g_clear_pointer (&data->password, g_free);
  g_free (data);
}

static MergePasswordsAsyncData *
merge_passwords_async_data_new (EphyPasswordManager                    *manager,
                                gboolean                                is_initial,
                                GList                                  *remotes_deleted,
                                GList                                  *remotes_updated,
                                EphySynchronizableManagerMergeCallback  callback,
                                gpointer                                user_data)
{
  MergePasswordsAsyncData *data;

  data = g_new0 (MergePasswordsAsyncData, 1);
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
  g_list_free_full (data->remotes_deleted, g_object_unref);
  g_list_free_full (data->remotes_updated, g_object_unref);
  g_free (data);
}

static ManageRecordAsyncData *
manage_record_async_data_new (EphyPasswordManager *manager,
                              EphyPasswordRecord  *record,
                              GTask               *task)
{
  ManageRecordAsyncData *data;

  data = g_new0 (ManageRecordAsyncData, 1);

  if (manager)
    data->manager = g_object_ref (manager);

  if (record)
    data->record = g_object_ref (record);

  if (task)
    data->task = g_object_ref (task);

  return data;
}

static void
manage_record_async_data_free (ManageRecordAsyncData *data)
{
  g_assert (data);

  g_clear_object (&data->manager);
  g_clear_object (&data->record);
  g_clear_object (&data->task);

  g_free (data);
}

static GHashTable *
get_attributes_table (const char *id,
                      const char *origin,
                      const char *target_origin,
                      const char *username,
                      const char *username_field,
                      const char *password_field,
                      gint64      server_time_modified)
{
  GHashTable *attributes = secret_attributes_build (EPHY_FORM_PASSWORD_SCHEMA, NULL);

  if (id)
    g_hash_table_insert (attributes,
                         g_strdup (ID_KEY),
                         g_strdup (id));
  if (origin)
    g_hash_table_insert (attributes,
                         g_strdup (ORIGIN_KEY),
                         g_strdup (origin));
  if (target_origin)
    g_hash_table_insert (attributes,
                         g_strdup (TARGET_ORIGIN_KEY),
                         g_strdup (target_origin));
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
                         g_strdup_printf ("%" PRId64, server_time_modified));

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
    g_list_free_full (value, g_free);
  g_hash_table_remove_all (self->cache);
}

static void
ephy_password_manager_cache_remove (EphyPasswordManager *self,
                                    const char          *origin,
                                    const char          *username)
{
  GList *usernames;
  GList *new_usernames = NULL;

  g_assert (EPHY_IS_PASSWORD_MANAGER (self));
  g_assert (self->cache);

  if (!origin || !username)
    return;

  usernames = g_hash_table_lookup (self->cache, origin);
  if (usernames) {
    for (GList *l = usernames; l && l->data; l = l->next) {
      if (g_strcmp0 (username, l->data))
        new_usernames = g_list_prepend (new_usernames, g_strdup (l->data));
    }
    g_hash_table_replace (self->cache, g_strdup (origin), new_usernames);
    g_list_free_full (usernames, g_free);
  }
}

static void
ephy_password_manager_cache_add (EphyPasswordManager *self,
                                 const char          *origin,
                                 const char          *username)
{
  GList *usernames;

  g_assert (EPHY_IS_PASSWORD_MANAGER (self));
  g_assert (self->cache);

  if (!origin || !username)
    return;

  usernames = g_hash_table_lookup (self->cache, origin);
  for (GList *l = usernames; l && l->data; l = l->next) {
    if (!g_strcmp0 (username, l->data))
      return;
  }
  usernames = g_list_prepend (usernames, g_strdup (username));
  g_hash_table_replace (self->cache, g_strdup (origin), usernames);
}

static void
populate_cache_cb (GList    *records,
                   gpointer  user_data)
{
  EphyPasswordManager *self = EPHY_PASSWORD_MANAGER (user_data);

  for (GList *l = records; l && l->data; l = l->next) {
    EphyPasswordRecord *record = EPHY_PASSWORD_RECORD (l->data);
    const char *origin = ephy_password_record_get_origin (record);
    const char *username = ephy_password_record_get_username (record);

    ephy_password_manager_cache_add (self, origin, username);
  }
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

  signals[SYNCHRONIZABLE_DELETED] = g_signal_lookup ("synchronizable-deleted",
                                                     EPHY_TYPE_SYNCHRONIZABLE_MANAGER);

  signals[SYNCHRONIZABLE_MODIFIED] = g_signal_lookup ("synchronizable-modified",
                                                      EPHY_TYPE_SYNCHRONIZABLE_MANAGER);
}

static void
ephy_password_manager_init (EphyPasswordManager *self)
{
  LOG ("Loading usernames into internal cache...");
  self->cache = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  ephy_password_manager_query (self, NULL, NULL, NULL, NULL, NULL, NULL,
                               populate_cache_cb, self);
}

EphyPasswordManager *
ephy_password_manager_new (void)
{
  return EPHY_PASSWORD_MANAGER (g_object_new (EPHY_TYPE_PASSWORD_MANAGER, NULL));
}

GList *
ephy_password_manager_get_usernames_for_origin (EphyPasswordManager *self,
                                                const char          *origin)
{
  g_assert (EPHY_IS_PASSWORD_MANAGER (self));
  g_assert (origin);

  return g_hash_table_lookup (self->cache, origin);
}

static void
secret_password_store_cb (GObject               *source_object,
                          GAsyncResult          *result,
                          ManageRecordAsyncData *data)
{
  GError *error = NULL;
  const char *origin;
  const char *username;

  origin = ephy_password_record_get_origin (data->record);
  username = ephy_password_record_get_username (data->record);

  secret_password_store_finish (result, &error);
  if (error) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      g_warning ("Failed to store password record for (%s, %s, %s, %s, %s) (is the secret service or secrets portal broken?): %s",
                 origin,
                 ephy_password_record_get_target_origin (data->record),
                 username,
                 ephy_password_record_get_username_field (data->record),
                 ephy_password_record_get_password_field (data->record),
                 error->message);
    }
    g_error_free (error);
  } else {
    ephy_password_manager_cache_add (data->manager, origin, username);
  }

  manage_record_async_data_free (data);
}

static void
ephy_password_manager_store_record (EphyPasswordManager *self,
                                    EphyPasswordRecord  *record)
{
  GHashTable *attributes;
  const char *origin;
  const char *target_origin;
  const char *username;
  const char *password;
  const char *username_field;
  const char *password_field;
  char *label;
  gint64 modified;

  g_assert (EPHY_IS_PASSWORD_MANAGER (self));
  g_assert (EPHY_IS_PASSWORD_RECORD (record));

  origin = ephy_password_record_get_origin (record);
  target_origin = ephy_password_record_get_target_origin (record);
  username = ephy_password_record_get_username (record);
  password = ephy_password_record_get_password (record);
  username_field = ephy_password_record_get_username_field (record);
  password_field = ephy_password_record_get_password_field (record);
  modified = ephy_synchronizable_get_server_time_modified (EPHY_SYNCHRONIZABLE (record));

  LOG ("Storing password record for (%s, %s, %s, %s, %s)",
       origin, target_origin, username, username_field, password_field);

  if (username) {
    /* Translators: The first %s is the username and the second one is the
     * security origin where this is happening. Example: gnome@gmail.com and
     * https://mail.google.com. */
    label = g_strdup_printf (_("Password for %s in a form in %s"), username, origin);
  } else {
    /* Translators: The %s is the security origin where this is happening.
     * Example: https://mail.google.com. */
    label = g_strdup_printf (_("Password in a form in %s"), origin);
  }

  attributes = get_attributes_table (ephy_password_record_get_id (record),
                                     origin, target_origin, username,
                                     username_field, password_field,
                                     modified);

  secret_password_storev (EPHY_FORM_PASSWORD_SCHEMA,
                          attributes, NULL, label, password, NULL,
                          (GAsyncReadyCallback)secret_password_store_cb,
                          manage_record_async_data_new (self, record, NULL));

  g_free (label);
  g_hash_table_unref (attributes);
}

static GList *
deduplicate_records (EphyPasswordManager *manager,
                     GList               *records)
{
  GList *newest = records;
  guint64 newest_modified = ephy_password_record_get_time_password_changed (newest->data);

  for (GList *l = records->next; l; l = l->next) {
    guint64 modified = ephy_password_record_get_time_password_changed (l->data);
    if (modified > newest_modified) {
      newest = l;
      newest_modified = modified;
    }
  }

  records = g_list_remove_link (records, newest);

  for (GList *l = records; l; l = l->next)
    ephy_password_manager_forget_record (manager, l->data, NULL, NULL);
  g_list_free_full (records, g_object_unref);

  return newest;
}

static void
update_credentials_cb (GList    *records,
                       gpointer  user_data)
{
  UpdateCredentialsAsyncData *data = (UpdateCredentialsAsyncData *)user_data;
  EphyPasswordRecord *record;

  /* Since we didn't include ID in our query, there could be multiple records
   * returned. We only want to have one saved at a time, so delete the rest.
   */
  if (g_list_length (records) > 1)
    records = deduplicate_records (data->manager, records);

  if (records) {
    record = EPHY_PASSWORD_RECORD (records->data);
    ephy_password_record_set_username (record, data->username);
    ephy_password_record_set_password (record, data->password);
    ephy_password_manager_store_record (data->manager, record);
    g_signal_emit (data->manager, signals[SYNCHRONIZABLE_MODIFIED], 0, record, FALSE);
  } else {
    LOG ("Attempted to update password record that doesn't exist (likely Epiphany bug)");
  }

  update_credentials_async_data_free (data);
}

void
ephy_password_manager_save (EphyPasswordManager *self,
                            const char          *origin,
                            const char          *target_origin,
                            const char          *username,
                            const char          *new_username,
                            const char          *password,
                            const char          *username_field,
                            const char          *password_field,
                            gboolean             is_new)
{
  EphyPasswordRecord *record;
  char *uuid;
  char *id;
  gint64 timestamp;

  g_assert (EPHY_IS_PASSWORD_MANAGER (self));
  g_assert (origin);
  g_assert (target_origin);
  g_assert (password);

  if (!is_new) {
    LOG ("Updating password for (%s, %s, %s, %s, %s)",
         origin, target_origin, username, username_field, password_field);
    ephy_password_manager_query (self, NULL,
                                 origin, target_origin, username,
                                 username_field, password_field,
                                 update_credentials_cb,
                                 update_credentials_async_data_new (self, new_username, password));
    return;
  }

  uuid = g_uuid_string_random ();
  id = g_strdup_printf ("{%s}", uuid);
  timestamp = g_get_real_time () / 1000;
  record = ephy_password_record_new (id, origin, target_origin,
                                     username, password,
                                     username_field, password_field,
                                     timestamp, timestamp);
  ephy_password_manager_store_record (self, record);
  g_signal_emit (self, signals[SYNCHRONIZABLE_MODIFIED], 0, record, FALSE);

  g_free (uuid);
  g_free (id);
  g_object_unref (record);
}

static void
retrieve_secret_cb (GObject        *source_object,
                    GAsyncResult   *result,
                    QueryAsyncData *data)
{
  SecretRetrievable *retrievable = SECRET_RETRIEVABLE (source_object);
  GHashTable *attributes = NULL;
  const char *id;
  const char *origin;
  const char *target_origin;
  const char *username;
  const char *username_field;
  const char *password_field;
  const char *timestamp;
  gint64 created;
  gint64 modified;
  const char *password;
  gint64 server_time_modified;
  EphyPasswordRecord *record;
  SecretValue *value = NULL;
  GError *error = NULL;

  value = secret_retrievable_retrieve_secret_finish (retrievable, result, &error);
  if (!value) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning ("Failed to retrieve password (is the secret service or secrets portal broken?): %s", error->message);
    g_error_free (error);
    goto out;
  }

  attributes = secret_retrievable_get_attributes (retrievable);
  id = g_hash_table_lookup (attributes, ID_KEY);
  origin = g_hash_table_lookup (attributes, ORIGIN_KEY);
  target_origin = g_hash_table_lookup (attributes, TARGET_ORIGIN_KEY);
  username = g_hash_table_lookup (attributes, USERNAME_KEY);
  username_field = g_hash_table_lookup (attributes, USERNAME_FIELD_KEY);
  password_field = g_hash_table_lookup (attributes, PASSWORD_FIELD_KEY);
  timestamp = g_hash_table_lookup (attributes, SERVER_TIME_MODIFIED_KEY);
  created = secret_retrievable_get_created (retrievable);
  modified = secret_retrievable_get_modified (retrievable);

  LOG ("Found password record for (%s, %s, %s, %s, %s)",
       origin, target_origin, username, username_field, password_field);

  if (!id || !origin || !target_origin || !timestamp) {
    LOG ("Password record is corrupted, skipping it...");
    goto out;
  }

  password = secret_value_get_text (value);

  record = ephy_password_record_new (id, origin, target_origin,
                                     username, password,
                                     username_field, password_field,
                                     created * 1000,
                                     modified * 1000);
  server_time_modified = g_ascii_strtod (timestamp, NULL);
  ephy_synchronizable_set_server_time_modified (EPHY_SYNCHRONIZABLE (record),
                                                server_time_modified);
  data->records = g_list_prepend (data->records, record);

out:
  if (value)
    secret_value_unref (value);
  if (attributes)
    g_hash_table_unref (attributes);
  g_object_unref (retrievable);

  if (--data->n_matches == 0) {
    if (data->callback)
      data->callback (data->records, data->user_data);
    query_async_data_free (data);
  }
}

static void
secret_password_search_cb (GObject        *source_object,
                           GAsyncResult   *result,
                           QueryAsyncData *data)
{
  GList *matches;
  GError *error = NULL;

  matches = secret_password_search_finish (result, &error);
  if (!matches) {
    if (error) {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to search secret storage (is the secret service or secrets portal broken?): %s", error->message);
      g_error_free (error);
    }
    if (data->callback)
      data->callback (NULL, data->user_data);
    query_async_data_free (data);
    return;
  }

  data->records = NULL;
  data->n_matches = g_list_length (matches);

  for (GList *l = matches; l; l = l->next) {
    SecretRetrievable *retrievable = SECRET_RETRIEVABLE (l->data);
    secret_retrievable_retrieve_secret (g_object_ref (retrievable),
                                        NULL,
                                        (GAsyncReadyCallback)retrieve_secret_cb,
                                        data);
  }

  g_list_free_full (matches, g_object_unref);
}

void
ephy_password_manager_query (EphyPasswordManager              *self,
                             const char                       *id,
                             const char                       *origin,
                             const char                       *target_origin,
                             const char                       *username,
                             const char                       *username_field,
                             const char                       *password_field,
                             EphyPasswordManagerQueryCallback  callback,
                             gpointer                          user_data)
{
  QueryAsyncData *data;
  GHashTable *attributes;

  g_assert (EPHY_IS_PASSWORD_MANAGER (self));

  LOG ("Querying password records for (%s, %s, %s, %s)",
       origin, username, username_field, password_field);

  attributes = get_attributes_table (id, origin, target_origin, username,
                                     username_field, password_field, -1);
  data = query_async_data_new (callback, user_data);

  secret_password_searchv (EPHY_FORM_PASSWORD_SCHEMA,
                           attributes,
                           SECRET_SEARCH_ALL | SECRET_SEARCH_UNLOCK | SECRET_SEARCH_LOAD_SECRETS,
                           NULL,
                           (GAsyncReadyCallback)secret_password_search_cb,
                           data);

  g_hash_table_unref (attributes);
}

gboolean
ephy_password_manager_find (EphyPasswordManager *self,
                            const char          *origin,
                            const char          *target_origin,
                            const char          *username,
                            const char          *username_field,
                            const char          *password_field)
{
  GHashTable *attributes;
  g_autoptr (GList) list = NULL;

  g_assert (EPHY_IS_PASSWORD_MANAGER (self));

  LOG ("Querying password records for (%s, %s, %s, %s)",
       origin, username, username_field, password_field);

  attributes = get_attributes_table (NULL, origin, target_origin, username,
                                     username_field, password_field, -1);

  list = secret_password_searchv_sync (EPHY_FORM_PASSWORD_SCHEMA,
                                       attributes,
                                       SECRET_SEARCH_ALL | SECRET_SEARCH_UNLOCK | SECRET_SEARCH_LOAD_SECRETS,
                                       NULL,
                                       NULL);

  g_hash_table_unref (attributes);

  return !!list;
}

static void
secret_password_clear_cb (GObject      *source_object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  g_autoptr (GError) error = NULL;
  ManageRecordAsyncData *data = user_data;

  secret_password_clear_finish (result, &error);
  if (error) {
    if (data && data->task)
      g_task_return_error (data->task, error);
    else if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning ("Failed to clear secrets (is the secret service or secrets portal broken?): %s", error->message);
    g_clear_pointer (&data, manage_record_async_data_free);
    return;
  }

  if (data) {
    if (data->record)
      ephy_password_manager_store_record (data->manager, data->record);
    if (data->task)
      g_task_return_boolean (data->task, TRUE);
  }

  g_clear_pointer (&data, manage_record_async_data_free);
}

static void
ephy_password_manager_forget_record (EphyPasswordManager *self,
                                     EphyPasswordRecord  *record,
                                     EphyPasswordRecord  *replacement,
                                     GTask               *task)
{
  GHashTable *attributes;
  ManageRecordAsyncData *clear_cb_data = NULL;

  g_assert (EPHY_IS_PASSWORD_MANAGER (self));
  g_assert (EPHY_IS_PASSWORD_RECORD (record));

  attributes = get_attributes_table (ephy_password_record_get_id (record),
                                     ephy_password_record_get_origin (record),
                                     ephy_password_record_get_target_origin (record),
                                     ephy_password_record_get_username (record),
                                     ephy_password_record_get_username_field (record),
                                     ephy_password_record_get_password_field (record),
                                     -1);

  clear_cb_data = manage_record_async_data_new (self, replacement, task);

  LOG ("Forgetting password record for (%s, %s, %s, %s, %s)",
       ephy_password_record_get_origin (record),
       ephy_password_record_get_target_origin (record),
       ephy_password_record_get_username (record),
       ephy_password_record_get_username_field (record),
       ephy_password_record_get_password_field (record));

  secret_password_clearv (EPHY_FORM_PASSWORD_SCHEMA, attributes, NULL,
                          (GAsyncReadyCallback)secret_password_clear_cb,
                          clear_cb_data);

  ephy_password_manager_cache_remove (self,
                                      ephy_password_record_get_origin (record),
                                      ephy_password_record_get_username (record));
  g_hash_table_unref (attributes);
}

static void
forget_cb (GList    *records,
           gpointer  data)
{
  GTask *task = data;
  EphyPasswordManager *self = EPHY_PASSWORD_MANAGER (g_task_get_source_object (task));
  EphyPasswordRecord *record;

  g_assert (g_list_length (records) == 1);

  record = EPHY_PASSWORD_RECORD (records->data);
  g_signal_emit (self, signals[SYNCHRONIZABLE_DELETED], 0, record);
  ephy_password_manager_forget_record (self, record, NULL, task);
}

gboolean
ephy_password_manager_forget_finish (EphyPasswordManager  *self,
                                     GAsyncResult         *result,
                                     GError              **error)
{
  g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

void
ephy_password_manager_forget (EphyPasswordManager *self,
                              const char          *id,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  GTask *task = NULL;

  g_assert (EPHY_IS_PASSWORD_MANAGER (self));
  g_assert (id);

  task = g_task_new (self, cancellable, callback, user_data);

  /* synchronizable-deleted signal needs an EphySynchronizable object,
  * therefore we need to obtain the password record first and then emit
  * the signal before clearing the password from the secret schema. */
  ephy_password_manager_query (self, id,
                               NULL, NULL, NULL, NULL, NULL,
                               forget_cb, task);
}

static void
forget_all_cb (GList    *records,
               gpointer  user_data)
{
  EphyPasswordManager *self = EPHY_PASSWORD_MANAGER (user_data);
  GHashTable *attributes;

  attributes = secret_attributes_build (EPHY_FORM_PASSWORD_SCHEMA, NULL);
  secret_password_clearv (EPHY_FORM_PASSWORD_SCHEMA, attributes, NULL,
                          (GAsyncReadyCallback)secret_password_clear_cb, NULL);

  for (GList *l = records; l && l->data; l = l->next)
    g_signal_emit (self, signals[SYNCHRONIZABLE_DELETED], 0, l->data);

  ephy_password_manager_cache_clear (self);

  g_hash_table_unref (attributes);
}

void
ephy_password_manager_forget_all (EphyPasswordManager *self)
{
  g_assert (EPHY_IS_PASSWORD_MANAGER (self));

  /* synchronizable-deleted signal needs an EphySynchronizable object, therefore
   * we need to obtain the password records first and emit the signal for each
   * one before clearing the secret schema. */
  ephy_password_manager_query (self, NULL, NULL, NULL, NULL, NULL, NULL,
                               forget_all_cb, self);
}

static const char *
synchronizable_manager_get_collection_name (EphySynchronizableManager *manager)
{
  return ephy_sync_utils_sync_with_firefox () ? "passwords" : "ephy-passwords";
}

static GType
synchronizable_manager_get_synchronizable_type (EphySynchronizableManager *manager)
{
  return EPHY_TYPE_PASSWORD_RECORD;
}

static gboolean
synchronizable_manager_is_initial_sync (EphySynchronizableManager *manager)
{
  return ephy_sync_utils_get_passwords_sync_is_initial ();
}

static void
synchronizable_manager_set_is_initial_sync (EphySynchronizableManager *manager,
                                            gboolean                   is_initial)
{
  ephy_sync_utils_set_passwords_sync_is_initial (is_initial);
}

static gint64
synchronizable_manager_get_sync_time (EphySynchronizableManager *manager)
{
  return ephy_sync_utils_get_passwords_sync_time ();
}

static void
synchronizable_manager_set_sync_time (EphySynchronizableManager *manager,
                                      gint64                     sync_time)
{
  ephy_sync_utils_set_passwords_sync_time (sync_time);
}

static void
synchronizable_manager_add (EphySynchronizableManager *manager,
                            EphySynchronizable        *synchronizable)
{
  EphyPasswordManager *self = EPHY_PASSWORD_MANAGER (manager);
  EphyPasswordRecord *record = EPHY_PASSWORD_RECORD (synchronizable);

  ephy_password_manager_store_record (self, record);
}

static void
synchronizable_manager_remove (EphySynchronizableManager *manager,
                               EphySynchronizable        *synchronizable)
{
  EphyPasswordManager *self = EPHY_PASSWORD_MANAGER (manager);
  EphyPasswordRecord *record = EPHY_PASSWORD_RECORD (synchronizable);

  ephy_password_manager_forget_record (self, record, NULL, NULL);
}

static void
replace_existing_cb (GList    *records,
                     gpointer  user_data)
{
  ManageRecordAsyncData *data = (ManageRecordAsyncData *)user_data;

  g_assert (g_list_length (records) == 1);

  ephy_password_manager_forget_record (data->manager, records->data, data->record, NULL);
  manage_record_async_data_free (data);
}

static void
ephy_password_manager_replace_existing (EphyPasswordManager *self,
                                        EphyPasswordRecord  *record)
{
  g_assert (EPHY_IS_PASSWORD_MANAGER (self));
  g_assert (EPHY_IS_PASSWORD_RECORD (record));

  ephy_password_manager_query (self, ephy_password_record_get_id (record),
                               NULL, NULL, NULL, NULL, NULL,
                               replace_existing_cb,
                               manage_record_async_data_new (self, record, NULL));
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
get_record_by_id (GList      *records,
                  const char *id)
{
  g_assert (id);

  for (GList *l = records; l && l->data; l = l->next) {
    if (!g_strcmp0 (ephy_password_record_get_id (l->data), id))
      return l->data;
  }

  return NULL;
}

static EphyPasswordRecord *
get_record_by_parameters (GList      *records,
                          const char *origin,
                          const char *target_origin,
                          const char *username,
                          const char *username_field,
                          const char *password_field)
{
  for (GList *l = records; l && l->data; l = l->next) {
    if (!g_strcmp0 (ephy_password_record_get_username (l->data), username) &&
        !g_strcmp0 (ephy_password_record_get_origin (l->data), origin) &&
        !g_strcmp0 (ephy_password_record_get_target_origin (l->data), target_origin) &&
        !g_strcmp0 (ephy_password_record_get_username_field (l->data), username_field) &&
        !g_strcmp0 (ephy_password_record_get_password_field (l->data), password_field))
      return l->data;
  }

  return NULL;
}


static GList *
delete_record_by_id (GList      *records,
                     const char *id)
{
  for (GList *l = records; l && l->data; l = l->next) {
    if (!g_strcmp0 (ephy_password_record_get_id (l->data), id)) {
      g_object_unref (l->data);
      return g_list_delete_link (records, l);
    }
  }

  return records;
}

static GPtrArray *
ephy_password_manager_handle_initial_merge (EphyPasswordManager *self,
                                            GList               *local_records,
                                            GList               *remote_records)
{
  EphyPasswordRecord *record;
  GHashTable *dont_upload;
  GPtrArray *to_upload;
  const char *remote_id;
  const char *remote_origin;
  const char *remote_target_origin;
  const char *remote_username;
  const char *remote_password;
  const char *remote_username_field;
  const char *remote_password_field;
  guint64 remote_timestamp;
  guint64 local_timestamp;
  gint64 remote_server_time_modified;
  gint64 local_server_time_modified;

  g_assert (EPHY_IS_PASSWORD_MANAGER (self));

  /* A saved password record is uniquely identified by its ID or by its tuple
   * of (origin, username, username field, password field). When importing
   * password records from server, we may encounter duplicates either by ID
   * or by mentioned tuple. We start from the assumption that same ID means
   * same tuple but same tuple does not necessarily mean same ID. This is what
   * our merge logic is based on.
   */
  to_upload = g_ptr_array_new_with_free_func (g_object_unref);
  dont_upload = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  for (GList *l = remote_records; l && l->data; l = l->next) {
    remote_id = ephy_password_record_get_id (l->data);
    remote_origin = ephy_password_record_get_origin (l->data);
    remote_target_origin = ephy_password_record_get_target_origin (l->data);
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
          ephy_password_manager_forget_record (self, record, l->data, NULL);
          g_hash_table_add (dont_upload, g_strdup (remote_id));
        }
      }
    } else {
      record = get_record_by_parameters (local_records,
                                         remote_origin,
                                         remote_target_origin,
                                         remote_username,
                                         remote_username_field,
                                         remote_password_field);
      if (record) {
        /* Different id, same tuple. Keep the most recent modified. */
        local_timestamp = ephy_password_record_get_time_password_changed (record);
        if (local_timestamp > remote_timestamp) {
          /* Local record is newer. Keep it, upload it and delete remote record from server. */
          g_signal_emit (self, signals[SYNCHRONIZABLE_DELETED], 0, l->data);
        } else {
          /* Remote record is newer. Forget local record and store remote record. */
          ephy_password_manager_forget_record (self, record, l->data, NULL);
          g_hash_table_add (dont_upload, g_strdup (remote_id));
        }
      } else {
        record = get_record_by_parameters (local_records,
                                           remote_origin,
                                           remote_origin,
                                           remote_username,
                                           remote_username_field,
                                           remote_password_field);
        if (record) {
          /* A leftover from migration: the local record has incorrect target_origin
           * Replace it with remote record */
          ephy_password_manager_forget_record (self, record, l->data, NULL);
          g_hash_table_add (dont_upload, g_strdup (ephy_password_record_get_id (record)));
        } else {
          /* Different id, different tuple. This is a new record. */
          ephy_password_manager_store_record (self, l->data);
          g_hash_table_add (dont_upload, g_strdup (remote_id));
        }
      }
    }
  }

  /* Set the remaining local records to be uploaded to server. */
  for (GList *l = local_records; l && l->data; l = l->next) {
    record = EPHY_PASSWORD_RECORD (l->data);
    if (!g_hash_table_contains (dont_upload, ephy_password_record_get_id (record)))
      g_ptr_array_add (to_upload, g_object_ref (record));
  }

  g_hash_table_unref (dont_upload);

  return to_upload;
}

static GPtrArray *
ephy_password_manager_handle_regular_merge (EphyPasswordManager  *self,
                                            GList               **local_records,
                                            GList                *deleted_records,
                                            GList                *updated_records)
{
  EphyPasswordRecord *record;
  GPtrArray *to_upload;
  const char *remote_id;
  const char *remote_origin;
  const char *remote_target_origin;
  const char *remote_username;
  const char *remote_username_field;
  const char *remote_password_field;
  guint64 remote_timestamp;
  guint64 local_timestamp;

  g_assert (EPHY_IS_PASSWORD_MANAGER (self));

  to_upload = g_ptr_array_new_with_free_func (g_object_unref);

  for (GList *l = deleted_records; l && l->data; l = l->next) {
    remote_id = ephy_password_record_get_id (l->data);
    record = get_record_by_id (*local_records, remote_id);
    if (record) {
      ephy_password_manager_forget_record (self, record, NULL, NULL);
      *local_records = delete_record_by_id (*local_records, remote_id);
    }
  }

  /* See comment in ephy_password_manager_handle_initial_merge. */
  for (GList *l = updated_records; l && l->data; l = l->next) {
    remote_id = ephy_password_record_get_id (l->data);
    remote_origin = ephy_password_record_get_origin (l->data);
    remote_target_origin = ephy_password_record_get_target_origin (l->data);
    remote_username = ephy_password_record_get_username (l->data);
    remote_username_field = ephy_password_record_get_username_field (l->data);
    remote_password_field = ephy_password_record_get_password_field (l->data);
    remote_timestamp = ephy_password_record_get_time_password_changed (l->data);

    record = get_record_by_id (*local_records, remote_id);
    if (record) {
      /* Same id. Overwrite local record. */
      ephy_password_manager_forget_record (self, record, l->data, NULL);
    } else {
      record = get_record_by_parameters (*local_records,
                                         remote_origin,
                                         remote_target_origin,
                                         remote_username,
                                         remote_username_field,
                                         remote_password_field);
      if (record) {
        /* Different id, same tuple. Keep the most recent modified. */
        local_timestamp = ephy_password_record_get_time_password_changed (record);
        if (local_timestamp > remote_timestamp) {
          /* Local record is newer. Keep it, upload it and delete remote record from server. */
          g_ptr_array_add (to_upload, g_object_ref (record));
          g_signal_emit (self, signals[SYNCHRONIZABLE_DELETED], 0, l->data);
        } else {
          /* Remote record is newer. Forget local record and store remote record. */
          ephy_password_manager_forget_record (self, record, l->data, NULL);
        }
      } else {
        /* Different id, different tuple. This is a new record. */
        ephy_password_manager_store_record (self, l->data);
      }
    }
  }

  return to_upload;
}

static void
merge_cb (GList    *records,
          gpointer  user_data)
{
  MergePasswordsAsyncData *data = (MergePasswordsAsyncData *)user_data;
  GPtrArray *to_upload;

  if (data->is_initial)
    to_upload = ephy_password_manager_handle_initial_merge (data->manager, records,
                                                            data->remotes_updated);
  else
    to_upload = ephy_password_manager_handle_regular_merge (data->manager, &records,
                                                            data->remotes_deleted,
                                                            data->remotes_updated);

  data->callback (to_upload, data->user_data);

  merge_passwords_async_data_free (data);
}

static void
synchronizable_manager_merge (EphySynchronizableManager              *manager,
                              gboolean                                is_initial,
                              GList                                  *remotes_deleted,
                              GList                                  *remotes_updated,
                              EphySynchronizableManagerMergeCallback  callback,
                              gpointer                                user_data)
{
  EphyPasswordManager *self = EPHY_PASSWORD_MANAGER (manager);

  ephy_password_manager_query (self, NULL, NULL, NULL, NULL, NULL, NULL,
                               merge_cb,
                               merge_passwords_async_data_new (self,
                                                               is_initial,
                                                               g_list_copy_deep (remotes_deleted, (GCopyFunc)g_object_ref, NULL),
                                                               g_list_copy_deep (remotes_updated, (GCopyFunc)g_object_ref, NULL),
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
