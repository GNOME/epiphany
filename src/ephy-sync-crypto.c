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

#include "ephy-sync-crypto.h"

#include <libsoup/soup.h>
#include <nettle/hmac.h>
#include <nettle/pbkdf2.h>
#include <nettle/sha2.h>
#include <string.h>

#define HAWK_VERSION  1

static EphySyncCryptoHawkHeader *
ephy_sync_crypto_hawk_header_new (gchar                       *header,
                                  EphySyncCryptoHawkArtifacts *artifacts)
{
  EphySyncCryptoHawkHeader *hawk_header;

  hawk_header = g_slice_new (EphySyncCryptoHawkHeader);
  hawk_header->header = header;
  hawk_header->artifacts = artifacts;

  return hawk_header;
}

static EphySyncCryptoHawkArtifacts *
ephy_sync_crypto_hawk_artifacts_new (gchar *app,
                                     gchar *dlg,
                                     gchar *ext,
                                     gchar *hash,
                                     gchar *host,
                                     gchar *method,
                                     gchar *nonce,
                                     gchar *port,
                                     gchar *resource,
                                     gchar *ts)
{
  EphySyncCryptoHawkArtifacts *hawk_artifacts;

  hawk_artifacts = g_slice_new (EphySyncCryptoHawkArtifacts);
  hawk_artifacts->app = app;
  hawk_artifacts->dlg = dlg;
  hawk_artifacts->ext = ext;
  hawk_artifacts->hash = hash;
  hawk_artifacts->host = host;
  hawk_artifacts->method = method;
  hawk_artifacts->nonce = nonce;
  hawk_artifacts->port = port;
  hawk_artifacts->resource = resource;
  hawk_artifacts->ts = ts;

  return hawk_artifacts;
}

static void
ephy_sync_crypto_hawk_artifacts_free (EphySyncCryptoHawkArtifacts *hawk_artifacts)
{
  g_free (hawk_artifacts->app);
  g_free (hawk_artifacts->dlg);
  g_free (hawk_artifacts->ext);
  g_free (hawk_artifacts->hash);
  g_free (hawk_artifacts->host);
  g_free (hawk_artifacts->method);
  g_free (hawk_artifacts->nonce);
  g_free (hawk_artifacts->port);
  g_free (hawk_artifacts->resource);
  g_free (hawk_artifacts->ts);

  g_slice_free (EphySyncCryptoHawkArtifacts, hawk_artifacts);
}

static gchar *
generate_random_string (gsize length)
{
  guchar *bytes;
  gchar *base64_string;
  gchar *string;

  bytes = g_malloc (length);
  for (gsize i = 0; i < length; i++)
    bytes[i] = g_random_int ();

  base64_string = g_base64_encode (bytes, length);
  string = g_strndup (base64_string, length);

  g_free (bytes);
  g_free (base64_string);

  return string;
}

static gchar *
find_and_replace_string (const gchar *src,
                         const gchar *find,
                         const gchar *repl)
{
  const gchar *haystack = src;
  const gchar *needle = NULL;
  gsize haystack_length = strlen (src);
  gsize find_length = strlen (find);
  gsize repl_length = strlen (repl);
  gsize new_length = 0;
  gsize skip_length = 0;
  gchar *new = g_malloc (haystack_length + 1);

  while ((needle = g_strstr_len (haystack, -1, find)) != NULL) {
    haystack_length += find_length - repl_length;
    new = g_realloc (new, haystack_length + 1);
    skip_length = needle - haystack;
    memcpy (new + new_length, haystack, skip_length);
    memcpy (new + new_length + skip_length, repl, repl_length);
    new_length += skip_length + repl_length;
    haystack = needle + find_length;
  }
  strcpy (new + new_length, haystack);

  return new;
}

static gchar *
normalize_string (const gchar                 *mac_type,
                  EphySyncCryptoHawkArtifacts *artifacts)
{
  gchar *host;
  gchar *info;
  gchar *method;
  gchar *n_ext = NULL;
  gchar *normalized;
  gchar *tmp;

  g_return_val_if_fail (mac_type, NULL);
  g_return_val_if_fail (artifacts, NULL);

  info = g_strdup_printf ("hawk.%d.%s", HAWK_VERSION, mac_type);
  method = g_ascii_strup (artifacts->method, -1);
  host = g_ascii_strdown (artifacts->host, -1);

  normalized = g_strjoin ("\n",
                          info,
                          artifacts->ts,
                          artifacts->nonce,
                          method,
                          artifacts->resource,
                          host,
                          artifacts->port,
                          artifacts->hash ? artifacts->hash : "",
                          NULL);

  if (artifacts->ext && strlen (artifacts->ext) > 0) {
    tmp = find_and_replace_string (artifacts->ext, "\\", "\\\\");
    n_ext = find_and_replace_string (tmp, "\n", "\\n");
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

static gchar *
parse_content_type (const gchar *content_type)
{
  gchar **tokens;
  gchar *retval;

  tokens = g_strsplit (content_type, ";", -1);
  retval = g_ascii_strdown (g_strstrip (tokens[0]), -1);
  g_strfreev (tokens);

  return retval;
}

static gchar *
calculate_payload_hash (const gchar *payload,
                        const gchar *content_type)
{
  struct sha256_ctx ctx;
  guint8 *digest;
  gchar *content;
  gchar *update;
  gchar *retval;

  g_return_val_if_fail (payload, NULL);
  g_return_val_if_fail (content_type, NULL);

  content = parse_content_type (content_type);
  update = g_strdup_printf ("hawk.%d.payload\n%s\n%s\n",
                            HAWK_VERSION,
                            content,
                            payload);
  digest = g_malloc (SHA256_DIGEST_SIZE);

  sha256_init (&ctx);
  sha256_update (&ctx, strlen (update), (guint8 *) update);
  sha256_digest (&ctx, SHA256_DIGEST_SIZE, digest);
  retval = g_base64_encode (digest, SHA256_DIGEST_SIZE);

  g_free (content);
  g_free (update);
  g_free (digest);

  return retval;
}

static gchar *
calculate_mac (const gchar                 *mac_type,
               guint8                      *key,
               gsize                        key_length,
               EphySyncCryptoHawkArtifacts *artifacts)
{
  struct hmac_sha256_ctx ctx;
  guint8 *digest;
  gchar *normalized;
  gchar *mac;

  g_return_val_if_fail (mac_type, NULL);
  g_return_val_if_fail (key, NULL);
  g_return_val_if_fail (artifacts, NULL);

  normalized = normalize_string (mac_type, artifacts);
  digest = g_malloc (SHA256_DIGEST_SIZE);

  hmac_sha256_set_key (&ctx, key_length, key);
  hmac_sha256_update (&ctx, strlen (normalized), (guint8 *) normalized);
  hmac_sha256_digest (&ctx, SHA256_DIGEST_SIZE, digest);
  mac = g_base64_encode (digest, SHA256_DIGEST_SIZE);

  g_free (normalized);
  g_free (digest);

  return mac;
}

static gchar *
append_token_to_header (gchar       *header,
                        const gchar *token_name,
                        gchar       *token_value)
{
  gchar *new_header;
  gchar *tmp;

  tmp = header;
  new_header = g_strconcat (header, ", ",
                            token_name, "=\"",
                            token_value, "\"",
                            NULL);
  g_free (tmp);

  return new_header;
}

EphySyncCryptoHawkOptions *
ephy_sync_crypto_hawk_options_new (gchar *app,
                                   gchar *dlg,
                                   gchar *ext,
                                   gchar *content_type,
                                   gchar *hash,
                                   gchar *local_time_offset,
                                   gchar *nonce,
                                   gchar *payload,
                                   gchar *timestamp)
{
  EphySyncCryptoHawkOptions *hawk_options;

  hawk_options = g_slice_new (EphySyncCryptoHawkOptions);
  hawk_options->app = app;
  hawk_options->dlg = dlg;
  hawk_options->ext = ext;
  hawk_options->content_type = content_type;
  hawk_options->hash = hash;
  hawk_options->local_time_offset = local_time_offset;
  hawk_options->nonce = nonce;
  hawk_options->payload = payload;
  hawk_options->timestamp = timestamp;

  return hawk_options;
}

void
ephy_sync_crypto_hawk_options_free (EphySyncCryptoHawkOptions *hawk_options)
{
  g_free (hawk_options->app);
  g_free (hawk_options->dlg);
  g_free (hawk_options->ext);
  g_free (hawk_options->content_type);
  g_free (hawk_options->hash);
  g_free (hawk_options->local_time_offset);
  g_free (hawk_options->nonce);
  g_free (hawk_options->payload);
  g_free (hawk_options->timestamp);

  g_slice_free (EphySyncCryptoHawkOptions, hawk_options);
}

void
ephy_sync_crypto_hawk_header_free (EphySyncCryptoHawkHeader *hawk_header)
{
  g_free (hawk_header->header);
  ephy_sync_crypto_hawk_artifacts_free (hawk_header->artifacts);

  g_slice_free (EphySyncCryptoHawkHeader, hawk_header);
}

/*
 * Runs 1000 iterations of PBKDF2.
 * Uses sha256 as hash function.
 */
void
ephy_sync_crypto_pbkdf2_1k (guint8 *key,
                            gsize   key_length,
                            guint8 *salt,
                            gsize   salt_length,
                            guint8 *out,
                            gsize   out_length)
{
  pbkdf2_hmac_sha256 (key_length, key, 1000, salt_length, salt, out_length, out);
}

/*
 * HMAC-based Extract-and-Expand Key Derivation Function.
 * Uses sha256 as hash function.
 * https://tools.ietf.org/html/rfc5869
 */
void
ephy_sync_crypto_hkdf (guint8 *in,
                       gsize   in_length,
                       guint8 *salt,
                       gsize   salt_length,
                       guint8 *info,
                       gsize   info_length,
                       guint8 *out,
                       gsize   out_length)
{
  struct hmac_sha256_ctx ctx;
  const gsize hash_length = 32;
  gsize i, offset = 0;
  guint8 *tmp, *prk;
  guint8 counter;

  if (out_length > hash_length * 255)
    return;

  /* If salt value was not provided, use an array of hash_length zeros */
  if (salt == NULL) {
    salt = g_malloc0 (hash_length);
    salt_length = hash_length;
  }

  tmp = g_malloc0 (hash_length + info_length + 1);
  prk = g_malloc0 (hash_length);

  /* Step 1: Extract */
  hmac_sha256_set_key (&ctx, salt_length, salt);
  hmac_sha256_update (&ctx, in_length, in);
  hmac_sha256_digest (&ctx, hash_length, prk);

  /* Step 2: Expand */
  hmac_sha256_set_key (&ctx, hash_length, prk);

  for (i = 0, counter = 1; i < out_length; i += hash_length, counter++) {
    memcpy (tmp + offset, info, info_length);
    tmp[offset + info_length] = counter;

    hmac_sha256_update (&ctx, offset + info_length + 1, tmp);
    hmac_sha256_digest (&ctx, hash_length, tmp);

    offset = hash_length;

    memcpy (out + i, tmp, hash_length);
  }

  g_free (salt);
  g_free (tmp);
  g_free (prk);
}

EphySyncCryptoHawkHeader *
ephy_sync_crypto_compute_hawk_header (const gchar               *url,
                                      const gchar               *method,
                                      const gchar               *id,
                                      guint8                    *key,
                                      gsize                      key_length,
                                      EphySyncCryptoHawkOptions *options)
{
  EphySyncCryptoHawkArtifacts *artifacts;
  SoupURI *uri;
  gboolean has_options;
  const gchar *hostname;
  const gchar *resource;
  gchar *hash;
  gchar *header;
  gchar *mac;
  gchar *nonce;
  gchar *payload;
  gchar *timestamp;
  gint64 ts;
  guint port;

  g_return_val_if_fail (url && strlen (url) > 0, NULL);
  g_return_val_if_fail (method && strlen (method) > 0, NULL);
  g_return_val_if_fail (id && strlen (id) > 0, NULL);
  g_return_val_if_fail (key, NULL);

  has_options = options != NULL;
  ts = g_get_real_time () / 1000000;

  timestamp = has_options ? options->timestamp : NULL;
  if (timestamp) {
    gchar *local_time_offset;
    gint64 offset;

    local_time_offset = has_options ? options->local_time_offset : NULL;
    offset = local_time_offset ? g_ascii_strtoll (local_time_offset, NULL, 10) : 0;

    ts = g_ascii_strtoll (timestamp, NULL, 10) + offset;
  }

  nonce = has_options ? options->nonce : NULL;
  nonce = nonce ? nonce : generate_random_string (6);
  hash = has_options ? options->hash : NULL;
  payload = has_options ? options->payload : NULL;

  uri = soup_uri_new (url);
  g_return_val_if_fail (uri, NULL);
  hostname = soup_uri_get_host (uri);
  port = soup_uri_get_port (uri);
  resource = soup_uri_get_path (uri);

  if (!hash && payload) {
    const gchar *content_type;

    content_type = has_options ? options->content_type : "text/plain";
    hash = calculate_payload_hash (payload, content_type);
  }

  artifacts = ephy_sync_crypto_hawk_artifacts_new (has_options ? options->app : NULL,
                                                   has_options ? options->dlg : NULL,
                                                   has_options ? options->ext : NULL,
                                                   hash,
                                                   g_strdup (hostname),
                                                   g_strdup (method),
                                                   nonce,
                                                   g_strdup_printf ("%u", port),
                                                   g_strdup (resource),
                                                   g_strdup_printf ("%ld", ts));

  mac = calculate_mac ("header", key, key_length, artifacts);

  header = g_strconcat ("Hawk id=\"", id, "\"",
                        ", ts=\"", artifacts->ts, "\"",
                        ", nonce=\"", artifacts->nonce, "\"",
                        NULL);

  if (artifacts->hash && strlen (artifacts->hash) > 0)
    header = append_token_to_header (header, "hash", artifacts->hash);

  if (artifacts->ext && strlen (artifacts->ext) > 0) {
    gchar *h_ext;
    gchar *tmp_ext;

    tmp_ext = find_and_replace_string (artifacts->ext, "\\", "\\\\");
    h_ext = find_and_replace_string (tmp_ext, "\n", "\\n");
    header = append_token_to_header (header, "ext", h_ext);

    g_free (h_ext);
    g_free (tmp_ext);
  }

  header = append_token_to_header (header, "mac", mac);

  if (artifacts->app) {
    header = append_token_to_header (header, "app", artifacts->app);

    if (artifacts->dlg)
      header = append_token_to_header (header, "dlg", artifacts->dlg);
  }

  return ephy_sync_crypto_hawk_header_new (header, artifacts);
}
