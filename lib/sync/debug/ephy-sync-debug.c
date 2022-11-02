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
#include "ephy-sync-debug.h"

#include "ephy-debug.h"
#include "ephy-settings.h"
#include "ephy-sync-crypto.h"
#include "ephy-sync-utils.h"

#include <libsoup/soup.h>
#include <string.h>

static JsonObject *
ephy_sync_debug_load_secrets (void)
{
  GHashTable *attributes;
  JsonObject *secrets = NULL;
  SecretValue *value;
  JsonNode *node;
  GError *error = NULL;
  GList *result;
  char *user;

  user = ephy_sync_utils_get_sync_user ();
  if (!user) {
    LOG ("There is no sync user signed in.");
    return NULL;
  }

  attributes = secret_attributes_build (EPHY_SYNC_SECRET_SCHEMA,
                                        EPHY_SYNC_SECRET_ACCOUNT_KEY, user,
                                        NULL);
  result = secret_service_search_sync (NULL,
                                       EPHY_SYNC_SECRET_SCHEMA,
                                       attributes,
                                       SECRET_SEARCH_UNLOCK | SECRET_SEARCH_LOAD_SECRETS,
                                       NULL,
                                       &error);
  if (error) {
    LOG ("Error searching sync secrets: %s", error->message);
    g_error_free (error);
    goto free_attributes;
  }

  value = secret_item_get_secret (result->data);
  node = json_from_string (secret_value_get_text (value), &error);
  if (error) {
    LOG ("Sync secrets are not a valid JSON: %s", error->message);
    g_error_free (error);
    goto free_value;
  }

  secrets = json_node_dup_object (node);

  json_node_unref (node);
free_value:
  secret_value_unref (value);
  g_list_free_full (result, g_object_unref);
free_attributes:
  g_hash_table_unref (attributes);
  g_free (user);

  return secrets;
}

static SyncCryptoKeyBundle *
ephy_sync_debug_get_bundle_for_collection (const char *collection)
{
  SyncCryptoKeyBundle *bundle = NULL;
  JsonObject *secrets;
  JsonNode *node;
  JsonObject *json;
  JsonObject *collections;
  JsonArray *array;
  GError *error = NULL;
  const char *crypto_keys;

  g_assert (collection);

  secrets = ephy_sync_debug_load_secrets ();
  if (!secrets)
    return NULL;

  crypto_keys = json_object_get_string_member (secrets, "crypto_keys");
  node = json_from_string (crypto_keys, &error);
  if (error) {
    LOG ("Crypto keys are not a valid JSON: %s", error->message);
    g_error_free (error);
    goto free_secrets;
  }

  json = json_node_get_object (node);
  collections = json_object_get_object_member (json, "collections");
  array = json_object_has_member (collections, collection) ?
          json_object_get_array_member (collections, collection) :
          json_object_get_array_member (json, "default");
  bundle = ephy_sync_crypto_key_bundle_new (json_array_get_string_element (array, 0),
                                            json_array_get_string_element (array, 1));

  json_node_unref (node);
free_secrets:
  json_object_unref (secrets);

  return bundle;
}

static char *
ephy_sync_debug_make_upload_body (const char          *id,
                                  const char          *record,
                                  SyncCryptoKeyBundle *bundle)
{
  JsonNode *node;
  JsonObject *json;
  char *payload;
  char *body;

  g_assert (id);
  g_assert (record);
  g_assert (bundle);

  payload = ephy_sync_crypto_encrypt_record (record, bundle);
  json = json_object_new ();
  json_object_set_string_member (json, "id", id);
  json_object_set_string_member (json, "payload", payload);
  node = json_node_new (JSON_NODE_OBJECT);
  json_node_set_object (node, json);
  body = json_to_string (node, FALSE);

  g_free (payload);
  json_object_unref (json);
  json_node_unref (node);

  return body;
}

static char *
ephy_sync_debug_make_delete_body (const char          *id,
                                  SyncCryptoKeyBundle *bundle)
{
  JsonNode *node;
  JsonObject *json;
  char *record;
  char *payload;
  char *body;

  g_assert (id);
  g_assert (bundle);

  record = g_strdup_printf ("{\"id\": \"%s\", \"deleted\": true}", id);
  payload = ephy_sync_crypto_encrypt_record (record, bundle);
  json = json_object_new ();
  json_object_set_string_member (json, "id", id);
  json_object_set_string_member (json, "payload", payload);
  node = json_node_new (JSON_NODE_OBJECT);
  json_node_set_object (node, json);
  body = json_to_string (node, FALSE);

  g_free (record);
  g_free (payload);
  json_object_unref (json);
  json_node_unref (node);

  return body;
}

static char *
ephy_sync_debug_decrypt_record (const char          *payload,
                                SyncCryptoKeyBundle *bundle)
{
  JsonNode *node;
  GError *error = NULL;
  char *record;
  char *prettified = NULL;

  g_assert (payload);
  g_assert (bundle);

  record = ephy_sync_crypto_decrypt_record (payload, bundle);
  if (!record)
    return NULL;

  node = json_from_string (record, &error);
  if (error) {
    LOG ("Record is not a valid JSON: %s", error->message);
    g_error_free (error);
    goto free_record;
  }

  prettified = json_to_string (node, TRUE);

  json_node_unref (node);
free_record:
  g_free (record);

  return prettified;
}

static SoupMessage *
ephy_sync_debug_prepare_soup_message (const char   *url,
                                      const char   *method,
                                      const char   *body,
                                      const char   *hawk_id,
                                      const guint8 *hawk_key,
                                      gsize         hawk_key_len)
{
  SyncCryptoHawkOptions *options = NULL;
  SyncCryptoHawkHeader *header;
  SoupMessage *msg;
  SoupMessageHeaders *request_headers;
  const char *content_type = "application/json";

  g_assert (url);
  g_assert (method);
  g_assert (g_strcmp0 (method, "PUT") || body);
  g_assert (g_strcmp0 (method, "POST") || body);
  g_assert (hawk_id);
  g_assert (hawk_key && hawk_key_len > 0);

  msg = soup_message_new (method, url);

  request_headers = soup_message_get_request_headers (msg);

  if (body) {
    g_autoptr (GBytes) bytes = NULL;

    options = ephy_sync_crypto_hawk_options_new (NULL, NULL, NULL, content_type,
                                                 NULL, NULL, NULL, body, NULL);
    bytes = g_bytes_new (body, strlen (body));
    soup_message_set_request_body_from_bytes (msg, content_type, bytes);
  }

  if (!g_strcmp0 (method, "PUT") || !g_strcmp0 (method, "POST"))
    soup_message_headers_append (request_headers, "content-type", content_type);

  header = ephy_sync_crypto_hawk_header_new (url, method, hawk_id,
                                             hawk_key, hawk_key_len, options);
  soup_message_headers_append (request_headers, "authorization", header->header);

  ephy_sync_crypto_hawk_header_free (header);
  if (options)
    ephy_sync_crypto_hawk_options_free (options);

  return msg;
}

static char *
ephy_sync_debug_get_signed_certificate (const char           *session_token,
                                        SyncCryptoRSAKeyPair *keypair)
{
  SoupSession *session;
  SoupMessage *msg;
  JsonNode *node;
  JsonNode *response;
  JsonObject *json;
  JsonObject *public_key;
  JsonObject *json_body;
  g_autoptr (GError) error = NULL;
  guint8 *id;
  guint8 *key;
  guint8 *tmp;
  char *certificate = NULL;
  char *id_hex;
  char *body;
  char *url;
  char *n;
  char *e;
  guint status_code;
  g_autofree char *accounts_server = NULL;
  g_autoptr (GBytes) response_body = NULL;

  g_assert (session_token);
  g_assert (keypair);

  ephy_sync_crypto_derive_session_token (session_token, &id, &key, &tmp);
  id_hex = ephy_sync_utils_encode_hex (id, 32);
  n = mpz_get_str (NULL, 10, keypair->public.n);
  e = mpz_get_str (NULL, 10, keypair->public.e);

  public_key = json_object_new ();
  json_object_set_string_member (public_key, "algorithm", "RS");
  json_object_set_string_member (public_key, "n", n);
  json_object_set_string_member (public_key, "e", e);
  json_body = json_object_new ();
  json_object_set_int_member (json_body, "duration", 5 * 60 * 1000);
  json_object_set_object_member (json_body, "publicKey", public_key);
  node = json_node_new (JSON_NODE_OBJECT);
  json_node_set_object (node, json_body);
  body = json_to_string (node, FALSE);

  accounts_server = ephy_sync_utils_get_accounts_server ();
  url = g_strdup_printf ("%s/certificate/sign", accounts_server);
  msg = ephy_sync_debug_prepare_soup_message (url, "POST", body,
                                              id_hex, key, 32);
  session = soup_session_new ();
  response_body = soup_session_send_and_read (session, msg, NULL, &error);
  if (!response_body) {
    LOG ("Failed to get signed certificate: %s", error->message);
    goto free_session;
  }

  status_code = soup_message_get_status (msg);
  if (status_code != 200) {
    LOG ("Failed to get signed certificate: %s", (const char *)g_bytes_get_data (response_body, NULL));
    goto free_session;
  }

  response = json_from_string (g_bytes_get_data (response_body, NULL), &error);
  if (error) {
    LOG ("Response is not a valid JSON: %s", error->message);
    goto free_session;
  }

  json = json_node_get_object (response);
  certificate = g_strdup (json_object_get_string_member (json, "cert"));

  json_node_unref (response);
free_session:
  g_object_unref (session);
  g_object_unref (msg);
  g_free (url);
  g_free (body);
  json_node_unref (node);
  json_object_unref (json_body);
  g_free (e);
  g_free (n);
  g_free (id_hex);
  g_free (id);
  g_free (key);
  g_free (tmp);

  return certificate;
}

static gboolean
ephy_sync_debug_get_storage_credentials (char **storage_endpoint,
                                         char **storage_id,
                                         char **storage_key)
{
  SyncCryptoRSAKeyPair *keypair;
  SoupSession *session;
  SoupMessage *msg;
  JsonNode *response;
  JsonObject *secrets;
  JsonObject *json;
  g_autoptr (GError) error = NULL;
  char *certificate;
  char *audience;
  char *assertion;
  char *hashed_kb;
  char *client_state;
  char *authorization;
  guint8 *kb;
  const char *session_token;
  guint status_code;
  SoupMessageHeaders *request_headers;
  g_autoptr (GBytes) response_body = NULL;
  gboolean success = FALSE;
  g_autofree char *token_server = NULL;

  secrets = ephy_sync_debug_load_secrets ();
  if (!secrets)
    return FALSE;

  keypair = ephy_sync_crypto_rsa_key_pair_new ();
  session_token = json_object_get_string_member (secrets, "session_token");
  certificate = ephy_sync_debug_get_signed_certificate (session_token, keypair);
  if (!certificate)
    goto free_keypair;

  token_server = ephy_sync_utils_get_token_server ();
  audience = ephy_sync_utils_get_audience (token_server);
  assertion = ephy_sync_crypto_create_assertion (certificate, audience, 300, keypair);
  kb = ephy_sync_utils_decode_hex (json_object_get_string_member (secrets, "master_key"));
  hashed_kb = g_compute_checksum_for_data (G_CHECKSUM_SHA256, kb, 32);
  client_state = g_strndup (hashed_kb, 32);
  authorization = g_strdup_printf ("BrowserID %s", assertion);
  msg = soup_message_new ("GET", token_server);
  request_headers = soup_message_get_request_headers (msg);
  soup_message_headers_append (request_headers, "X-Client-State", client_state);
  soup_message_headers_append (request_headers, "authorization", authorization);
  session = soup_session_new ();
  response_body = soup_session_send_and_read (session, msg, NULL, &error);
  if (!response_body) {
    LOG ("Failed to get storage credentials: %s", error->message);
    goto free_session;
  }

  status_code = soup_message_get_status (msg);

  if (status_code != 200) {
    LOG ("Failed to get storage credentials: %s", (const char *)g_bytes_get_data (response_body, NULL));
    goto free_session;
  }

  response = json_from_string (g_bytes_get_data (response_body, NULL), &error);
  if (error) {
    LOG ("Response is not a valid JSON: %s", error->message);
    goto free_session;
  }

  json = json_node_get_object (response);
  *storage_endpoint = g_strdup (json_object_get_string_member (json, "api_endpoint"));
  *storage_id = g_strdup (json_object_get_string_member (json, "id"));
  *storage_key = g_strdup (json_object_get_string_member (json, "key"));
  success = TRUE;

  json_node_unref (response);
free_session:
  g_object_unref (session);
  g_object_unref (msg);
  g_free (authorization);
  g_free (client_state);
  g_free (hashed_kb);
  g_free (kb);
  g_free (assertion);
  g_free (audience);
  g_free (certificate);
free_keypair:
  ephy_sync_crypto_rsa_key_pair_free (keypair);
  json_object_unref (secrets);

  return success;
}

static char *
ephy_sync_debug_send_request (const char *endpoint,
                              const char *method,
                              const char *body)
{
  SoupSession *session;
  SoupMessage *msg;
  char *response = NULL;
  char *storage_endpoint = NULL;
  char *storage_id = NULL;
  char *storage_key = NULL;
  char *url;
  guint status_code;
  g_autoptr (GBytes) response_body = NULL;

  g_assert (endpoint);
  g_assert (method);
  g_assert (g_strcmp0 (method, "PUT") || body);
  g_assert (g_strcmp0 (method, "POST") || body);

  if (!ephy_sync_debug_get_storage_credentials (&storage_endpoint,
                                                &storage_id,
                                                &storage_key)) {
    LOG ("Failed to get storage credentials.");
    return NULL;
  }

  url = g_strdup_printf ("%s/%s", storage_endpoint, endpoint);
  msg = ephy_sync_debug_prepare_soup_message (url, method, body, storage_id,
                                              (const guint8 *)storage_key,
                                              strlen (storage_key));
  session = soup_session_new ();
  response_body = soup_session_send_and_read (session, msg, NULL, NULL);
  status_code = soup_message_get_status (msg);

  if (response_body) {
    if (status_code == 200)
      response = g_strdup (g_bytes_get_data (response_body, NULL));
    else
      LOG ("Failed to send storage request: %s", (const char *)g_bytes_get_data (response_body, NULL));
  }

  g_free (url);
  g_free (storage_endpoint);
  g_free (storage_id);
  g_free (storage_key);
  g_object_unref (session);
  g_object_unref (msg);

  return response;
}

/**
 * ephy_sync_debug_view_secrets:
 *
 * Displays the sync secrets stored in the sync secret schema.
 **/
void
ephy_sync_debug_view_secrets (void)
{
  GHashTable *attributes;
  GList *result;
  GError *error = NULL;

  attributes = secret_attributes_build (EPHY_SYNC_SECRET_SCHEMA, NULL);
  result = secret_service_search_sync (NULL,
                                       EPHY_SYNC_SECRET_SCHEMA,
                                       attributes,
                                       SECRET_SEARCH_UNLOCK | SECRET_SEARCH_LOAD_SECRETS,
                                       NULL,
                                       &error);
  if (error) {
    LOG ("Error searching sync secrets: %s", error->message);
    g_error_free (error);
    goto free_attributes;
  }

  /* All sync secrets are stored as a whole, as a JSON object, therefore
   * this list would normally have only one element, but for the sake of
   * debugging purposes we iterate through all the list.
   */
#if DEVELOPER_MODE
  for (GList *l = result; l && l->data; l = l->next) {
    GHashTable *attrs = secret_item_get_attributes (result->data);
    const char *account = g_hash_table_lookup (attrs, EPHY_SYNC_SECRET_ACCOUNT_KEY);
    SecretValue *value = secret_item_get_secret (result->data);
    LOG ("Sync secrets of %s: %s", account, secret_value_get_text (value));
    secret_value_unref (value);
    g_hash_table_unref (attrs);
  }
#endif

  g_list_free_full (result, g_object_unref);
free_attributes:
  g_hash_table_unref (attributes);
}

/**
 * ephy_sync_debug_view_collection:
 * @collection: the collection name
 * @decrypt: whether the records should be displayed decrypted or not
 *
 * Displays records in @collection.
 **/
void
ephy_sync_debug_view_collection (const char *collection,
                                 gboolean    decrypt)
{
  SyncCryptoKeyBundle *bundle;
  JsonNode *node;
  JsonArray *array;
  GError *error = NULL;
  char *endpoint;
  char *response;

  g_assert (collection);

  endpoint = g_strdup_printf ("storage/%s?full=true", collection);
  response = ephy_sync_debug_send_request (endpoint, "GET", NULL);

  if (!response)
    goto free_endpoint;

  node = json_from_string (response, &error);
  if (error) {
    LOG ("Response is not a valid JSON: %s", error->message);
    g_error_free (error);
    goto free_response;
  }

  if (!decrypt) {
    char *records = json_to_string (node, TRUE);
    LOG ("%s", records);
    g_free (records);
    goto free_node;
  }

  bundle = ephy_sync_debug_get_bundle_for_collection (collection);
  if (!bundle)
    goto free_node;

  array = json_node_get_array (node);
  for (guint i = 0; i < json_array_get_length (array); i++) {
    JsonObject *json = json_array_get_object_element (array, i);
    const char *payload = json_object_get_string_member (json, "payload");
    char *record = ephy_sync_debug_decrypt_record (payload, bundle);
    LOG ("%s\n", record);
    g_free (record);
  }

  ephy_sync_crypto_key_bundle_free (bundle);
free_node:
  json_node_unref (node);
free_response:
  g_free (response);
free_endpoint:
  g_free (endpoint);
}

/**
 * ephy_sync_debug_view_record:
 * @collection: the collection name
 * @id: the record id
 * @decrypt: whether the record should be displayed decrypted or not
 *
 * Displays record with id @id from collection @collection.
 **/
void
ephy_sync_debug_view_record (const char *collection,
                             const char *id,
                             gboolean    decrypt)
{
  SyncCryptoKeyBundle *bundle;
  JsonObject *json;
  JsonNode *node;
  GError *error = NULL;
  char *id_safe;
  char *endpoint;
  char *response;
  char *record;
  const char *payload;

  g_assert (collection);
  g_assert (id);

  id_safe = g_uri_escape_string (id, NULL, TRUE);
  endpoint = g_strdup_printf ("storage/%s/%s", collection, id_safe);
  response = ephy_sync_debug_send_request (endpoint, "GET", NULL);

  if (!response)
    goto free_endpoint;

  node = json_from_string (response, &error);
  if (error) {
    LOG ("Response is not a valid JSON: %s", error->message);
    g_error_free (error);
    goto free_response;
  }

  if (!decrypt) {
    record = json_to_string (node, TRUE);
    LOG ("%s", record);
    g_free (record);
    goto free_node;
  }

  bundle = ephy_sync_debug_get_bundle_for_collection (collection);
  if (!bundle)
    goto free_node;

  json = json_node_get_object (node);
  payload = json_object_get_string_member (json, "payload");
  record = ephy_sync_debug_decrypt_record (payload, bundle);
  LOG ("%s", record);

  g_free (record);
  ephy_sync_crypto_key_bundle_free (bundle);
free_node:
  json_node_unref (node);
free_response:
  g_free (response);
free_endpoint:
  g_free (endpoint);
  g_free (id_safe);
}

/**
 * ephy_sync_debug_upload_record:
 * @collection: the collection name
 * @id: the record id
 * @record: record's JSON representation
 *
 * Upload record with id @id to collection @collection.
 **/
void
ephy_sync_debug_upload_record (const char *collection,
                               const char *id,
                               const char *record)
{
  SyncCryptoKeyBundle *bundle;
  char *id_safe;
  char *endpoint;
  char *body;
  char *response;

  g_assert (collection);
  g_assert (id);
  g_assert (record);

  bundle = ephy_sync_debug_get_bundle_for_collection (collection);
  if (!bundle)
    return;

  id_safe = g_uri_escape_string (id, NULL, TRUE);
  endpoint = g_strdup_printf ("storage/%s/%s", collection, id_safe);
  body = ephy_sync_debug_make_upload_body (id, record, bundle);
  response = ephy_sync_debug_send_request (endpoint, "PUT", body);

  LOG ("%s", response);

  g_free (id_safe);
  g_free (endpoint);
  g_free (body);
  g_free (response);
  ephy_sync_crypto_key_bundle_free (bundle);
}

/**
 * ephy_sync_debug_delete_collection:
 * @collection: the collection name
 *
 * Marks all records in @collection as deleted.
 * Contrast this with ephy_sync_debug_erase_collection(), which
 * will permanently erase the collection and its records from the storage server.
 **/
void
ephy_sync_debug_delete_collection (const char *collection)
{
  SyncCryptoKeyBundle *bundle;
  JsonNode *node;
  JsonArray *array;
  GError *error = NULL;
  char *endpoint;
  char *response;

  g_assert (collection);

  endpoint = g_strdup_printf ("storage/%s", collection);
  response = ephy_sync_debug_send_request (endpoint, "GET", NULL);
  if (!response)
    goto free_endpoint;

  node = json_from_string (response, &error);
  if (error) {
    LOG ("Response is not a valid JSON: %s", error->message);
    g_error_free (error);
    goto free_response;
  }

  bundle = ephy_sync_debug_get_bundle_for_collection (collection);
  if (!bundle)
    goto free_node;

  array = json_node_get_array (node);
  for (guint i = 0; i < json_array_get_length (array); i++) {
    const char *id = json_array_get_string_element (array, i);
    char *id_safe = g_uri_escape_string (id, NULL, TRUE);
    char *body = ephy_sync_debug_make_delete_body (id, bundle);
    char *to = g_strdup_printf ("storage/%s/%s", collection, id_safe);
    char *resp = ephy_sync_debug_send_request (to, "PUT", body);

    LOG ("%s", resp);

    g_free (id_safe);
    g_free (body);
    g_free (to);
    g_free (resp);
  }

  ephy_sync_crypto_key_bundle_free (bundle);
free_node:
  json_node_unref (node);
free_response:
  g_free (response);
free_endpoint:
  g_free (endpoint);
}

/**
 * ephy_sync_debug_delete_record:
 * @collection: the collection name
 * @id: the record id
 *
 * Marks record with id @id from collection @collection as deleted.
 * Contrast this with ephy_sync_debug_erase_record(), which
 * will permanently erase the record from the storage server.
 **/
void
ephy_sync_debug_delete_record (const char *collection,
                               const char *id)
{
  SyncCryptoKeyBundle *bundle;
  char *id_safe;
  char *endpoint;
  char *body;
  char *response;

  g_assert (collection);
  g_assert (id);

  bundle = ephy_sync_debug_get_bundle_for_collection (collection);
  if (!bundle)
    return;

  id_safe = g_uri_escape_string (id, NULL, TRUE);
  endpoint = g_strdup_printf ("storage/%s/%s", collection, id_safe);
  body = ephy_sync_debug_make_delete_body (id, bundle);
  response = ephy_sync_debug_send_request (endpoint, "PUT", body);

  LOG ("%s", response);

  g_free (id_safe);
  g_free (endpoint);
  g_free (body);
  g_free (response);
  ephy_sync_crypto_key_bundle_free (bundle);
}

/**
 * ephy_sync_debug_erase_collection:
 * @collection: the collection name
 *
 * Deletes @collection and all its contained records from the storage server.
 * Contrast this with ephy_sync_debug_delete_collection(), which will only mark
 * the records in @collection as deleted.
 *
 * Note: After executing this, @collection will no longer appear in the output
 * of GET /info/collections and calls to GET /storage/@collection will return
 * an empty list.
 **/
void
ephy_sync_debug_erase_collection (const char *collection)
{
  char *endpoint;
  char *response;

  g_assert (collection);

  endpoint = g_strdup_printf ("storage/%s", collection);
  response = ephy_sync_debug_send_request (endpoint, "DELETE", NULL);

  LOG ("%s", response);

  g_free (endpoint);
  g_free (response);
}

/**
 * ephy_sync_debug_erase_record:
 * @collection: the collection name
 * @id: the record id
 *
 * Deletes record with id @id from collection @collection from the storage server.
 * Contrast this with ephy_sync_debug_delete_record(), which will only mark
 * the record as deleted.
 *
 * Note: After executing this, the record will no longer appear in the output
 * of GET /storage/@collection and GET /storage/@collection/@id will return a
 * 404 HTTP status code.
 **/
void
ephy_sync_debug_erase_record (const char *collection,
                              const char *id)
{
  char *id_safe;
  char *endpoint;
  char *response;

  g_assert (collection);
  g_assert (id);

  id_safe = g_uri_escape_string (id, NULL, TRUE);
  endpoint = g_strdup_printf ("storage/%s/%s", collection, id_safe);
  response = ephy_sync_debug_send_request (endpoint, "DELETE", NULL);

  LOG ("%s", response);

  g_free (id_safe);
  g_free (endpoint);
  g_free (response);
}

/**
 * ephy_sync_debug_view_collection_info:
 *
 * Displays the name of each existing collection on the storage server together
 * with the last-modified time of each collection.
 **/
void
ephy_sync_debug_view_collection_info (void)
{
  char *response;

  response = ephy_sync_debug_send_request ("info/collections", "GET", NULL);
  LOG ("%s", response);

  g_free (response);
}

/**
 * ephy_sync_debug_view_quota_info:
 *
 * Displays the current usage and quota (in KB) associated with the account.
 * The second item will be null if the storage server does not enforce quotas.
 **/
void
ephy_sync_debug_view_quota_info (void)
{
  char *response;

  response = ephy_sync_debug_send_request ("info/quota", "GET", NULL);
  LOG ("%s", response);

  g_free (response);
}

/**
 * ephy_sync_debug_view_collection_usage:
 *
 * Displays the name of each existing collection on the storage server together
 * with the data volume used for each collection (in KB).
 **/
void
ephy_sync_debug_view_collection_usage (void)
{
  char *response;

  response = ephy_sync_debug_send_request ("info/collection_usage", "GET", NULL);
  LOG ("%s", response);

  g_free (response);
}

/**
 * ephy_sync_debug_view_collection_counts:
 *
 * Displays the name of each existing collection on the storage server together
 * with the number of records in each collection.
 **/
void
ephy_sync_debug_view_collection_counts (void)
{
  char *response;

  response = ephy_sync_debug_send_request ("info/collection_counts", "GET", NULL);
  LOG ("%s", response);

  g_free (response);
}

/**
 * ephy_sync_debug_view_configuration_info:
 *
 * Displays information about the configuration of the storage server regarding
 * various protocol and size limits.
 **/
void
ephy_sync_debug_view_configuration_info (void)
{
  char *response;

  response = ephy_sync_debug_send_request ("info/configuration", "GET", NULL);
  LOG ("%s", response);

  g_free (response);
}

/**
 * ephy_sync_debug_view_meta_global_record:
 *
 * Displays the meta/global record on the storage server that contains general
 * metadata to describe the state of data on the server. This includes things
 * like the global storage version and the set of available collections on the
 * server.
 **/
void
ephy_sync_debug_view_meta_global_record (void)
{
  ephy_sync_debug_view_record ("meta", "global", FALSE);
}

/**
 * ephy_sync_debug_view_crypto_keys_record:
 *
 * Displays the crypto/keys record on the storage server that contains the
 * base64-encoded key bundles used to encrypt, decrypt and verify records in
 * each collection. The crypto/keys record itself is encrypted with a key bundle
 * derived from the master sync key which is only available to the sync clients.
 **/
void
ephy_sync_debug_view_crypto_keys_record (void)
{
  SyncCryptoKeyBundle *bundle;
  JsonNode *node;
  JsonObject *secrets;
  JsonObject *json;
  GError *error = NULL;
  char *response;
  char *crypto_keys;
  const char *payload;
  const char *key_b_hex;
  guint8 *kb;

  secrets = ephy_sync_debug_load_secrets ();
  if (!secrets)
    return;

  response = ephy_sync_debug_send_request ("storage/crypto/keys", "GET", NULL);
  if (!response)
    goto free_secrets;

  node = json_from_string (response, &error);
  if (error) {
    LOG ("Response is not a valid JSON: %s", error->message);
    g_error_free (error);
    goto free_response;
  }

  json = json_node_get_object (node);
  payload = json_object_get_string_member (json, "payload");
  key_b_hex = json_object_get_string_member (secrets, "master_key");
  kb = ephy_sync_utils_decode_hex (key_b_hex);
  bundle = ephy_sync_crypto_derive_master_bundle (kb);
  crypto_keys = ephy_sync_crypto_decrypt_record (payload, bundle);

  if (!crypto_keys)
    goto free_bundle;

  LOG ("%s", crypto_keys);

  g_free (crypto_keys);
free_bundle:
  ephy_sync_crypto_key_bundle_free (bundle);
  g_free (kb);
  json_node_unref (node);
free_response:
  g_free (response);
free_secrets:
  json_object_unref (secrets);
}

/**
 * ephy_sync_debug_view_connected_devices:
 *
 * Displays the current devices connected to Firefox Sync for the signed in user.
 *
 * https://github.com/mozilla/fxa-auth-server/blob/master/docs/api.md#get-accountdevices
 **/
void
ephy_sync_debug_view_connected_devices (void)
{
  JsonObject *secrets;
  SoupSession *session;
  SoupMessage *msg;
  guint8 *id;
  guint8 *key;
  guint8 *tmp;
  char *id_hex;
  char *url;
  const char *session_token;
  g_autofree char *accounts_server = NULL;
  g_autoptr (GBytes) response_body = NULL;

  secrets = ephy_sync_debug_load_secrets ();
  if (!secrets)
    return;

  session_token = json_object_get_string_member (secrets, "session_token");
  ephy_sync_crypto_derive_session_token (session_token, &id, &key, &tmp);

  accounts_server = ephy_sync_utils_get_accounts_server ();
  url = g_strdup_printf ("%s/account/devices", accounts_server);
  id_hex = ephy_sync_utils_encode_hex (id, 32);
  msg = ephy_sync_debug_prepare_soup_message (url, "GET", NULL, id_hex, key, 32);
  session = soup_session_new ();
  response_body = soup_session_send_and_read (session, msg, NULL, NULL);

  if (response_body)
    LOG ("%s", (const char *)g_bytes_get_data (response_body, NULL));

  g_object_unref (session);
  g_object_unref (msg);
  g_free (id_hex);
  g_free (url);
  g_free (id);
  g_free (key);
  g_free (tmp);
  json_object_unref (secrets);
}

/**
 * ephy_sync_debug_get_current_device:
 *
 * Gets the current device connected to Firefox Sync for the signed in user.
 *
 * https://github.com/mozilla/fxa-auth-server/blob/master/docs/api.md#get-accountdevices
 *
 * Return value: (transfer full): the current device as a #JsonObject
 **/
JsonObject *
ephy_sync_debug_get_current_device (void)
{
  JsonObject *retval = NULL;
  JsonObject *secrets;
  JsonNode *response;
  JsonArray *array;
  SoupSession *session;
  SoupMessage *msg;
  g_autoptr (GError) error = NULL;
  guint8 *id;
  guint8 *key;
  guint8 *tmp;
  char *id_hex;
  char *url;
  const char *session_token;
  guint status_code;
  g_autofree char *accounts_server = NULL;
  g_autoptr (GBytes) response_body = NULL;

  secrets = ephy_sync_debug_load_secrets ();
  if (!secrets)
    return NULL;

  session_token = json_object_get_string_member (secrets, "session_token");
  ephy_sync_crypto_derive_session_token (session_token, &id, &key, &tmp);

  accounts_server = ephy_sync_utils_get_accounts_server ();
  url = g_strdup_printf ("%s/account/devices", accounts_server);
  id_hex = ephy_sync_utils_encode_hex (id, 32);
  msg = ephy_sync_debug_prepare_soup_message (url, "GET", NULL, id_hex, key, 32);
  session = soup_session_new ();
  response_body = soup_session_send_and_read (session, msg, NULL, &error);
  if (!response_body) {
    LOG ("Failed to GET account devices: %s", error->message);
    goto free_session;
  }

  status_code = soup_message_get_status (msg);

  if (status_code != 200) {
    LOG ("Failed to GET account devices: %s", (const char *)g_bytes_get_data (response_body, NULL));
    goto free_session;
  }

  response = json_from_string (g_bytes_get_data (response_body, NULL), &error);
  if (error) {
    LOG ("Response is not a valid JSON: %s", error->message);
    goto free_session;
  }

  array = json_node_get_array (response);
  for (guint i = 0; i < json_array_get_length (array); i++) {
    JsonObject *device = json_array_get_object_element (array, i);

    if (json_object_get_boolean_member (device, "isCurrentDevice")) {
      retval = json_object_ref (device);
      break;
    }
  }

  json_node_unref (response);
free_session:
  g_object_unref (session);
  g_object_unref (msg);
  g_free (id_hex);
  g_free (url);
  g_free (id);
  g_free (key);
  g_free (tmp);
  json_object_unref (secrets);

  return retval;
}
