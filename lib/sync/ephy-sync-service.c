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
#include "ephy-sync-service.h"

#include "ephy-debug.h"
#include "ephy-embed-prefs.h"
#include "ephy-notification.h"
#include "ephy-settings.h"
#include "ephy-sync-crypto.h"

#include <glib/gi18n.h>
#include <json-glib/json-glib.h>
#include <libsecret/secret.h>
#include <libsoup/soup.h>
#include <string.h>

#define TOKEN_SERVER_URL "https://token.services.mozilla.com/1.0/sync/1.5"
#define FIREFOX_ACCOUNTS_SERVER_URL "https://api.accounts.firefox.com/v1/"

#define STORAGE_VERSION 5

#define ACCOUNT_KEY "firefox_account"

static const SecretSchema *
ephy_sync_service_get_secret_schema (void)
{
  static const SecretSchema schema = {
    "org.epiphany.SyncSecrets", SECRET_SCHEMA_NONE,
    {
      { ACCOUNT_KEY, SECRET_SCHEMA_ATTRIBUTE_STRING },
      { "NULL", 0 },
    }
  };

  return &schema;
}

#define EPHY_SYNC_SECRET_SCHEMA (ephy_sync_service_get_secret_schema ())

struct _EphySyncService {
  GObject      parent_instance;

  SoupSession *session;
  guint        source_id;

  char        *account;
  GHashTable  *secrets;
  GSList      *managers;

  gboolean     locked;
  char        *storage_endpoint;
  char        *storage_credentials_id;
  char        *storage_credentials_key;
  gint64       storage_credentials_expiry_time;
  GQueue      *storage_queue;

  char                 *certificate;
  SyncCryptoRSAKeyPair *rsa_key_pair;

  gboolean sync_periodically;
};

G_DEFINE_TYPE (EphySyncService, ephy_sync_service, G_TYPE_OBJECT);

enum {
  UID,
  SESSION_TOKEN,
  MASTER_KEY,
  CRYPTO_KEYS,
  LAST_SECRET
};

static const char * const secrets[LAST_SECRET] = {
  "uid",
  "session_token",
  "master_key",
  "crypto_keys"
};

enum {
  PROP_0,
  PROP_SYNC_PERIODICALLY,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

enum {
  STORE_FINISHED,
  SIGN_IN_ERROR,
  SYNC_FINISHED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

typedef struct {
  char                *endpoint;
  char                *method;
  char                *request_body;
  double               modified_since;
  double               unmodified_since;
  SoupSessionCallback  callback;
  gpointer             user_data;
} StorageRequestAsyncData;

typedef struct {
  EphySyncService *service;
  char            *email;
  char            *uid;
  char            *session_token;
  char            *unwrap_b_key;
  char            *token_id_hex;
  guint8          *req_hmac_key;
  guint8          *resp_hmac_key;
  guint8          *resp_xor_key;
} SignInAsyncData;

typedef struct {
  EphySyncService           *service;
  EphySynchronizableManager *manager;
  gboolean                   is_initial;
  gboolean                   is_last;
  GSList                    *remotes_deleted;
  GSList                    *remotes_updated;
} SyncCollectionAsyncData;

typedef struct {
  EphySyncService           *service;
  EphySynchronizableManager *manager;
  EphySynchronizable        *synchronizable;
} SyncAsyncData;

static StorageRequestAsyncData *
storage_request_async_data_new (const char          *endpoint,
                                const char          *method,
                                const char          *request_body,
                                double               modified_since,
                                double               unmodified_since,
                                SoupSessionCallback  callback,
                                gpointer             user_data)
{
  StorageRequestAsyncData *data;

  data = g_slice_new (StorageRequestAsyncData);
  data->endpoint = g_strdup (endpoint);
  data->method = g_strdup (method);
  data->request_body = g_strdup (request_body);
  data->modified_since = modified_since;
  data->unmodified_since = unmodified_since;
  data->callback = callback;
  data->user_data = user_data;

  return data;
}

static void
storage_request_async_data_free (StorageRequestAsyncData *data)
{
  g_assert (data);

  g_free (data->endpoint);
  g_free (data->method);
  g_free (data->request_body);
  g_slice_free (StorageRequestAsyncData, data);
}

static SignInAsyncData *
sign_in_async_data_new (EphySyncService *service,
                        const char      *email,
                        const char      *uid,
                        const char      *session_token,
                        const char      *unwrap_b_key,
                        const char      *token_id_hex,
                        const guint8    *req_hmac_key,
                        const guint8    *resp_hmac_key,
                        const guint8    *resp_xor_key)
{
  SignInAsyncData *data;

  data = g_slice_new (SignInAsyncData);
  data->service = g_object_ref (service);
  data->email = g_strdup (email);
  data->uid = g_strdup (uid);
  data->session_token = g_strdup (session_token);
  data->unwrap_b_key = g_strdup (unwrap_b_key);
  data->token_id_hex = g_strdup (token_id_hex);
  data->req_hmac_key = g_malloc (32);
  memcpy (data->req_hmac_key, req_hmac_key, 32);
  data->resp_hmac_key = g_malloc (32);
  memcpy (data->resp_hmac_key, resp_hmac_key, 32);
  data->resp_xor_key = g_malloc (2 * 32);
  memcpy (data->resp_xor_key, resp_xor_key, 2 * 32);

  return data;
}

static void
sign_in_async_data_free (SignInAsyncData *data)
{
  g_assert (data);

  g_object_unref (data->service);
  g_free (data->email);
  g_free (data->uid);
  g_free (data->session_token);
  g_free (data->unwrap_b_key);
  g_free (data->token_id_hex);
  g_free (data->req_hmac_key);
  g_free (data->resp_hmac_key);
  g_free (data->resp_xor_key);
  g_slice_free (SignInAsyncData, data);
}

static SyncCollectionAsyncData *
sync_collection_async_data_new (EphySyncService           *service,
                                EphySynchronizableManager *manager,
                                gboolean                   is_initial,
                                gboolean                   is_last)
{
  SyncCollectionAsyncData *data;

  data = g_slice_new (SyncCollectionAsyncData);
  data->service = g_object_ref (service);
  data->manager = g_object_ref (manager);
  data->is_initial = is_initial;
  data->is_last = is_last;
  data->remotes_deleted = NULL;
  data->remotes_updated = NULL;

  return data;
}

static void
sync_collection_async_data_free (SyncCollectionAsyncData *data)
{
  g_assert (data);

  g_object_unref (data->service);
  g_object_unref (data->manager);
  g_slist_free_full (data->remotes_deleted, g_object_unref);
  g_slist_free_full (data->remotes_updated, g_object_unref);
  g_slice_free (SyncCollectionAsyncData, data);
}

static SyncAsyncData *
sync_async_data_new (EphySyncService           *service,
                     EphySynchronizableManager *manager,
                     EphySynchronizable        *synchronizable)
{
  SyncAsyncData *data;

  data = g_slice_new (SyncAsyncData);
  data->service = g_object_ref (service);
  data->manager = g_object_ref (manager);
  data->synchronizable = g_object_ref (synchronizable);

  return data;
}

static void
sync_async_data_free (SyncAsyncData *data)
{
  g_assert (data);

  g_object_unref (data->service);
  g_object_unref (data->manager);
  g_object_unref (data->synchronizable);
  g_slice_free (SyncAsyncData, data);
}

static void
ephy_sync_service_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  EphySyncService *self = EPHY_SYNC_SERVICE (object);

  switch (prop_id) {
    case PROP_SYNC_PERIODICALLY:
      self->sync_periodically = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_sync_service_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  EphySyncService *self = EPHY_SYNC_SERVICE (object);

  switch (prop_id) {
    case PROP_SYNC_PERIODICALLY:
      g_value_set_boolean (value, self->sync_periodically);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static const char *
ephy_sync_service_get_secret (EphySyncService *self,
                              const char      *name)
{
  g_assert (EPHY_IS_SYNC_SERVICE (self));
  g_assert (name);

  return g_hash_table_lookup (self->secrets, name);
}

static void
ephy_sync_service_set_secret (EphySyncService *self,
                              const char      *name,
                              const char      *value)
{
  g_assert (EPHY_IS_SYNC_SERVICE (self));
  g_assert (name);
  g_assert (value);

  g_hash_table_replace (self->secrets, g_strdup (name), g_strdup (value));
}

static SyncCryptoKeyBundle *
ephy_sync_service_get_key_bundle (EphySyncService *self,
                                  const char      *collection)
{
  SyncCryptoKeyBundle *bundle = NULL;
  JsonNode *node;
  JsonObject *json;
  JsonObject *collections;
  JsonArray *array;
  GError *error = NULL;
  const char *crypto_keys;

  g_assert (EPHY_IS_SYNC_SERVICE (self));
  g_assert (collection);

  crypto_keys = ephy_sync_service_get_secret (self, secrets[CRYPTO_KEYS]);
  node = json_from_string (crypto_keys, &error);
  g_assert (!error);
  json = json_node_get_object (node);
  collections = json_object_get_object_member (json, "collections");
  array = json_object_has_member (collections, collection) ?
          json_object_get_array_member (collections, collection) :
          json_object_get_array_member (json, "default");
  bundle = ephy_sync_crypto_key_bundle_from_array (array);

  json_node_unref (node);

  return bundle;
}

static void
ephy_sync_service_clear_storage_credentials (EphySyncService *self)
{
  g_assert (EPHY_IS_SYNC_SERVICE (self));

  g_clear_pointer (&self->certificate, g_free);
  g_clear_pointer (&self->storage_endpoint, g_free);
  g_clear_pointer (&self->storage_credentials_id, g_free);
  g_clear_pointer (&self->storage_credentials_key, g_free);
  self->storage_credentials_expiry_time = 0;
}

static gboolean
ephy_sync_service_storage_credentials_is_expired (EphySyncService *self)
{
  g_assert (EPHY_IS_SYNC_SERVICE (self));

  if (!self->storage_credentials_id || !self->storage_credentials_key)
    return TRUE;

  if (self->storage_credentials_expiry_time == 0)
    return TRUE;

  /* Consider a 60 seconds safety interval. */
  return self->storage_credentials_expiry_time < g_get_real_time () / 1000000 - 60;
}

static void
ephy_sync_service_fxa_hawk_post_async (EphySyncService     *self,
                                       const char          *endpoint,
                                       const char          *id,
                                       guint8              *key,
                                       gsize                key_len,
                                       char                *request_body,
                                       SoupSessionCallback  callback,
                                       gpointer             user_data)
{
  SyncCryptoHawkOptions *hawk_options;
  SyncCryptoHawkHeader *hawk_header;
  SoupMessage *msg;
  char *url;
  const char *content_type = "application/json; charset=utf-8";

  g_assert (EPHY_IS_SYNC_SERVICE (self));
  g_assert (endpoint);
  g_assert (id);
  g_assert (key);
  g_assert (request_body);

  url = g_strdup_printf ("%s%s", FIREFOX_ACCOUNTS_SERVER_URL, endpoint);
  msg = soup_message_new (SOUP_METHOD_POST, url);
  soup_message_set_request (msg, content_type, SOUP_MEMORY_COPY,
                            request_body, strlen (request_body));

  hawk_options = ephy_sync_crypto_hawk_options_new (NULL, NULL, NULL,
                                                    content_type,
                                                    NULL, NULL, NULL,
                                                    request_body,
                                                    NULL);
  hawk_header = ephy_sync_crypto_compute_hawk_header (url, "POST", id,
                                                      key, key_len,
                                                      hawk_options);
  soup_message_headers_append (msg->request_headers,
                               "authorization", hawk_header->header);
  soup_message_headers_append (msg->request_headers,
                               "content-type", content_type);
  soup_session_queue_message (self->session, msg, callback, user_data);

  g_free (url);
  ephy_sync_crypto_hawk_options_free (hawk_options);
  ephy_sync_crypto_hawk_header_free (hawk_header);
}

static void
ephy_sync_service_fxa_hawk_get_async (EphySyncService     *self,
                                      const char          *endpoint,
                                      const char          *id,
                                      guint8              *key,
                                      gsize                key_len,
                                      SoupSessionCallback  callback,
                                      gpointer             user_data)
{
  SyncCryptoHawkHeader *hawk_header;
  SoupMessage *msg;
  char *url;

  g_assert (EPHY_IS_SYNC_SERVICE (self));
  g_assert (endpoint);
  g_assert (id);
  g_assert (key);

  url = g_strdup_printf ("%s%s", FIREFOX_ACCOUNTS_SERVER_URL, endpoint);
  msg = soup_message_new (SOUP_METHOD_GET, url);
  hawk_header = ephy_sync_crypto_compute_hawk_header (url, "GET", id,
                                                      key, key_len,
                                                      NULL);
  soup_message_headers_append (msg->request_headers, "authorization", hawk_header->header);
  soup_session_queue_message (self->session, msg, callback, user_data);

  g_free (url);
  ephy_sync_crypto_hawk_header_free (hawk_header);
}

static void
ephy_sync_service_send_storage_request (EphySyncService         *self,
                                        StorageRequestAsyncData *data)
{
  SyncCryptoHawkOptions *hawk_options = NULL;
  SyncCryptoHawkHeader *hawk_header;
  SoupMessage *msg;
  char *url;
  char *if_modified_since = NULL;
  char *if_unmodified_since = NULL;
  const char *content_type = "application/json; charset=utf-8";

  g_assert (EPHY_IS_SYNC_SERVICE (self));
  g_assert (data);

  url = g_strdup_printf ("%s/%s", self->storage_endpoint, data->endpoint);
  msg = soup_message_new (data->method, url);

  if (data->request_body) {
    hawk_options = ephy_sync_crypto_hawk_options_new (NULL, NULL, NULL,
                                                      content_type,
                                                      NULL, NULL, NULL,
                                                      data->request_body,
                                                      NULL);
    soup_message_set_request (msg, content_type, SOUP_MEMORY_COPY,
                              data->request_body, strlen (data->request_body));
  }

  if (!g_strcmp0 (data->method, SOUP_METHOD_PUT))
    soup_message_headers_append (msg->request_headers,
                                 "content-type", content_type);

  if (data->modified_since >= 0) {
    if_modified_since = g_strdup_printf ("%.2lf", data->modified_since);
    soup_message_headers_append (msg->request_headers,
                                 "X-If-Modified-Since", if_modified_since);
  }

  if (data->unmodified_since >= 0) {
    if_unmodified_since = g_strdup_printf ("%.2lf", data->unmodified_since);
    soup_message_headers_append (msg->request_headers,
                                 "X-If-Unmodified-Since", if_unmodified_since);
  }

  hawk_header = ephy_sync_crypto_compute_hawk_header (url, data->method,
                                                      self->storage_credentials_id,
                                                      (guint8 *)self->storage_credentials_key,
                                                      strlen (self->storage_credentials_key),
                                                      hawk_options);
  soup_message_headers_append (msg->request_headers,
                               "authorization", hawk_header->header);
  soup_session_queue_message (self->session, msg, data->callback, data->user_data);

  if (hawk_options)
    ephy_sync_crypto_hawk_options_free (hawk_options);

  g_free (url);
  g_free (if_modified_since);
  g_free (if_unmodified_since);
  ephy_sync_crypto_hawk_header_free (hawk_header);
  storage_request_async_data_free (data);
}

static gboolean
ephy_sync_service_certificate_is_valid (EphySyncService *self,
                                        const char      *certificate)
{
  JsonParser *parser;
  JsonObject *json;
  JsonObject *principal;
  GError *error = NULL;
  SoupURI *uri = NULL;
  char **pieces;
  char *header;
  char *payload;
  char *expected = NULL;
  const char *alg;
  const char *email;
  gsize len;
  gboolean retval = FALSE;

  g_assert (EPHY_IS_SYNC_SERVICE (self));
  g_assert (ephy_sync_service_get_secret (self, secrets[UID]));
  g_assert (certificate);

  pieces = g_strsplit (certificate, ".", 0);
  header = (char *)ephy_sync_crypto_base64_urlsafe_decode (pieces[0], &len, TRUE);
  payload = (char *)ephy_sync_crypto_base64_urlsafe_decode (pieces[1], &len, TRUE);
  parser = json_parser_new ();

  json_parser_load_from_data (parser, header, -1, &error);
  if (error) {
    g_warning ("Header is not a valid JSON: %s", error->message);
    goto out;
  }
  json = json_node_get_object (json_parser_get_root (parser));
  if (!json) {
    g_warning ("JSON node does not hold a JSON object");
    goto out;
  }
  alg = json_object_get_string_member (json, "alg");
  if (!alg) {
    g_warning ("JSON object has missing or invalid 'alg' member");
    goto out;
  }
  if (g_strcmp0 (alg, "RS256")) {
    g_warning ("Expected algorithm RS256, found %s", alg);
    goto out;
  }
  json_parser_load_from_data (parser, payload, -1, &error);
  if (error) {
    g_warning ("Payload is not a valid JSON: %s", error->message);
    goto out;
  }
  json = json_node_get_object (json_parser_get_root (parser));
  if (!json) {
    g_warning ("JSON node does not hold a JSON object");
    goto out;
  }
  principal = json_object_get_object_member (json, "principal");
  if (!principal) {
    g_warning ("JSON object has missing or invalid 'principal' member");
    goto out;
  }
  email = json_object_get_string_member (principal, "email");
  if (!email) {
    g_warning ("JSON object has missing or invalid 'email' member");
    goto out;
  }
  uri = soup_uri_new (FIREFOX_ACCOUNTS_SERVER_URL);
  expected = g_strdup_printf ("%s@%s",
                              ephy_sync_service_get_secret (self, secrets[UID]),
                              soup_uri_get_host (uri));
  retval = g_strcmp0 (email, expected) == 0;

out:
  g_free (expected);
  g_object_unref (parser);
  g_free (payload);
  g_free (header);
  g_strfreev (pieces);
  if (uri)
    soup_uri_free (uri);
  if (error)
    g_error_free (error);

  return retval;
}

static void
destroy_session_cb (SoupSession *session,
                    SoupMessage *msg,
                    gpointer     user_data)
{
  if (msg->status_code != 200)
    g_warning ("Failed to destroy session. Status code: %u, response: %s",
               msg->status_code, msg->response_body->data);
  else
    LOG ("Successfully destroyed session");
}

static void
ephy_sync_service_destroy_session (EphySyncService *self,
                                   const char      *session_token)
{
  SyncCryptoHawkOptions *hawk_options;
  SyncCryptoHawkHeader *hawk_header;
  SoupMessage *msg;
  guint8 *token_id;
  guint8 *req_hmac_key;
  guint8 *request_key;
  char *token_id_hex;
  char *url;
  const char *content_type = "application/json; charset=utf-8";
  const char *request_body = "{}";

  g_assert (EPHY_IS_SYNC_SERVICE (self));
  if (!session_token)
    session_token = ephy_sync_service_get_secret (self, secrets[SESSION_TOKEN]);
  g_assert (session_token);

  url = g_strdup_printf ("%ssession/destroy", FIREFOX_ACCOUNTS_SERVER_URL);
  ephy_sync_crypto_process_session_token (session_token, &token_id,
                                          &req_hmac_key, &request_key, 32);
  token_id_hex = ephy_sync_crypto_encode_hex (token_id, 32);

  msg = soup_message_new (SOUP_METHOD_POST, url);
  soup_message_set_request (msg, content_type, SOUP_MEMORY_STATIC,
                            request_body, strlen (request_body));
  hawk_options = ephy_sync_crypto_hawk_options_new (NULL, NULL, NULL,
                                                    content_type,
                                                    NULL, NULL, NULL,
                                                    request_body,
                                                    NULL);
  hawk_header = ephy_sync_crypto_compute_hawk_header (url, "POST", token_id_hex,
                                                      req_hmac_key, 32,
                                                      hawk_options);
  soup_message_headers_append (msg->request_headers,
                               "authorization", hawk_header->header);
  soup_message_headers_append (msg->request_headers,
                               "content-type", content_type);
  soup_session_queue_message (self->session, msg, destroy_session_cb, NULL);

  g_free (token_id_hex);
  g_free (token_id);
  g_free (req_hmac_key);
  g_free (request_key);
  g_free (url);
  ephy_sync_crypto_hawk_options_free (hawk_options);
  ephy_sync_crypto_hawk_header_free (hawk_header);
}

static void
obtain_storage_credentials_cb (SoupSession *session,
                               SoupMessage *msg,
                               gpointer     user_data)
{
  EphySyncService *self = EPHY_SYNC_SERVICE (user_data);
  JsonNode *node = NULL;
  JsonObject *json = NULL;
  GError *error = NULL;
  const char *api_endpoint;
  const char *id;
  const char *key;
  const char *message;
  const char *suggestion;
  int duration;

  if (msg->status_code != 200) {
    g_warning ("Failed to obtain storage credentials. Status code: %u, response: %s",
               msg->status_code, msg->response_body->data);
    goto out_error;
  }
  node = json_from_string (msg->response_body->data, &error);
  if (error) {
    g_warning ("Response is not a valid JSON: %s", error->message);
    goto out_error;
  }
  json = json_node_get_object (node);
  if (!json) {
    g_warning ("JSON node does not hold a JSON object");
    goto out_error;
  }
  api_endpoint = json_object_get_string_member (json, "api_endpoint");
  id = json_object_get_string_member (json, "id");
  key = json_object_get_string_member (json, "key");
  duration = json_object_get_int_member (json, "duration");
  if (!api_endpoint || !id || !key || !duration) {
    g_warning ("JSON object has missing or invalid members");
    goto out_error;
  }

  self->storage_endpoint = g_strdup (api_endpoint);
  self->storage_credentials_id = g_strdup (id);
  self->storage_credentials_key = g_strdup (key);
  self->storage_credentials_expiry_time = duration + g_get_real_time () / 1000000;

  while (!g_queue_is_empty (self->storage_queue))
    ephy_sync_service_send_storage_request (self, g_queue_pop_head (self->storage_queue));
  goto out;

out_error:
  message = _("Failed to obtain the storage credentials.");
  suggestion = _("Please visit Preferences and sign in again to continue syncing.");
  ephy_notification_show (ephy_notification_new (message, suggestion));
out:
  self->locked = FALSE;
  if (node)
    json_node_unref (node);
  if (error)
    g_error_free (error);
}

static char *
get_audience (const char *url)
{
  SoupURI *uri;
  const char *scheme;
  const char *host;
  char *audience;
  char *port;

  g_assert (url);

  uri = soup_uri_new (url);
  scheme = soup_uri_get_scheme (uri);
  host = soup_uri_get_host (uri);
  /* soup_uri_get_port returns the default port if URI does not have any port. */
  port = g_strdup_printf (":%u", soup_uri_get_port (uri));

  if (g_strstr_len (url, -1, port))
    audience = g_strdup_printf ("%s://%s%s", scheme, host, port);
  else
    audience = g_strdup_printf ("%s://%s", scheme, host);

  g_free (port);
  soup_uri_free (uri);

  return audience;
}

static void
ephy_sync_service_obtain_storage_credentials (EphySyncService *self)
{
  SoupMessage *msg;
  guint8 *key_b;
  char *hashed_key_b;
  char *client_state;
  char *audience;
  char *assertion;
  char *authorization;

  g_assert (EPHY_IS_SYNC_SERVICE (self));
  g_assert (self->certificate);
  g_assert (self->rsa_key_pair);

  audience = get_audience (TOKEN_SERVER_URL);
  assertion = ephy_sync_crypto_create_assertion (self->certificate, audience,
                                                 300, self->rsa_key_pair);
  key_b = ephy_sync_crypto_decode_hex (ephy_sync_service_get_secret (self, secrets[MASTER_KEY]));
  hashed_key_b = g_compute_checksum_for_data (G_CHECKSUM_SHA256, key_b, 32);
  client_state = g_strndup (hashed_key_b, 32);
  authorization = g_strdup_printf ("BrowserID %s", assertion);

  msg = soup_message_new (SOUP_METHOD_GET, TOKEN_SERVER_URL);
  /* We need to add the X-Client-State header so that the Token Server will
   * recognize accounts that were previously used to sync Firefox data too. */
  soup_message_headers_append (msg->request_headers, "X-Client-State", client_state);
  soup_message_headers_append (msg->request_headers, "authorization", authorization);
  soup_session_queue_message (self->session, msg, obtain_storage_credentials_cb, self);

  g_free (key_b);
  g_free (hashed_key_b);
  g_free (client_state);
  g_free (audience);
  g_free (assertion);
  g_free (authorization);
}

static void
obtain_signed_certificate_cb (SoupSession *session,
                              SoupMessage *msg,
                              gpointer     user_data)
{
  EphySyncService *self = EPHY_SYNC_SERVICE (user_data);
  JsonNode *node = NULL;
  JsonObject *json = NULL;
  GError *error = NULL;
  const char *suggestion = NULL;
  const char *message = NULL;
  const char *certificate = NULL;

  node = json_from_string (msg->response_body->data, &error);
  if (error) {
    g_warning ("Response is not a valid JSON: %s", error->message);
    goto out_error;
  }
  json = json_node_get_object (node);
  if (!json) {
    g_warning ("JSON node does not hold a JSON object");
    goto out_error;
  }

  if (msg->status_code == 200) {
    certificate = json_object_get_string_member (json, "cert");
    if (!certificate) {
      g_warning ("JSON object has missing or invalid 'cert' member");
      goto out_error;
    }
    if (!ephy_sync_service_certificate_is_valid (self, certificate)) {
      g_warning ("Invalid certificate");
      ephy_sync_crypto_rsa_key_pair_free (self->rsa_key_pair);
      goto out_error;
    }
    self->certificate = g_strdup (certificate);
    ephy_sync_service_obtain_storage_credentials (self);
    goto out_no_error;
  }

  /* Since a new Firefox Account password implies new tokens, this will fail
   * with an error code 110 (Invalid authentication token in request signature)
   * if the user has changed his password since the last time he signed in.
   * When this happens, notify the user to sign in with the new password. */
  if (json_object_get_int_member (json, "errno") == 110) {
    message = _("The password of your Firefox account seems to have been changed.");
    suggestion = _("Please visit Preferences and sign in with the new password to continue syncing.");
    ephy_sync_service_do_sign_out (self);
  }

  g_warning ("Failed to sign certificate. Status code: %u, response: %s",
             msg->status_code, msg->response_body->data);

out_error:
  message = message ? message : _("Failed to obtain a signed certificate.");
  suggestion = suggestion ? suggestion : _("Please visit Preferences and sign in again to continue syncing.");
  ephy_notification_show (ephy_notification_new (message, suggestion));
  self->locked = FALSE;
out_no_error:
  if (node)
    json_node_unref (node);
  if (error)
    g_error_free (error);
}

static void
ephy_sync_service_obtain_signed_certificate (EphySyncService *self)
{
  JsonNode *node;
  JsonObject *object_key;
  JsonObject *object_body;
  guint8 *token_id;
  guint8 *req_hmac_key;
  guint8 *request_key;
  const char *session_token;
  char *token_id_hex;
  char *request_body;
  char *n;
  char *e;

  g_assert (EPHY_IS_SYNC_SERVICE (self));

  /* Generate a new RSA key pair to sign the new certificate. */
  if (self->rsa_key_pair)
    ephy_sync_crypto_rsa_key_pair_free (self->rsa_key_pair);
  self->rsa_key_pair = ephy_sync_crypto_generate_rsa_key_pair ();

  /* Derive tokenID, reqHMACkey and requestKey from sessionToken. */
  session_token = ephy_sync_service_get_secret (self, secrets[SESSION_TOKEN]);
  ephy_sync_crypto_process_session_token (session_token, &token_id,
                                          &req_hmac_key, &request_key, 32);
  token_id_hex = ephy_sync_crypto_encode_hex (token_id, 32);

  n = mpz_get_str (NULL, 10, self->rsa_key_pair->public.n);
  e = mpz_get_str (NULL, 10, self->rsa_key_pair->public.e);
  node = json_node_new (JSON_NODE_OBJECT);
  object_body = json_object_new ();
  /* Milliseconds, limited to 24 hours. */
  json_object_set_int_member (object_body, "duration", 1 * 60 * 60 * 1000);
  object_key = json_object_new ();
  json_object_set_string_member (object_key, "algorithm", "RS");
  json_object_set_string_member (object_key, "n", n);
  json_object_set_string_member (object_key, "e", e);
  json_object_set_object_member (object_body, "publicKey", object_key);
  json_node_set_object (node, object_body);
  request_body = json_to_string (node, FALSE);
  ephy_sync_service_fxa_hawk_post_async (self, "certificate/sign", token_id_hex,
                                         req_hmac_key, 32, request_body,
                                         obtain_signed_certificate_cb, self);

  g_free (request_body);
  json_object_unref (object_body);
  json_node_unref (node);
  g_free (e);
  g_free (n);
  g_free (token_id_hex);
  g_free (request_key);
  g_free (req_hmac_key);
  g_free (token_id);
}

static void
ephy_sync_service_queue_storage_request (EphySyncService     *self,
                                         const char          *endpoint,
                                         const char          *method,
                                         const char          *request_body,
                                         double               modified_since,
                                         double               unmodified_since,
                                         SoupSessionCallback  callback,
                                         gpointer             user_data)
{
  StorageRequestAsyncData *data;

  g_assert (EPHY_IS_SYNC_SERVICE (self));
  g_assert (endpoint);
  g_assert (method);

  data = storage_request_async_data_new (endpoint, method, request_body,
                                         modified_since, unmodified_since,
                                         callback, user_data);

  /* If the storage credentials are valid, then directly send the request.
   * Otherwise, the request will remain queued and scheduled to be sent when
   * the new credentials are obtained. */
  if (!ephy_sync_service_storage_credentials_is_expired (self)) {
    ephy_sync_service_send_storage_request (self, data);
  } else {
    g_queue_push_tail (self->storage_queue, data);
    if (!self->locked) {
      /* Mark as locked so other requests won't lead to conflicts while obtaining
       * new storage credentials. */
      self->locked = TRUE;
      ephy_sync_service_clear_storage_credentials (self);
      ephy_sync_service_obtain_signed_certificate (self);
    }
  }
}

static void
delete_synchronizable_cb (SoupSession *session,
                          SoupMessage *msg,
                          gpointer     user_data)
{
  if (msg->status_code == 200) {
    LOG ("Successfully deleted from server");
  } else {
    g_warning ("Failed to delete object. Status code: %u, response: %s",
               msg->status_code, msg->response_body->data);
  }
}

static void
ephy_sync_service_delete_synchronizable (EphySyncService           *self,
                                         EphySynchronizableManager *manager,
                                         EphySynchronizable        *synchronizable)
{
  JsonNode *node;
  JsonObject *object;
  SyncCryptoKeyBundle *bundle;
  char *endpoint;
  char *record;
  char *payload;
  char *body;
  char *id_safe;
  const char *collection;
  const char *id;

  g_assert (EPHY_IS_SYNC_SERVICE (self));
  g_assert (EPHY_IS_SYNCHRONIZABLE_MANAGER (manager));
  g_assert (EPHY_IS_SYNCHRONIZABLE (synchronizable));
  g_assert (ephy_sync_service_is_signed_in (self));

  collection = ephy_synchronizable_manager_get_collection_name (manager);
  id = ephy_synchronizable_get_id (synchronizable);
  /* Firefox uses UUIDs with curly braces as IDs for saved passwords records.
   * Curly braces are unsafe characters in URLs so they must be encoded. */
  id_safe = soup_uri_encode (id, NULL);
  endpoint = g_strdup_printf ("storage/%s/%s", collection, id_safe);

  node = json_node_new (JSON_NODE_OBJECT);
  object = json_object_new ();
  json_node_set_object (node, object);
  json_object_set_string_member (object, "id", id);
  json_object_set_boolean_member (object, "deleted", TRUE);
  record = json_to_string (node, FALSE);
  bundle = ephy_sync_service_get_key_bundle (self, collection);
  payload = ephy_sync_crypto_encrypt_record (record,  bundle);
  json_object_remove_member (object, "type");
  json_object_remove_member (object, "deleted");
  json_object_set_string_member (object, "payload", payload);
  body = json_to_string (node, FALSE);

  LOG ("Deleting object with id %s from collection %s...", id, collection);
  ephy_sync_service_queue_storage_request (self, endpoint,
                                           SOUP_METHOD_PUT, body, -1, -1,
                                           delete_synchronizable_cb, NULL);

  g_free (id_safe);
  g_free (endpoint);
  g_free (record);
  g_free (payload);
  g_free (body);
  json_object_unref (object);
  json_node_unref (node);
  ephy_sync_crypto_key_bundle_free (bundle);
}

static void
download_synchronizable_cb (SoupSession *session,
                            SoupMessage *msg,
                            gpointer     user_data)
{
  SyncAsyncData *data = (SyncAsyncData *)user_data;
  EphySynchronizable *synchronizable;
  SyncCryptoKeyBundle *bundle = NULL;
  JsonNode *node = NULL;
  GError *error = NULL;
  GType type;
  const char *collection;
  gboolean is_deleted;

  if (msg->status_code != 200) {
    g_warning ("Failed to download object. Status code: %u, response: %s",
               msg->status_code, msg->response_body->data);
    goto out;
  }
  node = json_from_string (msg->response_body->data, &error);
  if (error) {
    g_warning ("Response is not a valid JSON");
    goto out;
  }
  type = ephy_synchronizable_manager_get_synchronizable_type (data->manager);
  collection = ephy_synchronizable_manager_get_collection_name (data->manager);
  bundle = ephy_sync_service_get_key_bundle (data->service, collection);
  synchronizable = EPHY_SYNCHRONIZABLE (ephy_synchronizable_from_bso (node, type, bundle, &is_deleted));
  if (!synchronizable) {
    g_warning ("Failed to create synchronizable object from BSO");
    goto out;
  }

  /* Delete the local object and add the remote one if it is not marked as deleted. */
  ephy_synchronizable_manager_remove (data->manager, data->synchronizable);
  if (!is_deleted) {
    ephy_synchronizable_manager_add (data->manager, synchronizable);
    LOG ("Successfully downloaded from server");
  } else {
    LOG ("The newer version was a deleted object");
  }

  g_object_unref (synchronizable);
out:
  if (node)
    json_node_unref (node);
  if (error)
    g_error_free (error);
  if (bundle)
    ephy_sync_crypto_key_bundle_free (bundle);
  sync_async_data_free (data);
}

static void
ephy_sync_service_download_synchronizable (EphySyncService           *self,
                                           EphySynchronizableManager *manager,
                                           EphySynchronizable        *synchronizable)
{
  SyncAsyncData *data;
  char *endpoint;
  char *id_safe;
  const char *collection;
  const char *id;

  g_assert (EPHY_IS_SYNC_SERVICE (self));
  g_assert (EPHY_IS_SYNCHRONIZABLE_MANAGER (manager));
  g_assert (EPHY_IS_SYNCHRONIZABLE (synchronizable));
  g_assert (ephy_sync_service_is_signed_in (self));

  id = ephy_synchronizable_get_id (synchronizable);
  collection = ephy_synchronizable_manager_get_collection_name (manager);
  /* Firefox uses UUIDs with curly braces as IDs for saved passwords records.
   * Curly braces are unsafe characters in URLs so they must be encoded. */
  id_safe = soup_uri_encode (id, NULL);
  endpoint = g_strdup_printf ("storage/%s/%s", collection, id_safe);
  data = sync_async_data_new (self, manager, synchronizable);

  LOG ("Downloading object with id %s...", id);
  ephy_sync_service_queue_storage_request (self, endpoint,
                                           SOUP_METHOD_GET, NULL, -1, -1,
                                           download_synchronizable_cb, data);

  g_free (endpoint);
  g_free (id_safe);
}

static void
upload_synchronizable_cb (SoupSession *session,
                          SoupMessage *msg,
                          gpointer     user_data)
{
  SyncAsyncData *data = (SyncAsyncData *)user_data;
  double time_modified;

  /* Code 412 means that there is a more recent version of the object
   * on the server. Download it. */
  if (msg->status_code == 412) {
    LOG ("Found a newer version of the object on the server, downloading it...");
    ephy_sync_service_download_synchronizable (data->service, data->manager, data->synchronizable);
  } else if (msg->status_code == 200) {
    LOG ("Successfully uploaded to server");
    time_modified = g_ascii_strtod (msg->response_body->data, NULL);
    ephy_synchronizable_set_server_time_modified (data->synchronizable, time_modified);
    ephy_synchronizable_manager_save (data->manager, data->synchronizable);
  } else {
    g_warning ("Failed to upload object. Status code: %u, response: %s",
               msg->status_code, msg->response_body->data);
  }

  sync_async_data_free (data);
}

static void
ephy_sync_service_upload_synchronizable (EphySyncService           *self,
                                         EphySynchronizableManager *manager,
                                         EphySynchronizable        *synchronizable)
{
  SyncCryptoKeyBundle *bundle;
  SyncAsyncData *data;
  JsonNode *bso;
  char *endpoint;
  char *body;
  char *id_safe;
  const char *collection;
  const char *id;

  g_assert (EPHY_IS_SYNC_SERVICE (self));
  g_assert (EPHY_IS_SYNCHRONIZABLE_MANAGER (manager));
  g_assert (EPHY_IS_SYNCHRONIZABLE (synchronizable));
  g_assert (ephy_sync_service_is_signed_in (self));

  collection = ephy_synchronizable_manager_get_collection_name (manager);
  bundle = ephy_sync_service_get_key_bundle (self, collection);
  bso = ephy_synchronizable_to_bso (synchronizable, bundle);
  id = ephy_synchronizable_get_id (synchronizable);
  /* Firefox uses UUIDs with curly braces as IDs for saved passwords records.
   * Curly braces are unsafe characters in URLs so they must be encoded. */
  id_safe = soup_uri_encode (id, NULL);
  endpoint = g_strdup_printf ("storage/%s/%s", collection, id_safe);
  data = sync_async_data_new (self, manager, synchronizable);
  body = json_to_string (bso, FALSE);

  LOG ("Uploading object with id %s...", id);
  ephy_sync_service_queue_storage_request (self, endpoint, SOUP_METHOD_PUT, body, -1,
                                           ephy_synchronizable_get_server_time_modified (synchronizable),
                                           upload_synchronizable_cb, data);

  g_free (id_safe);
  g_free (body);
  g_free (endpoint);
  json_node_unref (bso);
  ephy_sync_crypto_key_bundle_free (bundle);
}

static void
merge_finished_cb (GSList   *to_upload,
                   gpointer  user_data)
{
  SyncCollectionAsyncData *data = (SyncCollectionAsyncData *)user_data;

  for (GSList *l = to_upload; l && l->data; l = l->next)
    ephy_sync_service_upload_synchronizable (data->service, data->manager, l->data);

  if (data->is_last)
    g_signal_emit (data->service, signals[SYNC_FINISHED], 0);

  if (to_upload)
    g_slist_free_full (to_upload, g_object_unref);
  sync_collection_async_data_free (data);
}

static void
sync_collection_cb (SoupSession *session,
                    SoupMessage *msg,
                    gpointer     user_data)
{
  SyncCollectionAsyncData *data = (SyncCollectionAsyncData *)user_data;
  EphySynchronizable *remote;
  SyncCryptoKeyBundle *bundle;
  JsonNode *node = NULL;
  JsonArray *array = NULL;
  GError *error = NULL;
  GType type;
  const char *collection;
  const char *last_modified;
  gboolean is_deleted;

  collection = ephy_synchronizable_manager_get_collection_name (data->manager);

  if (msg->status_code != 200) {
    g_warning ("Failed to get records in collection %s. Status code: %u, response: %s",
               collection, msg->status_code, msg->response_body->data);
    goto out_error;
  }
  node = json_from_string (msg->response_body->data, &error);
  if (error) {
    g_warning ("Response is not a valid JSON: %s", error->message);
    goto out_error;
  }
  array = json_node_get_array (node);
  if (!array) {
    g_warning ("JSON node does not hold an array");
    goto out_error;
  }

  type = ephy_synchronizable_manager_get_synchronizable_type (data->manager);
  bundle = ephy_sync_service_get_key_bundle (data->service, collection);
  for (guint i = 0; i < json_array_get_length (array); i++) {
    remote = EPHY_SYNCHRONIZABLE (ephy_synchronizable_from_bso (json_array_get_element (array, i),
                                                                type, bundle, &is_deleted));
    if (!remote) {
      g_warning ("Failed to create synchronizable object from BSO, skipping...");
      continue;
    }
    if (is_deleted)
      data->remotes_deleted = g_slist_prepend (data->remotes_deleted, remote);
    else
      data->remotes_updated = g_slist_prepend (data->remotes_updated, remote);
  }

  LOG ("Found %u deleted objects and %u new/updated objects in %s collection",
       g_slist_length (data->remotes_deleted),
       g_slist_length (data->remotes_updated),
       collection);

  /* Update sync time. */
  last_modified = soup_message_headers_get_one (msg->response_headers, "X-Last-Modified");
  ephy_synchronizable_manager_set_sync_time (data->manager, g_ascii_strtod (last_modified, NULL));
  ephy_synchronizable_manager_set_is_initial_sync (data->manager, FALSE);

  ephy_synchronizable_manager_merge (data->manager, data->is_initial,
                                     data->remotes_deleted, data->remotes_updated,
                                     merge_finished_cb, data);
  goto out_no_error;

out_error:
  if (data->is_last)
    g_signal_emit (data->service, signals[SYNC_FINISHED], 0);
  sync_collection_async_data_free (data);
out_no_error:
  if (node)
    json_node_unref (node);
  if (error)
    g_error_free (error);
}

static void
ephy_sync_service_sync_collection (EphySyncService           *self,
                                   EphySynchronizableManager *manager,
                                   gboolean                   is_last)
{
  SyncCollectionAsyncData *data;
  const char *collection;
  char *endpoint;
  gboolean is_initial;

  g_assert (EPHY_IS_SYNC_SERVICE (self));
  g_assert (EPHY_IS_SYNCHRONIZABLE_MANAGER (manager));
  g_assert (ephy_sync_service_is_signed_in (self));

  collection = ephy_synchronizable_manager_get_collection_name (manager);
  is_initial = ephy_synchronizable_manager_is_initial_sync (manager);

  if (is_initial) {
    endpoint = g_strdup_printf ("storage/%s?full=true", collection);
  } else {
    endpoint = g_strdup_printf ("storage/%s?newer=%.2lf&full=true", collection,
                                ephy_synchronizable_manager_get_sync_time (manager));
  }

  LOG ("Syncing %s collection %s...", collection, is_initial ? "initial" : "regular");
  data = sync_collection_async_data_new (self, manager, is_initial, is_last);
  ephy_sync_service_queue_storage_request (self, endpoint, SOUP_METHOD_GET,
                                           NULL, -1, -1,
                                           sync_collection_cb, data);

  g_free (endpoint);
}

static gboolean
ephy_sync_service_sync (gpointer user_data)
{
  EphySyncService *self = EPHY_SYNC_SERVICE (user_data);
  guint index = 0;
  guint num_managers;

  g_assert (ephy_sync_service_is_signed_in (self));

  if (!self->managers) {
    g_signal_emit (self, signals[SYNC_FINISHED], 0);
    return G_SOURCE_CONTINUE;
  }

  num_managers = g_slist_length (self->managers);
  for (GSList *l = self->managers; l && l->data; l = l->next)
    ephy_sync_service_sync_collection (self, l->data, ++index == num_managers);

  return G_SOURCE_CONTINUE;
}

static void
ephy_sync_service_unregister_client_id (EphySyncService *self)
{
  char *client_id;
  char *endpoint;

  g_assert (EPHY_IS_SYNC_SERVICE (self));

  client_id = g_settings_get_string (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_CLIENT_ID);
  endpoint = g_strdup_printf ("storage/clients/%s", client_id);

  ephy_sync_service_queue_storage_request (self, endpoint, SOUP_METHOD_DELETE,
                                           NULL, -1, -1, NULL, NULL);
  g_settings_set_string (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_CLIENT_ID, "");

  g_free (endpoint);
  g_free (client_id);
}

static void
ephy_sync_service_register_client_id (EphySyncService *self)
{
  SyncCryptoKeyBundle *bundle;
  JsonNode *node;
  JsonObject *record;
  JsonObject *payload;
  JsonArray *array;
  char *client_id;
  char *name;
  char *protocol;
  char *payload_clear;
  char *payload_cipher;
  char *body;
  char *endpoint;

  g_assert (EPHY_IS_SYNC_SERVICE (self));

  node = json_node_new (JSON_NODE_OBJECT);
  record = json_object_new ();
  payload = json_object_new ();
  array = json_array_new ();
  protocol = g_strdup_printf ("1.%d", STORAGE_VERSION);
  json_array_add_string_element (array, protocol);
  json_object_set_array_member (payload, "protocols", array);
  client_id = ephy_sync_crypto_get_random_sync_id ();
  json_object_set_string_member (payload, "id", client_id);
  name = g_strdup_printf ("%s on Epiphany", client_id);
  json_object_set_string_member (payload, "name", name);
  json_object_set_string_member (payload, "type", "desktop");
  json_object_set_string_member (payload, "os", "Linux");
  json_object_set_string_member (payload, "application", "Epiphany");
  json_object_set_string_member (payload, "fxaDeviceId",
                                 ephy_sync_service_get_secret (self, secrets[UID]));
  json_node_set_object (node, payload);
  payload_clear = json_to_string (node, FALSE);
  bundle = ephy_sync_service_get_key_bundle (self, "clients");
  payload_cipher = ephy_sync_crypto_encrypt_record (payload_clear, bundle);
  json_object_set_string_member (record, "id", client_id);
  json_object_set_string_member (record, "payload", payload_cipher);
  json_node_set_object (node, record);
  body = json_to_string (node, FALSE);
  endpoint = g_strdup_printf ("storage/clients/%s", client_id);

  ephy_sync_service_queue_storage_request (self, endpoint, SOUP_METHOD_PUT,
                                           body, -1, -1, NULL, NULL);
  g_settings_set_string (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_CLIENT_ID, client_id);

  g_free (endpoint);
  g_free (body);
  g_free (payload_cipher);
  ephy_sync_crypto_key_bundle_free (bundle);
  g_free (payload_clear);
  g_free (name);
  g_free (client_id);
  g_free (protocol);
  json_object_unref(payload);
  json_object_unref(record);
  json_node_unref (node);
}

static void
ephy_sync_service_stop_periodical_sync (EphySyncService *self)
{
  g_assert (EPHY_IS_SYNC_SERVICE (self));

  if (self->source_id != 0) {
    g_source_remove (self->source_id);
    self->source_id = 0;
  }
}

static void
ephy_sync_service_schedule_periodical_sync (EphySyncService *self)
{
  guint seconds;

  g_assert (EPHY_IS_SYNC_SERVICE (self));

  seconds = g_settings_get_uint (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_FREQUENCY) * 60;
  self->source_id = g_timeout_add_seconds (seconds, ephy_sync_service_sync, self);

  LOG ("Scheduled new sync with frequency %u minutes", seconds / 60);
}

static void
sync_frequency_changed_cb (GSettings       *settings,
                           char            *key,
                           EphySyncService *self)
{
  g_assert (EPHY_IS_SYNC_SERVICE (self));

  ephy_sync_service_stop_periodical_sync (self);
  ephy_sync_service_schedule_periodical_sync (self);
}

static void
forget_secrets_cb (SecretService *service,
                   GAsyncResult  *result,
                   gpointer       user_data)
{
  GError *error = NULL;

  secret_service_clear_finish (service, result, &error);
  if (error) {
    g_warning ("Failed to clear sync secrets: %s", error->message);
    g_error_free (error);
  }
}

static void
ephy_sync_service_forget_secrets (EphySyncService *self)
{
  GHashTable *attributes;

  g_assert (EPHY_IS_SYNC_SERVICE (self));
  g_assert (self->secrets);

  attributes = secret_attributes_build (EPHY_SYNC_SECRET_SCHEMA,
                                        ACCOUNT_KEY, self->account,
                                        NULL);
  secret_service_clear (NULL, EPHY_SYNC_SECRET_SCHEMA, attributes, NULL,
                        (GAsyncReadyCallback)forget_secrets_cb, NULL);
  g_hash_table_remove_all (self->secrets);

  g_hash_table_unref (attributes);
}

static void
load_secrets_cb (SecretService   *service,
                 GAsyncResult    *result,
                 EphySyncService *self)
{
  SecretValue *value = NULL;
  JsonNode *node = NULL;
  JsonObject *object;
  GList *res = NULL;
  GError *error = NULL;
  const char *message;
  const char *suggestion;

  res = secret_service_search_finish (service, result, &error);
  if (error) {
    g_warning ("Failed to search for sync secrets: %s", error->message);
    g_error_free (error);
    message = _("Could not find the sync secrets for the current sync user.");
    goto out_error;
  }

  if (!(res && res->data)) {
    message = _("The sync secrets for the current sync user are null.");
    goto out_error;
  }

  value = secret_item_get_secret ((SecretItem *)res->data);
  node = json_from_string (secret_value_get_text (value), &error);
  if (error) {
    g_warning ("Sync secrets are not a valid JSON: %s", error->message);
    g_error_free (error);
    message = _("The sync secrets for the current sync user are invalid.");
    goto out_error;
  }

  /* Set secrets and start periodical sync. */
  object = json_node_get_object (node);
  for (GList *l = json_object_get_members (object); l && l->data; l = l->next)
    ephy_sync_service_set_secret (self, l->data,
                                  json_object_get_string_member (object, l->data));

  if (self->sync_periodically)
    ephy_sync_service_start_periodical_sync (self);
  goto out_no_error;

out_error:
  suggestion = _("Please visit Preferences and sign in again to continue syncing.");
  ephy_notification_show (ephy_notification_new (message, suggestion));
out_no_error:
  if (value)
    secret_value_unref (value);
  if (res)
    g_list_free_full (res, g_object_unref);
  if (node)
    json_node_unref (node);
}

static void
ephy_sync_service_load_secrets (EphySyncService *self)
{
  GHashTable *attributes;

  g_assert (EPHY_IS_SYNC_SERVICE (self));
  g_assert (self->secrets);

  attributes = secret_attributes_build (EPHY_SYNC_SECRET_SCHEMA,
                                        ACCOUNT_KEY, self->account,
                                        NULL);
  secret_service_search (NULL, EPHY_SYNC_SECRET_SCHEMA, attributes,
                         SECRET_SEARCH_UNLOCK | SECRET_SEARCH_LOAD_SECRETS,
                         NULL, (GAsyncReadyCallback)load_secrets_cb, self);

  g_hash_table_unref (attributes);
}

static void
store_secrets_cb (SecretService   *service,
                  GAsyncResult    *result,
                  EphySyncService *self)
{
  GError *error = NULL;

  secret_service_store_finish (service, result, &error);
  if (error) {
    g_warning ("Failed to store sync secrets: %s", error->message);
    ephy_sync_service_destroy_session (self, NULL);
    g_clear_pointer (&self->account, g_free);
    g_hash_table_remove_all (self->secrets);
  } else {
    ephy_sync_service_register_client_id (self);
  }

  g_signal_emit (self, signals[STORE_FINISHED], 0, error);

  if (error)
    g_error_free (error);
}

static void
ephy_sync_service_store_secrets (EphySyncService *self)
{
  JsonNode *node;
  JsonObject *object;
  SecretValue *secret;
  GHashTable *attributes;
  GHashTableIter iter;
  gpointer key;
  gpointer value;
  char *json_string;
  char *label;

  g_assert (EPHY_IS_SYNC_SERVICE (self));
  g_assert (self->secrets);
  g_assert (self->account);

  node = json_node_new (JSON_NODE_OBJECT);
  object = json_object_new ();
  g_hash_table_iter_init (&iter, self->secrets);
  while (g_hash_table_iter_next (&iter, &key, &value))
    json_object_set_string_member (object, key, value);
  json_node_set_object (node, object);
  json_string = json_to_string (node, FALSE);

  secret = secret_value_new (json_string, -1, "text/plain");
  attributes = secret_attributes_build (EPHY_SYNC_SECRET_SCHEMA,
                                        ACCOUNT_KEY, self->account,
                                        NULL);
  /* Translators: %s is the email of the user. */
  label = g_strdup_printf (_("The sync secrets of %s"), self->account);

  secret_service_store (NULL, EPHY_SYNC_SECRET_SCHEMA,
                        attributes, NULL, label, secret, NULL,
                        (GAsyncReadyCallback)store_secrets_cb, self);

  g_free (label);
  g_free (json_string);
  secret_value_unref (secret);
  g_hash_table_unref (attributes);
  json_object_unref (object);
  json_node_unref (node);
}

static void
ephy_sync_service_dispose (GObject *object)
{
  EphySyncService *self = EPHY_SYNC_SERVICE (object);

  if (ephy_sync_service_is_signed_in (self))
    ephy_sync_service_stop_periodical_sync (self);

  ephy_sync_service_clear_storage_credentials (self);
  g_clear_object (&self->session);
  g_clear_pointer (&self->account, g_free);
  g_clear_pointer (&self->rsa_key_pair, ephy_sync_crypto_rsa_key_pair_free);
  g_clear_pointer (&self->secrets, g_hash_table_destroy);
  g_clear_pointer (&self->managers, g_slist_free);
  g_queue_free_full (self->storage_queue, (GDestroyNotify)storage_request_async_data_free);
  self->storage_queue = NULL;

  G_OBJECT_CLASS (ephy_sync_service_parent_class)->dispose (object);
}

static void
ephy_sync_service_constructed (GObject *object)
{
  EphySyncService *self = EPHY_SYNC_SERVICE (object);
  WebKitSettings *settings;
  const char *user_agent;

  G_OBJECT_CLASS (ephy_sync_service_parent_class)->constructed (object);

  if (self->sync_periodically) {
    settings = ephy_embed_prefs_get_settings ();
    user_agent = webkit_settings_get_user_agent (settings);
    g_object_set (self->session, "user-agent", user_agent, NULL);

    g_signal_connect (EPHY_SETTINGS_SYNC, "changed::"EPHY_PREFS_SYNC_FREQUENCY,
                      G_CALLBACK (sync_frequency_changed_cb), self);
  }
}

static void
ephy_sync_service_init (EphySyncService *self)
{
  char *account;

  self->session = soup_session_new ();
  self->storage_queue = g_queue_new ();
  self->secrets = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  account = g_settings_get_string (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_USER);
  if (g_strcmp0 (account, "")) {
    self->account = g_strdup (account);
    ephy_sync_service_load_secrets (self);
  }

  g_free (account);
}

static void
ephy_sync_service_class_init (EphySyncServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = ephy_sync_service_set_property;
  object_class->get_property = ephy_sync_service_get_property;
  object_class->constructed = ephy_sync_service_constructed;
  object_class->dispose = ephy_sync_service_dispose;

  obj_properties[PROP_SYNC_PERIODICALLY] =
    g_param_spec_boolean ("sync-periodically",
                          "Sync periodically",
                          "Whether should periodically sync data",
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);

  signals[STORE_FINISHED] =
    g_signal_new ("sync-secrets-store-finished",
                  EPHY_TYPE_SYNC_SERVICE,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_ERROR);

  signals[SIGN_IN_ERROR] =
    g_signal_new ("sync-sign-in-error",
                  EPHY_TYPE_SYNC_SERVICE,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_STRING);

  signals[SYNC_FINISHED] =
    g_signal_new ("sync-finished",
                  EPHY_TYPE_SYNC_SERVICE,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

EphySyncService *
ephy_sync_service_new (gboolean sync_periodically)
{
  return EPHY_SYNC_SERVICE (g_object_new (EPHY_TYPE_SYNC_SERVICE,
                                          "sync-periodically", sync_periodically,
                                          NULL));
}

gboolean
ephy_sync_service_is_signed_in (EphySyncService *self)
{
  g_return_val_if_fail (EPHY_IS_SYNC_SERVICE (self), FALSE);

  return self->account != NULL;
}

const char *
ephy_sync_service_get_sync_user (EphySyncService *self)
{
  g_return_val_if_fail (EPHY_IS_SYNC_SERVICE (self), NULL);

  return self->account;
}

static void
ephy_sync_service_report_sign_in_error (EphySyncService *self,
                                        const char      *message,
                                        const char      *session_token,
                                        gboolean         clear_secrets)
{
  g_assert (EPHY_IS_SYNC_SERVICE (self));
  g_assert (message);

  g_signal_emit (self, signals[SIGN_IN_ERROR], 0, message);
  ephy_sync_service_destroy_session (self, session_token);

  if (clear_secrets) {
    g_clear_pointer (&self->account, g_free);
    g_hash_table_remove_all (self->secrets);
  }
}

static char *
ephy_sync_service_upload_crypto_keys_record (EphySyncService *self)
{
  SyncCryptoKeyBundle *bundle;
  JsonNode *node;
  JsonObject *record;
  char *payload_clear;
  char *payload_cipher;
  char *body;
  const char *master_key_hex;
  guint8 *master_key;

  g_assert (EPHY_IS_SYNC_SERVICE (self));
  master_key_hex = ephy_sync_service_get_secret (self, secrets[MASTER_KEY]);
  g_assert (master_key_hex);

  node = json_node_new (JSON_NODE_OBJECT);
  record = json_object_new ();
  payload_clear = ephy_sync_crypto_generate_crypto_keys (32);
  master_key = ephy_sync_crypto_decode_hex (master_key_hex);
  bundle = ephy_sync_crypto_derive_key_bundle (master_key, 32);
  payload_cipher = ephy_sync_crypto_encrypt_record (payload_clear, bundle);
  json_object_set_string_member (record, "payload", payload_cipher);
  json_object_set_string_member (record, "id", "keys");
  json_node_set_object (node, record);
  body = json_to_string (node, FALSE);

  ephy_sync_service_queue_storage_request (self, "storage/crypto/keys",
                                           SOUP_METHOD_PUT, body,
                                           -1, -1, NULL, NULL);

  g_free (body);
  g_free (payload_cipher);
  g_free (master_key);
  json_object_unref (record);
  json_node_unref (node);
  ephy_sync_crypto_key_bundle_free (bundle);

  return payload_clear;
}

static void
obtain_crypto_keys_cb (SoupSession *session,
                       SoupMessage *msg,
                       gpointer     user_data)
{
  EphySyncService *self = EPHY_SYNC_SERVICE (user_data);
  SyncCryptoKeyBundle *bundle = NULL;
  JsonNode *node = NULL;
  JsonObject *json = NULL;
  GError *error = NULL;
  const char *payload;
  char *crypto_keys = NULL;
  guint8 *key_b = NULL;

  if (msg->status_code == 404) {
    crypto_keys = ephy_sync_service_upload_crypto_keys_record (self);
    goto store_secrets;
  }

  if (msg->status_code != 200) {
    g_warning ("Failed to get crypto/keys record. Status code: %u, response: %s",
               msg->status_code, msg->response_body->data);
    goto out_error;
  }

  node = json_from_string (msg->response_body->data, &error);
  if (error) {
    g_warning ("Response is not a valid JSON: %s", error->message);
    goto out_error;
  }
  json = json_node_get_object (node);
  if (!json) {
    g_warning ("JSON node does not hold an object");
    goto out_error;
  }
  payload = json_object_get_string_member (json, "payload");
  if (!payload) {
    g_warning ("JSON object has missing or invalid 'payload' member");
    goto out_error;
  }
  /* Derive the Sync Key bundle from kB. The bundle consists of two 32 bytes keys:
   * the first one used as a symmetric encryption key (AES) and the second one
   * used as a HMAC key. */
  key_b = ephy_sync_crypto_decode_hex (ephy_sync_service_get_secret (self, secrets[MASTER_KEY]));
  bundle = ephy_sync_crypto_derive_key_bundle (key_b, 32);
  crypto_keys = ephy_sync_crypto_decrypt_record (payload, bundle);
  if (!crypto_keys) {
    g_warning ("Failed to decrypt crypto/keys record");
    goto out_error;
  }

store_secrets:
  ephy_sync_service_set_secret (self, secrets[CRYPTO_KEYS], crypto_keys);
  ephy_sync_service_store_secrets (self);
  goto out_no_error;
out_error:
  ephy_sync_service_report_sign_in_error (self, _("Failed to retrieve crypto keys."),
                                          NULL, TRUE);
out_no_error:
  if (bundle)
    ephy_sync_crypto_key_bundle_free (bundle);
  if (node)
    json_node_unref (node);
  if (error)
    g_error_free (error);
  g_free (crypto_keys);
  g_free (key_b);
}

static void
ephy_sync_service_obtain_crypto_keys (EphySyncService *self)
{
  g_assert (EPHY_IS_SYNC_SERVICE (self));

  ephy_sync_service_queue_storage_request (self, "storage/crypto/keys",
                                           SOUP_METHOD_GET, NULL, -1, -1,
                                           obtain_crypto_keys_cb, self);
}

static JsonObject *
make_engine_object (int version)
{
  JsonObject *object;
  char *sync_id;

  object = json_object_new ();
  sync_id = ephy_sync_crypto_get_random_sync_id ();
  json_object_set_int_member (object, "version", version);
  json_object_set_string_member (object, "syncID", sync_id);

  g_free (sync_id);

  return object;
}

static void
ephy_sync_service_upload_meta_global_record (EphySyncService *self)
{
  JsonNode *node;
  JsonObject *record;
  JsonObject *payload;
  JsonObject *engines;
  JsonArray *declined;
  char *sync_id;
  char *payload_str;
  char *body;

  g_assert (EPHY_IS_SYNC_SERVICE (self));

  node = json_node_new (JSON_NODE_OBJECT);
  record = json_object_new ();
  payload = json_object_new ();
  engines = json_object_new ();
  declined = json_array_new ();
  json_array_add_string_element (declined, "addons");
  json_array_add_string_element (declined, "prefs");
  json_array_add_string_element (declined, "tabs");
  json_object_set_array_member (payload, "declined", declined);
  json_object_set_object_member (engines, "clients", make_engine_object (1));
  json_object_set_object_member (engines, "bookmarks", make_engine_object (2));
  json_object_set_object_member (engines, "history", make_engine_object (1));
  json_object_set_object_member (engines, "passwords", make_engine_object (1));
  json_object_set_object_member (engines, "forms", make_engine_object (1));
  json_object_set_object_member (payload, "engines", engines);
  json_object_set_int_member (payload, "storageVersion", STORAGE_VERSION);
  sync_id = ephy_sync_crypto_get_random_sync_id ();
  json_object_set_string_member (payload, "syncID", sync_id);
  json_node_set_object (node, payload);
  payload_str = json_to_string (node, FALSE);
  json_object_set_string_member (record, "payload", payload_str);
  json_object_set_string_member (record, "id", "global");
  json_node_set_object (node, record);
  body = json_to_string (node, FALSE);

  ephy_sync_service_queue_storage_request (self, "storage/meta/global",
                                           SOUP_METHOD_PUT, body,
                                           -1, -1, NULL, NULL);

  g_free (body);
  g_free (payload_str);
  g_free (sync_id);
  json_object_unref (payload);
  json_object_unref (record);
  json_node_unref (node);
}

static void
check_storage_version_cb (SoupSession *session,
                          SoupMessage *msg,
                          gpointer     user_data)
{
  EphySyncService *self = EPHY_SYNC_SERVICE (user_data);
  JsonParser *parser = NULL;
  JsonObject *json = NULL;
  GError *error = NULL;
  char *payload = NULL;
  char *message = NULL;
  int storage_version;

  if (msg->status_code == 404) {
    ephy_sync_service_upload_meta_global_record (self);
    goto obtain_crypto_keys;
  }

  if (msg->status_code != 200) {
    g_warning ("Failed to get meta/global record. Status code: %u, response: %s",
               msg->status_code, msg->response_body->data);
    goto out_error;
  }

  parser = json_parser_new ();
  json_parser_load_from_data (parser, msg->response_body->data, -1, &error);
  if (error) {
    g_warning ("Response is not a valid JSON: %s", error->message);
    goto out_error;
  }
  json = json_node_get_object (json_parser_get_root (parser));
  if (!json) {
    g_warning ("JSON node does not hold a JSON object");
    goto out_error;
  }
  if (!json_object_get_string_member (json, "payload")) {
    g_warning ("JSON object has missing or invalid 'payload' member");
    goto out_error;
  }
  payload = g_strdup (json_object_get_string_member (json, "payload"));
  json_parser_load_from_data (parser, payload, -1, &error);
  if (error) {
    g_warning ("Payload is not a valid JSON: %s", error->message);
    goto out_error;
  }
  json = json_node_get_object (json_parser_get_root (parser));
  if (!json) {
    g_warning ("JSON node does not hold a JSON object");
    goto out_error;
  }
  if (!json_object_get_int_member (json, "storageVersion")) {
    g_warning ("JSON object has missing or invalid 'storageVersion' member");
    goto out_error;
  }
  storage_version = json_object_get_int_member (json, "storageVersion");
  if (storage_version != STORAGE_VERSION) {
    /* Translators: the %d is the storage version, the \n is a newline character. */
    message = g_strdup_printf (_("Your Firefox Account uses storage version %d "
                                 "which Epiphany does not support.\n"
                                 "Create a new account to use the latest storage version."),
                               storage_version);
    goto out_error;
  }

obtain_crypto_keys:
  ephy_sync_service_obtain_crypto_keys (self);
  goto out_no_error;
out_error:
  message = message ? message : _("Failed to verify storage version.");
  ephy_sync_service_report_sign_in_error (self, message, NULL, TRUE);
out_no_error:
  if (parser)
    g_object_unref (parser);
  if (error)
    g_error_free (error);
  g_free (payload);
  g_free (message);
}

static void
ephy_sync_service_check_storage_version (EphySyncService *self)
{
  g_assert (EPHY_IS_SYNC_SERVICE (self));

  ephy_sync_service_queue_storage_request (self, "storage/meta/global",
                                           SOUP_METHOD_GET, NULL, -1, -1,
                                           check_storage_version_cb, self);
}

static void
ephy_sync_service_conclude_sign_in (EphySyncService *self,
                                    SignInAsyncData *data,
                                    const char      *bundle)
{
  guint8 *unwrap_key_b;
  guint8 *key_a;
  guint8 *key_b;
  char *key_b_hex;

  g_assert (EPHY_IS_SYNC_SERVICE (self));
  g_assert (data);
  g_assert (bundle);

  /* Derive the master sync keys form the key bundle. */
  unwrap_key_b = ephy_sync_crypto_decode_hex (data->unwrap_b_key);
  if (!ephy_sync_crypto_compute_sync_keys (bundle, data->resp_hmac_key,
                                           data->resp_xor_key, unwrap_key_b,
                                           &key_a, &key_b, 32)) {
    ephy_sync_service_report_sign_in_error (self, _("Failed to retrieve the Sync Key"),
                                            data->session_token, FALSE);
    goto out;
  }

  /* Save email and tokens. */
  self->account = g_strdup (data->email);
  ephy_sync_service_set_secret (self, secrets[UID], data->uid);
  ephy_sync_service_set_secret (self, secrets[SESSION_TOKEN], data->session_token);
  key_b_hex = ephy_sync_crypto_encode_hex (key_b, 32);
  ephy_sync_service_set_secret (self, secrets[MASTER_KEY], key_b_hex);

  ephy_sync_service_check_storage_version (self);

  g_free (key_b_hex);
  g_free (key_b);
  g_free (key_a);
out:
  g_free (unwrap_key_b);
  sign_in_async_data_free (data);
}

static void
get_account_keys_cb (SoupSession *session,
                     SoupMessage *msg,
                     gpointer     user_data)
{
  SignInAsyncData *data = (SignInAsyncData *)user_data;
  JsonNode *node = NULL;
  JsonObject *json = NULL;
  GError *error = NULL;
  const char *bundle;

  node = json_from_string (msg->response_body->data, &error);
  if (error) {
    g_warning ("Response is not a valid JSON: %s", error->message);
    goto out_error;
  }
  json = json_node_get_object (node);
  if (!json) {
    g_warning ("JSON node does not hold a JSON object");
    goto out_error;
  }

  if (msg->status_code == 200) {
    bundle = json_object_get_string_member (json, "bundle");
    if (!bundle) {
      g_warning ("JSON object has invalid or missing 'bundle' member");
      goto out_error;
    }
    /* Extract the master sync keys from the bundle and save tokens. */
    ephy_sync_service_conclude_sign_in (data->service, data, bundle);
    goto out_no_error;
  }

  /* If account is not verified, poll the Firefox Accounts Server until the
   * verification has completed. */
  if (json_object_get_int_member (json, "errno") == 104) {
    LOG ("Account not verified, retrying...");
    ephy_sync_service_fxa_hawk_get_async (data->service, "account/keys",
                                          data->token_id_hex, data->req_hmac_key,
                                          32, get_account_keys_cb, data);
    goto out_no_error;
  }

  g_warning ("Failed to get /account/keys. Status code: %u, response: %s",
             msg->status_code, msg->response_body->data);

out_error:
  ephy_sync_service_report_sign_in_error (data->service,
                                          _("Failed to retrieve the Sync Key"),
                                          data->session_token, FALSE);
  sign_in_async_data_free (data);
out_no_error:
  if (node)
    json_node_unref (node);
  if (error)
    g_error_free (error);
}

void
ephy_sync_service_do_sign_in (EphySyncService *self,
                              const char      *email,
                              const char      *uid,
                              const char      *session_token,
                              const char      *key_fetch_token,
                              const char      *unwrap_b_key)
{
  SignInAsyncData *data;
  guint8 *token_id;
  guint8 *req_hmac_key;
  guint8 *resp_hmac_key;
  guint8 *resp_xor_key;
  char *token_id_hex;

  g_return_if_fail (EPHY_IS_SYNC_SERVICE (self));
  g_return_if_fail (email);
  g_return_if_fail (uid);
  g_return_if_fail (session_token);
  g_return_if_fail (key_fetch_token);
  g_return_if_fail (unwrap_b_key);

  /* Derive tokenID, reqHMACkey, respHMACkey and respXORkey from keyFetchToken.
   * tokenID and reqHMACkey are used to sign a HAWK GET requests to the /account/keys
   * endpoint. The server looks up the stored table entry with tokenID, checks
   * the request HMAC for validity, then returns the pre-encrypted response.
   * See https://github.com/mozilla/fxa-auth-server/wiki/onepw-protocol#fetching-sync-keys */
  ephy_sync_crypto_process_key_fetch_token (key_fetch_token, &token_id, &req_hmac_key,
                                            &resp_hmac_key, &resp_xor_key, 32);
  token_id_hex = ephy_sync_crypto_encode_hex (token_id, 32);

  /* Get the master sync key bundle from the /account/keys endpoint. */
  data = sign_in_async_data_new (self, email, uid,
                                 session_token, unwrap_b_key,
                                 token_id_hex, req_hmac_key,
                                 resp_hmac_key, resp_xor_key);
  ephy_sync_service_fxa_hawk_get_async (self, "account/keys", token_id_hex,
                                        req_hmac_key, 32,
                                        get_account_keys_cb, data);

  g_free (token_id_hex);
  g_free (token_id);
  g_free (req_hmac_key);
  g_free (resp_hmac_key);
  g_free (resp_xor_key);
}

static void
synchronizable_deleted_cb (EphySynchronizableManager *manager,
                           EphySynchronizable        *synchronizable,
                           EphySyncService           *self)
{
  g_assert (EPHY_IS_SYNCHRONIZABLE_MANAGER (manager));
  g_assert (EPHY_IS_SYNCHRONIZABLE (synchronizable));
  g_assert (EPHY_IS_SYNC_SERVICE (self));

  if (!ephy_sync_service_is_signed_in (self))
    return;

  ephy_sync_service_delete_synchronizable (self, manager, synchronizable);
}

static void
synchronizable_modified_cb (EphySynchronizableManager *manager,
                            EphySynchronizable        *synchronizable,
                            EphySyncService           *self)
{
  g_assert (EPHY_IS_SYNCHRONIZABLE_MANAGER (manager));
  g_assert (EPHY_IS_SYNCHRONIZABLE (synchronizable));
  g_assert (EPHY_IS_SYNC_SERVICE (self));

  if (!ephy_sync_service_is_signed_in (self))
    return;

  ephy_sync_service_upload_synchronizable (self, manager, synchronizable);
}

void
ephy_sync_service_register_manager (EphySyncService           *self,
                                    EphySynchronizableManager *manager)
{
  g_return_if_fail (EPHY_IS_SYNC_SERVICE (self));
  g_return_if_fail (EPHY_IS_SYNCHRONIZABLE_MANAGER (manager));

  if (!g_slist_find (self->managers, manager)) {
    self->managers = g_slist_prepend (self->managers, manager);

    g_signal_connect (manager, "synchronizable-deleted",
                      G_CALLBACK (synchronizable_deleted_cb), self);
    g_signal_connect (manager, "synchronizable-modified",
                      G_CALLBACK (synchronizable_modified_cb), self);
  }
}

void
ephy_sync_service_unregister_manager (EphySyncService           *self,
                                      EphySynchronizableManager *manager)
{
  g_return_if_fail (EPHY_IS_SYNC_SERVICE (self));
  g_return_if_fail (EPHY_IS_SYNCHRONIZABLE_MANAGER (manager));

  self->managers = g_slist_remove (self->managers, manager);

  g_signal_handlers_disconnect_by_func (manager, synchronizable_deleted_cb, self);
  g_signal_handlers_disconnect_by_func (manager, synchronizable_modified_cb, self);
}


void
ephy_sync_service_do_sign_out (EphySyncService *self)
{
  g_return_if_fail (EPHY_IS_SYNC_SERVICE (self));

  ephy_sync_service_unregister_client_id (self);
  ephy_sync_service_stop_periodical_sync (self);
  ephy_sync_service_destroy_session (self, NULL);
  ephy_sync_service_clear_storage_credentials (self);
  ephy_sync_service_forget_secrets (self);
  g_clear_pointer (&self->account, g_free);

  /* Clear storage messages queue. */
  while (!g_queue_is_empty (self->storage_queue))
    storage_request_async_data_free (g_queue_pop_head (self->storage_queue));

  /* Clear managers. */
  for (GSList *l = self->managers; l && l->data; l = l->next) {
    g_signal_handlers_disconnect_by_func (l->data, synchronizable_deleted_cb, self);
    g_signal_handlers_disconnect_by_func (l->data, synchronizable_modified_cb, self);
  }
  g_slist_free (self->managers);
  self->managers = NULL;

  g_settings_set_string (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_USER, "");
  g_settings_set_boolean (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_BOOKMARKS_INITIAL, TRUE);
  g_settings_set_boolean (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_PASSWORDS_INITIAL, TRUE);
}

void
ephy_sync_service_do_sync (EphySyncService *self)
{
  g_return_if_fail (EPHY_IS_SYNC_SERVICE (self));
  g_return_if_fail (ephy_sync_service_is_signed_in (self));

  ephy_sync_service_sync (self);
}

void
ephy_sync_service_start_periodical_sync (EphySyncService *self)
{
  g_return_if_fail (EPHY_IS_SYNC_SERVICE (self));
  g_return_if_fail (ephy_sync_service_is_signed_in (self));
  g_return_if_fail (self->sync_periodically);

  ephy_sync_service_sync (self);
  ephy_sync_service_schedule_periodical_sync (self);
}
