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

#include "config.h"
#include "ephy-sync-service.h"

#include "ephy-debug.h"
#include "ephy-settings.h"
#include "ephy-sync-crypto.h"
#include "ephy-sync-secret.h"

#include <json-glib/json-glib.h>
#include <libsoup/soup.h>
#include <string.h>

#define EMAIL_REGEX             "^[a-z0-9]([a-z0-9.]+[a-z0-9])?@[a-z0-9.-]+$"
#define TOKEN_SERVER_URL        "https://token.services.mozilla.com/1.0/sync/1.5"
#define FXA_BASEURL             "https://api.accounts.firefox.com/"
#define FXA_VERSION             "v1/"
#define STATUS_OK               200
#define current_time_in_seconds (g_get_real_time () / 1000000)

struct _EphySyncService {
  GObject parent_instance;

  SoupSession *soup_session;

  gchar *uid;
  gchar *sessionToken;
  gchar *keyFetchToken;
  gchar *unwrapBKey;
  gchar *kA;
  gchar *kB;

  gchar *user_email;
  gint64 last_auth_at;

  gchar *certificate;

  gchar *storage_endpoint;
  gchar *storage_credentials_id;
  gchar *storage_credentials_key;
  gint64 storage_credentials_expiry_time;

  EphySyncCryptoRSAKeyPair *keypair;
};

G_DEFINE_TYPE (EphySyncService, ephy_sync_service, G_TYPE_OBJECT);

typedef struct {
  EphySyncService *service;
  const gchar *endpoint;
  const gchar *method;
  gchar *request_body;
  gint64 modified_since;
  gint64 unmodified_since;
  SoupSessionCallback callback;
  gpointer user_data;
} StorageServerRequestAsyncData;

static gchar *
build_json_string (const gchar *first_key,
                   const gchar *first_value,
                   ...) G_GNUC_NULL_TERMINATED;

static StorageServerRequestAsyncData *
storage_server_request_async_data_new (EphySyncService     *service,
                                       const gchar         *endpoint,
                                       const gchar         *method,
                                       gchar               *request_body,
                                       gint64               modified_since,
                                       gint64               unmodified_since,
                                       SoupSessionCallback  callback,
                                       gpointer             user_data)
{
  StorageServerRequestAsyncData *data;

  data = g_slice_new (StorageServerRequestAsyncData);
  data->service = g_object_ref (service);
  data->endpoint = endpoint;
  data->method = method;
  data->request_body = g_strdup (request_body);
  data->modified_since = modified_since;
  data->unmodified_since = unmodified_since;
  data->callback = callback;
  data->user_data = user_data;

  return data;
}

static void
storage_server_request_async_data_free (StorageServerRequestAsyncData *data)
{
  if (G_UNLIKELY (!data))
    return;

  g_object_unref (data->service);
  g_free (data->request_body);
  g_slice_free (StorageServerRequestAsyncData, data);
}

static gchar *
build_json_string (const gchar *first_key,
                   const gchar *first_value,
                   ...)
{
  va_list args;
  gchar *json;
  gchar *key;
  gchar *value;
  gchar *tmp;

  json = g_strconcat ("{\"", first_key, "\": \"", first_value, "\"", NULL);
  va_start (args, first_value);

  while ((key = va_arg (args, gchar *)) != NULL) {
    value = va_arg (args, gchar *);
    tmp = json;
    json = g_strconcat (json, ", \"", key, "\": \"", value, "\"", NULL);
    g_free (tmp);
  }

  va_end (args);
  tmp = json;
  json = g_strconcat (json, "}", NULL);
  g_free (tmp);

  return json;
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
  g_assert (uri != NULL);

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

static void
destroy_session_response_cb (SoupSession *session,
                             SoupMessage *message,
                             gpointer     user_data)
{
  JsonParser *parser;
  JsonObject *json;

  if (message->status_code == STATUS_OK) {
    LOG ("Session destroyed");
    return;
  }

  parser = json_parser_new ();
  json_parser_load_from_data (parser, message->response_body->data, -1, NULL);
  json = json_node_get_object (json_parser_get_root (parser));

  g_warning ("Failed to destroy session: errno: %ld, errmsg: %s",
             json_object_get_int_member (json, "errno"),
             json_object_get_string_member (json, "message"));

  g_object_unref (parser);
}

static gboolean
ephy_sync_service_storage_credentials_is_expired (EphySyncService *self)
{
  if (self->storage_credentials_id == NULL || self->storage_credentials_key == NULL)
    return TRUE;

  if (self->storage_credentials_expiry_time == 0)
    return TRUE;

  /* Consider a 60 seconds safety interval. */
  return self->storage_credentials_expiry_time < current_time_in_seconds - 60;
}

static void
ephy_sync_service_fxa_hawk_post_async (EphySyncService     *self,
                                       const gchar         *endpoint,
                                       const gchar         *id,
                                       guint8              *key,
                                       gsize                key_length,
                                       gchar               *request_body,
                                       SoupSessionCallback  callback,
                                       gpointer             user_data)
{
  EphySyncCryptoHawkOptions *hawk_options;
  EphySyncCryptoHawkHeader *hawk_header;
  SoupMessage *message;
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
  soup_session_queue_message (self->soup_session, message, callback, user_data);

  g_free (url);
  ephy_sync_crypto_hawk_options_free (hawk_options);
  ephy_sync_crypto_hawk_header_free (hawk_header);
}

static guint
ephy_sync_service_fxa_hawk_get_sync (EphySyncService  *self,
                                     const gchar      *endpoint,
                                     const gchar      *id,
                                     guint8           *key,
                                     gsize             key_length,
                                     JsonNode        **node)
{
  EphySyncCryptoHawkHeader *hawk_header;
  SoupMessage *message;
  JsonParser *parser;
  gchar *url;

  url = g_strdup_printf ("%s%s%s", FXA_BASEURL, FXA_VERSION, endpoint);
  message = soup_message_new (SOUP_METHOD_GET, url);
  hawk_header = ephy_sync_crypto_compute_hawk_header (url, "GET",
                                                      id,
                                                      key, key_length,
                                                      NULL);
  soup_message_headers_append (message->request_headers,
                               "authorization", hawk_header->header);
  soup_session_send_message (self->soup_session, message);

  if (node != NULL) {
    parser = json_parser_new ();
    json_parser_load_from_data (parser,
                                message->response_body->data,
                                -1, NULL);
    *node = json_node_copy (json_parser_get_root (parser));
    g_object_unref (parser);
  }

  g_free (url);
  ephy_sync_crypto_hawk_header_free (hawk_header);

  return message->status_code;
}

static void
ephy_sync_service_send_storage_request (EphySyncService               *self,
                                        StorageServerRequestAsyncData *data)
{
  EphySyncCryptoHawkOptions *hawk_options = NULL;
  EphySyncCryptoHawkHeader *hawk_header;
  SoupMessage *message;
  gchar *url;
  gchar *if_modified_since = NULL;
  gchar *if_unmodified_since = NULL;
  const gchar *content_type = "application/json";

  url = g_strdup_printf ("%s/%s", self->storage_endpoint, data->endpoint);
  message = soup_message_new (data->method, url);

  if (data->request_body != NULL) {
    hawk_options = ephy_sync_crypto_hawk_options_new (NULL, NULL, NULL,
                                                      g_strdup (content_type),
                                                      NULL, NULL, NULL,
                                                      g_strdup (data->request_body),
                                                      NULL);
    soup_message_set_request (message, content_type,
                              SOUP_MEMORY_COPY,
                              data->request_body, strlen (data->request_body));
  }

  if (g_strcmp0 (data->method, SOUP_METHOD_POST) == 0) {
    soup_message_headers_append (message->request_headers,
                                 "content-type", content_type);
  }

  if (data->modified_since >= 0) {
    if_modified_since = g_strdup_printf ("%ld", data->modified_since);
    soup_message_headers_append (message->request_headers,
                                 "X-If-Modified-Since", if_modified_since);
  }

  if (data->unmodified_since >= 0) {
    if_unmodified_since = g_strdup_printf ("%ld", data->unmodified_since);
    soup_message_headers_append (message->request_headers,
                                 "X-If-Unmodified-Since", if_unmodified_since);
  }

  hawk_header = ephy_sync_crypto_compute_hawk_header (url, data->method,
                                                      self->storage_credentials_id,
                                                      (guint8 *) self->storage_credentials_key,
                                                      strlen (self->storage_credentials_key),
                                                      hawk_options);
  soup_message_headers_append (message->request_headers,
                               "authorization", hawk_header->header);
  soup_session_queue_message (self->soup_session, message,
                              data->callback, data->user_data);

  if (hawk_options != NULL)
    ephy_sync_crypto_hawk_options_free (hawk_options);

  g_free (url);
  g_free (if_modified_since);
  g_free (if_unmodified_since);
  storage_server_request_async_data_free (data);
}

static gboolean
ephy_sync_service_certificate_is_valid (EphySyncService *self,
                                        const gchar     *certificate)
{
  JsonParser *parser;
  JsonObject *json;
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
  json = json_node_get_object (json_parser_get_root (parser));
  algorithm = json_object_get_string_member (json, "alg");

  if (g_str_equal (algorithm, "RS256") == FALSE) {
    g_warning ("Expected algorithm RS256, found %s. Giving up.", algorithm);
    goto out;
  }

  json_parser_load_from_data (parser, payload, -1, NULL);
  json = json_node_get_object (json_parser_get_root (parser));
  principal = json_object_get_object_member (json, "principal");
  email = json_object_get_string_member (principal, "email");
  uid_email = g_strdup_printf ("%s@%s",
                               self->uid,
                               soup_uri_get_host (soup_uri_new (FXA_BASEURL)));

  if (g_str_equal (uid_email, email) == FALSE) {
    g_warning ("Expected email %s, found %s. Giving up.", uid_email, email);
    goto out;
  }

  self->last_auth_at = json_object_get_int_member (json, "fxa-lastAuthAt");
  retval = TRUE;

out:
  g_free (header);
  g_free (payload);
  g_free (uid_email);
  g_strfreev (pieces);
  g_object_unref (parser);

  return retval;
}

static void
obtain_storage_credentials_response_cb (SoupSession *session,
                                        SoupMessage *message,
                                        gpointer     user_data)
{
  StorageServerRequestAsyncData *data;
  EphySyncService *service;
  JsonParser *parser;
  JsonObject *json;
  JsonObject *errors;
  JsonArray *array;

  data = (StorageServerRequestAsyncData *) user_data;
  service = EPHY_SYNC_SERVICE (data->service);

  parser = json_parser_new ();
  json_parser_load_from_data (parser, message->response_body->data, -1, NULL);
  json = json_node_get_object (json_parser_get_root (parser));

  /* FIXME: Since a new Firefox Account password means a new kB, and a new kB
   * means a new "X-Client-State" header, this will fail with a 401 error status
   * code and an "invalid-client-state" status string if the user has changed his
   * password since the last time he signed in. If this happens, the user needs
   * to be asked to sign in again with the new password.
   */
  if (message->status_code == STATUS_OK) {
    service->storage_endpoint = g_strdup (json_object_get_string_member (json, "api_endpoint"));
    service->storage_credentials_id = g_strdup (json_object_get_string_member (json, "id"));
    service->storage_credentials_key = g_strdup (json_object_get_string_member (json, "key"));
    service->storage_credentials_expiry_time = json_object_get_int_member (json, "duration") + current_time_in_seconds;
  } else if (message->status_code == 401) {
    array = json_object_get_array_member (json, "errors");
    errors = json_node_get_object (json_array_get_element (array, 0));
    g_warning ("Failed to talk to the Token Server: %s: %s",
               json_object_get_string_member (json, "status"),
               json_object_get_string_member (errors, "description"));
    storage_server_request_async_data_free (data);
    goto out;
  } else {
    g_warning ("Failed to talk to the Token Server, status code %u. "
               "See https://docs.services.mozilla.com/token/apis.html#error-responses",
               message->status_code);
    storage_server_request_async_data_free (data);
    goto out;
  }

  ephy_sync_service_send_storage_request (service, data);

out:
  g_object_unref (parser);
}

static void
ephy_sync_service_obtain_storage_credentials (EphySyncService *self,
                                              gpointer         user_data)
{
  SoupMessage *message;
  guint8 *kB = NULL;
  gchar *hashed_kB = NULL;
  gchar *client_state = NULL;
  gchar *audience = NULL;
  gchar *assertion = NULL;
  gchar *authorization = NULL;

  g_return_if_fail (self->certificate != NULL);
  g_return_if_fail (self->keypair != NULL);

  audience = get_audience_for_url (TOKEN_SERVER_URL);
  assertion = ephy_sync_crypto_create_assertion (self->certificate,
                                                 audience,
                                                 5 * 60,
                                                 self->keypair);
  g_return_if_fail (assertion != NULL);

  kB = ephy_sync_crypto_decode_hex (self->kB);
  hashed_kB = g_compute_checksum_for_data (G_CHECKSUM_SHA256, kB, EPHY_SYNC_TOKEN_LENGTH);
  client_state = g_strndup (hashed_kB, EPHY_SYNC_TOKEN_LENGTH);
  authorization = g_strdup_printf ("BrowserID %s", assertion);

  message = soup_message_new (SOUP_METHOD_GET, TOKEN_SERVER_URL);
  /* We need to add the X-Client-State header so that the Token Server will
   * recognize accounts that were previously used to sync Firefox data too.
   */
  soup_message_headers_append (message->request_headers,
                               "X-Client-State", client_state);
  soup_message_headers_append (message->request_headers,
                               "authorization", authorization);
  soup_session_queue_message (self->soup_session, message,
                              obtain_storage_credentials_response_cb, user_data);

  g_free (kB);
  g_free (hashed_kB);
  g_free (client_state);
  g_free (audience);
  g_free (assertion);
  g_free (authorization);
}

static void
obtain_signed_certificate_response_cb (SoupSession *session,
                                       SoupMessage *message,
                                       gpointer     user_data)
{
  StorageServerRequestAsyncData *data;
  EphySyncService *service;
  JsonParser *parser;
  JsonObject *json;
  const gchar *certificate;

  data = (StorageServerRequestAsyncData *) user_data;
  service = EPHY_SYNC_SERVICE (data->service);

  parser = json_parser_new ();
  json_parser_load_from_data (parser, message->response_body->data, -1, NULL);
  json = json_node_get_object (json_parser_get_root (parser));

  if (message->status_code != STATUS_OK) {
    g_warning ("FxA server errno: %ld, errmsg: %s",
               json_object_get_int_member (json, "errno"),
               json_object_get_string_member (json, "message"));
    storage_server_request_async_data_free (data);
    goto out;
  }

  certificate = json_object_get_string_member (json, "cert");

  if (ephy_sync_service_certificate_is_valid (service, certificate) == FALSE) {
    ephy_sync_crypto_rsa_key_pair_free (service->keypair);
    storage_server_request_async_data_free (data);
    goto out;
  }

  service->certificate = g_strdup (certificate);

  /* See the comment in ephy_sync_service_send_storage_message(). */
  ephy_sync_service_obtain_storage_credentials (service, user_data);

out:
  g_object_unref (parser);
}

static void
ephy_sync_service_obtain_signed_certificate (EphySyncService *self,
                                             gpointer         user_data)
{
  EphySyncCryptoProcessedST *processed_st;
  gchar *tokenID;
  gchar *public_key_json;
  gchar *request_body;
  gchar *n_str;
  gchar *e_str;

  g_return_if_fail (self->sessionToken != NULL);

  if (self->keypair != NULL)
    ephy_sync_crypto_rsa_key_pair_free (self->keypair);

  self->keypair = ephy_sync_crypto_generate_rsa_key_pair ();
  g_return_if_fail (self->keypair != NULL);

  processed_st = ephy_sync_crypto_process_session_token (self->sessionToken);
  tokenID = ephy_sync_crypto_encode_hex (processed_st->tokenID, 0);

  n_str = mpz_get_str (NULL, 10, self->keypair->public.n);
  e_str = mpz_get_str (NULL, 10, self->keypair->public.e);
  public_key_json = build_json_string ("algorithm", "RS",
                                       "n", n_str,
                                       "e", e_str,
                                       NULL);
  /* Duration is the lifetime of the certificate in milliseconds. The FxA server
   * limits the duration to 24 hours. For our purposes, a duration of 30 minutes
   * will suffice.
   */
  request_body = g_strdup_printf ("{\"publicKey\": %s, \"duration\": %d}",
                                  public_key_json, 30 * 60 * 1000);
  ephy_sync_service_fxa_hawk_post_async (self,
                                         "certificate/sign",
                                         tokenID,
                                         processed_st->reqHMACkey,
                                         EPHY_SYNC_TOKEN_LENGTH,
                                         request_body,
                                         obtain_signed_certificate_response_cb,
                                         user_data);

  ephy_sync_crypto_processed_st_free (processed_st);
  g_free (tokenID);
  g_free (public_key_json);
  g_free (request_body);
  g_free (n_str);
  g_free (e_str);
}

static void
ephy_sync_service_send_storage_message (EphySyncService     *self,
                                        const gchar         *endpoint,
                                        const gchar         *method,
                                        gchar               *request_body,
                                        gint64               modified_since,
                                        gint64               unmodified_since,
                                        SoupSessionCallback  callback,
                                        gpointer             user_data)
{
  StorageServerRequestAsyncData *data;

  data = storage_server_request_async_data_new (self, endpoint,
                                                method, request_body,
                                                modified_since, unmodified_since,
                                                callback, user_data);

  if (ephy_sync_service_storage_credentials_is_expired (self) == FALSE) {
    ephy_sync_service_send_storage_request (self, data);
    return;
  }

  /* Drop the old certificate and storage credentials. */
  g_clear_pointer (&self->certificate, g_free);
  g_clear_pointer (&self->storage_credentials_id, g_free);
  g_clear_pointer (&self->storage_credentials_key, g_free);
  self->storage_credentials_expiry_time = 0;

  /* The only purpose of certificates is to obtain a signed BrowserID that is
   * needed to talk to the Token Server. From the Token Server we will obtain
   * the credentials needed to talk to the Storage Server. Since both
   * ephy_sync_service_obtain_signed_certificate() and
   * ephy_sync_service_obtain_storage_credentials() complete asynchronously, we
   * need to entrust them the task of sending the request to the Storage Server.
   */
  ephy_sync_service_obtain_signed_certificate (self, data);
}

static void
ephy_sync_service_finalize (GObject *object)
{
  EphySyncService *self = EPHY_SYNC_SERVICE (object);

  if (self->keypair != NULL)
    ephy_sync_crypto_rsa_key_pair_free (self->keypair);

  G_OBJECT_CLASS (ephy_sync_service_parent_class)->finalize (object);
}

static void
ephy_sync_service_dispose (GObject *object)
{
  EphySyncService *self = EPHY_SYNC_SERVICE (object);

  g_clear_object (&self->soup_session);
  g_clear_pointer (&self->user_email, g_free);
  g_clear_pointer (&self->certificate, g_free);
  g_clear_pointer (&self->storage_endpoint, g_free);
  g_clear_pointer (&self->storage_credentials_id, g_free);
  g_clear_pointer (&self->storage_credentials_key, g_free);
  ephy_sync_service_delete_all_tokens (self);

  G_OBJECT_CLASS (ephy_sync_service_parent_class)->dispose (object);
}

static void
ephy_sync_service_class_init (EphySyncServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ephy_sync_service_finalize;
  object_class->dispose = ephy_sync_service_dispose;
}

static void
ephy_sync_service_init (EphySyncService *self)
{
  gchar *email;

  self->soup_session = soup_session_new ();

  email = g_settings_get_string (EPHY_SETTINGS_MAIN, EPHY_PREFS_SYNC_USER);

  if (g_str_equal (email, "") == FALSE) {
    if (g_regex_match_simple (EMAIL_REGEX, email, 0, 0) == TRUE) {
      ephy_sync_service_set_user_email (self, email);
      ephy_sync_secret_load_tokens (self);
    } else {
      g_warning ("Invalid email");
    }
  }
}

EphySyncService *
ephy_sync_service_new (void)
{
  return EPHY_SYNC_SERVICE (g_object_new (EPHY_TYPE_SYNC_SERVICE, NULL));
}

const gchar *
ephy_sync_service_token_name_from_type (EphySyncServiceTokenType token_type)
{
  switch (token_type) {
    case TOKEN_UID:
      return "uid";
    case TOKEN_SESSIONTOKEN:
      return "sessionToken";
    case TOKEN_KEYFETCHTOKEN:
      return "keyFetchToken";
    case TOKEN_UNWRAPBKEY:
      return "unwrapBKey";
    case TOKEN_KA:
      return "kA";
    case TOKEN_KB:
      return "kB";
  default:
    g_assert_not_reached ();
  }
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
ephy_sync_service_get_token (EphySyncService          *self,
                             EphySyncServiceTokenType  type)
{
  switch (type) {
    case TOKEN_UID:
      return self->uid;
    case TOKEN_SESSIONTOKEN:
      return self->sessionToken;
    case TOKEN_KEYFETCHTOKEN:
      return self->keyFetchToken;
    case TOKEN_UNWRAPBKEY:
      return self->unwrapBKey;
    case TOKEN_KA:
      return self->kA;
    case TOKEN_KB:
      return self->kB;
    default:
      g_assert_not_reached ();
  }
}

void
ephy_sync_service_set_token (EphySyncService          *self,
                             gchar                    *value,
                             EphySyncServiceTokenType  type)
{
  switch (type) {
    case TOKEN_UID:
      g_free (self->uid);
      self->uid = value;
      break;
    case TOKEN_SESSIONTOKEN:
      g_free (self->sessionToken);
      self->sessionToken = value;
      break;
    case TOKEN_KEYFETCHTOKEN:
      g_free (self->keyFetchToken);
      self->keyFetchToken = value;
      break;
    case TOKEN_UNWRAPBKEY:
      g_free (self->unwrapBKey);
      self->unwrapBKey = value;
      break;
    case TOKEN_KA:
      g_free (self->kA);
      self->kA = value;
      break;
    case TOKEN_KB:
      g_free (self->kB);
      self->kB = value;
      break;
    default:
      g_assert_not_reached ();
  }
}

void
ephy_sync_service_set_and_store_tokens (EphySyncService          *self,
                                        gchar                    *first_value,
                                        EphySyncServiceTokenType  first_type,
                                        ...)
{
  EphySyncServiceTokenType type;
  gchar *value;
  va_list args;

  ephy_sync_service_set_token (self, first_value, first_type);
  ephy_sync_secret_store_token (self->user_email, first_value, first_type);

  va_start (args, first_type);
  while ((value = va_arg (args, gchar *)) != NULL) {
    type = va_arg (args, EphySyncServiceTokenType);
    ephy_sync_service_set_token (self, value, type);
    ephy_sync_secret_store_token (self->user_email, value, type);
  }
  va_end (args);
}

void
ephy_sync_service_delete_all_tokens (EphySyncService *self)
{
  g_clear_pointer (&self->uid, g_free);
  g_clear_pointer (&self->sessionToken, g_free);
  g_clear_pointer (&self->keyFetchToken, g_free);
  g_clear_pointer (&self->unwrapBKey, g_free);
  g_clear_pointer (&self->kA, g_free);
  g_clear_pointer (&self->kB, g_free);
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
  tokenID = ephy_sync_crypto_encode_hex (processed_st->tokenID, 0);

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
                              destroy_session_response_cb, NULL);

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
  JsonNode *node;
  JsonObject *json;
  guint8 *unwrapKB;
  gchar *tokenID;
  guint status_code;
  gboolean retval = FALSE;

  unwrapKB = ephy_sync_crypto_decode_hex (unwrapBKey);
  processed_kft = ephy_sync_crypto_process_key_fetch_token (keyFetchToken);
  tokenID = ephy_sync_crypto_encode_hex (processed_kft->tokenID, 0);
  status_code = ephy_sync_service_fxa_hawk_get_sync (self,
                                                     "account/keys",
                                                     tokenID,
                                                     processed_kft->reqHMACkey,
                                                     EPHY_SYNC_TOKEN_LENGTH,
                                                     &node);
  json = json_node_get_object (node);

  if (status_code != STATUS_OK) {
    g_warning ("FxA server errno: %ld, errmsg: %s",
               json_object_get_int_member (json, "errno"),
               json_object_get_string_member (json, "message"));
    goto out;
  }

  sync_keys = ephy_sync_crypto_retrieve_sync_keys (json_object_get_string_member (json, "bundle"),
                                                   processed_kft->respHMACkey,
                                                   processed_kft->respXORkey,
                                                   unwrapKB);

  if (sync_keys == NULL)
    goto out;

  /* Everything is okay, save the tokens. */
  ephy_sync_service_set_user_email (self, email);
  ephy_sync_service_set_and_store_tokens (self,
                                          g_strdup (keyFetchToken), TOKEN_KEYFETCHTOKEN,
                                          g_strdup (unwrapBKey), TOKEN_UNWRAPBKEY,
                                          ephy_sync_crypto_encode_hex (sync_keys->kA, 0), TOKEN_KA,
                                          ephy_sync_crypto_encode_hex (sync_keys->kB, 0), TOKEN_KB,
                                          NULL);
  retval = TRUE;

out:
  ephy_sync_crypto_processed_kft_free (processed_kft);
  ephy_sync_crypto_sync_keys_free (sync_keys);
  json_node_free (node);
  g_free (tokenID);
  g_free (unwrapKB);

  return retval;
}
