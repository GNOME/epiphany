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
#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-sync-crypto.h"
#include "ephy-sync-secret.h"
#include "ephy-sync-service.h"
#include "ephy-sync-utils.h"

#include <glib/gi18n.h>
#include <json-glib/json-glib.h>
#include <libsoup/soup.h>
#include <string.h>

#define FXA_BASEURL   "https://api.accounts.firefox.com/"
#define FXA_VERSION   "v1/"
#define STATUS_OK     200

struct _EphySyncService {
  GObject parent_instance;

  SoupSession *soup_session;
  JsonParser *parser;

  gchar *user_email;
  GHashTable *tokens;
};


G_DEFINE_TYPE (EphySyncService, ephy_sync_service, G_TYPE_OBJECT);

static void
save_and_store_tokens (EphySyncService   *self,
                       gchar             *token_value,
                       EphySyncTokenType  token_type,
                       ...) G_GNUC_NULL_TERMINATED;

static guint
synchronous_hawk_get_request (EphySyncService  *self,
                              const gchar      *endpoint,
                              const gchar      *id,
                              guint8           *key,
                              gsize             key_length,
                              JsonObject      **jobject)
{
  EphySyncCryptoHawkHeader *hawk_header;
  SoupMessage *message;
  JsonNode *root;
  gchar *url;

  url = g_strdup_printf ("%s%s%s", FXA_BASEURL, FXA_VERSION, endpoint);
  message = soup_message_new (SOUP_METHOD_GET, url);
  hawk_header = ephy_sync_crypto_compute_hawk_header (url, "GET",
                                                      id,
                                                      key, key_length,
                                                      NULL);
  soup_message_headers_append (message->request_headers,
                               "authorization", hawk_header->header);
LOG ("[%d] Sending synchronous HAWK GET request to %s endpoint", __LINE__, endpoint);
  soup_session_send_message (self->soup_session, message);
LOG ("[%d] Got response from server: %u", __LINE__, message->status_code);

  json_parser_load_from_data (self->parser,
                              message->response_body->data,
                              -1, NULL);
  root = json_parser_get_root (self->parser);
  g_assert (JSON_NODE_HOLDS_OBJECT (root));
  *jobject = json_node_get_object (root);

  g_free (url);
  ephy_sync_crypto_hawk_header_free (hawk_header);

  return message->status_code;
}

static guint
synchronous_hawk_post_request (EphySyncService  *self,
                               const gchar      *endpoint,
                               const gchar      *id,
                               guint8           *key,
                               gsize             key_length,
                               gchar            *request_body,
                               JsonObject      **jobject)
{
  EphySyncCryptoHawkHeader *hawk_header;
  EphySyncCryptoHawkOptions *hawk_options;
  SoupMessage *message;
  JsonNode *root;
  gchar *url;

  url = g_strdup_printf ("%s%s%s", FXA_BASEURL, FXA_VERSION, endpoint);
  message = soup_message_new (SOUP_METHOD_POST, url);
  soup_message_set_request (message,
                            "application/json",
                            SOUP_MEMORY_TAKE,
                            request_body,
                            strlen (request_body));
  hawk_options = ephy_sync_crypto_hawk_options_new (NULL, NULL, NULL,
                                                    g_strdup ("application/json"),
                                                    NULL, NULL, NULL,
                                                    g_strdup (request_body),
                                                    NULL);
  hawk_header = ephy_sync_crypto_compute_hawk_header (url, "POST",
                                                      id,
                                                      key, key_length,
                                                      hawk_options);
  soup_message_headers_append (message->request_headers,
                               "authorization", hawk_header->header);
  soup_message_headers_append (message->request_headers,
                               "content-type", "application/json");
LOG ("[%d] Sending synchronous HAWK POST request to %s endpoint", __LINE__, endpoint);
  soup_session_send_message (self->soup_session, message);
LOG ("[%d] Got response from server: %u", __LINE__, message->status_code);

  json_parser_load_from_data (self->parser,
                              message->response_body->data,
                              -1, NULL);
  root = json_parser_get_root (self->parser);
  g_assert (JSON_NODE_HOLDS_OBJECT (root));
  *jobject = json_node_get_object (root);

  g_free (url);
  ephy_sync_crypto_hawk_options_free (hawk_options);
  ephy_sync_crypto_hawk_header_free (hawk_header);

  return message->status_code;
}

static guint
synchronous_fxa_post_request (EphySyncService  *self,
                              const gchar      *endpoint,
                              gchar            *request_body,
                              JsonObject      **jobject)
{
  SoupMessage *message;
  JsonNode *root;
  gchar *url;

  url = g_strdup_printf ("%s%s%s", FXA_BASEURL, FXA_VERSION, endpoint);
  message = soup_message_new (SOUP_METHOD_POST, url);
  soup_message_set_request (message,
                            "application/json",
                            SOUP_MEMORY_TAKE,
                            request_body,
                            strlen (request_body));
LOG ("[%d] Sending synchronous POST request to %s endpoint", __LINE__, endpoint);
  soup_session_send_message (self->soup_session, message);
LOG ("[%d] Got response from server: %u", __LINE__, message->status_code);

  json_parser_load_from_data (self->parser,
                              message->response_body->data,
                              -1, NULL);
  root = json_parser_get_root (self->parser);
  g_assert (JSON_NODE_HOLDS_OBJECT (root));
  *jobject = json_node_get_object (root);

  g_free (url);

  return message->status_code;
}

static void
save_and_store_tokens (EphySyncService   *self,
                       gchar             *token_value,
                       EphySyncTokenType  token_type,
                       ...)
{
  EphySyncTokenType type;
  gchar *value;
  va_list args;

  ephy_sync_service_save_token (self, token_value, token_type);
  ephy_sync_secret_store_token (self->user_email, token_value, token_type);

  va_start (args, token_type);
  while ((value = va_arg (args, gchar *)) != NULL) {
    type = va_arg (args, EphySyncTokenType);
    ephy_sync_service_save_token (self, value, type);
    ephy_sync_secret_store_token (self->user_email, value, type);
  }
  va_end (args);
}

static void
ephy_sync_service_finalize (GObject *object)
{
  EphySyncService *self = EPHY_SYNC_SERVICE (object);

  g_free (self->user_email);
  g_hash_table_destroy (self->tokens);
  g_clear_object (&self->soup_session);
  g_clear_object (&self->parser);

  G_OBJECT_CLASS (ephy_sync_service_parent_class)->finalize (object);
}

static void
ephy_sync_service_class_init (EphySyncServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ephy_sync_service_finalize;
}

static void
ephy_sync_service_init (EphySyncService *self)
{
  gchar *sync_user = NULL;

  self->tokens = g_hash_table_new_full (g_str_hash, g_str_equal,
                                        NULL, g_free);
  self->soup_session = soup_session_new ();
  self->parser = json_parser_new ();

  sync_user = g_settings_get_string (EPHY_SETTINGS_MAIN,
                                     EPHY_PREFS_SYNC_USER);

  if (sync_user && sync_user[0]) {
    ephy_sync_service_set_user_email (self, sync_user);
    ephy_sync_secret_load_tokens (self);
  }

LOG ("[%d] sync service inited", __LINE__);
}

EphySyncService *
ephy_sync_service_new (void)
{
  return EPHY_SYNC_SERVICE (g_object_new (EPHY_TYPE_SYNC_SERVICE,
                                          NULL));
}

gchar *
ephy_sync_service_get_user_email (EphySyncService *self)
{
  return self->user_email;
}

void
ephy_sync_service_set_user_email (EphySyncService *self,
                                  const gchar     *emailUTF8)
{
  g_free (self->user_email);
  self->user_email = g_strdup (emailUTF8);
}

gchar *
ephy_sync_service_get_token (EphySyncService   *self,
                             EphySyncTokenType  token_type)
{
  const gchar *token_name;
  gchar *token_value;

  token_name = ephy_sync_utils_token_name_from_type (token_type);
  token_value = (gchar *) g_hash_table_lookup (self->tokens, token_name);

LOG ("[%d] Returning token %s with value %s", __LINE__, token_name, token_value);

  return token_value;
}

void
ephy_sync_service_save_token (EphySyncService   *self,
                              gchar             *token_value,
                              EphySyncTokenType  token_type)
{
  const gchar *token_name;

LOG ("[%d] token_type: %d", __LINE__, token_type);
  token_name = ephy_sync_utils_token_name_from_type (token_type);
  g_hash_table_insert (self->tokens,
                       (gpointer) token_name,
                       (gpointer) token_value);

LOG ("[%d] Saved token %s with value %s", __LINE__, token_name, token_value);
}

void
ephy_sync_service_delete_token (EphySyncService   *self,
                                EphySyncTokenType  token_type)
{
  const gchar *token_name;

  token_name = ephy_sync_utils_token_name_from_type (token_type);
  g_hash_table_remove (self->tokens, token_name);

LOG ("[%d] Deleted token %s", __LINE__, token_name);
}

void
ephy_sync_service_delete_all_tokens (EphySyncService *self)
{
  g_hash_table_remove_all (self->tokens);

LOG ("[%d] Deleted all tokens", __LINE__);
}

gboolean
ephy_sync_service_login (EphySyncService  *self,
                         const gchar      *emailUTF8,
                         const gchar      *passwordUTF8,
                         gchar           **error_message)
{
  EphySyncCryptoStretchedCredentials *stretched_credentials = NULL;
  EphySyncCryptoProcessedKFT *processed_kft = NULL;
  EphySyncCryptoSyncKeys *sync_keys = NULL;
  JsonObject *jobject;
  gchar *request_body;
  gchar *tokenID = NULL;
  gchar *authPW = NULL;
  gchar *unwrapBKey = NULL;
  gchar *uid = NULL;
  gchar *sessionToken = NULL;
  gchar *keyFetchToken = NULL;
  gchar *kA = NULL;
  gchar *kB = NULL;
  gchar *wrapKB = NULL;
  guint status_code;
  gboolean retval = FALSE;

  stretched_credentials = ephy_sync_crypto_stretch (emailUTF8, passwordUTF8);
  authPW = ephy_sync_utils_encode_hex (stretched_credentials->authPW, 0);
  request_body = ephy_sync_utils_build_json_string ("authPW", authPW,
                                                    "email", emailUTF8,
                                                    NULL);
  status_code = synchronous_fxa_post_request (self,
                                              "account/login?keys=true",
                                              request_body,
                                              &jobject);
  if (status_code != STATUS_OK) {
    g_warning ("FxA server errno: %ld, errmsg: %s",
               json_object_get_int_member (jobject, "errno"),
               json_object_get_string_member (jobject, "message"));
    *error_message = g_strdup_printf ("%s.",
                                      json_object_get_string_member (jobject, "message"));
    g_free (authPW);
    goto out;
  } else if (json_object_get_boolean_member (jobject, "verified") == FALSE) {
    g_warning ("Firefox Account not verified");
    *error_message = g_strdup (_("Account not verified!"));
    g_free (authPW);
    goto out;
  }

  uid = g_strdup (json_object_get_string_member (jobject, "uid"));
  sessionToken = g_strdup (json_object_get_string_member (jobject, "sessionToken"));
  keyFetchToken = g_strdup (json_object_get_string_member (jobject, "keyFetchToken"));

  /* Proceed with key fetching */
  processed_kft = ephy_sync_crypto_process_key_fetch_token (keyFetchToken);
  tokenID = ephy_sync_utils_encode_hex (processed_kft->tokenID, 0);
  status_code = synchronous_hawk_get_request (self,
                                              "account/keys",
                                              tokenID,
                                              processed_kft->reqHMACkey,
                                              EPHY_SYNC_TOKEN_LENGTH,
                                              &jobject);
  if (status_code != STATUS_OK) {
    g_warning ("FxA server errno: %ld, errmsg: %s",
               json_object_get_int_member (jobject, "errno"),
               json_object_get_string_member (jobject, "message"));
    /* Translators: the %s refers to the error message. */
    *error_message = g_strdup_printf (_("Failed to retrieve keys: %s. Please try again."),
                                      json_object_get_string_member (jobject, "message"));
    g_free (authPW);
    g_free (uid);
    g_free (sessionToken);
    g_free (keyFetchToken);
    goto out;
  }

  sync_keys = ephy_sync_crypto_retrieve_sync_keys (json_object_get_string_member (jobject, "bundle"),
                                                   processed_kft->respHMACkey,
                                                   processed_kft->respXORkey,
                                                   stretched_credentials->unwrapBKey);
  if (sync_keys == NULL) {
    *error_message = g_strdup (_("Something went wrong, please try again."));
    g_free (authPW);
    g_free (uid);
    g_free (sessionToken);
    g_free (keyFetchToken);
    goto out;
  }

  /* Everything okay, save and store tokens */
  ephy_sync_service_set_user_email (self, emailUTF8);
  unwrapBKey = ephy_sync_utils_encode_hex (stretched_credentials->unwrapBKey, 0);
  kA = ephy_sync_utils_encode_hex (sync_keys->kA, 0);
  kB = ephy_sync_utils_encode_hex (sync_keys->kB, 0);
  wrapKB = ephy_sync_utils_encode_hex (sync_keys->wrapKB, 0);
  save_and_store_tokens (self,
                         authPW, EPHY_SYNC_TOKEN_AUTHPW,
                         unwrapBKey, EPHY_SYNC_TOKEN_UNWRAPBKEY,
                         uid, EPHY_SYNC_TOKEN_UID,
                         sessionToken, EPHY_SYNC_TOKEN_SESSIONTOKEN,
                         keyFetchToken, EPHY_SYNC_TOKEN_KEYFETCHTOKEN,
                         kA, EPHY_SYNC_TOKEN_KA,
                         kB, EPHY_SYNC_TOKEN_KB,
                         wrapKB, EPHY_SYNC_TOKEN_WRAPKB,
                         NULL);
  retval = TRUE;

out:
  ephy_sync_crypto_stretched_credentials_free (stretched_credentials);
  ephy_sync_crypto_processed_kft_free (processed_kft);
  ephy_sync_crypto_sync_keys_free (sync_keys);
  g_free (tokenID);

  return retval;
}
