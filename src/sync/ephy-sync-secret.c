/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2016 Gabriel Ivascu <ivascu.gabriel59@gmail.com>
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
#include "ephy-sync-secret.h"

#include <glib/gi18n.h>

GQuark sync_secret_error_quark (void);
G_DEFINE_QUARK (sync-secret-error-quark, sync_secret_error)
#define SYNC_SECRET_ERROR sync_secret_error_quark ()

enum {
  SYNC_SECRET_ERROR_STORE = 101,
  SYNC_SECRET_ERROR_LOAD = 102
};

const SecretSchema *
ephy_sync_secret_get_token_schema (void)
{
  static const SecretSchema schema = {
    "org.epiphany.SyncTokens", SECRET_SCHEMA_NONE,
    {
      { EMAIL_KEY, SECRET_SCHEMA_ATTRIBUTE_STRING },
      { "NULL", 0 },
    }
  };

  return &schema;
}

static void
forget_tokens_cb (SecretService *service,
                  GAsyncResult  *result,
                  gpointer       user_data)
{
  GError *error = NULL;

  secret_service_clear_finish (service, result, &error);

  if (error != NULL) {
    g_warning ("sync-secret: Failed to clear the secret schema: %s", error->message);
    g_error_free (error);
  }
}

void
ephy_sync_secret_forget_tokens (void)
{
  GHashTable *attributes;

  attributes = secret_attributes_build (EPHY_SYNC_TOKEN_SCHEMA, NULL);
  secret_service_clear (NULL, EPHY_SYNC_TOKEN_SCHEMA, attributes,
                        NULL, (GAsyncReadyCallback)forget_tokens_cb, NULL);

  g_hash_table_unref (attributes);
}

static void
load_tokens_cb (SecretService *service,
                GAsyncResult  *result,
                gpointer       user_data)
{
  EphySyncService *sync_service = EPHY_SYNC_SERVICE (user_data);
  SecretItem *item;
  GHashTable *attributes;
  SecretValue *value = NULL;
  JsonParser *parser = NULL;
  JsonObject *json;
  GList *matches = NULL;
  GList *members = NULL;
  GError *error = NULL;
  GError *ret_error = NULL;
  const char *tokens;
  const char *email;
  char *user_email;

  matches = secret_service_search_finish (service, result, &error);

  if (error != NULL || matches == NULL) {
    g_set_error (&ret_error,
                 SYNC_SECRET_ERROR,
                 SYNC_SECRET_ERROR_LOAD,
                 _("The sync tokens could not be found."));
    if (error != NULL)
      g_warning ("sync-secret: Failed to find the tokens: %s", error->message);
    goto out;
  }

  if (g_list_length (matches) > 1) {
    g_set_error (&ret_error,
                 SYNC_SECRET_ERROR,
                 SYNC_SECRET_ERROR_LOAD,
                 _("Found more than one set of sync tokens."));
    g_warning ("sync-secret: Was expecting exactly one match, found more.");
    goto out;
  }

  item = (SecretItem *)matches->data;
  attributes = secret_item_get_attributes (item);
  email = g_hash_table_lookup (attributes, EMAIL_KEY);
  user_email = ephy_sync_service_get_user_email (sync_service);

  if (email == NULL || g_strcmp0 (email, user_email) != 0) {
    g_set_error (&ret_error,
                 SYNC_SECRET_ERROR,
                 SYNC_SECRET_ERROR_LOAD,
                 _("Could not found the sync tokens for the currently logged in user."));
    g_warning ("sync-secret: Emails differ: %s vs %s", email, user_email);
    goto out;
  }

  value = secret_item_get_secret (item);
  if (value == NULL) {
    g_set_error (&ret_error,
                 SYNC_SECRET_ERROR,
                 SYNC_SECRET_ERROR_LOAD,
                 _("Could not get the secret value of the sync tokens."));
    g_warning ("sync-secret: The secret item of the tokens has a NULL value.");
    goto out;
  }

  parser = json_parser_new ();
  tokens = secret_value_get_text (value);
  json_parser_load_from_data (parser, tokens, -1, &error);

  if (error != NULL) {
    g_set_error (&ret_error,
                 SYNC_SECRET_ERROR,
                 SYNC_SECRET_ERROR_LOAD,
                 _("The sync tokens are not a valid JSON."));
    g_warning ("sync-secret: Failed to load JSON from data: %s", error->message);
    goto out;
  }

  json = json_node_get_object (json_parser_get_root (parser));
  members = json_object_get_members (json);

  /* Set the tokens. */
  for (GList *m = members; m != NULL; m = m->next) {
    ephy_sync_service_set_token (sync_service,
                                 json_object_get_string_member (json, m->data),
                                 ephy_sync_utils_token_type_from_name (m->data));
  }

out:
  /* Notify whether the tokens were successfully loaded. */
  g_signal_emit_by_name (sync_service, "sync-tokens-load-finished", ret_error);

  if (error != NULL)
    g_error_free (error);

  if (ret_error != NULL)
    g_error_free (ret_error);

  if (value != NULL)
    secret_value_unref (value);

  if (parser != NULL)
    g_object_unref (parser);

  g_list_free (members);
  g_list_free_full (matches, g_object_unref);
}

void
ephy_sync_secret_load_tokens (EphySyncService *service)
{
  GHashTable *attributes;

  attributes = secret_attributes_build (EPHY_SYNC_TOKEN_SCHEMA, NULL);
  secret_service_search (NULL, EPHY_SYNC_TOKEN_SCHEMA, attributes,
                         SECRET_SEARCH_ALL | SECRET_SEARCH_UNLOCK | SECRET_SEARCH_LOAD_SECRETS,
                         NULL, (GAsyncReadyCallback)load_tokens_cb, service);

  g_hash_table_unref (attributes);
}

static void
store_tokens_cb (SecretService *service,
                 GAsyncResult  *result,
                 gpointer       user_data)
{
  EphySyncService *sync_service = EPHY_SYNC_SERVICE (user_data);
  GError *error = NULL;

  secret_service_store_finish (service, result, &error);

  /* Notify whether the tokens were successfully stored. */
  g_signal_emit_by_name (sync_service, "sync-tokens-store-finished", error);

  if (error != NULL)
    g_error_free (error);
}

void
ephy_sync_secret_store_tokens (EphySyncService *service,
                               const char      *email,
                               const char      *uid,
                               const char      *sessionToken,
                               const char      *keyFetchToken,
                               const char      *unwrapBKey,
                               const char      *kA,
                               const char      *kB)
{
  SecretValue *value;
  GHashTable *attributes;
  char *tokens;
  char *label;

  g_return_if_fail (email != NULL);
  g_return_if_fail (uid != NULL);
  g_return_if_fail (sessionToken != NULL);
  g_return_if_fail (keyFetchToken != NULL);
  g_return_if_fail (unwrapBKey != NULL);
  g_return_if_fail (kA != NULL);
  g_return_if_fail (kB != NULL);

  tokens = ephy_sync_utils_build_json_string ("uid", uid,
                                              "sessionToken", sessionToken,
                                              "keyFetchToken", keyFetchToken,
                                              "unwrapBKey", unwrapBKey,
                                              "kA", kA,
                                              "kB", kB,
                                              NULL);
  value = secret_value_new (tokens, -1, "text/plain");
  attributes = secret_attributes_build (EPHY_SYNC_TOKEN_SCHEMA,
                                        EMAIL_KEY, email,
                                        NULL);
  /* Translators: The %s represents the email of the user. */
  label = g_strdup_printf (_("The sync tokens of %s"), email);

  secret_service_store (NULL, EPHY_SYNC_TOKEN_SCHEMA, attributes,
                        NULL, label, value, NULL,
                        (GAsyncReadyCallback)store_tokens_cb, service);

  g_free (tokens);
  g_free (label);
  secret_value_unref (value);
  g_hash_table_unref (attributes);
}
