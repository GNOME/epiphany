/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2016 Gabriel Ivascu <ivascu.gabriel59@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ephy-debug.h"
#include "ephy-sync-secret.h"

#include <glib/gi18n.h>

const SecretSchema *
ephy_sync_secret_get_token_schema (void)
{
  static const SecretSchema schema = {
    "org.epiphany.SyncToken", SECRET_SCHEMA_NONE,
    {
      { EMAIL_KEY, SECRET_SCHEMA_ATTRIBUTE_STRING },
      { TOKEN_KEY, SECRET_SCHEMA_ATTRIBUTE_STRING },
      { "NULL", 0 },
    }
  };
  return &schema;
}

static void
forget_all_tokens_cb (SecretService *service,
                      GAsyncResult  *result,
                      GTask         *task)
{
  GError *error = NULL;

  secret_service_clear_finish (service, result, &error);

  if (error != NULL) {
LOG ("%s:%d", __func__, __LINE__);
    g_task_return_error (task, error);
  }
  else {
LOG ("%s:%d", __func__, __LINE__);
    g_task_return_boolean (task, TRUE);
  }

  g_object_unref (task);
}

static void
load_tokens_cb (SecretService *service,
                GAsyncResult  *result,
                gpointer       user_data)
{
  EphySyncService *sync_service;
  GHashTable *attributes;
  SecretItem *secret_item;
  SecretValue *secret_value;
  GList *matches;
  GList *tmp;
  const gchar *emailUTF8;
  const gchar *token_name;
  const gchar *token_value_hex;

  sync_service = EPHY_SYNC_SERVICE (user_data);
  matches = secret_service_search_finish (service, result, NULL);

  for (tmp = matches; tmp != NULL; tmp = tmp->next) {
    secret_item = tmp->data;

    attributes = secret_item_get_attributes (secret_item);
    emailUTF8 = g_hash_table_lookup (attributes, EMAIL_KEY);
    token_name = g_hash_table_lookup (attributes, TOKEN_KEY);
    secret_value = secret_item_get_secret (secret_item);
    token_value_hex = secret_value_get_text (secret_value);

    if (g_strcmp0 (emailUTF8, sync_service->user_email) == 0) {
      ephy_sync_service_set_token (sync_service, token_name, token_value_hex);
LOG ("[%d] Set token %s with value %s for email: %s", __LINE__, token_name, token_value_hex, emailUTF8);
    }

    g_hash_table_unref (attributes);
  }

LOG ("%s:%d", __func__, __LINE__);
  g_list_free_full (matches, g_object_unref);
}

static void
store_token_cb (SecretService *service,
                GAsyncResult  *result,
                GTask         *task)
{
  GError *error = NULL;

  secret_service_store_finish (service, result, &error);

  if (error != NULL) {
LOG ("%s:%d", __func__, __LINE__);
    g_task_return_error (task, error);
  }
  else {
LOG ("%s:%d", __func__, __LINE__);
    g_task_return_boolean (task, TRUE);
  }

  g_object_unref (task);
}

void
ephy_sync_secret_forget_all_tokens (GAsyncReadyCallback callback,
                                    gpointer            user_data)
{
  GHashTable *attributes;
  GTask *task;

LOG ("%s:%d", __func__, __LINE__);

  task = g_task_new (NULL, NULL, callback, user_data);
  attributes = secret_attributes_build (EPHY_SYNC_TOKEN_SCHEMA, NULL);
  secret_service_clear (NULL,
                        EPHY_SYNC_TOKEN_SCHEMA,
                        attributes,
                        NULL,
                        (GAsyncReadyCallback) forget_all_tokens_cb,
                        g_object_ref (task));

  g_hash_table_unref (attributes);
  g_object_unref (task);
}

void
ephy_sync_secret_load_tokens (EphySyncService *sync_service)
{
  GHashTable *attributes;

  attributes = secret_attributes_build (EPHY_SYNC_TOKEN_SCHEMA, NULL);

  secret_service_search (NULL,
                         EPHY_SYNC_TOKEN_SCHEMA,
                         attributes,
                         SECRET_SEARCH_ALL | SECRET_SEARCH_UNLOCK | SECRET_SEARCH_LOAD_SECRETS,
                         NULL,
                         (GAsyncReadyCallback) load_tokens_cb,
                         sync_service);

  g_hash_table_unref (attributes);
}

void
ephy_sync_secret_store_token (const gchar         *emailUTF8,
                              const gchar         *token_name,
                              const gchar         *token_value,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  SecretValue *secret_value;
  GHashTable *attributes;
  GTask *task;
  gchar *label;

  g_return_if_fail (token_name);
  g_return_if_fail (token_value);

LOG ("%s:%d", __func__, __LINE__);

  task = g_task_new (NULL, NULL, callback, user_data);
  secret_value = secret_value_new (token_value, -1, "text/plain");
  attributes = secret_attributes_build (EPHY_SYNC_TOKEN_SCHEMA,
                                        EMAIL_KEY, emailUTF8,
                                        TOKEN_KEY, token_name,
                                        NULL);
LOG ("size: %u", g_hash_table_size (attributes));
  /* Translators: The %s is the name of the token whose value is stored.
   * Example: quickStretchedPW or authPW
   */
  label = g_strdup_printf (_("Token value for %s token"), token_name);

  secret_service_store (NULL,
                        EPHY_SYNC_TOKEN_SCHEMA,
                        attributes,
                        NULL,
                        label,
                        secret_value,
                        NULL,
                        (GAsyncReadyCallback) store_token_cb,
                        g_object_ref (task));

  g_free (label);
  secret_value_unref (secret_value);
  g_hash_table_unref (attributes);
  g_object_unref (task);
}
