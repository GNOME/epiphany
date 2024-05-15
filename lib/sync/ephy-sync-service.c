/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2016 Gabriel Ivascu <gabrielivascu@gnome.org>
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
#include "ephy-notification.h"
#include "ephy-settings.h"
#include "ephy-sync-crypto.h"
#include "ephy-sync-utils.h"
#include "ephy-user-agent.h"

#include <glib/gi18n.h>
#include <json-glib/json-glib.h>
#include <inttypes.h>
#include <libsoup/soup.h>
#include <math.h>
#include <string.h>

struct _EphySyncService {
  GObject parent_instance;

  SoupSession *session;
  guint source_id;

  GCancellable *cancellable;

  char *user;
  char *crypto_keys;
  GHashTable *secrets;
  GSList *managers;

  gboolean locked;
  char *storage_endpoint;
  char *storage_credentials_id;
  char *storage_credentials_key;
  gint64 storage_credentials_expiry_time;
  GQueue *storage_queue;

  char *certificate;
  SyncCryptoRSAKeyPair *key_pair;

  gboolean sync_periodically;
  gboolean is_signing_in;
};

G_DEFINE_FINAL_TYPE (EphySyncService, ephy_sync_service, G_TYPE_OBJECT);

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
  LOAD_FINISHED,
  SIGN_IN_ERROR,
  SYNC_FINISHED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

typedef void (*SoupSessionCallback) (SoupSession *session,
                                     SoupMessage *msg,
                                     gpointer     user_data);

typedef struct {
  char *endpoint;
  char *method;
  char *request_body;
  gint64 modified_since;
  gint64 unmodified_since;
  SoupSessionCallback callback;
  gpointer user_data;
} StorageRequestAsyncData;

typedef struct {
  EphySyncService *service;
  char *email;
  char *uid;
  char *session_token;
  char *unwrap_kb;
  char *token_id_hex;
  guint8 *req_hmac_key;
  guint8 *resp_hmac_key;
  guint8 *resp_xor_key;
} SignInAsyncData;

typedef struct {
  EphySyncService *service;
  EphySynchronizableManager *manager;
  gboolean is_initial;
  gboolean is_last;
  GList *remotes_deleted;
  GList *remotes_updated;
} SyncCollectionAsyncData;

typedef struct {
  EphySyncService *service;
  EphySynchronizableManager *manager;
  EphySynchronizable *synchronizable;
} SyncAsyncData;

typedef struct {
  EphySyncService *service;
  EphySynchronizableManager *manager;
  GPtrArray *synchronizables;
  guint start;
  guint end;
  char *batch_id;
  gboolean batch_is_last;
  gboolean sync_done;
} BatchUploadAsyncData;

static StorageRequestAsyncData *
storage_request_async_data_new (const char          *endpoint,
                                const char          *method,
                                const char          *request_body,
                                gint64               modified_since,
                                gint64               unmodified_since,
                                SoupSessionCallback  callback,
                                gpointer             user_data)
{
  StorageRequestAsyncData *data;

  data = g_new (StorageRequestAsyncData, 1);
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
  g_free (data);
}

typedef struct {
  SoupSessionCallback callback;
  gpointer user_data;
} SendAndReadAsyncData;

static SendAndReadAsyncData *
send_and_read_async_data_new (SoupSessionCallback callback,
                              gpointer            user_data)
{
  SendAndReadAsyncData *data;

  data = g_new (SendAndReadAsyncData, 1);
  data->callback = callback;
  data->user_data = user_data;

  return data;
}

static void
send_and_read_async_ready_cb (SoupSession          *session,
                              GAsyncResult         *result,
                              SendAndReadAsyncData *data)
{
  GBytes *bytes;
  SoupMessage *msg;
  GError *error = NULL;

  bytes = soup_session_send_and_read_finish (session, result, &error);
  if (!bytes) {
    g_warning ("Failed to send request: %s", error->message);
    g_error_free (error);
  }

  msg = soup_session_get_async_result_message (session, result);
  g_object_set_data_full (G_OBJECT (msg),
                          "ephy-request-body", bytes ? bytes : g_bytes_new (NULL, 0),
                          (GDestroyNotify)g_bytes_unref);
  data->callback (session, msg, data->user_data);
  g_free (data);
}

static void
storage_request_async_ready_cb (SoupSession             *session,
                                GAsyncResult            *result,
                                StorageRequestAsyncData *data)
{
  GBytes *bytes;
  SoupMessage *msg;
  GError *error = NULL;

  bytes = soup_session_send_and_read_finish (session, result, &error);
  if (!bytes) {
    g_warning ("Failed to send storage request: %s", error->message);
    g_error_free (error);
  }

  msg = soup_session_get_async_result_message (session, result);
  g_object_set_data_full (G_OBJECT (msg),
                          "ephy-request-body", bytes ? bytes : g_bytes_new (NULL, 0),
                          (GDestroyNotify)g_bytes_unref);
  data->callback (session, msg, data->user_data);
  storage_request_async_data_free (data);
}

static SignInAsyncData *
sign_in_async_data_new (EphySyncService *service,
                        const char      *email,
                        const char      *uid,
                        const char      *session_token,
                        const char      *unwrap_kb,
                        const char      *token_id_hex,
                        const guint8    *req_hmac_key,
                        const guint8    *resp_hmac_key,
                        const guint8    *resp_xor_key)
{
  SignInAsyncData *data;

  data = g_new (SignInAsyncData, 1);
  data->service = g_object_ref (service);
  data->email = g_strdup (email);
  data->uid = g_strdup (uid);
  data->session_token = g_strdup (session_token);
  data->unwrap_kb = g_strdup (unwrap_kb);
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
  g_free (data->unwrap_kb);
  g_free (data->token_id_hex);
  g_free (data->req_hmac_key);
  g_free (data->resp_hmac_key);
  g_free (data->resp_xor_key);
  g_free (data);
}

static SyncCollectionAsyncData *
sync_collection_async_data_new (EphySyncService           *service,
                                EphySynchronizableManager *manager,
                                gboolean                   is_initial,
                                gboolean                   is_last)
{
  SyncCollectionAsyncData *data;

  data = g_new (SyncCollectionAsyncData, 1);
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
  g_list_free_full (data->remotes_deleted, g_object_unref);
  g_list_free_full (data->remotes_updated, g_object_unref);
  g_free (data);
}

static SyncAsyncData *
sync_async_data_new (EphySyncService           *service,
                     EphySynchronizableManager *manager,
                     EphySynchronizable        *synchronizable)
{
  SyncAsyncData *data;

  data = g_new (SyncAsyncData, 1);
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
  g_free (data);
}

static inline BatchUploadAsyncData *
batch_upload_async_data_new (EphySyncService           *service,
                             EphySynchronizableManager *manager,
                             GPtrArray                 *synchronizables,
                             guint                      start,
                             guint                      end,
                             const char                *batch_id,
                             gboolean                   batch_is_last,
                             gboolean                   sync_done)
{
  BatchUploadAsyncData *data;

  data = g_new (BatchUploadAsyncData, 1);
  data->service = g_object_ref (service);
  data->manager = g_object_ref (manager);
  data->synchronizables = g_ptr_array_ref (synchronizables);
  data->start = start;
  data->end = end;
  data->batch_id = g_strdup (batch_id);
  data->batch_is_last = batch_is_last;
  data->sync_done = sync_done;

  return data;
}

static inline BatchUploadAsyncData *
batch_upload_async_data_dup (BatchUploadAsyncData *data)
{
  g_assert (data);

  return batch_upload_async_data_new (data->service, data->manager,
                                      data->synchronizables, data->start,
                                      data->end, data->batch_id,
                                      data->batch_is_last, data->sync_done);
}

static inline void
batch_upload_async_data_free (BatchUploadAsyncData *data)
{
  g_assert (data);

  g_object_unref (data->service);
  g_object_unref (data->manager);
  g_ptr_array_unref (data->synchronizables);
  g_free (data->batch_id);
  g_free (data);
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
  if (!crypto_keys) {
    g_warning ("Missing crypto-keys secret");
    return NULL;
  }

  node = json_from_string (crypto_keys, &error);
  g_assert (!error);
  json = json_node_get_object (node);
  collections = json_object_get_object_member (json, "collections");
  array = json_object_has_member (collections, collection) ?
          json_object_get_array_member (collections, collection) :
          json_object_get_array_member (json, "default");
  bundle = ephy_sync_crypto_key_bundle_new (json_array_get_string_element (array, 0),
                                            json_array_get_string_element (array, 1));

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
ephy_sync_service_fxa_hawk_post (EphySyncService     *self,
                                 const char          *endpoint,
                                 const char          *id,
                                 guint8              *key,
                                 gsize                key_len,
                                 const char          *request_body,
                                 SoupSessionCallback  callback,
                                 gpointer             user_data)
{
  SyncCryptoHawkOptions *options;
  SyncCryptoHawkHeader *header;
  SoupMessage *msg;
  SoupMessageHeaders *request_headers;
  char *url;
  const char *content_type = "application/json; charset=utf-8";
  g_autofree char *accounts_server = NULL;
  g_autoptr (GBytes) bytes = NULL;

  g_assert (EPHY_IS_SYNC_SERVICE (self));
  g_assert (endpoint);
  g_assert (id);
  g_assert (key);
  g_assert (request_body);

  accounts_server = ephy_sync_utils_get_accounts_server ();
  url = g_strdup_printf ("%s/%s", accounts_server, endpoint);
  msg = soup_message_new (SOUP_METHOD_POST, url);
  bytes = g_bytes_new (request_body, strlen (request_body));
  soup_message_set_request_body_from_bytes (msg, content_type, bytes);
  request_headers = soup_message_get_request_headers (msg);

  options = ephy_sync_crypto_hawk_options_new (NULL, NULL, NULL, content_type,
                                               NULL, NULL, NULL, request_body,
                                               NULL);
  header = ephy_sync_crypto_hawk_header_new (url, "POST", id, key, key_len, options);
  soup_message_headers_append (request_headers, "authorization", header->header);
  soup_message_headers_append (request_headers, "content-type", content_type);
  soup_session_send_and_read_async (self->session, msg, G_PRIORITY_DEFAULT, NULL,
                                    (GAsyncReadyCallback)send_and_read_async_ready_cb,
                                    send_and_read_async_data_new (callback, user_data));

  g_free (url);
  ephy_sync_crypto_hawk_options_free (options);
  ephy_sync_crypto_hawk_header_free (header);
}

static void
ephy_sync_service_fxa_hawk_get (EphySyncService     *self,
                                const char          *endpoint,
                                const char          *id,
                                guint8              *key,
                                gsize                key_len,
                                SoupSessionCallback  callback,
                                gpointer             user_data)
{
  SyncCryptoHawkHeader *header;
  SoupMessage *msg;
  SoupMessageHeaders *request_headers;
  char *url;
  g_autofree char *accounts_server = NULL;

  g_assert (EPHY_IS_SYNC_SERVICE (self));
  g_assert (endpoint);
  g_assert (id);
  g_assert (key);

  accounts_server = ephy_sync_utils_get_accounts_server ();
  url = g_strdup_printf ("%s/%s", accounts_server, endpoint);
  msg = soup_message_new (SOUP_METHOD_GET, url);
  header = ephy_sync_crypto_hawk_header_new (url, "GET", id, key, key_len, NULL);
  request_headers = soup_message_get_request_headers (msg);
  soup_message_headers_append (request_headers, "authorization", header->header);
  soup_session_send_and_read_async (self->session, msg, G_PRIORITY_DEFAULT, NULL,
                                    (GAsyncReadyCallback)send_and_read_async_ready_cb,
                                    send_and_read_async_data_new (callback, user_data));

  g_free (url);
  ephy_sync_crypto_hawk_header_free (header);
}

static void
ephy_sync_service_send_storage_request (EphySyncService         *self,
                                        StorageRequestAsyncData *data)
{
  SyncCryptoHawkOptions *options = NULL;
  SyncCryptoHawkHeader *header;
  SoupMessage *msg;
  SoupMessageHeaders *request_headers;
  char *url;
  char *if_modified_since = NULL;
  char *if_unmodified_since = NULL;
  const char *content_type = "application/json; charset=utf-8";

  g_assert (EPHY_IS_SYNC_SERVICE (self));
  g_assert (data);

  url = g_strdup_printf ("%s/%s", self->storage_endpoint, data->endpoint);
  msg = soup_message_new (data->method, url);

  if (data->request_body) {
    g_autoptr (GBytes) bytes = NULL;

    options = ephy_sync_crypto_hawk_options_new (NULL, NULL, NULL, content_type,
                                                 NULL, NULL, NULL, data->request_body,
                                                 NULL);
    bytes = g_bytes_new (data->request_body, strlen (data->request_body));
    soup_message_set_request_body_from_bytes (msg, content_type, bytes);
  }

  request_headers = soup_message_get_request_headers (msg);

  if (!g_strcmp0 (data->method, SOUP_METHOD_PUT) || !g_strcmp0 (data->method, SOUP_METHOD_POST))
    soup_message_headers_append (request_headers, "content-type", content_type);

  if (data->modified_since >= 0) {
    if_modified_since = g_strdup_printf ("%" PRId64, data->modified_since);
    soup_message_headers_append (request_headers, "X-If-Modified-Since", if_modified_since);
  }

  if (data->unmodified_since >= 0) {
    if_unmodified_since = g_strdup_printf ("%" PRId64, data->unmodified_since);
    soup_message_headers_append (request_headers, "X-If-Unmodified-Since", if_unmodified_since);
  }

  header = ephy_sync_crypto_hawk_header_new (url, data->method,
                                             self->storage_credentials_id,
                                             (guint8 *)self->storage_credentials_key,
                                             strlen (self->storage_credentials_key),
                                             options);
  soup_message_headers_append (request_headers, "authorization", header->header);
  soup_session_send_and_read_async (self->session, msg, G_PRIORITY_DEFAULT, NULL,
                                    (GAsyncReadyCallback)storage_request_async_ready_cb,
                                    data);

  g_free (url);
  g_free (if_modified_since);
  g_free (if_unmodified_since);
  ephy_sync_crypto_hawk_header_free (header);
  if (options)
    ephy_sync_crypto_hawk_options_free (options);
}

static void
ephy_sync_service_send_all_storage_requests (EphySyncService *self)
{
  StorageRequestAsyncData *data;

  g_assert (EPHY_IS_SYNC_SERVICE (self));

  while (!g_queue_is_empty (self->storage_queue)) {
    data = g_queue_pop_head (self->storage_queue);
    ephy_sync_service_send_storage_request (self, data);
  }
}

static void
ephy_sync_service_clear_storage_queue (EphySyncService *self)
{
  g_assert (EPHY_IS_SYNC_SERVICE (self));

  while (!g_queue_is_empty (self->storage_queue))
    storage_request_async_data_free (g_queue_pop_head (self->storage_queue));
}

static gboolean
ephy_sync_service_verify_certificate (EphySyncService *self,
                                      const char      *certificate)
{
  JsonParser *parser;
  JsonObject *json;
  JsonObject *principal;
  GError *error = NULL;
  g_autoptr (GUri) uri = NULL;
  char **pieces;
  char *header;
  char *payload;
  char *expected = NULL;
  const char *alg;
  const char *email;
  gsize len;
  gboolean retval = FALSE;
  g_autofree char *accounts_server = NULL;

  g_assert (EPHY_IS_SYNC_SERVICE (self));
  g_assert (ephy_sync_service_get_secret (self, secrets[UID]));
  g_assert (certificate);

  pieces = g_strsplit (certificate, ".", 0);
  header = (char *)ephy_sync_utils_base64_urlsafe_decode (pieces[0], &len, TRUE);
  payload = (char *)ephy_sync_utils_base64_urlsafe_decode (pieces[1], &len, TRUE);
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
  accounts_server = ephy_sync_utils_get_accounts_server ();
  uri = g_uri_parse (accounts_server, G_URI_FLAGS_PARSE_RELAXED, NULL);
  expected = g_strdup_printf ("%s@%s",
                              ephy_sync_service_get_secret (self, secrets[UID]),
                              g_uri_get_host (uri));
  retval = g_strcmp0 (email, expected) == 0;

out:
  g_free (expected);
  g_object_unref (parser);
  g_free (payload);
  g_free (header);
  g_strfreev (pieces);
  if (error)
    g_error_free (error);

  return retval;
}

static void
forget_secrets_cb (GObject      *source_object,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  GError *error = NULL;

  secret_password_clear_finish (result, &error);
  if (error) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning ("Failed to clear sync secrets: %s", error->message);
    g_error_free (error);
  } else {
    LOG ("Successfully cleared sync secrets");
  }
}

static void
ephy_sync_service_forget_secrets (EphySyncService *self)
{
  GHashTable *attributes;
  char *user;

  g_assert (EPHY_IS_SYNC_SERVICE (self));
  g_assert (self->secrets);

  user = ephy_sync_utils_get_sync_user ();
  g_assert (user);

  attributes = secret_attributes_build (EPHY_SYNC_SECRET_SCHEMA,
                                        EPHY_SYNC_SECRET_ACCOUNT_KEY, user,
                                        NULL);
  secret_password_clearv (EPHY_SYNC_SECRET_SCHEMA, attributes, self->cancellable,
                          (GAsyncReadyCallback)forget_secrets_cb, NULL);
  g_hash_table_remove_all (self->secrets);

  g_hash_table_unref (attributes);
  g_free (user);
}

static void
destroy_session_cb (SoupSession *session,
                    SoupMessage *msg,
                    gpointer     user_data)
{
  guint status_code;
  g_autoptr (GBytes) response_body = NULL;

  status_code = soup_message_get_status (msg);
  response_body = g_bytes_ref (g_object_get_data (G_OBJECT (msg), "ephy-request-body"));

  if (status_code != 200) {
    g_warning ("Failed to destroy session. Status code: %u, response: %s",
               status_code, (const char *)g_bytes_get_data (response_body, NULL));
  } else {
    LOG ("Successfully destroyed session");
  }
}

static void
destroy_session_send_and_read_ready_cb (SoupSession  *session,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  GBytes *bytes;
  SoupMessage *msg;
  g_autoptr (GError) error = NULL;

  bytes = soup_session_send_and_read_finish (session, result, &error);
  if (!bytes)
    g_warning ("Failed to send request: %s", error->message);

  msg = soup_session_get_async_result_message (session, result);
  g_object_set_data_full (G_OBJECT (msg),
                          "ephy-request-body", bytes ? bytes : g_bytes_new (NULL, 0),
                          (GDestroyNotify)g_bytes_unref);
  destroy_session_cb (session, msg, user_data);
}

static void
ephy_sync_service_destroy_session (EphySyncService *self,
                                   const char      *session_token)
{
  SyncCryptoHawkOptions *options;
  SyncCryptoHawkHeader *header;
  SoupMessage *msg;
  SoupMessageHeaders *request_headers;
  guint8 *token_id;
  guint8 *req_hmac_key;
  guint8 *tmp;
  char *token_id_hex;
  char *url;
  const char *content_type = "application/json; charset=utf-8";
  const char *request_body = "{}";
  g_autofree char *accounts_server = NULL;
  g_autoptr (GBytes) bytes = NULL;

  g_assert (EPHY_IS_SYNC_SERVICE (self));
  if (!session_token)
    session_token = ephy_sync_service_get_secret (self, secrets[SESSION_TOKEN]);
  g_assert (session_token);

  /* This also destroys the device associated with the session token. */
  accounts_server = ephy_sync_utils_get_accounts_server ();
  url = g_strdup_printf ("%s/session/destroy", accounts_server);
  ephy_sync_crypto_derive_session_token (session_token, &token_id,
                                         &req_hmac_key, &tmp);
  token_id_hex = ephy_sync_utils_encode_hex (token_id, 32);

  msg = soup_message_new (SOUP_METHOD_POST, url);
  bytes = g_bytes_new_static (request_body, strlen (request_body));
  soup_message_set_request_body_from_bytes (msg, content_type, bytes);
  request_headers = soup_message_get_request_headers (msg);
  options = ephy_sync_crypto_hawk_options_new (NULL, NULL, NULL, content_type,
                                               NULL, NULL, NULL, request_body,
                                               NULL);
  header = ephy_sync_crypto_hawk_header_new (url, "POST", token_id_hex,
                                             req_hmac_key, 32, options);
  soup_message_headers_append (request_headers, "authorization", header->header);
  soup_message_headers_append (request_headers, "content-type", content_type);
  soup_session_send_and_read_async (self->session, msg, G_PRIORITY_DEFAULT, NULL,
                                    (GAsyncReadyCallback)destroy_session_send_and_read_ready_cb,
                                    NULL);

  g_free (token_id_hex);
  g_free (token_id);
  g_free (req_hmac_key);
  g_free (tmp);
  g_free (url);
  ephy_sync_crypto_hawk_options_free (options);
  ephy_sync_crypto_hawk_header_free (header);
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
    g_clear_pointer (&self->user, g_free);
    g_hash_table_remove_all (self->secrets);
  }

  self->is_signing_in = FALSE;
}

static JsonNode *
json_from_response_body (GBytes  *bytes,
                         GError **error)
{
  gconstpointer data = g_bytes_get_data (bytes, NULL);
  if (data)
    return json_from_string (data, error);
  g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, _("Response body is empty, do you need to install glib-networking?"));
  return NULL;
}

static void
get_storage_credentials_cb (SoupSession *session,
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
  guint status_code;
  g_autoptr (GBytes) response_body = NULL;

  status_code = soup_message_get_status (msg);
  response_body = g_bytes_ref (g_object_get_data (G_OBJECT (msg), "ephy-request-body"));

  if (status_code != 200) {
    g_warning ("Failed to obtain storage credentials. Status code: %u, response: %s",
               status_code, (const char *)g_bytes_get_data (response_body, NULL));
    goto out_error;
  }
  node = json_from_response_body (response_body, &error);
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

  ephy_sync_service_send_all_storage_requests (self);
  goto out;

out_error:
  message = _("Failed to obtain storage credentials.");
  suggestion = _("Please visit Firefox Sync and sign in again to continue syncing.");

  if (self->is_signing_in)
    ephy_sync_service_report_sign_in_error (self, message, NULL, TRUE);
  else
    ephy_notification_show (ephy_notification_new (message, suggestion));

  ephy_sync_service_clear_storage_queue (self);

out:
  self->locked = FALSE;

  if (node)
    json_node_unref (node);
  if (error)
    g_error_free (error);
}

static void
get_storage_credentials_ready_cb (SoupSession  *session,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  GBytes *bytes;
  SoupMessage *msg;
  g_autoptr (GError) error = NULL;

  bytes = soup_session_send_and_read_finish (session, result, &error);
  if (!bytes)
    g_warning ("Failed to send store credentials request: %s", error->message);

  msg = soup_session_get_async_result_message (session, result);
  g_object_set_data_full (G_OBJECT (msg),
                          "ephy-request-body", bytes ? bytes : g_bytes_new (NULL, 0),
                          (GDestroyNotify)g_bytes_unref);
  get_storage_credentials_cb (session, msg, user_data);
}

static void
ephy_sync_service_trade_browserid_assertion (EphySyncService *self)
{
  SoupMessage *msg;
  SoupMessageHeaders *request_headers;
  guint8 *kb;
  char *hashed_kb;
  char *client_state;
  char *audience;
  char *assertion;
  char *authorization;
  g_autofree char *token_server = NULL;

  g_assert (EPHY_IS_SYNC_SERVICE (self));
  g_assert (self->certificate);
  g_assert (self->key_pair);

  token_server = ephy_sync_utils_get_token_server ();
  audience = ephy_sync_utils_get_audience (token_server);
  assertion = ephy_sync_crypto_create_assertion (self->certificate, audience,
                                                 300, self->key_pair);
  kb = ephy_sync_utils_decode_hex (ephy_sync_service_get_secret (self, secrets[MASTER_KEY]));
  hashed_kb = g_compute_checksum_for_data (G_CHECKSUM_SHA256, kb, 32);
  client_state = g_strndup (hashed_kb, 32);
  authorization = g_strdup_printf ("BrowserID %s", assertion);

  msg = soup_message_new (SOUP_METHOD_GET, token_server);
  request_headers = soup_message_get_request_headers (msg);
  /* We need to add the X-Client-State header so that the Token Server will
   * recognize accounts that were previously used to sync Firefox data too.
   */
  soup_message_headers_append (request_headers, "X-Client-State", client_state);
  soup_message_headers_append (request_headers, "authorization", authorization);
  soup_session_send_and_read_async (self->session, msg, G_PRIORITY_DEFAULT, NULL,
                                    (GAsyncReadyCallback)get_storage_credentials_ready_cb,
                                    self);

  g_free (kb);
  g_free (hashed_kb);
  g_free (client_state);
  g_free (audience);
  g_free (assertion);
  g_free (authorization);
}

static void
get_signed_certificate_cb (SoupSession *session,
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
  guint status_code;
  g_autoptr (GBytes) response_body = NULL;

  status_code = soup_message_get_status (msg);
  response_body = g_bytes_ref (g_object_get_data (G_OBJECT (msg), "ephy-request-body"));

  node = json_from_response_body (response_body, &error);
  if (error) {
    g_warning ("Response is not a valid JSON: %s", error->message);
    goto out_error;
  }
  json = json_node_get_object (node);
  if (!json) {
    g_warning ("JSON node does not hold a JSON object");
    goto out_error;
  }

  if (status_code == 200) {
    certificate = json_object_get_string_member (json, "cert");
    if (!certificate) {
      g_warning ("JSON object has missing or invalid 'cert' member");
      goto out_error;
    }

    if (!ephy_sync_service_verify_certificate (self, certificate)) {
      g_warning ("Invalid certificate");
      ephy_sync_crypto_rsa_key_pair_free (self->key_pair);
      goto out_error;
    }

    self->certificate = g_strdup (certificate);
    ephy_sync_service_trade_browserid_assertion (self);
    goto out_no_error;
  }

  /* Since a new Mozilla Account password implies new tokens, this will fail
   * with an error code 110 (Invalid authentication token in request signature)
   * if the user has changed his password since the last time he signed in.
   * When this happens, notify the user to sign in with the new password.
   */
  if (json_object_get_int_member (json, "errno") == 110) {
    message = _("The password of your Mozilla account seems to have been changed.");
    suggestion = _("Please visit Firefox Sync and sign in with the new password to continue syncing.");
    ephy_sync_service_sign_out (self);
  }

  g_warning ("Failed to sign certificate. Status code: %u, response: %s",
             status_code, (const char *)g_bytes_get_data (response_body, NULL));

out_error:
  message = message ? message : _("Failed to obtain signed certificate.");
  suggestion = suggestion ? suggestion : _("Please visit Firefox Sync and sign in again to continue syncing.");

  if (self->is_signing_in)
    ephy_sync_service_report_sign_in_error (self, message, NULL, TRUE);
  else
    ephy_notification_show (ephy_notification_new (message, suggestion));

  ephy_sync_service_clear_storage_queue (self);
  self->locked = FALSE;

out_no_error:
  if (node)
    json_node_unref (node);
  if (error)
    g_error_free (error);
}

static void
ephy_sync_service_get_storage_credentials (EphySyncService *self)
{
  JsonNode *node;
  JsonObject *object_key;
  JsonObject *object_body;
  guint8 *token_id;
  guint8 *req_hmac_key;
  guint8 *tmp;
  const char *session_token;
  char *token_id_hex;
  char *request_body;
  char *n;
  char *e;

  g_assert (EPHY_IS_SYNC_SERVICE (self));

  /* To get the storage credentials from the Token Server, we need to create a
   * BrowserID assertion. For that we need to obtain an identity certificate
   * signed by the Mozilla Accounts Server.
   */

  /* Generate a new RSA key pair to sign the new certificate. */
  if (self->key_pair)
    ephy_sync_crypto_rsa_key_pair_free (self->key_pair);
  self->key_pair = ephy_sync_crypto_rsa_key_pair_new ();

  /* Derive tokenID and reqHMACkey from sessionToken. */
  session_token = ephy_sync_service_get_secret (self, secrets[SESSION_TOKEN]);
  if (!session_token)
    return;

  ephy_sync_crypto_derive_session_token (session_token, &token_id,
                                         &req_hmac_key, &tmp);
  token_id_hex = ephy_sync_utils_encode_hex (token_id, 32);

  n = mpz_get_str (NULL, 10, self->key_pair->public.n);
  e = mpz_get_str (NULL, 10, self->key_pair->public.e);
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
  ephy_sync_service_fxa_hawk_post (self, "certificate/sign", token_id_hex,
                                   req_hmac_key, 32, request_body,
                                   get_signed_certificate_cb, self);

  g_free (request_body);
  json_object_unref (object_body);
  json_node_unref (node);
  g_free (e);
  g_free (n);
  g_free (token_id_hex);
  g_free (tmp);
  g_free (req_hmac_key);
  g_free (token_id);
}

static void
ephy_sync_service_queue_storage_request (EphySyncService     *self,
                                         const char          *endpoint,
                                         const char          *method,
                                         const char          *request_body,
                                         gint64               modified_since,
                                         gint64               unmodified_since,
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
   * the new credentials are obtained.
   */
  if (!ephy_sync_service_storage_credentials_is_expired (self)) {
    ephy_sync_service_send_storage_request (self, data);
  } else {
    g_queue_push_tail (self->storage_queue, data);
    if (!self->locked) {
      /* Mark as locked so other requests won't lead to conflicts while
       * obtaining new storage credentials.
       */
      self->locked = TRUE;
      ephy_sync_service_clear_storage_credentials (self);
      ephy_sync_service_get_storage_credentials (self);
    }
  }
}

static void
delete_synchronizable_cb (SoupSession *session,
                          SoupMessage *msg,
                          gpointer     user_data)
{
  guint status_code;
  g_autoptr (GBytes) response_body = NULL;

  status_code = soup_message_get_status (msg);
  response_body = g_bytes_ref (g_object_get_data (G_OBJECT (msg), "ephy-request-body"));

  if (status_code == 200) {
    LOG ("Successfully deleted from server");
  } else {
    g_warning ("Failed to delete object. Status code: %u, response: %s",
               status_code, (const char *)g_bytes_get_data (response_body, NULL));
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
  g_assert (ephy_sync_utils_user_is_signed_in ());

  collection = ephy_synchronizable_manager_get_collection_name (manager);
  bundle = ephy_sync_service_get_key_bundle (self, collection);
  if (!bundle)
    return;

  id = ephy_synchronizable_get_id (synchronizable);
  /* Firefox uses UUIDs with curly braces as IDs for saved passwords records.
   * Curly braces are unsafe characters in URLs so they must be encoded.
   */
  id_safe = g_uri_escape_string (id, NULL, TRUE);
  endpoint = g_strdup_printf ("storage/%s/%s", collection, id_safe);

  node = json_node_new (JSON_NODE_OBJECT);
  object = json_object_new ();
  json_node_set_object (node, object);
  json_object_set_string_member (object, "id", id);
  json_object_set_boolean_member (object, "deleted", TRUE);
  record = json_to_string (node, FALSE);
  payload = ephy_sync_crypto_encrypt_record (record, bundle);
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
  guint status_code;
  g_autoptr (GBytes) response_body = NULL;

  status_code = soup_message_get_status (msg);
  response_body = g_bytes_ref (g_object_get_data (G_OBJECT (msg), "ephy-request-body"));

  if (status_code != 200) {
    g_warning ("Failed to download object. Status code: %u, response: %s",
               status_code, (const char *)g_bytes_get_data (response_body, NULL));
    goto out;
  }
  node = json_from_response_body (response_body, &error);
  if (error) {
    g_warning ("Response is not a valid JSON");
    goto out;
  }
  type = ephy_synchronizable_manager_get_synchronizable_type (data->manager);
  collection = ephy_synchronizable_manager_get_collection_name (data->manager);
  bundle = ephy_sync_service_get_key_bundle (data->service, collection);
  if (!bundle)
    goto out;

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
  g_assert (ephy_sync_utils_user_is_signed_in ());

  id = ephy_synchronizable_get_id (synchronizable);
  collection = ephy_synchronizable_manager_get_collection_name (manager);
  /* Firefox uses UUIDs with curly braces as IDs for saved passwords records.
   * Curly braces are unsafe characters in URLs so they must be encoded.
   */
  id_safe = g_uri_escape_string (id, NULL, TRUE);
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
  gint64 time_modified;
  guint status_code;
  g_autoptr (GBytes) response_body = NULL;

  status_code = soup_message_get_status (msg);
  response_body = g_bytes_ref (g_object_get_data (G_OBJECT (msg), "ephy-request-body"));

  /* Code 412 means that there is a more recent version on the server.
   * Download it.
   */
  if (status_code == 412) {
    LOG ("Found a newer version of the object on the server, downloading it...");
    ephy_sync_service_download_synchronizable (data->service, data->manager, data->synchronizable);
  } else if (status_code == 200) {
    LOG ("Successfully uploaded to server");
    time_modified = ceil (g_ascii_strtod (g_bytes_get_data (response_body, NULL), NULL));
    ephy_synchronizable_set_server_time_modified (data->synchronizable, time_modified);
    ephy_synchronizable_manager_save (data->manager, data->synchronizable);
  } else {
    g_warning ("Failed to upload object. Status code: %u, response: %s",
               status_code, (const char *)g_bytes_get_data (response_body, NULL));
  }

  sync_async_data_free (data);
}

static void
ephy_sync_service_upload_synchronizable (EphySyncService           *self,
                                         EphySynchronizableManager *manager,
                                         EphySynchronizable        *synchronizable,
                                         gboolean                   should_force)
{
  SyncCryptoKeyBundle *bundle;
  SyncAsyncData *data;
  JsonNode *bso;
  char *endpoint;
  char *body;
  char *id_safe;
  const char *collection;
  const char *id;
  gint64 time_modified;

  g_assert (EPHY_IS_SYNC_SERVICE (self));
  g_assert (EPHY_IS_SYNCHRONIZABLE_MANAGER (manager));
  g_assert (EPHY_IS_SYNCHRONIZABLE (synchronizable));
  g_assert (ephy_sync_utils_user_is_signed_in ());

  collection = ephy_synchronizable_manager_get_collection_name (manager);
  bundle = ephy_sync_service_get_key_bundle (self, collection);
  if (!bundle)
    return;

  bso = ephy_synchronizable_to_bso (synchronizable, bundle);
  id = ephy_synchronizable_get_id (synchronizable);
  /* Firefox uses UUIDs with curly braces as IDs for saved passwords records.
   * Curly braces are unsafe characters in URLs so they must be encoded.
   */
  id_safe = g_uri_escape_string (id, NULL, TRUE);
  endpoint = g_strdup_printf ("storage/%s/%s", collection, id_safe);
  data = sync_async_data_new (self, manager, synchronizable);
  body = json_to_string (bso, FALSE);

  LOG ("Uploading object with id %s...", id);
  time_modified = ephy_synchronizable_get_server_time_modified (synchronizable);
  ephy_sync_service_queue_storage_request (self, endpoint, SOUP_METHOD_PUT, body,
                                           -1, should_force ? -1 : time_modified,
                                           upload_synchronizable_cb, data);

  g_free (id_safe);
  g_free (body);
  g_free (endpoint);
  json_node_unref (bso);
  ephy_sync_crypto_key_bundle_free (bundle);
}

static GPtrArray *
ephy_sync_service_split_into_batches (EphySyncService           *self,
                                      EphySynchronizableManager *manager,
                                      GPtrArray                 *synchronizables,
                                      guint                      start,
                                      guint                      end)
{
  SyncCryptoKeyBundle *bundle;
  GPtrArray *batches;
  const char *collection;

  g_assert (EPHY_IS_SYNC_SERVICE (self));
  g_assert (EPHY_IS_SYNCHRONIZABLE_MANAGER (manager));
  g_assert (synchronizables);

  collection = ephy_synchronizable_manager_get_collection_name (manager);
  bundle = ephy_sync_service_get_key_bundle (self, collection);
  if (!bundle)
    return NULL;

  batches = g_ptr_array_new_with_free_func (g_free);

  for (guint i = start; i < end; i += EPHY_SYNC_BATCH_SIZE) {
    JsonNode *node = json_node_new (JSON_NODE_ARRAY);
    JsonArray *array = json_array_new ();

    for (guint k = i; k < MIN (i + EPHY_SYNC_BATCH_SIZE, end); k++) {
      EphySynchronizable *synchronizable = g_ptr_array_index (synchronizables, k);
      JsonNode *bso = ephy_synchronizable_to_bso (synchronizable, bundle);
      JsonObject *object = json_object_ref (json_node_get_object (bso));

      json_array_add_object_element (array, object);
      json_node_unref (bso);
    }

    json_node_take_array (node, array);
    g_ptr_array_add (batches, json_to_string (node, FALSE));
    json_node_unref (node);
  }

  ephy_sync_crypto_key_bundle_free (bundle);

  return batches;
}

static void
commit_batch_cb (SoupSession *session,
                 SoupMessage *msg,
                 gpointer     user_data)
{
  BatchUploadAsyncData *data = user_data;
  const char *last_modified;
  guint status_code;
  SoupMessageHeaders *response_headers;
  g_autoptr (GBytes) response_body = NULL;

  status_code = soup_message_get_status (msg);
  response_headers = soup_message_get_response_headers (msg);
  response_body = g_bytes_ref (g_object_get_data (G_OBJECT (msg), "ephy-request-body"));

  if (status_code != 200) {
    g_warning ("Failed to commit batch. Status code: %u, response: %s",
               status_code, (const char *)g_bytes_get_data (response_body, NULL));
  } else {
    LOG ("Successfully committed batches");
    /* Update sync time. */
    last_modified = soup_message_headers_get_one (response_headers, "X-Last-Modified");
    ephy_synchronizable_manager_set_sync_time (data->manager, g_ascii_strtod (last_modified, NULL));
  }

  if (data->sync_done)
    g_signal_emit (data->service, signals[SYNC_FINISHED], 0);
  batch_upload_async_data_free (data);
}

static void
upload_batch_cb (SoupSession *session,
                 SoupMessage *msg,
                 gpointer     user_data)
{
  BatchUploadAsyncData *data = user_data;
  const char *collection;
  char *endpoint = NULL;
  guint status_code;
  g_autoptr (GBytes) response_body = NULL;

  status_code = soup_message_get_status (msg);
  response_body = g_bytes_ref (g_object_get_data (G_OBJECT (msg), "ephy-request-body"));

  /* Note: "202 Accepted" status code. */
  if (status_code != 202) {
    g_warning ("Failed to upload batch. Status code: %u, response: %s",
               status_code, (const char *)g_bytes_get_data (response_body, NULL));
  } else {
    LOG ("Successfully uploaded batch");
  }

  if (!data->batch_is_last)
    goto out;

  collection = ephy_synchronizable_manager_get_collection_name (data->manager);
  endpoint = g_strdup_printf ("storage/%s?commit=true&batch=%s", collection, data->batch_id);
  ephy_sync_service_queue_storage_request (data->service, endpoint,
                                           SOUP_METHOD_POST, "[]", -1, -1,
                                           commit_batch_cb,
                                           batch_upload_async_data_dup (data));

out:
  g_free (endpoint);
  /* Remove last reference to the array with the items to upload. */
  if (data->batch_is_last)
    g_ptr_array_unref (data->synchronizables);
  batch_upload_async_data_free (data);
}

static void
start_batch_upload_cb (SoupSession *session,
                       SoupMessage *msg,
                       gpointer     user_data)
{
  BatchUploadAsyncData *data = user_data;
  GPtrArray *batches = NULL;
  JsonNode *node = NULL;
  JsonObject *object;
  g_autoptr (GError) error = NULL;
  const char *collection;
  char *endpoint = NULL;
  guint status_code;
  g_autoptr (GBytes) response_body = NULL;

  status_code = soup_message_get_status (msg);
  response_body = g_bytes_ref (g_object_get_data (G_OBJECT (msg), "ephy-request-body"));

  /* Note: "202 Accepted" status code. */
  if (status_code != 202) {
    g_warning ("Failed to start batch upload. Status code: %u, response: %s",
               status_code, (const char *)g_bytes_get_data (response_body, NULL));
    goto out;
  }

  node = json_from_response_body (response_body, &error);
  if (error) {
    g_warning ("Response is not a valid JSON: %s", error->message);
    goto out;
  }

  object = json_node_get_object (node);
  data->batch_id = g_uri_escape_string (json_object_get_string_member (object, "batch"),
                                        NULL, TRUE);
  collection = ephy_synchronizable_manager_get_collection_name (data->manager);
  endpoint = g_strdup_printf ("storage/%s?batch=%s", collection, data->batch_id);

  batches = ephy_sync_service_split_into_batches (data->service, data->manager,
                                                  data->synchronizables,
                                                  data->start, data->end);
  for (guint i = 0; i < batches->len; i++) {
    BatchUploadAsyncData *data_dup = batch_upload_async_data_dup (data);

    if (i == batches->len - 1)
      data_dup->batch_is_last = TRUE;

    ephy_sync_service_queue_storage_request (data->service, endpoint, SOUP_METHOD_POST,
                                             g_ptr_array_index (batches, i), -1, -1,
                                             upload_batch_cb, data_dup);
  }

out:
  g_free (endpoint);
  if (node)
    json_node_unref (node);
  if (batches)
    g_ptr_array_unref (batches);
  batch_upload_async_data_free (data);
}

static void
merge_collection_finished_cb (GPtrArray *to_upload,
                              gpointer   user_data)
{
  SyncCollectionAsyncData *data = user_data;
  BatchUploadAsyncData *bdata;
  guint step = EPHY_SYNC_MAX_BATCHES * EPHY_SYNC_BATCH_SIZE;
  const char *collection;
  char *endpoint = NULL;

  if (!to_upload || to_upload->len == 0) {
    if (data->is_last)
      g_signal_emit (data->service, signals[SYNC_FINISHED], 0);
    goto out;
  }

  collection = ephy_synchronizable_manager_get_collection_name (data->manager);
  endpoint = g_strdup_printf ("storage/%s?batch=true", collection);

  for (guint i = 0; i < to_upload->len; i += step) {
    bdata = batch_upload_async_data_new (data->service, data->manager,
                                         to_upload, i,
                                         MIN (i + step, to_upload->len),
                                         NULL, FALSE,
                                         data->is_last && i + step >= to_upload->len);
    ephy_sync_service_queue_storage_request (data->service, endpoint,
                                             SOUP_METHOD_POST, "[]", -1, -1,
                                             start_batch_upload_cb, bdata);
  }

out:
  g_free (endpoint);
  sync_collection_async_data_free (data);
}

static void
sync_collection_cb (SoupSession *session,
                    SoupMessage *msg,
                    gpointer     user_data)
{
  SyncCollectionAsyncData *data = (SyncCollectionAsyncData *)user_data;
  EphySynchronizable *remote;
  SyncCryptoKeyBundle *bundle = NULL;
  JsonNode *node = NULL;
  JsonArray *array = NULL;
  g_autoptr (GError) error = NULL;
  GType type;
  const char *collection;
  gboolean is_deleted;
  guint status_code;
  g_autoptr (GBytes) response_body = NULL;

  collection = ephy_synchronizable_manager_get_collection_name (data->manager);

  status_code = soup_message_get_status (msg);
  response_body = g_bytes_ref (g_object_get_data (G_OBJECT (msg), "ephy-request-body"));

  if (status_code != 200) {
    g_warning ("Failed to get records in collection %s. Status code: %u, response: %s",
               collection, status_code, (const char *)g_bytes_get_data (response_body, NULL));
    goto out_error;
  }
  node = json_from_response_body (response_body, &error);
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
  if (!bundle)
    goto out_error;

  for (guint i = 0; i < json_array_get_length (array); i++) {
    remote = EPHY_SYNCHRONIZABLE (ephy_synchronizable_from_bso (json_array_get_element (array, i),
                                                                type, bundle, &is_deleted));
    if (!remote) {
      g_warning ("Failed to create synchronizable object from BSO, skipping...");
      continue;
    }
    if (is_deleted)
      data->remotes_deleted = g_list_prepend (data->remotes_deleted, remote);
    else
      data->remotes_updated = g_list_prepend (data->remotes_updated, remote);
  }

  LOG ("Found %u deleted objects and %u new/updated objects in %s collection",
       g_list_length (data->remotes_deleted),
       g_list_length (data->remotes_updated),
       collection);

  ephy_synchronizable_manager_set_is_initial_sync (data->manager, FALSE);
  ephy_synchronizable_manager_merge (data->manager, data->is_initial,
                                     data->remotes_deleted, data->remotes_updated,
                                     merge_collection_finished_cb, data);
  goto out_no_error;

out_error:
  if (data->is_last)
    g_signal_emit (data->service, signals[SYNC_FINISHED], 0);
  sync_collection_async_data_free (data);
out_no_error:
  if (bundle)
    ephy_sync_crypto_key_bundle_free (bundle);
  if (node)
    json_node_unref (node);
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
  g_assert (ephy_sync_utils_user_is_signed_in ());

  collection = ephy_synchronizable_manager_get_collection_name (manager);
  is_initial = ephy_synchronizable_manager_is_initial_sync (manager);

  if (is_initial) {
    endpoint = g_strdup_printf ("storage/%s?full=true", collection);
  } else {
    endpoint = g_strdup_printf ("storage/%s?newer=%" PRId64 "&full=true", collection,
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
ephy_sync_service_sync_internal (EphySyncService *self)
{
  GNetworkMonitor *monitor;
  guint index = 0;
  guint num_managers;

  g_assert (ephy_sync_utils_user_is_signed_in ());

  monitor = g_network_monitor_get_default ();
  if (g_network_monitor_get_connectivity (monitor) != G_NETWORK_CONNECTIVITY_FULL) {
    LOG ("Not syncing because connectivity is not full");
    g_signal_emit (self, signals[SYNC_FINISHED], 0);
    return G_SOURCE_CONTINUE;
  }

  if (!self->managers) {
    LOG ("Not syncing because no sync managers are registered");
    g_signal_emit (self, signals[SYNC_FINISHED], 0);
    return G_SOURCE_CONTINUE;
  }

  num_managers = g_slist_length (self->managers);
  for (GSList *l = self->managers; l && l->data; l = l->next)
    ephy_sync_service_sync_collection (self, l->data, ++index == num_managers);

  ephy_sync_utils_set_sync_time (g_get_real_time () / 1000000);

  return G_SOURCE_CONTINUE;
}

static void
ephy_sync_service_schedule_periodical_sync (EphySyncService *self)
{
  guint seconds;

  g_assert (EPHY_IS_SYNC_SERVICE (self));

  seconds = ephy_sync_utils_get_sync_frequency () * 60;
  self->source_id = g_timeout_add_seconds (seconds,
                                           (GSourceFunc)ephy_sync_service_sync_internal,
                                           self);
  g_source_set_name_by_id (self->source_id, "[epiphany] sync_service_sync");

  LOG ("Scheduled new sync with frequency %u minutes", seconds / 60);
}

static void
ephy_sync_service_stop_periodical_sync (EphySyncService *self)
{
  g_assert (EPHY_IS_SYNC_SERVICE (self));

  g_clear_handle_id (&self->source_id, g_source_remove);
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
sync_error (const char *message)
{
  ephy_notification_show (ephy_notification_new (message, _("Please visit Firefox Sync and sign in again to continue syncing.")));

  /* Reset the sync user so that it will be considered signed out
   * when the preferences dialog is opened. */
  ephy_sync_utils_set_sync_user (NULL);
  ephy_sync_utils_set_sync_time (0);
  ephy_sync_utils_set_bookmarks_sync_is_initial (TRUE);
  ephy_sync_utils_set_passwords_sync_is_initial (TRUE);
  ephy_sync_utils_set_history_sync_is_initial (TRUE);
}

static void
retrieve_secret_cb (GObject         *source_object,
                    GAsyncResult    *result,
                    EphySyncService *self)
{
  g_autoptr (SecretRetrievable) retrievable = SECRET_RETRIEVABLE (source_object); /* owned because we took a ref in load_secrets_cb() */
  g_autoptr (SecretValue) value = NULL;
  g_autoptr (JsonNode) node = NULL;
  g_autoptr (GError) error = NULL;
  g_autofree char *message = NULL;
  JsonObject *object;

  value = secret_retrievable_retrieve_secret_finish (retrievable, result, &error);
  if (!value) {
    message = g_strdup_printf (_("The sync secrets for the current sync user are invalid: %s"), error->message);
    sync_error (message);
    return;
  }

  node = json_from_string (secret_value_get_text (value), &error);
  if (error) {
    message = g_strdup_printf (_("The sync secrets for the current sync user are not valid JSON: %s"), error->message);
    sync_error (message);
    return;
  }

  /* Set secrets and start periodical sync. */
  object = json_node_get_object (node);
  for (GList *l = json_object_get_members (object); l && l->data; l = l->next)
    ephy_sync_service_set_secret (self, l->data,
                                  json_object_get_string_member (object, l->data));

  g_signal_emit (self, signals[LOAD_FINISHED], 0);
}

static void
load_secrets_cb (GObject         *source_object,
                 GAsyncResult    *result,
                 EphySyncService *self)
{
  g_autolist (SecretRetrievable) retrievables = NULL;
  g_autoptr (GError) error = NULL;
  g_autofree char *message = NULL;

  retrievables = secret_password_search_finish (result, &error);
  if (error) {
    if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      return;
    message = g_strdup_printf (_("Could not find the sync secrets for the current sync user: %s"), error->message);
    sync_error (message);
    return;
  }

  if (!(retrievables && retrievables->data)) {
    message = _("Could not find the sync secrets for the current sync user.");
    sync_error (message);
    return;
  }

  secret_retrievable_retrieve_secret (g_object_ref ((SecretRetrievable *)retrievables->data),
                                      self->cancellable,
                                      (GAsyncReadyCallback)retrieve_secret_cb,
                                      self);
}

static void
ephy_sync_service_load_secrets (EphySyncService *self)
{
  GHashTable *attributes;
  char *user;

  g_assert (EPHY_IS_SYNC_SERVICE (self));
  g_assert (self->secrets);

  user = ephy_sync_utils_get_sync_user ();
  attributes = secret_attributes_build (EPHY_SYNC_SECRET_SCHEMA,
                                        EPHY_SYNC_SECRET_ACCOUNT_KEY, user,
                                        NULL);
  secret_password_searchv (EPHY_SYNC_SECRET_SCHEMA, attributes,
                           SECRET_SEARCH_UNLOCK | SECRET_SEARCH_LOAD_SECRETS,
                           self->cancellable, (GAsyncReadyCallback)load_secrets_cb, self);

  g_hash_table_unref (attributes);
  g_free (user);
}

static void
store_secrets_cb (GObject         *source_object,
                  GAsyncResult    *result,
                  EphySyncService *self)
{
  g_autoptr (GError) error = NULL;

  secret_password_store_finish (result, &error);
  if (error) {
    if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      return;
    g_warning ("Failed to store sync secrets (is the secret service or secrets portal broken?): %s", error->message);
    g_prefix_error_literal (&error, _("Failed to store sync secrets (is the secret service or secrets portal broken?): "));
    ephy_sync_service_destroy_session (self, NULL);
    g_hash_table_remove_all (self->secrets);
  } else {
    LOG ("Successfully stored sync secrets");
    ephy_sync_utils_set_sync_user (self->user);
  }

  g_signal_emit (self, signals[STORE_FINISHED], 0, error);
  self->is_signing_in = FALSE;

  g_clear_pointer (&self->user, g_free);
}

static void
ephy_sync_service_store_secrets (EphySyncService *self)
{
  JsonNode *node;
  JsonObject *object;
  GHashTable *attributes;
  GHashTableIter iter;
  gpointer key;
  gpointer value;
  char *json_string;
  char *label;

  g_assert (EPHY_IS_SYNC_SERVICE (self));
  g_assert (self->secrets);
  g_assert (self->user);

  node = json_node_new (JSON_NODE_OBJECT);
  object = json_object_new ();
  g_hash_table_iter_init (&iter, self->secrets);
  while (g_hash_table_iter_next (&iter, &key, &value))
    json_object_set_string_member (object, key, value);
  json_node_set_object (node, object);
  json_string = json_to_string (node, FALSE);

  attributes = secret_attributes_build (EPHY_SYNC_SECRET_SCHEMA,
                                        EPHY_SYNC_SECRET_ACCOUNT_KEY, self->user,
                                        NULL);
  /* Translators: %s is the email of the user. */
  label = g_strdup_printf (_("The sync secrets of %s"), self->user);

  LOG ("Storing sync secrets...");
  secret_password_storev (EPHY_SYNC_SECRET_SCHEMA,
                          attributes, NULL, label, json_string, NULL,
                          (GAsyncReadyCallback)store_secrets_cb, self);

  g_free (label);
  g_free (json_string);
  g_hash_table_unref (attributes);
  json_object_unref (object);
  json_node_unref (node);
}

static void
upload_client_record_cb (SoupSession *session,
                         SoupMessage *msg,
                         gpointer     user_data)
{
  EphySyncService *self = EPHY_SYNC_SERVICE (user_data);
  guint status_code;
  g_autoptr (GBytes) response_body = NULL;

  status_code = soup_message_get_status (msg);
  response_body = g_bytes_ref (g_object_get_data (G_OBJECT (msg), "ephy-request-body"));

  if (status_code != 200) {
    g_warning ("Failed to upload client record. Status code: %u, response: %s",
               status_code, (const char *)g_bytes_get_data (response_body, NULL));
    if (self->is_signing_in)
      ephy_sync_service_report_sign_in_error (self, _("Failed to upload client record."), NULL, TRUE);
  } else {
    LOG ("Successfully uploaded client record");
    if (self->is_signing_in)
      ephy_sync_service_store_secrets (self);
  }
}

static void
ephy_sync_service_upload_client_record (EphySyncService *self)
{
  SyncCryptoKeyBundle *bundle;
  JsonNode *node;
  JsonObject *bso;
  char *device_bso_id;
  char *device_id;
  char *device_name;
  char *record;
  char *encrypted;
  char *body;
  char *endpoint;

  g_assert (EPHY_IS_SYNC_SERVICE (self));

  bundle = ephy_sync_service_get_key_bundle (self, "clients");
  if (!bundle)
    return;

  /* Make device ID and name. */
  device_bso_id = ephy_sync_utils_get_device_bso_id ();
  device_id = ephy_sync_utils_get_device_id ();
  device_name = ephy_sync_utils_get_device_name ();

  /* Make BSO as string. */
  record = ephy_sync_utils_make_client_record (device_bso_id, device_id, device_name);
  encrypted = ephy_sync_crypto_encrypt_record (record, bundle);

  bso = json_object_new ();
  json_object_set_string_member (bso, "id", device_bso_id);
  json_object_set_string_member (bso, "payload", encrypted);

  node = json_node_new (JSON_NODE_OBJECT);
  json_node_set_object (node, bso);
  body = json_to_string (node, FALSE);

  /* Upload BSO and store the new device ID and name. */
  LOG ("Uploading client record, device_bso_id=%s, device_id=%s, device_name=%s",
       device_bso_id, device_id, device_name);
  endpoint = g_strdup_printf ("storage/clients/%s", device_bso_id);
  ephy_sync_service_queue_storage_request (self, endpoint,
                                           SOUP_METHOD_PUT, body, -1, -1,
                                           upload_client_record_cb, self);

  g_free (device_bso_id);
  g_free (device_id);
  g_free (device_name);
  g_free (record);
  g_free (encrypted);
  g_free (endpoint);
  g_free (body);
  json_object_unref (bso);
  json_node_unref (node);
  ephy_sync_crypto_key_bundle_free (bundle);
}

static void
ephy_sync_service_finalize (GObject *object)
{
  EphySyncService *self = EPHY_SYNC_SERVICE (object);

  if (ephy_sync_utils_user_is_signed_in ())
    ephy_sync_service_stop_periodical_sync (self);

  if (self->key_pair)
    ephy_sync_crypto_rsa_key_pair_free (self->key_pair);

  g_free (self->crypto_keys);
  g_slist_free (self->managers);
  g_queue_free_full (self->storage_queue, (GDestroyNotify)storage_request_async_data_free);
  ephy_sync_service_clear_storage_credentials (self);

  G_OBJECT_CLASS (ephy_sync_service_parent_class)->finalize (object);
}

static void
ephy_sync_service_dispose (GObject *object)
{
  EphySyncService *self = EPHY_SYNC_SERVICE (object);

  g_clear_object (&self->session);
  g_clear_pointer (&self->secrets, g_hash_table_unref);

  if (self->cancellable) {
    g_cancellable_cancel (self->cancellable);
    g_clear_object (&self->cancellable);
  }

  G_OBJECT_CLASS (ephy_sync_service_parent_class)->dispose (object);
}

static void
ephy_sync_service_constructed (GObject *object)
{
  EphySyncService *self = EPHY_SYNC_SERVICE (object);

  G_OBJECT_CLASS (ephy_sync_service_parent_class)->constructed (object);

  if (self->sync_periodically) {
    g_object_set (self->session,
                  "user-agent", ephy_user_agent_get (),
                  NULL);

    g_signal_connect (EPHY_SETTINGS_SYNC, "changed::"EPHY_PREFS_SYNC_FREQUENCY,
                      G_CALLBACK (sync_frequency_changed_cb), self);
  }
}

static void
ephy_sync_service_init (EphySyncService *self)
{
  self->session = soup_session_new ();
  self->storage_queue = g_queue_new ();
  self->secrets = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  self->cancellable = g_cancellable_new ();

  if (ephy_sync_utils_user_is_signed_in ())
    ephy_sync_service_load_secrets (self);
}

static void
ephy_sync_service_class_init (EphySyncServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = ephy_sync_service_set_property;
  object_class->get_property = ephy_sync_service_get_property;
  object_class->constructed = ephy_sync_service_constructed;
  object_class->dispose = ephy_sync_service_dispose;
  object_class->finalize = ephy_sync_service_finalize;

  obj_properties[PROP_SYNC_PERIODICALLY] =
    g_param_spec_boolean ("sync-periodically",
                          NULL, NULL,
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

  signals[LOAD_FINISHED] =
    g_signal_new ("sync-secrets-load-finished",
                  EPHY_TYPE_SYNC_SERVICE,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

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

static void
upload_crypto_keys_cb (SoupSession *session,
                       SoupMessage *msg,
                       gpointer     user_data)
{
  EphySyncService *self = EPHY_SYNC_SERVICE (user_data);
  guint status_code;
  g_autoptr (GBytes) response_body = NULL;

  status_code = soup_message_get_status (msg);
  response_body = g_bytes_ref (g_object_get_data (G_OBJECT (msg), "ephy-request-body"));

  if (status_code != 200) {
    g_warning ("Failed to upload crypto/keys record. Status code: %u, response: %s",
               status_code, (const char *)g_bytes_get_data (response_body, NULL));
    ephy_sync_service_report_sign_in_error (self,
                                            _("Failed to upload crypto/keys record."),
                                            NULL, TRUE);
  } else {
    LOG ("Successfully uploaded crypto/keys record");
    ephy_sync_service_set_secret (self, secrets[CRYPTO_KEYS], self->crypto_keys);
    ephy_sync_service_upload_client_record (self);
  }

  g_clear_pointer (&self->crypto_keys, g_free);
}

static void
ephy_sync_service_upload_crypto_keys (EphySyncService *self)
{
  SyncCryptoKeyBundle *bundle;
  JsonNode *node;
  JsonObject *record;
  char *payload;
  char *body;
  const char *kb_hex;
  guint8 *kb;

  g_assert (EPHY_IS_SYNC_SERVICE (self));
  kb_hex = ephy_sync_service_get_secret (self, secrets[MASTER_KEY]);
  g_assert (kb_hex);

  node = json_node_new (JSON_NODE_OBJECT);
  record = json_object_new ();
  self->crypto_keys = ephy_sync_crypto_generate_crypto_keys ();
  kb = ephy_sync_utils_decode_hex (kb_hex);
  bundle = ephy_sync_crypto_derive_master_bundle (kb);
  payload = ephy_sync_crypto_encrypt_record (self->crypto_keys, bundle);
  json_object_set_string_member (record, "payload", payload);
  json_object_set_string_member (record, "id", "keys");
  json_node_set_object (node, record);
  body = json_to_string (node, FALSE);

  ephy_sync_service_queue_storage_request (self, "storage/crypto/keys",
                                           SOUP_METHOD_PUT, body, -1, -1,
                                           upload_crypto_keys_cb, self);

  g_free (body);
  g_free (payload);
  g_free (kb);
  json_object_unref (record);
  json_node_unref (node);
  ephy_sync_crypto_key_bundle_free (bundle);
}

static void
get_crypto_keys_cb (SoupSession *session,
                    SoupMessage *msg,
                    gpointer     user_data)
{
  EphySyncService *self = EPHY_SYNC_SERVICE (user_data);
  SyncCryptoKeyBundle *bundle = NULL;
  JsonNode *node = NULL;
  JsonObject *json = NULL;
  g_autoptr (GError) error = NULL;
  const char *payload;
  char *crypto_keys = NULL;
  guint8 *kb = NULL;
  guint status_code;
  g_autoptr (GBytes) response_body = NULL;

  status_code = soup_message_get_status (msg);
  response_body = g_bytes_ref (g_object_get_data (G_OBJECT (msg), "ephy-request-body"));

  if (status_code == 404) {
    LOG ("crypto/keys record not found, uploading new one...");
    ephy_sync_service_upload_crypto_keys (self);
    return;
  }

  if (status_code != 200) {
    g_warning ("Failed to get crypto/keys record. Status code: %u, response: %s",
               status_code, (const char *)g_bytes_get_data (response_body, NULL));
    goto out_error;
  }

  node = json_from_response_body (response_body, &error);
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
   * used as a HMAC key.
   */
  kb = ephy_sync_utils_decode_hex (ephy_sync_service_get_secret (self, secrets[MASTER_KEY]));
  bundle = ephy_sync_crypto_derive_master_bundle (kb);
  crypto_keys = ephy_sync_crypto_decrypt_record (payload, bundle);
  if (!crypto_keys) {
    g_warning ("Failed to decrypt crypto/keys record");
    goto out_error;
  }

  ephy_sync_service_set_secret (self, secrets[CRYPTO_KEYS], crypto_keys);
  ephy_sync_service_upload_client_record (self);
  goto out_no_error;

out_error:
  ephy_sync_service_report_sign_in_error (self, _("Failed to retrieve crypto keys."),
                                          NULL, TRUE);
out_no_error:
  if (bundle)
    ephy_sync_crypto_key_bundle_free (bundle);
  if (node)
    json_node_unref (node);
  g_free (crypto_keys);
  g_free (kb);
}

static void
ephy_sync_service_get_crypto_keys (EphySyncService *self)
{
  g_assert (EPHY_IS_SYNC_SERVICE (self));

  LOG ("Getting account's crypto keys...");
  ephy_sync_service_queue_storage_request (self, "storage/crypto/keys",
                                           SOUP_METHOD_GET, NULL, -1, -1,
                                           get_crypto_keys_cb, self);
}

static JsonObject *
make_engine_object (int version)
{
  JsonObject *object;
  char *sync_id;

  object = json_object_new ();
  sync_id = ephy_sync_utils_get_random_sync_id ();
  json_object_set_int_member (object, "version", version);
  json_object_set_string_member (object, "syncID", sync_id);

  g_free (sync_id);

  return object;
}

static void
upload_meta_global_cb (SoupSession *session,
                       SoupMessage *msg,
                       gpointer     user_data)
{
  EphySyncService *self = EPHY_SYNC_SERVICE (user_data);
  guint status_code;
  g_autoptr (GBytes) response_body = NULL;

  status_code = soup_message_get_status (msg);
  response_body = g_bytes_ref (g_object_get_data (G_OBJECT (msg), "ephy-request-body"));

  if (status_code != 200) {
    g_warning ("Failed to upload meta/global record. Status code: %u, response: %s",
               status_code, (const char *)g_bytes_get_data (response_body, NULL));
    ephy_sync_service_report_sign_in_error (self,
                                            _("Failed to upload meta/global record."),
                                            NULL, TRUE);
  } else {
    LOG ("Successfully uploaded meta/global record");
    ephy_sync_service_get_crypto_keys (self);
  }
}

static void
ephy_sync_service_upload_meta_global (EphySyncService *self)
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
  json_object_set_array_member (payload, "declined", declined);
  json_object_set_object_member (engines, "clients", make_engine_object (1));
  json_object_set_object_member (engines, "bookmarks", make_engine_object (2));
  json_object_set_object_member (engines, "history", make_engine_object (1));
  json_object_set_object_member (engines, "passwords", make_engine_object (1));
  json_object_set_object_member (engines, "tabs", make_engine_object (1));
  json_object_set_object_member (engines, "forms", make_engine_object (1));
  json_object_set_object_member (payload, "engines", engines);
  json_object_set_int_member (payload, "storageVersion", EPHY_SYNC_STORAGE_VERSION);
  sync_id = ephy_sync_utils_get_random_sync_id ();
  json_object_set_string_member (payload, "syncID", sync_id);
  json_node_set_object (node, payload);
  payload_str = json_to_string (node, FALSE);
  json_object_set_string_member (record, "payload", payload_str);
  json_object_set_string_member (record, "id", "global");
  json_node_set_object (node, record);
  body = json_to_string (node, FALSE);

  ephy_sync_service_queue_storage_request (self, "storage/meta/global",
                                           SOUP_METHOD_PUT, body, -1, -1,
                                           upload_meta_global_cb, self);

  g_free (body);
  g_free (payload_str);
  g_free (sync_id);
  json_object_unref (payload);
  json_object_unref (record);
  json_node_unref (node);
}

static void
verify_storage_version_cb (SoupSession *session,
                           SoupMessage *msg,
                           gpointer     user_data)
{
  EphySyncService *self = EPHY_SYNC_SERVICE (user_data);
  JsonParser *parser = NULL;
  JsonObject *json = NULL;
  g_autoptr (GError) error = NULL;
  char *payload = NULL;
  char *message = NULL;
  int storage_version;
  guint status_code;
  g_autoptr (GBytes) response_body = NULL;

  status_code = soup_message_get_status (msg);
  response_body = g_bytes_ref (g_object_get_data (G_OBJECT (msg), "ephy-request-body"));

  if (status_code == 404) {
    LOG ("meta/global record not found, uploading new one...");
    ephy_sync_service_upload_meta_global (self);
    return;
  }

  if (status_code != 200) {
    g_warning ("Failed to get meta/global record. Status code: %u, response: %s",
               status_code, (const char *)g_bytes_get_data (response_body, NULL));
    goto out_error;
  }

  parser = json_parser_new ();
  json_parser_load_from_data (parser, g_bytes_get_data (response_body, NULL), -1, &error);
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
  if (storage_version != EPHY_SYNC_STORAGE_VERSION) {
    /* Translators: the %d is the storage version, the \n is a newline character. */
    message = g_strdup_printf (_("Your Mozilla account uses storage version %d. "
                                 "Web only supports version %d."),
                               EPHY_SYNC_STORAGE_VERSION,
                               storage_version);
    goto out_error;
  }

  ephy_sync_service_get_crypto_keys (self);
  goto out_no_error;

out_error:
  message = message ? message : g_strdup (_("Failed to verify storage version."));
  ephy_sync_service_report_sign_in_error (self, message, NULL, TRUE);
out_no_error:
  if (parser)
    g_object_unref (parser);
  g_free (payload);
  g_free (message);
}

static void
ephy_sync_service_verify_storage_version (EphySyncService *self)
{
  g_assert (EPHY_IS_SYNC_SERVICE (self));

  LOG ("Verifying account's storage version...");
  ephy_sync_service_queue_storage_request (self, "storage/meta/global",
                                           SOUP_METHOD_GET, NULL, -1, -1,
                                           verify_storage_version_cb, self);
}

static void
upload_fxa_device_cb (SoupSession *session,
                      SoupMessage *msg,
                      gpointer     user_data)
{
  EphySyncService *self = user_data;
  JsonNode *node;
  JsonObject *object;
  g_autoptr (GError) error = NULL;
  guint status_code;
  g_autoptr (GBytes) response_body = NULL;

  status_code = soup_message_get_status (msg);
  response_body = g_bytes_ref (g_object_get_data (G_OBJECT (msg), "ephy-request-body"));

  if (status_code != 200) {
    g_warning ("Failed to upload device info on FxA Server. Status code: %u, response: %s",
               status_code, (const char *)g_bytes_get_data (response_body, NULL));
    goto out_error;
  }

  node = json_from_response_body (response_body, &error);
  if (error) {
    g_warning ("Response is not a valid JSON: %s", error->message);
    goto out_error;
  }

  object = json_node_get_object (node);
  ephy_sync_utils_set_device_id (json_object_get_string_member (object, "id"));
  json_node_unref (node);

  LOG ("Successfully uploaded device info on FxA Server");
  if (self->is_signing_in)
    ephy_sync_service_verify_storage_version (self);
  return;

out_error:
  if (self->is_signing_in)
    ephy_sync_service_report_sign_in_error (self, _("Failed to upload device info"), NULL, TRUE);
}

static void
ephy_sync_service_upload_fxa_device (EphySyncService *self)
{
  JsonNode *node;
  JsonObject *object;
  const char *session_token;
  char *body;
  char *device_name;
  char *token_id_hex;
  guint8 *token_id;
  guint8 *req_hmac_key;
  guint8 *tmp;

  g_assert (EPHY_IS_SYNC_SERVICE (self));

  session_token = ephy_sync_service_get_secret (self, secrets[SESSION_TOKEN]);
  if (!session_token)
    return;

  object = json_object_new ();
  device_name = ephy_sync_utils_get_device_name ();
  json_object_set_string_member (object, "name", device_name);
  json_object_set_string_member (object, "type", "desktop");

  /* If we are signing in, the ID for the newly registered device will be returned
   * by the FxA server in the response. Otherwise, we are updating the current
   * device (i.e. setting its name), so we use the previously obtained ID. */
  if (!self->is_signing_in) {
    char *device_id = ephy_sync_utils_get_device_id ();
    json_object_set_string_member (object, "id", device_id);
    g_free (device_id);
  }

  node = json_node_new (JSON_NODE_OBJECT);
  json_node_take_object (node, object);
  body = json_to_string (node, FALSE);

  ephy_sync_crypto_derive_session_token (session_token, &token_id, &req_hmac_key, &tmp);
  token_id_hex = ephy_sync_utils_encode_hex (token_id, 32);

  LOG ("Uploading device info on FxA Server...");
  ephy_sync_service_fxa_hawk_post (self, "account/device", token_id_hex,
                                   req_hmac_key, 32, body,
                                   upload_fxa_device_cb, self);

  g_free (body);
  g_free (device_name);
  g_free (token_id_hex);
  g_free (token_id);
  g_free (req_hmac_key);
  g_free (tmp);
  json_node_unref (node);
}

static void
ephy_sync_service_sign_in_finish (EphySyncService *self,
                                  SignInAsyncData *data,
                                  const char      *bundle)
{
  guint8 *unwrap_kb;
  guint8 *ka;
  guint8 *kb;
  char *kb_hex;

  g_assert (EPHY_IS_SYNC_SERVICE (self));
  g_assert (data);
  g_assert (bundle);

  /* Derive the master sync keys form the key bundle. */
  unwrap_kb = ephy_sync_utils_decode_hex (data->unwrap_kb);
  if (!ephy_sync_crypto_derive_master_keys (bundle, data->resp_hmac_key,
                                            data->resp_xor_key, unwrap_kb,
                                            &ka, &kb)) {
    ephy_sync_service_report_sign_in_error (self, _("Failed to retrieve the Sync Key"),
                                            data->session_token, FALSE);
    goto out;
  }

  /* Cache the user email until the secrets are stored. We cannot use
   * ephy_sync_utils_set_sync_user() here because that will trigger the
   * 'changed' signal of EPHY_PREFS_SYNC_USER which in turn will cause
   * the web extension to destroy its own sync service and create a new
   * one. That new sync service will fail to load the sync secrets from
   * disk because the secrets are not yet stored at this point, thus it
   * will be unable to operate.
   */
  self->user = g_strdup (data->email);
  ephy_sync_service_set_secret (self, secrets[UID], data->uid);
  ephy_sync_service_set_secret (self, secrets[SESSION_TOKEN], data->session_token);
  kb_hex = ephy_sync_utils_encode_hex (kb, 32);
  ephy_sync_service_set_secret (self, secrets[MASTER_KEY], kb_hex);

  ephy_sync_service_upload_fxa_device (self);

  g_free (kb_hex);
  g_free (kb);
  g_free (ka);
out:
  g_free (unwrap_kb);
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
  g_autoptr (GError) error = NULL;
  const char *bundle;
  guint status_code;
  g_autoptr (GBytes) response_body = NULL;

  status_code = soup_message_get_status (msg);
  response_body = g_bytes_ref (g_object_get_data (G_OBJECT (msg), "ephy-request-body"));

  node = json_from_response_body (response_body, &error);
  if (error) {
    g_warning ("Response is not a valid JSON: %s", error->message);
    goto out_error;
  }
  json = json_node_get_object (node);
  if (!json) {
    g_warning ("JSON node does not hold a JSON object");
    goto out_error;
  }

  if (status_code == 200) {
    bundle = json_object_get_string_member (json, "bundle");
    if (!bundle) {
      g_warning ("JSON object has invalid or missing 'bundle' member");
      goto out_error;
    }
    /* Extract the master sync keys from the bundle and save tokens. */
    ephy_sync_service_sign_in_finish (data->service, data, bundle);
    goto out_no_error;
  }

  /* If account is not verified, poll the Mozilla Accounts Server
   * until the verification has completed.
   */
  if (json_object_get_int_member (json, "errno") == 104) {
    LOG ("Account not verified, retrying...");
    ephy_sync_service_fxa_hawk_get (data->service, "account/keys",
                                    data->token_id_hex, data->req_hmac_key, 32,
                                    get_account_keys_cb, data);
    goto out_no_error;
  }

  g_warning ("Failed to get /account/keys. Status code: %u, response: %s",
             status_code, (const char *)g_bytes_get_data (response_body, NULL));

out_error:
  ephy_sync_service_report_sign_in_error (data->service,
                                          _("Failed to retrieve the Sync Key"),
                                          data->session_token, FALSE);
  sign_in_async_data_free (data);
out_no_error:
  if (node)
    json_node_unref (node);
}

void
ephy_sync_service_sign_in (EphySyncService *self,
                           const char      *email,
                           const char      *uid,
                           const char      *session_token,
                           const char      *key_fetch_token,
                           const char      *unwrap_kb)
{
  SignInAsyncData *data;
  guint8 *token_id;
  guint8 *req_hmac_key;
  guint8 *resp_hmac_key;
  guint8 *resp_xor_key;
  char *token_id_hex;

  g_assert (EPHY_IS_SYNC_SERVICE (self));
  g_assert (email);
  g_assert (uid);
  g_assert (session_token);
  g_assert (key_fetch_token);
  g_assert (unwrap_kb);

  self->is_signing_in = TRUE;

  /* Derive tokenID, reqHMACkey, respHMACkey and respXORkey from keyFetchToken.
   * tokenID and reqHMACkey are used to sign HAWK GET requests to /account/keys
   * endpoint. The server looks up the stored table entry with tokenID, checks
   * the request HMAC for validity, then returns the pre-encrypted response.
   * See https://github.com/mozilla/fxa-auth-server/wiki/onepw-protocol#fetching-sync-keys
   */
  ephy_sync_crypto_derive_key_fetch_token (key_fetch_token,
                                           &token_id, &req_hmac_key,
                                           &resp_hmac_key, &resp_xor_key);
  token_id_hex = ephy_sync_utils_encode_hex (token_id, 32);

  /* Get the master sync key bundle from the /account/keys endpoint. */
  data = sign_in_async_data_new (self, email, uid,
                                 session_token, unwrap_kb,
                                 token_id_hex, req_hmac_key,
                                 resp_hmac_key, resp_xor_key);
  LOG ("Getting account's Sync Key...");
  ephy_sync_service_fxa_hawk_get (self, "account/keys",
                                  token_id_hex, req_hmac_key, 32,
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
  GNetworkMonitor *monitor;

  g_assert (EPHY_IS_SYNCHRONIZABLE_MANAGER (manager));
  g_assert (EPHY_IS_SYNCHRONIZABLE (synchronizable));
  g_assert (EPHY_IS_SYNC_SERVICE (self));

  monitor = g_network_monitor_get_default ();
  if (g_network_monitor_get_connectivity (monitor) != G_NETWORK_CONNECTIVITY_FULL)
    return;

  if (!ephy_sync_utils_user_is_signed_in ())
    return;

  ephy_sync_service_delete_synchronizable (self, manager, synchronizable);
}

static void
synchronizable_modified_cb (EphySynchronizableManager *manager,
                            EphySynchronizable        *synchronizable,
                            gboolean                   should_force,
                            EphySyncService           *self)
{
  GNetworkMonitor *monitor;

  g_assert (EPHY_IS_SYNCHRONIZABLE_MANAGER (manager));
  g_assert (EPHY_IS_SYNCHRONIZABLE (synchronizable));
  g_assert (EPHY_IS_SYNC_SERVICE (self));

  monitor = g_network_monitor_get_default ();
  if (g_network_monitor_get_connectivity (monitor) != G_NETWORK_CONNECTIVITY_FULL)
    return;

  if (!ephy_sync_utils_user_is_signed_in ())
    return;

  ephy_sync_service_upload_synchronizable (self, manager, synchronizable, should_force);
}

void
ephy_sync_service_register_manager (EphySyncService           *self,
                                    EphySynchronizableManager *manager)
{
  g_assert (EPHY_IS_SYNC_SERVICE (self));
  g_assert (EPHY_IS_SYNCHRONIZABLE_MANAGER (manager));

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
  g_assert (EPHY_IS_SYNC_SERVICE (self));
  g_assert (EPHY_IS_SYNCHRONIZABLE_MANAGER (manager));

  self->managers = g_slist_remove (self->managers, manager);

  g_signal_handlers_disconnect_by_func (manager, synchronizable_deleted_cb, self);
  g_signal_handlers_disconnect_by_func (manager, synchronizable_modified_cb, self);
}

void
ephy_sync_service_update_device_name (EphySyncService *self,
                                      const char      *name)
{
  g_assert (EPHY_IS_SYNC_SERVICE (self));
  g_assert (name);

  ephy_sync_utils_set_device_name (name);
  ephy_sync_service_upload_fxa_device (self);
  ephy_sync_service_upload_client_record (self);
}

static void
delete_open_tabs_record_cb (SoupSession *session,
                            SoupMessage *msg,
                            gpointer     user_data)
{
  EphySyncService *self = EPHY_SYNC_SERVICE (user_data);
  const char *session_token;
  guint status_code;
  g_autoptr (GBytes) response_body = NULL;

  status_code = soup_message_get_status (msg);
  response_body = g_bytes_ref (g_object_get_data (G_OBJECT (msg), "ephy-request-body"));

  if (status_code != 200) {
    g_warning ("Failed to delete open tabs record. Status code: %u, response: %s",
               status_code, (const char *)g_bytes_get_data (response_body, NULL));
  } else {
    LOG ("Successfully deleted open tabs record");
  }

  ephy_sync_service_clear_storage_queue (self);
  ephy_sync_service_clear_storage_credentials (self);

  session_token = ephy_sync_service_get_secret (self, secrets[SESSION_TOKEN]);
  ephy_sync_service_destroy_session (self, session_token);

  ephy_sync_service_forget_secrets (self);
  ephy_sync_utils_set_device_id (NULL);
  ephy_sync_utils_set_sync_user (NULL);
}

static void
delete_client_record_cb (SoupSession *session,
                         SoupMessage *msg,
                         gpointer     user_data)
{
  EphySyncService *self = EPHY_SYNC_SERVICE (user_data);
  char *endpoint;
  char *device_bso_id;
  guint status_code;
  g_autoptr (GBytes) response_body = NULL;

  status_code = soup_message_get_status (msg);
  response_body = g_bytes_ref (g_object_get_data (G_OBJECT (msg), "ephy-request-body"));

  if (status_code != 200) {
    g_warning ("Failed to delete client record. Status code: %u, response: %s",
               status_code, (const char *)g_bytes_get_data (response_body, NULL));
  } else {
    LOG ("Successfully deleted client record");
  }

  device_bso_id = ephy_sync_utils_get_device_bso_id ();
  /* Delete the open tabs record associated to this device. */
  endpoint = g_strdup_printf ("storage/tabs/%s", device_bso_id);
  ephy_sync_service_queue_storage_request (self, endpoint,
                                           SOUP_METHOD_DELETE,
                                           NULL, -1, -1,
                                           delete_open_tabs_record_cb, self);
  g_free (endpoint);
  g_free (device_bso_id);
}

static void
ephy_sync_service_delete_client_record (EphySyncService *self)
{
  char *endpoint;
  char *device_bso_id;

  g_assert (EPHY_IS_SYNC_SERVICE (self));

  device_bso_id = ephy_sync_utils_get_device_bso_id ();
  /* Delete the client record associated to this device. */
  endpoint = g_strdup_printf ("storage/clients/%s", device_bso_id);
  ephy_sync_service_queue_storage_request (self, endpoint,
                                           SOUP_METHOD_DELETE,
                                           NULL, -1, -1,
                                           delete_client_record_cb, self);
  g_free (endpoint);
  g_free (device_bso_id);
}

void
ephy_sync_service_sign_out (EphySyncService *self)
{
  g_assert (EPHY_IS_SYNC_SERVICE (self));

  ephy_sync_service_stop_periodical_sync (self);
  ephy_sync_service_delete_client_record (self);

  /* Clear managers. */
  for (GSList *l = self->managers; l && l->data; l = l->next) {
    g_signal_handlers_disconnect_by_func (l->data, synchronizable_deleted_cb, self);
    g_signal_handlers_disconnect_by_func (l->data, synchronizable_modified_cb, self);
  }
  g_clear_pointer (&self->managers, g_slist_free);

  ephy_sync_utils_set_bookmarks_sync_is_initial (TRUE);
  ephy_sync_utils_set_passwords_sync_is_initial (TRUE);
  ephy_sync_utils_set_history_sync_is_initial (TRUE);
  ephy_sync_utils_set_sync_time (0);
}

void
ephy_sync_service_sync (EphySyncService *self)
{
  g_assert (EPHY_IS_SYNC_SERVICE (self));
  g_assert (ephy_sync_utils_user_is_signed_in ());

  ephy_sync_service_sync_internal (self);
}

void
ephy_sync_service_start_sync (EphySyncService *self)
{
  g_assert (EPHY_IS_SYNC_SERVICE (self));
  g_assert (self->sync_periodically);

  if (ephy_sync_utils_user_is_signed_in ()) {
    ephy_sync_service_sync_internal (self);
    ephy_sync_service_schedule_periodical_sync (self);
  }
}
