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

#define TOKEN_SERVER_URL  "https://token.services.mozilla.com/1.0/sync/1.5"
#define FXA_BASEURL       "https://api.accounts.firefox.com/"
#define FXA_VERSION       "v1/"
#define STATUS_OK         200

struct _EphySyncService {
  GObject parent_instance;

  SoupSession *soup_session;
  JsonParser *parser;

  GHashTable *tokens;

  gchar *user_email;
  gint64 last_auth_at;

  gchar *certificate;
  gchar *storage_endpoint;
  gchar *token_server_id;
  gchar *token_server_key;

  EphySyncCryptoRSAKeyPair *keypair;
};

G_DEFINE_TYPE (EphySyncService, ephy_sync_service, G_TYPE_OBJECT);

static guint
synchronous_hawk_post_request (EphySyncService  *self,
                              const gchar       *endpoint,
                              const gchar       *id,
                              guint8            *key,
                              gsize              key_length,
                              gchar             *request_body,
                              JsonObject       **jobject)
{
  EphySyncCryptoHawkOptions *hawk_options;
  EphySyncCryptoHawkHeader *hawk_header;
  SoupMessage *message;
  JsonNode *root;
  gchar *url;
  const gchar *content_type = "application/json";

  url = g_strdup_printf ("%s%s%s", FXA_BASEURL, FXA_VERSION, endpoint);
  message = soup_message_new (SOUP_METHOD_POST, url);
  soup_message_set_request (message, content_type,
                            SOUP_MEMORY_COPY,
                            request_body, strlen (request_body));

  hawk_options = ephy_sync_crypto_hawk_options_new (NULL, NULL, NULL,
                                                    g_strdup (content_type),
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
                               "content-type", content_type);
  soup_session_send_message (self->soup_session, message);

  if (jobject != NULL) {
    json_parser_load_from_data (self->parser,
                                message->response_body->data,
                                -1, NULL);
    root = json_parser_get_root (self->parser);
    g_assert (JSON_NODE_HOLDS_OBJECT (root));
    *jobject = json_node_get_object (root);
  }

  g_free (url);
  ephy_sync_crypto_hawk_options_free (hawk_options);
  ephy_sync_crypto_hawk_header_free (hawk_header);

  return message->status_code;
}

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

  if (jobject != NULL) {
    json_parser_load_from_data (self->parser,
                                message->response_body->data,
                                -1, NULL);
    root = json_parser_get_root (self->parser);
    g_assert (JSON_NODE_HOLDS_OBJECT (root));
    *jobject = json_node_get_object (root);
  }

  g_free (url);
  ephy_sync_crypto_hawk_header_free (hawk_header);

  return message->status_code;
}

static void
session_destroyed_cb (SoupSession *session,
                      SoupMessage *message,
                      gpointer     user_data)
{
  JsonParser *parser;
  JsonNode *root;
  JsonObject *object;

  if (message->status_code == STATUS_OK) {
    LOG ("Session destroyed");
    return;
  }

  parser = json_parser_new ();
  json_parser_load_from_data (parser, message->response_body->data, -1, NULL);
  root = json_parser_get_root (parser);
  object = json_node_get_object (root);

  g_warning ("Failed to destroy session: errno: %ld, errmsg: %s",
             json_object_get_int_member (object, "errno"),
             json_object_get_string_member (object, "message"));

  g_object_unref (parser);
}

static gchar *
get_audience_for_url (const gchar *url)
{
  SoupURI *uri;
  const gchar *scheme;
  const gchar *host;
  gchar *port_str;
  guint port;
  gchar *audience;

  uri = soup_uri_new (url);

  if (uri == NULL) {
    g_warning ("Failed to build SoupURI, invalid url: %s", url);
    return NULL;
  }

  scheme = soup_uri_get_scheme (uri);
  host = soup_uri_get_host (uri);
  port = soup_uri_get_port (uri);
  port_str = g_strdup_printf (":%u", port);

  /* Even if the url doesn't contain the port, soup_uri_get_port() will return
   * the default port for the url's scheme so we need to check if the port was
   * really present in the url.
   */
  if (g_strstr_len (url, -1, port_str) != NULL)
    audience = g_strdup_printf ("%s://%s:%u", scheme, host, port);
  else
    audience = g_strdup_printf ("%s://%s", scheme, host);

  soup_uri_free (uri);
  g_free (port_str);

  return audience;
}

static guchar *
base64_parse (const gchar *string,
              gsize       *out_len)
{
  gchar *suffix;
  gchar *full;
  guchar *decoded;
  gsize len;

  len = ((4 - strlen (string) % 4) % 4);
  suffix = g_strnfill (len, '=');
  full = g_strconcat (string, suffix, NULL);
  decoded = g_base64_decode (full, out_len);

  g_free (suffix);
  g_free (full);

  return decoded;
}

static gboolean
check_certificate (EphySyncService *self,
                   const gchar     *certificate)
{
  JsonParser *parser;
  JsonNode *root;
  JsonObject *object;
  JsonObject *principal;
  gchar **pieces;
  gchar *header;
  gchar *payload;
  gchar *uid_email;
  const gchar *algorithm;
  const gchar *email;
  gsize header_len;
  gsize payload_len;
  gboolean retval = FALSE;

  g_return_val_if_fail (certificate != NULL, FALSE);

  pieces = g_strsplit (certificate, ".", 0);
  header = (gchar *) base64_parse (pieces[0], &header_len);
  payload = (gchar *) base64_parse (pieces[1], &payload_len);

  parser = json_parser_new ();
  json_parser_load_from_data (parser, header, -1, NULL);
  root = json_parser_get_root (parser);
  object = json_node_get_object (root);
  algorithm = json_object_get_string_member (object, "alg");

  if (g_str_equal (algorithm, "RS256") == FALSE) {
    g_warning ("Expected algorithm RS256, found %s. Giving up.", algorithm);
    goto out;
  }

  json_parser_load_from_data (parser, payload, -1, NULL);
  root = json_parser_get_root (parser);
  object = json_node_get_object (root);
  principal = json_object_get_object_member (object, "principal");
  email = json_object_get_string_member (principal, "email");
  uid_email = g_strdup_printf ("%s@%s",
                               ephy_sync_service_get_token (self, EPHY_SYNC_TOKEN_UID),
                               soup_uri_get_host (soup_uri_new (FXA_BASEURL)));

  if (g_str_equal (uid_email, email) == FALSE) {
    g_warning ("Expected email %s, found %s. Giving up.", uid_email, email);
    goto out;
  }

  self->last_auth_at = json_object_get_int_member (object, "fxa-lastAuthAt");
  retval = TRUE;

out:
  g_free (header);
  g_free (payload);
  g_free (uid_email);
  g_strfreev (pieces);
  g_object_unref (parser);

  return retval;
}

static gboolean
query_token_server (EphySyncService *self,
                    const gchar     *assertion)
{
  SoupMessage *message;
  JsonParser *parser;
  JsonNode *root;
  JsonObject *object;
  gchar *authorization;
  gboolean retval = FALSE;

  g_return_val_if_fail (assertion != NULL, FALSE);

  message = soup_message_new (SOUP_METHOD_GET, TOKEN_SERVER_URL);
  authorization = g_strdup_printf ("BrowserID %s", assertion);
  soup_message_headers_append (message->request_headers,
                               "authorization", authorization);
  soup_session_send_message (self->soup_session, message);

  parser = json_parser_new ();
  json_parser_load_from_data (parser,
                              message->response_body->data,
                              -1, NULL);
  root = json_parser_get_root (parser);
  object = json_node_get_object (root);

  if (message->status_code == STATUS_OK) {
    self->storage_endpoint = g_strdup (json_object_get_string_member (object, "api_endpoint"));
    self->token_server_id = g_strdup (json_object_get_string_member (object, "id"));
    self->token_server_key = g_strdup (json_object_get_string_member (object, "key"));
  } else if (message->status_code == 400) {
    g_warning ("Failed to talk to the Token Server: malformed request");
    goto out;
  } else if (message->status_code == 401) {
    JsonArray *array;
    JsonNode *node;
    JsonObject *errors;
    const gchar *status;
    const gchar *description;

    status = json_object_get_string_member (object, "status");
    array = json_object_get_array_member (object, "errors");
    node = json_array_get_element (array, 0);
    errors = json_node_get_object (node);
    description = json_object_get_string_member (errors, "description");

    g_warning ("Failed to talk to the Token Server: %s: %s",
               status, description);
    goto out;
  } else if (message->status_code == 404) {
    g_warning ("Failed to talk to the Token Server: unknown URL");
    goto out;
  } else if (message->status_code == 405) {
    g_warning ("Failed to talk to the Token Server: unsupported method");
    goto out;
  } else if (message->status_code == 406) {
    g_warning ("Failed to talk to the Token Server: unacceptable");
    goto out;
  } else if (message->status_code == 503) {
    g_warning ("Failed to talk to the Token Server: service unavailable");
    goto out;
  }

  retval = TRUE;

out:
  g_free (authorization);
  g_object_unref (parser);

  return retval;
}

static void
ephy_sync_service_finalize (GObject *object)
{
  EphySyncService *self = EPHY_SYNC_SERVICE (object);

  g_free (self->user_email);
  g_free (self->certificate);
  g_free (self->storage_endpoint);
  g_free (self->token_server_id);
  g_free (self->token_server_key);
  g_hash_table_destroy (self->tokens);
  g_clear_object (&self->soup_session);
  g_clear_object (&self->parser);
  ephy_sync_crypto_rsa_key_pair_free (self->keypair);

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
                                  const gchar     *email)
{
  g_free (self->user_email);
  self->user_email = g_strdup (email);
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
ephy_sync_service_set_token (EphySyncService   *self,
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
ephy_sync_service_set_and_store_tokens (EphySyncService   *self,
                                        gchar             *token_value,
                                        EphySyncTokenType  token_type,
                                        ...)
{
  EphySyncTokenType type;
  gchar *value;
  va_list args;

  ephy_sync_service_set_token (self, token_value, token_type);
  ephy_sync_secret_store_token (self->user_email, token_value, token_type);

  va_start (args, token_type);
  while ((value = va_arg (args, gchar *)) != NULL) {
    type = va_arg (args, EphySyncTokenType);
    ephy_sync_service_set_token (self, value, type);
    ephy_sync_secret_store_token (self->user_email, value, type);
  }
  va_end (args);
}

void
ephy_sync_service_delete_all_tokens (EphySyncService *self)
{
  g_hash_table_remove_all (self->tokens);

LOG ("[%d] Deleted all tokens", __LINE__);
}

void
ephy_sync_service_destroy_session (EphySyncService *self,
                                   const gchar     *sessionToken)
{
  EphySyncCryptoProcessedST *processed_st;
  EphySyncCryptoHawkOptions *hawk_options;
  EphySyncCryptoHawkHeader *hawk_header;
  SoupMessage *message;
  gchar *tokenID;
  gchar *url;
  const gchar *content_type = "application/json";
  const gchar *endpoint = "session/destroy";
  const gchar *request_body = "{}";

  g_return_if_fail (sessionToken != NULL);

  url = g_strdup_printf ("%s%s%s", FXA_BASEURL, FXA_VERSION, endpoint);
  processed_st = ephy_sync_crypto_process_session_token (sessionToken);
  tokenID = ephy_sync_utils_encode_hex (processed_st->tokenID, 0);

  message = soup_message_new (SOUP_METHOD_POST, url);
  soup_message_set_request (message, content_type,
                            SOUP_MEMORY_STATIC,
                            request_body, strlen (request_body));
  hawk_options = ephy_sync_crypto_hawk_options_new (NULL, NULL, NULL,
                                                    g_strdup (content_type),
                                                    NULL, NULL, NULL,
                                                    g_strdup (request_body),
                                                    NULL);
  hawk_header = ephy_sync_crypto_compute_hawk_header (url, "POST",
                                                      tokenID,
                                                      processed_st->reqHMACkey,
                                                      EPHY_SYNC_TOKEN_LENGTH,
                                                      hawk_options);
  soup_message_headers_append (message->request_headers,
                               "authorization", hawk_header->header);
  soup_message_headers_append (message->request_headers,
                               "content-type", content_type);
  soup_session_queue_message (self->soup_session, message,
                              session_destroyed_cb, NULL);

  ephy_sync_crypto_hawk_options_free (hawk_options);
  ephy_sync_crypto_hawk_header_free (hawk_header);
  ephy_sync_crypto_processed_st_free (processed_st);
  g_free (tokenID);
  g_free (url);
}

gboolean
ephy_sync_service_fetch_sync_keys (EphySyncService *self,
                                   const gchar     *email,
                                   const gchar     *keyFetchToken,
                                   const gchar     *unwrapBKey)
{
  EphySyncCryptoProcessedKFT *processed_kft = NULL;
  EphySyncCryptoSyncKeys *sync_keys = NULL;
  JsonObject *jobject;
  guint8 *unwrapKB;
  gchar *tokenID;
  guint status_code;
  gboolean retval = FALSE;

  unwrapKB = ephy_sync_utils_decode_hex (unwrapBKey);
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
    goto out;
  }

  sync_keys = ephy_sync_crypto_retrieve_sync_keys (json_object_get_string_member (jobject, "bundle"),
                                                   processed_kft->respHMACkey,
                                                   processed_kft->respXORkey,
                                                   unwrapKB);

  if (sync_keys == NULL)
    goto out;

  /* Everything is okay, save the tokens. */
  ephy_sync_service_set_user_email (self, email);
  ephy_sync_service_set_and_store_tokens (self,
                                          g_strdup (keyFetchToken), EPHY_SYNC_TOKEN_KEYFETCHTOKEN,
                                          g_strdup (unwrapBKey), EPHY_SYNC_TOKEN_UNWRAPBKEY,
                                          ephy_sync_utils_encode_hex (sync_keys->kA, 0), EPHY_SYNC_TOKEN_KA,
                                          ephy_sync_utils_encode_hex (sync_keys->kB, 0), EPHY_SYNC_TOKEN_KB,
                                          NULL);
  retval = TRUE;

LOG ("kA: %s", ephy_sync_utils_encode_hex (sync_keys->kA, 0));
LOG ("kB: %s", ephy_sync_utils_encode_hex (sync_keys->kB, 0));

out:
  ephy_sync_crypto_processed_kft_free (processed_kft);
  ephy_sync_crypto_sync_keys_free (sync_keys);
  g_free (tokenID);
  g_free (unwrapKB);

  return retval;
}

gboolean
ephy_sync_service_sign_certificate (EphySyncService *self)
{
  EphySyncCryptoProcessedST *processed_st;
  EphySyncCryptoRSAKeyPair *keypair;
  JsonObject *jobject;
  gchar *sessionToken;
  gchar *tokenID;
  gchar *public_key_json;
  gchar *request_body;
  gchar *n_str;
  gchar *e_str;
  const gchar *certificate;
  guint status_code;
  gboolean retval = FALSE;

  sessionToken = ephy_sync_service_get_token (self, EPHY_SYNC_TOKEN_SESSIONTOKEN);
  g_return_val_if_fail (sessionToken != NULL, FALSE);

  keypair = ephy_sync_crypto_generate_rsa_key_pair ();
  g_return_val_if_fail (keypair != NULL, FALSE);

  processed_st = ephy_sync_crypto_process_session_token (sessionToken);
  tokenID = ephy_sync_utils_encode_hex (processed_st->tokenID, 0);

  n_str = mpz_get_str (NULL, 10, keypair->public.n);
  e_str = mpz_get_str (NULL, 10, keypair->public.e);
  public_key_json = ephy_sync_utils_build_json_string ("algorithm", "RS",
                                                       "n", n_str,
                                                       "e", e_str,
                                                       NULL);
  /* The server allows a maximum certificate lifespan of 24 hours == 86400000 ms */
  request_body = g_strdup_printf ("{\"publicKey\": %s, \"duration\": 86400000}",
                                  public_key_json);
  status_code = synchronous_hawk_post_request (self,
                                               "certificate/sign",
                                               tokenID,
                                               processed_st->reqHMACkey,
                                               EPHY_SYNC_TOKEN_LENGTH,
                                               request_body,
                                               &jobject);

  if (status_code != STATUS_OK) {
    g_warning ("FxA server errno: %ld, errmsg: %s",
               json_object_get_int_member (jobject, "errno"),
               json_object_get_string_member (jobject, "message"));
    goto out;
  }

  /* Check if the certificate is valid */
  certificate = json_object_get_string_member (jobject, "cert");
  if (check_certificate (self, certificate) == FALSE) {
    ephy_sync_crypto_rsa_key_pair_free (keypair);
    goto out;
  }

  self->certificate = g_strdup (certificate);
  self->keypair = keypair;
  retval = TRUE;

out:
  ephy_sync_crypto_processed_st_free (processed_st);
  g_free (tokenID);
  g_free (public_key_json);
  g_free (request_body);
  g_free (n_str);
  g_free (e_str);

  return retval;
}
