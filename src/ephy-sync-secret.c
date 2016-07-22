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

#include "ephy-sync-secret.h"

#include <glib/gi18n.h>

const SecretSchema *
ephy_sync_secret_get_token_schema (void)
{
  static const SecretSchema schema = {
    "org.epiphany.SyncToken", SECRET_SCHEMA_NONE,
    {
      { EMAIL_KEY,      SECRET_SCHEMA_ATTRIBUTE_STRING },
      { TOKEN_TYPE_KEY, SECRET_SCHEMA_ATTRIBUTE_INTEGER },
      { TOKEN_NAME_KEY, SECRET_SCHEMA_ATTRIBUTE_STRING },
      { "NULL", 0 },
    }
  };
  return &schema;
}

static void
forget_all_tokens_cb (SecretService *service,
                      GAsyncResult  *result,
                      gpointer       user_data)
{
  GError *error = NULL;

  secret_service_clear_finish (service, result, &error);

  if (error != NULL)
    g_warning ("Failed to clear token secret schema: %s", error->message);
}

static void
store_token_cb (SecretService *service,
                GAsyncResult  *result,
                gpointer       user_data)
{
  GError *error = NULL;

  secret_service_store_finish (service, result, &error);

  if (error != NULL)
    g_warning ("Failed to store token in secret schema: %s", error->message);
}

void
ephy_sync_secret_forget_all_tokens (void)
{
  GHashTable *attributes;

  attributes = secret_attributes_build (EPHY_SYNC_TOKEN_SCHEMA, NULL);
  secret_service_clear (NULL,
                        EPHY_SYNC_TOKEN_SCHEMA,
                        attributes,
                        NULL,
                        (GAsyncReadyCallback) forget_all_tokens_cb,
                        NULL);

  g_hash_table_unref (attributes);
}

void
ephy_sync_secret_load_tokens (EphySyncService *sync_service)
{
  SecretItem *secret_item;
  SecretValue *secret_value;
  GHashTable *attributes;
  GError *error = NULL;
  GList *matches;
  GList *tmp;
  EphySyncServiceTokenType type;
  const gchar *emailUTF8;
  const gchar *value;
  gchar *user_email;

  user_email = ephy_sync_service_get_user_email (sync_service);
  attributes = secret_attributes_build (EPHY_SYNC_TOKEN_SCHEMA, NULL);

  /* Do this synchronously so the tokens will be available immediately */
  matches = secret_service_search_sync (NULL,
                                        EPHY_SYNC_TOKEN_SCHEMA,
                                        attributes,
                                        SECRET_SEARCH_ALL | SECRET_SEARCH_UNLOCK | SECRET_SEARCH_LOAD_SECRETS,
                                        NULL,
                                        &error);

  for (tmp = matches; tmp != NULL; tmp = tmp->next) {
    secret_item = tmp->data;

    attributes = secret_item_get_attributes (secret_item);
    emailUTF8 = g_hash_table_lookup (attributes, EMAIL_KEY);
    type = g_ascii_strtoull (g_hash_table_lookup (attributes, TOKEN_TYPE_KEY),
                             NULL, 10);
    secret_value = secret_item_get_secret (secret_item);
    value = secret_value_get_text (secret_value);

    /* Sanity check */
    if (g_strcmp0 (emailUTF8, user_email))
      continue;

    ephy_sync_service_set_token (sync_service, g_strdup (value), type);

    g_hash_table_unref (attributes);
  }

  g_list_free_full (matches, g_object_unref);
}

void
ephy_sync_secret_store_token (const gchar              *emailUTF8,
                              gchar                    *value,
                              EphySyncServiceTokenType  type)
{
  SecretValue *secret_value;
  GHashTable *attributes;
  const gchar *name;
  gchar *label;

  g_return_if_fail (emailUTF8);
  g_return_if_fail (value);

  name = ephy_sync_service_token_name_from_type (type);
  secret_value = secret_value_new (value, -1, "text/plain");
  attributes = secret_attributes_build (EPHY_SYNC_TOKEN_SCHEMA,
                                        EMAIL_KEY, emailUTF8,
                                        TOKEN_TYPE_KEY, type,
                                        TOKEN_NAME_KEY, name,
                                        NULL);
  /* Translators: The %s is the name of the token whose value is stored.
   * Example: quickStretchedPW or authPW
   */
  label = g_strdup_printf (_("Token value for %s token"), name);

  secret_service_store (NULL,
                        EPHY_SYNC_TOKEN_SCHEMA,
                        attributes,
                        NULL,
                        label,
                        secret_value,
                        NULL,
                        (GAsyncReadyCallback) store_token_cb,
                        NULL);

  g_free (label);
  secret_value_unref (secret_value);
  g_hash_table_unref (attributes);
}
