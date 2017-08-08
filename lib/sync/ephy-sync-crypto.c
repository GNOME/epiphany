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
#include "ephy-sync-crypto.h"

#include "ephy-string.h"
#include "ephy-sync-utils.h"

#include <glib/gstdio.h>
#include <inttypes.h>
#include <json-glib/json-glib.h>
#include <libsoup/soup.h>
#include <nettle/cbc.h>
#include <nettle/aes.h>
#include <string.h>

#define HAWK_VERSION  1
#define NONCE_LEN     6
#define IV_LEN        16

SyncCryptoHawkOptions *
ephy_sync_crypto_hawk_options_new (const char *app,
                                   const char *dlg,
                                   const char *ext,
                                   const char *content_type,
                                   const char *hash,
                                   const char *local_time_offset,
                                   const char *nonce,
                                   const char *payload,
                                   const char *timestamp)
{
  SyncCryptoHawkOptions *options;

  options = g_slice_new (SyncCryptoHawkOptions);
  options->app = g_strdup (app);
  options->dlg = g_strdup (dlg);
  options->ext = g_strdup (ext);
  options->content_type = g_strdup (content_type);
  options->hash = g_strdup (hash);
  options->local_time_offset = g_strdup (local_time_offset);
  options->nonce = g_strdup (nonce);
  options->payload = g_strdup (payload);
  options->timestamp = g_strdup (timestamp);

  return options;
}

void
ephy_sync_crypto_hawk_options_free (SyncCryptoHawkOptions *options)
{
  g_return_if_fail (options);

  g_free (options->app);
  g_free (options->dlg);
  g_free (options->ext);
  g_free (options->content_type);
  g_free (options->hash);
  g_free (options->local_time_offset);
  g_free (options->nonce);
  g_free (options->payload);
  g_free (options->timestamp);

  g_slice_free (SyncCryptoHawkOptions, options);
}

static SyncCryptoHawkArtifacts *
ephy_sync_crypto_hawk_artifacts_new (const char *app,
                                     const char *dlg,
                                     const char *ext,
                                     const char *hash,
                                     const char *host,
                                     const char *method,
                                     const char *nonce,
                                     guint       port,
                                     const char *resource,
                                     gint64      ts)
{
  SyncCryptoHawkArtifacts *artifacts;

  artifacts = g_slice_new (SyncCryptoHawkArtifacts);
  artifacts->app = g_strdup (app);
  artifacts->dlg = g_strdup (dlg);
  artifacts->ext = g_strdup (ext);
  artifacts->hash = g_strdup (hash);
  artifacts->host = g_strdup (host);
  artifacts->method = g_strdup (method);
  artifacts->nonce = g_strdup (nonce);
  artifacts->port = g_strdup_printf ("%u", port);
  artifacts->resource = g_strdup (resource);
  artifacts->ts = g_strdup_printf ("%" PRId64, ts);

  return artifacts;
}

static void
ephy_sync_crypto_hawk_artifacts_free (SyncCryptoHawkArtifacts *artifacts)
{
  g_assert (artifacts);

  g_free (artifacts->app);
  g_free (artifacts->dlg);
  g_free (artifacts->ext);
  g_free (artifacts->hash);
  g_free (artifacts->host);
  g_free (artifacts->method);
  g_free (artifacts->nonce);
  g_free (artifacts->port);
  g_free (artifacts->resource);
  g_free (artifacts->ts);

  g_slice_free (SyncCryptoHawkArtifacts, artifacts);
}

static char *
hawk_parse_content_type (const char *content_type)
{
  char **tokens;
  char *retval;

  g_assert (content_type);

  tokens = g_strsplit (content_type, ";", -1);
  retval = g_ascii_strdown (g_strstrip (tokens[0]), -1);
  g_strfreev (tokens);

  return retval;
}

static char *
hawk_compute_payload_hash (const char *payload,
                           const char *content_type)
{
  guint8 *digest;
  char *digest_hex;
  char *content;
  char *update;
  char *hash;

  g_assert (payload);
  g_assert (content_type);

  content = hawk_parse_content_type (content_type);
  update = g_strdup_printf ("hawk.%d.payload\n%s\n%s\n",
                            HAWK_VERSION, content, payload);

  digest_hex = g_compute_checksum_for_string (G_CHECKSUM_SHA256, update, -1);
  digest = ephy_sync_utils_decode_hex (digest_hex);
  hash = g_base64_encode (digest, g_checksum_type_get_length (G_CHECKSUM_SHA256));

  g_free (content);
  g_free (update);
  g_free (digest_hex);
  g_free (digest);

  return hash;
}

static char *
hawk_append_to_header (char       *header,
                       const char *name,
                       const char *value)
{
  char *new_header;
  char *tmp;

  g_assert (header);
  g_assert (name);
  g_assert (value);

  tmp = header;
  new_header = g_strconcat (header, ", ", name, "=\"", value, "\"", NULL);
  g_free (tmp);

  return new_header;
}

static char *
hawk_normalize_string (const char              *type,
                       SyncCryptoHawkArtifacts *artifacts)
{
  char *host;
  char *info;
  char *method;
  char *n_ext = NULL;
  char *normalized;
  char *tmp;

  g_assert (type);
  g_assert (artifacts);

  info = g_strdup_printf ("hawk.%d.%s", HAWK_VERSION, type);
  method = g_ascii_strup (artifacts->method, -1);
  host = g_ascii_strdown (artifacts->host, -1);

  normalized = g_strjoin ("\n",
                          info, artifacts->ts, artifacts->nonce,
                          method, artifacts->resource, host,
                          artifacts->port, artifacts->hash ? artifacts->hash : "",
                          NULL);

  if (artifacts->ext && strlen (artifacts->ext) > 0) {
    tmp = ephy_string_find_and_replace (artifacts->ext, "\\", "\\\\");
    n_ext = ephy_string_find_and_replace (tmp, "\n", "\\n");
    g_free (tmp);
  }

  tmp = normalized;
  normalized = g_strconcat (normalized, "\n",
                            n_ext ? n_ext : "", "\n",
                            artifacts->app ? artifacts->app : "",
                            artifacts->app ? "\n" : "",
                            artifacts->app && artifacts->dlg ? artifacts->dlg : "",
                            artifacts->app && artifacts->dlg ? "\n" : "",
                            NULL);

  g_free (host);
  g_free (info);
  g_free (method);
  g_free (n_ext);
  g_free (tmp);

  return normalized;
}

static char *
hawk_compute_mac (const char              *type,
                  const guint8            *key,
                  gsize                    key_len,
                  SyncCryptoHawkArtifacts *artifacts)
{
  guint8 *digest;
  char *digest_hex;
  char *normalized;
  char *mac;

  g_assert (type);
  g_assert (key);
  g_assert (artifacts);

  /* Serialize the mac type and artifacts into a HAWK string. */
  normalized = hawk_normalize_string (type, artifacts);
  digest_hex = g_compute_hmac_for_string (G_CHECKSUM_SHA256,
                                          key, key_len,
                                          normalized, -1);
  digest = ephy_sync_utils_decode_hex (digest_hex);
  mac = g_base64_encode (digest, g_checksum_type_get_length (G_CHECKSUM_SHA256));

  g_free (normalized);
  g_free (digest_hex);
  g_free (digest);

  return mac;
}

SyncCryptoHawkHeader *
ephy_sync_crypto_hawk_header_new (const char            *url,
                                  const char            *method,
                                  const char            *id,
                                  const guint8          *key,
                                  gsize                  key_len,
                                  SyncCryptoHawkOptions *options)
{
  SyncCryptoHawkHeader *hawk_header;
  SyncCryptoHawkArtifacts *artifacts;
  SoupURI *uri;
  char *resource;
  char *hash;
  char *header;
  char *mac;
  char *nonce;
  char *payload;
  char *timestamp;
  guint8 *bytes;
  gint64 ts;

  g_return_val_if_fail (url, NULL);
  g_return_val_if_fail (method, NULL);
  g_return_val_if_fail (id, NULL);
  g_return_val_if_fail (key, NULL);

  ts = g_get_real_time () / 1000000;
  hash = options ? g_strdup (options->hash) : NULL;
  payload = options ? options->payload : NULL;
  timestamp = options ? options->timestamp : NULL;
  uri = soup_uri_new (url);
  resource = !soup_uri_get_query (uri) ? g_strdup (soup_uri_get_path (uri))
                                       : g_strconcat (soup_uri_get_path (uri),
                                                      "?",
                                                      soup_uri_get_query (uri),
                                                      NULL);

  if (options && options->nonce) {
    nonce = g_strdup (options->nonce);
  } else {
    bytes = g_malloc (NONCE_LEN / 2);
    ephy_sync_utils_generate_random_bytes (NULL, NONCE_LEN / 2, bytes);
    nonce = ephy_sync_utils_encode_hex (bytes, NONCE_LEN / 2);
    g_free (bytes);
  }

  if (timestamp) {
    char *local_time_offset;
    gint64 offset;

    local_time_offset = options ? options->local_time_offset : NULL;
    offset = local_time_offset ? g_ascii_strtoll (local_time_offset, NULL, 10) : 0;
    ts = g_ascii_strtoll (timestamp, NULL, 10) + offset;
  }

  if (!hash && payload) {
    const char *content_type = options ? options->content_type : "text/plain";

    /* Calculate hash for the given payload. */
    hash = hawk_compute_payload_hash (payload, content_type);
  }

  /* Create artifacts from options. */
  artifacts = ephy_sync_crypto_hawk_artifacts_new (options ? options->app : NULL,
                                                   options ? options->dlg : NULL,
                                                   options ? options->ext : NULL,
                                                   hash,
                                                   soup_uri_get_host (uri),
                                                   method,
                                                   nonce,
                                                   soup_uri_get_port (uri),
                                                   resource,
                                                   ts);

  header = g_strconcat ("Hawk id=\"", id, "\"",
                        ", ts=\"", artifacts->ts, "\"",
                        ", nonce=\"", artifacts->nonce, "\"",
                        NULL);

  /* Append pre-calculated payload hash if any. */
  if (artifacts->hash && strlen (artifacts->hash) > 0)
    header = hawk_append_to_header (header, "hash", artifacts->hash);

  /* Append the application specific data if any. */
  if (artifacts->ext && strlen (artifacts->ext) > 0) {
    char *h_ext;
    char *tmp_ext;

    tmp_ext = ephy_string_find_and_replace (artifacts->ext, "\\", "\\\\");
    h_ext = ephy_string_find_and_replace (tmp_ext, "\n", "\\n");
    header = hawk_append_to_header (header, "ext", h_ext);

    g_free (h_ext);
    g_free (tmp_ext);
  }

  /* Calculate and append a message authentication code (MAC). */
  mac = hawk_compute_mac ("header", key, key_len, artifacts);
  header = hawk_append_to_header (header, "mac", mac);

  /* Append the Oz application id if any. */
  if (artifacts->app) {
    header = hawk_append_to_header (header, "app", artifacts->app);

    /* Append the Oz delegated-by application id if any. */
    if (artifacts->dlg)
      header = hawk_append_to_header (header, "dlg", artifacts->dlg);
  }

  hawk_header = g_slice_new (SyncCryptoHawkHeader);
  hawk_header->header = g_strdup (header);
  hawk_header->artifacts = artifacts;

  soup_uri_free (uri);
  g_free (hash);
  g_free (mac);
  g_free (nonce);
  g_free (resource);
  g_free (header);

  return hawk_header;
}

void
ephy_sync_crypto_hawk_header_free (SyncCryptoHawkHeader *header)
{
  g_return_if_fail (header);

  g_free (header->header);
  ephy_sync_crypto_hawk_artifacts_free (header->artifacts);

  g_slice_free (SyncCryptoHawkHeader, header);
}

SyncCryptoRSAKeyPair *
ephy_sync_crypto_rsa_key_pair_new (void)
{
  SyncCryptoRSAKeyPair *key_pair;
  struct rsa_public_key public;
  struct rsa_private_key private;
  int success;

  rsa_public_key_init (&public);
  rsa_private_key_init (&private);

  /* The public exponent, usually one of the small Fermat primes 3, 5, 17, 257, 65537. */
  mpz_set_ui (public.e, 65537);

  /* Key sizes below 2048 are considered breakable and should not be used. */
  success = rsa_generate_keypair (&public, &private,
                                  NULL, ephy_sync_utils_generate_random_bytes,
                                  NULL, NULL, 2048, 0);
  /* Given correct parameters, this never fails. */
  g_assert (success);

  key_pair = g_slice_new (SyncCryptoRSAKeyPair);
  key_pair->public = public;
  key_pair->private = private;

  return key_pair;
}

void
ephy_sync_crypto_rsa_key_pair_free (SyncCryptoRSAKeyPair *key_pair)
{
  g_return_if_fail (key_pair);

  rsa_public_key_clear (&key_pair->public);
  rsa_private_key_clear (&key_pair->private);

  g_slice_free (SyncCryptoRSAKeyPair, key_pair);
}

SyncCryptoKeyBundle *
ephy_sync_crypto_key_bundle_new (const char *aes_key_b64,
                                 const char *hmac_key_b64)
{
  SyncCryptoKeyBundle *bundle;
  guint8 *aes_key;
  guint8 *hmac_key;
  gsize aes_key_len;
  gsize hmac_key_len;

  g_return_val_if_fail (aes_key_b64, NULL);
  g_return_val_if_fail (hmac_key_b64, NULL);

  aes_key = g_base64_decode (aes_key_b64, &aes_key_len);
  g_return_val_if_fail (aes_key_len == 32, NULL);
  hmac_key = g_base64_decode (hmac_key_b64, &hmac_key_len);
  g_return_val_if_fail (hmac_key_len == 32, NULL);

  bundle = g_slice_new (SyncCryptoKeyBundle);
  bundle->aes_key_hex = ephy_sync_utils_encode_hex (aes_key, aes_key_len);
  bundle->hmac_key_hex = ephy_sync_utils_encode_hex (hmac_key, hmac_key_len);

  g_free (aes_key);
  g_free (hmac_key);

  return bundle;
}

void
ephy_sync_crypto_key_bundle_free (SyncCryptoKeyBundle *bundle)
{
  g_return_if_fail (bundle);

  g_free (bundle->aes_key_hex);
  g_free (bundle->hmac_key_hex);

  g_slice_free (SyncCryptoKeyBundle, bundle);
}

static char *
ephy_sync_crypto_kw (const char *name)
{
  g_assert (name);

  /* Concatenate the given name to the Mozilla prefix.
   * See https://raw.githubusercontent.com/wiki/mozilla/fxa-auth-server/images/onepw-create.png
   */
  return g_strconcat ("identity.mozilla.com/picl/v1/", name, NULL);
}

static guint8 *
ephy_sync_crypto_concat_bytes (const guint8 *bytes,
                               gsize         len,
                               ...)
{
  va_list args;
  guint8 *next;
  guint8 *out;
  gsize next_len;
  gsize out_len;

  out_len = len;
  out = g_malloc (out_len);
  memcpy (out, bytes, out_len);

  va_start (args, len);
  while ((next = va_arg (args, guint8 *)) != NULL) {
    next_len = va_arg (args, gsize);
    out = g_realloc (out, out_len + next_len);
    memcpy (out + out_len, next, next_len);
    out_len += next_len;
  }

  va_end (args);

  return out;
}

static void
ephy_sync_crypto_hkdf (const guint8 *in,
                       gsize         in_len,
                       guint8       *salt,
                       gsize         salt_len,
                       const guint8 *info,
                       gsize         info_len,
                       guint8       *out,
                       gsize         out_len)
{
  char *prk_hex;
  char *tmp_hex;
  guint8 *tmp;
  guint8 *prk;
  guint8 *out_full;
  guint8 *data;
  guint8 counter;
  gsize hash_len;
  gsize data_len;
  gsize n;

  g_assert (in);
  g_assert (info);
  g_assert (out);

  hash_len = g_checksum_type_get_length (G_CHECKSUM_SHA256);
  g_assert (out_len <= hash_len * 255);

  /* Implementation of the HMAC-based Extract-and-Expand Key Derivation Function.
   * See https://tools.ietf.org/html/rfc5869
   */

  /* If salt value was not provided, use an array of hash_len zeros. */
  if (!salt) {
    salt = g_malloc0 (hash_len);
    salt_len = hash_len;
  }

  /* Step 1: Extract */
  prk_hex = g_compute_hmac_for_data (G_CHECKSUM_SHA256,
                                     salt, salt_len,
                                     in, in_len);
  prk = ephy_sync_utils_decode_hex (prk_hex);

  /* Step 2: Expand */
  counter = 1;
  n = (out_len + hash_len - 1) / hash_len;
  out_full = g_malloc (n * hash_len);

  for (gsize i = 0; i < n; i++, counter++) {
    if (i == 0) {
      data = ephy_sync_crypto_concat_bytes (info, info_len, &counter, 1, NULL);
      data_len = info_len + 1;
    } else {
      data = ephy_sync_crypto_concat_bytes (out_full + (i - 1) * hash_len, hash_len,
                                            info, info_len,
                                            &counter, 1,
                                            NULL);
      data_len = hash_len + info_len + 1;
    }

    tmp_hex = g_compute_hmac_for_data (G_CHECKSUM_SHA256,
                                       prk, hash_len,
                                       data, data_len);
    tmp = ephy_sync_utils_decode_hex (tmp_hex);
    memcpy (out_full + i * hash_len, tmp, hash_len);

    g_free (data);
    g_free (tmp);
    g_free (tmp_hex);
  }

  memcpy (out, out_full, out_len);

  g_free (prk_hex);
  g_free (salt);
  g_free (prk);
  g_free (out_full);
}

void
ephy_sync_crypto_derive_session_token (const char  *session_token,
                                       guint8     **token_id,
                                       guint8     **req_hmac_key,
                                       guint8     **request_key)
{
  guint8 *token;
  guint8 *out;
  char *info;
  gsize len = 32; /* sessionToken is always 32 bytes. */

  g_return_if_fail (session_token);
  g_return_if_fail (token_id);
  g_return_if_fail (req_hmac_key);
  g_return_if_fail (request_key);

  token = ephy_sync_utils_decode_hex (session_token);
  info = ephy_sync_crypto_kw ("sessionToken");
  out = g_malloc (3 * len);

  /* Use the sessionToken to derive tokenID, reqHMACkey and requestKey. */
  ephy_sync_crypto_hkdf (token, len,
                         NULL, 0,
                         (guint8 *)info, strlen (info),
                         out, 3 * len);

  *token_id = g_malloc (len);
  *req_hmac_key = g_malloc (len);
  *request_key = g_malloc (len);
  memcpy (*token_id, out, len);
  memcpy (*req_hmac_key, out + len, len);
  memcpy (*request_key, out + 2 * len, len);

  g_free (token);
  g_free (out);
  g_free (info);
}

void
ephy_sync_crypto_derive_key_fetch_token (const char  *key_fetch_token,
                                         guint8     **token_id,
                                         guint8     **req_hmac_key,
                                         guint8     **resp_hmac_key,
                                         guint8     **resp_xor_key)
{
  guint8 *kft;
  guint8 *out1;
  guint8 *out2;
  guint8 *key_request_key;
  char *info_kft;
  char *info_keys;
  gsize len = 32; /* keyFetchToken is always 32 bytes. */

  g_return_if_fail (key_fetch_token);
  g_return_if_fail (token_id);
  g_return_if_fail (req_hmac_key);
  g_return_if_fail (resp_hmac_key);
  g_return_if_fail (resp_xor_key);

  kft = ephy_sync_utils_decode_hex (key_fetch_token);
  info_kft = ephy_sync_crypto_kw ("keyFetchToken");
  info_keys = ephy_sync_crypto_kw ("account/keys");
  out1 = g_malloc (3 * len);
  out2 = g_malloc (3 * len);

  /* Use the keyFetchToken to derive tokenID, reqHMACkey and keyRequestKey. */
  ephy_sync_crypto_hkdf (kft, len,
                         NULL, 0,
                         (guint8 *)info_kft, strlen (info_kft),
                         out1, 3 * len);

  *token_id = g_malloc (len);
  *req_hmac_key = g_malloc (len);
  key_request_key = g_malloc (len);
  memcpy (*token_id, out1, len);
  memcpy (*req_hmac_key, out1 + len, len);
  memcpy (key_request_key, out1 + 2 * len, len);

  /* Use the keyRequestKey to derive respHMACkey and respXORkey. */
  ephy_sync_crypto_hkdf (key_request_key, len,
                         NULL, 0,
                         (guint8 *)info_keys, strlen (info_keys),
                         out2, 3 * len);

  *resp_hmac_key = g_malloc (len);
  *resp_xor_key = g_malloc (2 * len);
  memcpy (*resp_hmac_key, out2, len);
  memcpy (*resp_xor_key, out2 + len, 2 * len);

  g_free (kft);
  g_free (out1);
  g_free (out2);
  g_free (info_kft);
  g_free (info_keys);
  g_free (key_request_key);
}

static guint8 *
ephy_sync_crypto_xor_bytes (const guint8 *a,
                            const guint8 *b,
                            gsize         len)
{
  guint8 *xored;

  g_assert (a);
  g_assert (b);

  xored = g_malloc (len);
  for (gsize i = 0; i < len; i++)
    xored[i] = a[i] ^ b[i];

  return xored;
}

static gboolean
ephy_sync_crypto_compate_bytes (const guint8 *a,
                                const guint8 *b,
                                gsize         len)
{
  g_assert (a);
  g_assert (b);

  for (gsize i = 0; i < len; i++)
    if (a[i] != b[i])
      return FALSE;

  return TRUE;
}

gboolean
ephy_sync_crypto_derive_master_keys (const char    *bundle_hex,
                                     const guint8  *resp_hmac_key,
                                     const guint8  *resp_xor_key,
                                     const guint8  *unwrap_kb,
                                     guint8       **ka,
                                     guint8       **kb)
{
  guint8 *bundle;
  guint8 *ciphertext;
  guint8 *resp_hmac;
  guint8 *resp_hmac_2;
  guint8 *xored;
  guint8 *wrap_kb;
  char *resp_hmac_2_hex;
  gboolean retval = TRUE;
  gsize len = 32; /* The master sync keys are always 32 bytes. */

  g_return_val_if_fail (bundle_hex, FALSE);
  g_return_val_if_fail (resp_hmac_key, FALSE);
  g_return_val_if_fail (resp_xor_key, FALSE);
  g_return_val_if_fail (unwrap_kb, FALSE);
  g_return_val_if_fail (ka, FALSE);
  g_return_val_if_fail (kb, FALSE);

  bundle = ephy_sync_utils_decode_hex (bundle_hex);
  ciphertext = g_malloc (2 * len);
  resp_hmac = g_malloc (len);

  /* Compute the MAC and compare it to the expected value. */
  memcpy (ciphertext, bundle, 2 * len);
  memcpy (resp_hmac, bundle + 2 * len, len);
  resp_hmac_2_hex = g_compute_hmac_for_data (G_CHECKSUM_SHA256,
                                             resp_hmac_key, len,
                                             ciphertext, 2 * len);
  resp_hmac_2 = ephy_sync_utils_decode_hex (resp_hmac_2_hex);
  if (!ephy_sync_crypto_compate_bytes (resp_hmac, resp_hmac_2, len)) {
    g_warning ("HMAC values differs from the one expected");
    retval = FALSE;
    goto out;
  }

  /* XOR the extracted ciphertext with the respXORkey, then split into the
   * separate kA and wrap(kB) values.
   */
  xored = ephy_sync_crypto_xor_bytes (ciphertext, resp_xor_key, 2 * len);
  *ka = g_malloc (len);
  memcpy (*ka, xored, len);
  wrap_kb = g_malloc (len);
  memcpy (wrap_kb, xored + len, len);
  /* XOR wrap(kB) with unwrapBKey to obtain kB. There is no MAC on wrap(kB). */
  *kb = ephy_sync_crypto_xor_bytes (unwrap_kb, wrap_kb, len);

  g_free (wrap_kb);
  g_free (xored);
out:
  g_free (resp_hmac_2);
  g_free (resp_hmac_2_hex);
  g_free (resp_hmac);
  g_free (ciphertext);
  g_free (bundle);

  return retval;
}

SyncCryptoKeyBundle *
ephy_sync_crypto_derive_master_bundle (const guint8 *key)
{
  SyncCryptoKeyBundle *bundle;
  guint8 *salt;
  guint8 *prk;
  guint8 *tmp;
  guint8 *aes_key;
  char *prk_hex;
  char *aes_key_hex;
  char *hmac_key_hex;
  const char *info = "identity.mozilla.com/picl/v1/oldsync";
  gsize len = 32; /* kB is always 32 bytes. */

  g_return_val_if_fail (key, NULL);

  /* Perform a two step HKDF with an all-zeros salt.
   * T(1) will represent the AES key, T(2) will represent the HMAC key.
   */
  salt = g_malloc0 (len);
  prk_hex = g_compute_hmac_for_data (G_CHECKSUM_SHA256,
                                     salt, len,
                                     key, len);
  prk = ephy_sync_utils_decode_hex (prk_hex);
  tmp = ephy_sync_crypto_concat_bytes ((guint8 *)info, strlen (info),
                                       "\x01", 1,
                                       NULL);
  aes_key_hex = g_compute_hmac_for_data (G_CHECKSUM_SHA256,
                                         prk, len,
                                         tmp, strlen (info) + 1);
  aes_key = ephy_sync_utils_decode_hex (aes_key_hex);
  g_free (tmp);
  tmp = ephy_sync_crypto_concat_bytes (aes_key, len,
                                       (guint8 *)info, strlen (info),
                                       "\x02", 1,
                                       NULL);
  hmac_key_hex = g_compute_hmac_for_data (G_CHECKSUM_SHA256,
                                          prk, len,
                                          tmp, len + strlen (info) + 1);

  bundle = g_slice_new (SyncCryptoKeyBundle);
  bundle->aes_key_hex = g_strdup (aes_key_hex);
  bundle->hmac_key_hex = g_strdup (hmac_key_hex);

  g_free (hmac_key_hex);
  g_free (tmp);
  g_free (aes_key_hex);
  g_free (prk);
  g_free (prk_hex);
  g_free (salt);

  return bundle;
}

char *
ephy_sync_crypto_generate_crypto_keys (void)
{
  JsonNode *node;
  JsonObject *object;
  JsonArray *array;
  guint8 *aes_key;
  guint8 *hmac_key;
  char *aes_key_b64;
  char *hmac_key_b64;
  char *payload;
  gsize len = 32; /* Crypto keys are always 32 bytes. */

  aes_key = g_malloc (len);
  ephy_sync_utils_generate_random_bytes (NULL, len, aes_key);
  aes_key_b64 = g_base64_encode (aes_key, len);
  hmac_key = g_malloc (len);
  ephy_sync_utils_generate_random_bytes (NULL, len, hmac_key);
  hmac_key_b64 = g_base64_encode (hmac_key, len);

  node = json_node_new (JSON_NODE_OBJECT);
  object = json_object_new ();
  array = json_array_new ();
  json_array_add_string_element (array, aes_key_b64);
  json_array_add_string_element (array, hmac_key_b64);
  json_object_set_array_member (object, "default", array);
  json_object_set_object_member (object, "collections", json_object_new ());
  json_object_set_string_member (object, "collection", "crypto");
  json_object_set_string_member (object, "id", "keys");
  json_node_set_object (node, object);
  payload = json_to_string (node, FALSE);

  json_object_unref (object);
  json_node_unref (node);
  g_free (hmac_key_b64);
  g_free (hmac_key);
  g_free (aes_key_b64);
  g_free (aes_key);

  return payload;
}

char *
ephy_sync_crypto_create_assertion (const char           *certificate,
                                   const char           *audience,
                                   guint64               seconds,
                                   SyncCryptoRSAKeyPair *key_pair)
{
  mpz_t signature;
  const char *header = "{\"alg\": \"RS256\"}";
  char *body;
  char *body_b64;
  char *header_b64;
  char *to_sign;
  char *sig_b64;
  char *assertion;
  char *digest_hex;
  guint8 *digest;
  guint8 *sig;
  guint64 expires_at;
  gsize expected_size;
  gsize count;
  int success;

  g_return_val_if_fail (certificate, NULL);
  g_return_val_if_fail (audience, NULL);
  g_return_val_if_fail (key_pair, NULL);

  /* Encode the header and body to base64 url safe and join them. */
  expires_at = g_get_real_time () / 1000 + seconds * 1000;
  body = g_strdup_printf ("{\"exp\": %lu, \"aud\": \"%s\"}", expires_at, audience);
  body_b64 = ephy_sync_utils_base64_urlsafe_encode ((guint8 *)body, strlen (body), TRUE);
  header_b64 = ephy_sync_utils_base64_urlsafe_encode ((guint8 *)header, strlen (header), TRUE);
  to_sign = g_strdup_printf ("%s.%s", header_b64, body_b64);

  /* Compute the SHA256 hash of the message to be signed. */
  digest_hex = g_compute_checksum_for_string (G_CHECKSUM_SHA256, to_sign, -1);
  digest = ephy_sync_utils_decode_hex (digest_hex);

  /* Use the provided key pair to RSA sign the message. */
  mpz_init (signature);
  success = rsa_sha256_sign_digest_tr (&key_pair->public, &key_pair->private,
                                       NULL, ephy_sync_utils_generate_random_bytes,
                                       digest, signature);
  /* Given correct parameters, this never fails. */
  g_assert (success);

  expected_size = (mpz_sizeinbase (signature, 2) + 7) / 8;
  sig = g_malloc (expected_size);
  mpz_export (sig, &count, 1, sizeof (guint8), 0, 0, signature);
  /* Given correct parameters, this never fails. */
  g_assert (count == expected_size);

  /* Join certificate, header, body and signed message to create the assertion. */
  sig_b64 = ephy_sync_utils_base64_urlsafe_encode (sig, count, TRUE);
  assertion = g_strdup_printf ("%s~%s.%s.%s", certificate, header_b64, body_b64, sig_b64);

  g_free (body);
  g_free (body_b64);
  g_free (header_b64);
  g_free (to_sign);
  g_free (sig_b64);
  g_free (sig);
  g_free (digest_hex);
  g_free (digest);
  mpz_clear (signature);

  return assertion;
}

static guint8 *
ephy_sync_crypto_pad (const char *text,
                      gsize       block_len,
                      gsize      *out_len)
{
  guint8 *out;
  gsize text_len = strlen (text);

  g_assert (text);
  g_assert (out_len);

  *out_len = text_len + block_len - text_len % block_len;
  out = g_malloc (*out_len);
  memset (out, block_len - text_len % block_len, *out_len);
  memcpy (out, text, text_len);

  return out;
}

static guint8 *
ephy_sync_crypto_aes_256_encrypt (const char   *text,
                                  const guint8 *key,
                                  const guint8 *iv,
                                  gsize        *out_len)
{
  guint8 *padded;
  guint8 *encrypted;
  gsize padded_len;
  struct CBC_CTX(struct aes256_ctx, AES_BLOCK_SIZE) ctx;

  g_assert (text);
  g_assert (key);
  g_assert (iv);
  g_assert (out_len);

  padded = ephy_sync_crypto_pad (text, AES_BLOCK_SIZE, &padded_len);
  encrypted = g_malloc (padded_len);

  aes256_set_encrypt_key(&ctx.ctx, key);
  CBC_SET_IV(&ctx, iv);
  CBC_ENCRYPT(&ctx, aes256_encrypt, padded_len, encrypted, padded);

  *out_len = padded_len;
  g_free (padded);

  return encrypted;
}

char *
ephy_sync_crypto_encrypt_record (const char          *cleartext,
                                 SyncCryptoKeyBundle *bundle)
{
  JsonNode *node;
  JsonObject *object;
  char *payload;
  char *iv_b64;
  char *ciphertext_b64;
  char *hmac;
  guint8 *aes_key;
  guint8 *hmac_key;
  guint8 *ciphertext;
  guint8 *iv;
  gsize ciphertext_len;

  g_return_val_if_fail (cleartext, NULL);
  g_return_val_if_fail (bundle, NULL);

  /* Get the encryption key and the HMAC key. */
  aes_key = ephy_sync_utils_decode_hex (bundle->aes_key_hex);
  hmac_key = ephy_sync_utils_decode_hex (bundle->hmac_key_hex);

  /* Generate a random 16 bytes initialization vector. */
  iv = g_malloc (IV_LEN);
  ephy_sync_utils_generate_random_bytes (NULL, IV_LEN, iv);

  /* Encrypt the record using the AES key. */
  ciphertext = ephy_sync_crypto_aes_256_encrypt (cleartext, aes_key,
                                                 iv, &ciphertext_len);
  ciphertext_b64 = g_base64_encode (ciphertext, ciphertext_len);
  iv_b64 = g_base64_encode (iv, IV_LEN);
  /* SHA256 expects a 32 bytes key. */
  hmac = g_compute_hmac_for_string (G_CHECKSUM_SHA256,
                                    hmac_key, 32,
                                    ciphertext_b64, -1);

  node = json_node_new (JSON_NODE_OBJECT);
  object = json_object_new ();
  json_object_set_string_member (object, "ciphertext", ciphertext_b64);
  json_object_set_string_member (object, "IV", iv_b64);
  json_object_set_string_member (object, "hmac", hmac);
  json_node_set_object (node, object);
  payload = json_to_string (node, FALSE);

  json_object_unref (object);
  json_node_unref (node);
  g_free (hmac);
  g_free (iv_b64);
  g_free (ciphertext_b64);
  g_free (ciphertext);
  g_free (iv);
  g_free (aes_key);
  g_free (hmac_key);

  return payload;
}

static gboolean
ephy_sync_crypto_hmac_is_valid (const char   *text,
                                const guint8 *key,
                                const char   *expected)
{
  char *hmac;
  gboolean retval;

  g_assert (text);
  g_assert (key);
  g_assert (expected);

  /* SHA256 expects a 32 bytes key. */
  hmac = g_compute_hmac_for_string (G_CHECKSUM_SHA256, key, 32, text, -1);
  retval = g_strcmp0 (hmac, expected) == 0;
  g_free (hmac);

  return retval;
}

static char *
ephy_sync_crypto_unpad (const guint8 *data,
                        gsize         data_len,
                        gsize         block_len)
{
  char *out;
  gsize out_len;
  gsize padding = data[data_len - 1];

  g_assert (data);

  if (padding >= 1 && padding <= block_len)
    out_len = data_len - padding;
  else
    out_len = data_len;

  out = g_malloc0 (out_len + 1);
  memcpy (out, data, out_len);

  return out;
}

static char *
ephy_sync_crypto_aes_256_decrypt (const guint8 *data,
                                  gsize         data_len,
                                  const guint8 *key,
                                  const guint8 *iv)
{
  guint8 *decrypted;
  char *unpadded;
  struct CBC_CTX(struct aes256_ctx, AES_BLOCK_SIZE) ctx;

  g_assert (data);
  g_assert (key);
  g_assert (iv);

  decrypted = g_malloc (data_len);

  aes256_set_decrypt_key (&ctx.ctx, key);
  CBC_SET_IV (&ctx, iv);
  CBC_DECRYPT (&ctx, aes256_decrypt, data_len, decrypted, data);

  unpadded = ephy_sync_crypto_unpad (decrypted, data_len, AES_BLOCK_SIZE);
  g_free (decrypted);

  return unpadded;
}

char *
ephy_sync_crypto_decrypt_record (const char          *payload,
                                 SyncCryptoKeyBundle *bundle)
{
  JsonNode *node = NULL;
  JsonObject *json = NULL;
  GError *error = NULL;
  guint8 *aes_key = NULL;
  guint8 *hmac_key = NULL;
  guint8 *ciphertext = NULL;
  guint8 *iv = NULL;
  char *cleartext = NULL;
  const char *ciphertext_b64;
  const char *iv_b64;
  const char *hmac;
  gsize ciphertext_len;
  gsize iv_len;

  g_return_val_if_fail (payload, NULL);
  g_return_val_if_fail (bundle, NULL);

  /* Extract ciphertext, iv and hmac from payload. */
  node = json_from_string (payload, &error);
  if (error) {
    g_warning ("Payload is not a valid JSON: %s", error->message);
    goto out;
  }
  json = json_node_get_object (node);
  if (!json) {
    g_warning ("JSON node does not hold a JSON object");
    goto out;
  }
  ciphertext_b64 = json_object_get_string_member (json, "ciphertext");
  iv_b64 = json_object_get_string_member (json, "IV");
  hmac = json_object_get_string_member (json, "hmac");
  if (!ciphertext_b64 || !iv_b64 || !hmac) {
    g_warning ("JSON object has missing or invalid members");
    goto out;
  }

  /* Get the encryption key and the HMAC key. */
  aes_key = ephy_sync_utils_decode_hex (bundle->aes_key_hex);
  hmac_key = ephy_sync_utils_decode_hex (bundle->hmac_key_hex);

  /* Under no circumstances should a client try to decrypt a record
   * if the HMAC verification fails.
   */
  if (!ephy_sync_crypto_hmac_is_valid (ciphertext_b64, hmac_key, hmac)) {
    g_warning ("Incorrect HMAC value");
    goto out;
  }

  /* Finally, decrypt the record. */
  ciphertext = g_base64_decode (ciphertext_b64, &ciphertext_len);
  iv = g_base64_decode (iv_b64, &iv_len);
  cleartext = ephy_sync_crypto_aes_256_decrypt (ciphertext, ciphertext_len,
                                                aes_key, iv);

out:
  g_free (ciphertext);
  g_free (iv);
  g_free (aes_key);
  g_free (hmac_key);
  if (node)
    json_node_unref (node);
  if (error)
    g_error_free (error);

  return cleartext;
}
