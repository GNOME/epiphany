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
#include "ephy-uri-helpers.h"

#include <glib/gi18n.h>

const SecretSchema *
ephy_password_manager_get_password_schema (void)
{
  static const SecretSchema schema = {
    "org.epiphany.FormPassword", SECRET_SCHEMA_NONE,
    {
      { URI_KEY, SECRET_SCHEMA_ATTRIBUTE_STRING },
      { FORM_USERNAME_KEY, SECRET_SCHEMA_ATTRIBUTE_STRING },
      { FORM_PASSWORD_KEY, SECRET_SCHEMA_ATTRIBUTE_STRING },
      { USERNAME_KEY, SECRET_SCHEMA_ATTRIBUTE_STRING },
      { "NULL", 0 },
    }
  };
  return &schema;
}

struct _EphyPasswordManager {
  GObject parent_instance;

  GSList *records;
};

G_DEFINE_TYPE (EphyPasswordManager, ephy_password_manager, G_TYPE_OBJECT)

typedef struct {
  EphyPasswordManagerQueryCallback callback;
  gpointer user_data;
} QueryAsyncData;

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
  g_slice_free (QueryAsyncData, data);
}

static void
ephy_password_manager_dispose (GObject *object)
{
  EphyPasswordManager *self = EPHY_PASSWORD_MANAGER (object);

  g_slist_free_full (self->records, g_object_unref);
  self->records = NULL;

  G_OBJECT_CLASS (ephy_password_manager_parent_class)->dispose (object);
}

static void
ephy_password_manager_class_init (EphyPasswordManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ephy_password_manager_dispose;
}

static void
secret_service_search_all_cb (SecretService       *service,
                              GAsyncResult        *result,
                              EphyPasswordManager *self)
{
  GList *matches = NULL;
  GError *error = NULL;

  matches = secret_service_search_finish (service, result, &error);
  if (error) {
    g_warning ("Failed to search secrets in password schema: %s", error->message);
    g_error_free (error);
    return;
  }

  for (GList *l = matches; l && l->data; l = l->next) {
    SecretItem *item = (SecretItem *)l->data;
    SecretValue *value = secret_item_get_secret (item);
    GHashTable *attributes = secret_item_get_attributes (item);
    const char *uri = g_hash_table_lookup (attributes, URI_KEY);
    const char *form_username = g_hash_table_lookup (attributes, FORM_USERNAME_KEY);
    const char *form_password = g_hash_table_lookup (attributes, FORM_PASSWORD_KEY);
    const char *username = g_hash_table_lookup (attributes, USERNAME_KEY);
    const char *password = secret_value_get (value, NULL);
    EphyPasswordRecord *record;

    LOG ("Loading password record for uri=%s, form_username=%s, form_password=%s, username=%s",
         uri, form_username, form_password, username);

    record = ephy_password_record_new (uri, form_username, form_password, username, password);
    self->records = g_slist_prepend (self->records, record);

    secret_value_unref (value);
    g_hash_table_unref (attributes);
  }

  g_list_free_full (matches, g_object_unref);
}

static void
ephy_password_manager_load_passwords (EphyPasswordManager *self)
{
  GHashTable *attributes;

  g_assert (EPHY_IS_PASSWORD_MANAGER (self));

  attributes = secret_attributes_build (EPHY_FORM_PASSWORD_SCHEMA, NULL);
  secret_service_search (NULL,
                         EPHY_FORM_PASSWORD_SCHEMA,
                         attributes,
                         SECRET_SEARCH_ALL | SECRET_SEARCH_UNLOCK | SECRET_SEARCH_LOAD_SECRETS,
                         NULL,
                         (GAsyncReadyCallback)secret_service_search_all_cb,
                         self);

  g_hash_table_unref (attributes);
}

static void
ephy_password_manager_init (EphyPasswordManager *self)
{
  ephy_password_manager_load_passwords (self);
}

EphyPasswordManager *
ephy_password_manager_new (void)
{
  return EPHY_PASSWORD_MANAGER (g_object_new (EPHY_TYPE_PASSWORD_MANAGER, NULL));
}

GSList *
ephy_password_manager_get_cached_by_uri (EphyPasswordManager *self,
                                         const char          *uri)
{
  GSList *list = NULL;
  char *origin;

  g_return_val_if_fail (EPHY_IS_PASSWORD_MANAGER (self), NULL);
  g_return_val_if_fail (uri, NULL);

  origin = ephy_uri_to_security_origin (uri);

  for (GSList *l = self->records; l && l->data; l = l->next) {
    EphyPasswordRecord *record = EPHY_PASSWORD_RECORD (l->data);

    if (!g_strcmp0 (origin, ephy_password_record_get_origin (record)))
      list = g_slist_prepend (list, record);
  }

  g_free (origin);

  return list;
}

static EphyPasswordRecord *
ephy_password_manager_get_cached (EphyPasswordManager *self,
                                  const char          *origin,
                                  const char          *form_username,
                                  const char          *form_password,
                                  const char          *username)
{
  g_assert (EPHY_IS_PASSWORD_MANAGER (self));

  for (GSList *l = self->records; l && l->data; l = l->next) {
    EphyPasswordRecord *record = EPHY_PASSWORD_RECORD (l->data);

    if (!g_strcmp0 (origin, ephy_password_record_get_origin (record)) &&
        !g_strcmp0 (form_username, ephy_password_record_get_form_username (record)) &&
        !g_strcmp0 (form_password, ephy_password_record_get_form_password (record)) &&
        !g_strcmp0 (username, ephy_password_record_get_username (record)))
      return record;
  }

  return NULL;
}

void
ephy_password_manager_save (EphyPasswordManager *self,
                            const char          *uri,
                            const char          *form_username,
                            const char          *form_password,
                            const char          *username,
                            const char          *password)
{
  EphyPasswordRecord *record = NULL;
  char *origin;

  g_return_if_fail (EPHY_IS_PASSWORD_MANAGER (self));
  g_return_if_fail (uri);
  g_return_if_fail (password);
  g_return_if_fail (!form_username || username);
  g_return_if_fail (!form_password || password);

  origin = ephy_uri_to_security_origin (uri);

  /* Check if there is an existing record for the given parameters.
   * If there is, don't create a new record, only update password. */
  record = ephy_password_manager_get_cached (self, origin,
                                             form_username, form_password,
                                             username);
  if (record) {
    ephy_password_record_set_password (record, password);
  } else {
    record = ephy_password_record_new (origin, form_username, form_password,
                                       username, password);
    self->records = g_slist_prepend (self->records, record);
  }

  /* Add/overwrite the password in the secret schema. */
  ephy_password_manager_store (origin,
                               form_username, form_password,
                               username, password,
                               NULL, NULL);

  g_free (origin);
}

static GHashTable *
get_attributes_table (const char *uri,
                      const char *form_username,
                      const char *form_password,
                      const char *username)
{
  GHashTable *attributes = secret_attributes_build (EPHY_FORM_PASSWORD_SCHEMA, NULL);

  if (uri)
    g_hash_table_insert (attributes, g_strdup (URI_KEY), ephy_uri_to_security_origin (uri));
  if (username)
    g_hash_table_insert (attributes, g_strdup (USERNAME_KEY), g_strdup (username));
  if (form_username)
    g_hash_table_insert (attributes, g_strdup (FORM_USERNAME_KEY), g_strdup (form_username));
  if (form_password)
    g_hash_table_insert (attributes, g_strdup (FORM_PASSWORD_KEY), g_strdup (form_password));

  return attributes;
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
    const char *origin = g_hash_table_lookup (attributes, URI_KEY);
    const char *form_username = g_hash_table_lookup (attributes, FORM_USERNAME_KEY);
    const char *form_password = g_hash_table_lookup (attributes, FORM_PASSWORD_KEY);
    const char *username = g_hash_table_lookup (attributes, USERNAME_KEY);
    const char *password = secret_value_get (value, NULL);
    EphyPasswordRecord *record;

    LOG ("Query found password record for (%s, %s, %s, %s)",
         origin, form_username, form_password, username);

    record = ephy_password_record_new (origin, form_username, form_password, username, password);
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
                             const char                       *form_username,
                             const char                       *form_password,
                             const char                       *username,
                             EphyPasswordManagerQueryCallback  callback,
                             gpointer                          user_data)
{
  QueryAsyncData *data;
  GHashTable *attributes;

  g_return_if_fail (EPHY_IS_PASSWORD_MANAGER (self));

  LOG ("Querying password records for uri=%s, form_username=%s, form_password=%s, username=%s",
       uri, form_username, form_password, username);

  attributes = get_attributes_table (uri, form_username, form_password, username);
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

void
ephy_password_manager_store (const char          *uri,
                             const char          *form_username,
                             const char          *form_password,
                             const char          *username,
                             const char          *password,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  SecretValue *value;
  GHashTable *attributes;
  GTask *task;
  char *origin;
  char *label;

  g_return_if_fail (uri);
  g_return_if_fail (password);
  g_return_if_fail (!form_username || username);
  g_return_if_fail (!form_password || password);

  task = g_task_new (NULL, NULL, callback, user_data);
  value = secret_value_new (password, -1, "text/plain");
  attributes = get_attributes_table (uri, form_username, form_password, username);
  origin = ephy_uri_to_security_origin (uri);

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

  LOG ("Storing password record for uri=%s, form_username=%s, form_password=%s, username=%s",
       uri, form_username, form_password, username);

  secret_service_store (NULL, EPHY_FORM_PASSWORD_SCHEMA,
                        attributes, NULL, label, value, NULL,
                        (GAsyncReadyCallback)secret_service_store_cb,
                        g_object_ref (task));

  g_free (label);
  g_free (origin);
  secret_value_unref (value);
  g_hash_table_unref (attributes);
  g_object_unref (task);
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
  }
}

void
ephy_password_manager_forget (EphyPasswordManager *self,
                              const char          *origin,
                              const char          *form_username,
                              const char          *form_password,
                              const char          *username)
{
  EphyPasswordRecord *record;
  GHashTable *attributes;

  g_return_if_fail (EPHY_IS_PASSWORD_MANAGER (self));
  g_return_if_fail (origin);
  g_return_if_fail (form_password);
  g_return_if_fail (!form_username || username);

  attributes = get_attributes_table (origin, form_username, form_password, username);
  secret_service_clear (NULL, EPHY_FORM_PASSWORD_SCHEMA, attributes, NULL,
                        (GAsyncReadyCallback)secret_service_clear_cb, NULL);

  /* If record is in cache, delete it. */
  record = ephy_password_manager_get_cached (self, origin,
                                             form_username, form_password,
                                             username);
  if (record) {
    self->records = g_slist_remove (self->records, record);
    g_object_unref (record);
  }
}

void
ephy_password_manager_forget_all (EphyPasswordManager *self)
{
  GHashTable *attributes;

  g_return_if_fail (EPHY_IS_PASSWORD_MANAGER (self));

  attributes = secret_attributes_build (EPHY_FORM_PASSWORD_SCHEMA, NULL);
  secret_service_clear (NULL, EPHY_FORM_PASSWORD_SCHEMA, attributes, NULL,
                        (GAsyncReadyCallback)secret_service_clear_cb, NULL);

  g_slist_free_full (self->records, g_object_unref);
  self->records = NULL;

  g_hash_table_unref (attributes);
}
