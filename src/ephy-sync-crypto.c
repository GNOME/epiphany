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
#include "ephy-sync-crypto.h"

#include <glib/gstdio.h>
#include <libsoup/soup.h>
#include <nettle/aes.h>
#include <string.h>

#define HAWK_VERSION  1

static const gchar hex_digits[] = "0123456789abcdef";

static guint8 *
concatenate_bytes (guint8 *first_data,
                   gsize   first_length,
                   ...) G_GNUC_NULL_TERMINATED;

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

static EphySyncCryptoProcessedKFT *
ephy_sync_crypto_processed_kft_new (guint8 *tokenID,
                                    guint8 *reqHMACkey,
                                    guint8 *respHMACkey,
                                    guint8 *respXORkey)
{
  EphySyncCryptoProcessedKFT *processed_kft;

  processed_kft = g_slice_new (EphySyncCryptoProcessedKFT);
  processed_kft->tokenID = tokenID;
  processed_kft->reqHMACkey = reqHMACkey;
  processed_kft->respHMACkey = respHMACkey;
  processed_kft->respXORkey = respXORkey;

  return processed_kft;
}

static EphySyncCryptoProcessedST *
ephy_sync_crypto_processed_st_new (guint8 *tokenID,
                                   guint8 *reqHMACkey,
                                   guint8 *requestKey)
{
  EphySyncCryptoProcessedST *processed_st;

  processed_st = g_slice_new (EphySyncCryptoProcessedST);
  processed_st->tokenID = tokenID;
  processed_st->reqHMACkey = reqHMACkey;
  processed_st->requestKey = requestKey;

  return processed_st;
}

static EphySyncCryptoSyncKeys *
ephy_sync_crypto_sync_keys_new (guint8 *kA,
                                guint8 *kB,
                                guint8 *wrapKB)
{
  EphySyncCryptoSyncKeys *sync_keys;

  sync_keys = g_slice_new (EphySyncCryptoSyncKeys);
  sync_keys->kA = kA;
  sync_keys->kB = kB;
  sync_keys->wrapKB = wrapKB;

  return sync_keys;
}

static EphySyncCryptoRSAKeyPair *
ephy_sync_crypto_rsa_key_pair_new (struct rsa_public_key  public,
                                   struct rsa_private_key private)
{
  EphySyncCryptoRSAKeyPair *keypair;

  keypair = g_slice_new (EphySyncCryptoRSAKeyPair);
  keypair->public = public;
  keypair->private = private;

  return keypair;
}

void
ephy_sync_crypto_hawk_options_free (EphySyncCryptoHawkOptions *hawk_options)
{
  g_return_if_fail (hawk_options != NULL);

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

static void
ephy_sync_crypto_hawk_artifacts_free (EphySyncCryptoHawkArtifacts *hawk_artifacts)
{
  g_return_if_fail (hawk_artifacts != NULL);

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

void
ephy_sync_crypto_hawk_header_free (EphySyncCryptoHawkHeader *hawk_header)
{
  g_return_if_fail (hawk_header != NULL);

  g_free (hawk_header->header);
  ephy_sync_crypto_hawk_artifacts_free (hawk_header->artifacts);

  g_slice_free (EphySyncCryptoHawkHeader, hawk_header);
}

void
ephy_sync_crypto_processed_kft_free (EphySyncCryptoProcessedKFT *processed_kft)
{
  g_return_if_fail (processed_kft != NULL);

  g_free (processed_kft->tokenID);
  g_free (processed_kft->reqHMACkey);
  g_free (processed_kft->respHMACkey);
  g_free (processed_kft->respXORkey);

  g_slice_free (EphySyncCryptoProcessedKFT, processed_kft);
}

void
ephy_sync_crypto_processed_st_free (EphySyncCryptoProcessedST *processed_st)
{
  g_return_if_fail (processed_st != NULL);

  g_free (processed_st->tokenID);
  g_free (processed_st->reqHMACkey);
  g_free (processed_st->requestKey);

  g_slice_free (EphySyncCryptoProcessedST, processed_st);
}

void
ephy_sync_crypto_sync_keys_free (EphySyncCryptoSyncKeys *sync_keys)
{
  g_return_if_fail (sync_keys != NULL);

  g_free (sync_keys->kA);
  g_free (sync_keys->kB);
  g_free (sync_keys->wrapKB);

  g_slice_free (EphySyncCryptoSyncKeys, sync_keys);
}

void
ephy_sync_crypto_rsa_key_pair_free (EphySyncCryptoRSAKeyPair *keypair)
{
  g_return_if_fail (keypair != NULL);

  rsa_public_key_clear (&keypair->public);
  rsa_private_key_clear (&keypair->private);

  g_slice_free (EphySyncCryptoRSAKeyPair, keypair);
}

static gchar *
kw (const gchar *name)
{
  return g_strconcat ("identity.mozilla.com/picl/v1/", name, NULL);
}

static guint8 *
xor (guint8 *a,
     guint8 *b,
     gsize   length)
{
  guint8 *xored;

  xored = g_malloc (length);
  for (gsize i = 0; i < length; i++)
    xored[i] = a[i] ^ b[i];

  return xored;
}

static gboolean
are_equal (guint8 *a,
           guint8 *b)
{
  gchar *a_hex;
  gchar *b_hex;
  gboolean retval;

  a_hex = ephy_sync_crypto_encode_hex (a, 0);
  b_hex = ephy_sync_crypto_encode_hex (b, 0);
  retval = g_str_equal (a_hex, b_hex);

  g_free (a_hex);
  g_free (b_hex);

  return retval;
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
  guint8 *digest;
  gchar *digest_hex;
  gchar *content;
  gchar *update;
  gchar *hash;

  g_return_val_if_fail (payload, NULL);
  g_return_val_if_fail (content_type, NULL);

  content = parse_content_type (content_type);
  update = g_strdup_printf ("hawk.%d.payload\n%s\n%s\n",
                            HAWK_VERSION,
                            content,
                            payload);

  digest_hex = g_compute_checksum_for_string (G_CHECKSUM_SHA256, update, -1);
  digest = ephy_sync_crypto_decode_hex (digest_hex);
  hash = g_base64_encode (digest, g_checksum_type_get_length (G_CHECKSUM_SHA256));

  g_free (content);
  g_free (update);
  g_free (digest_hex);
  g_free (digest);

  return hash;
}

static gchar *
calculate_mac (const gchar                 *mac_type,
               guint8                      *key,
               gsize                        key_length,
               EphySyncCryptoHawkArtifacts *artifacts)
{
  guint8 *digest;
  gchar *digest_hex;
  gchar *normalized;
  gchar *mac;

  g_return_val_if_fail (mac_type, NULL);
  g_return_val_if_fail (key, NULL);
  g_return_val_if_fail (artifacts, NULL);

  normalized = normalize_string (mac_type, artifacts);
  digest_hex = g_compute_hmac_for_string (G_CHECKSUM_SHA256,
                                          key, key_length,
                                          normalized, -1);
  digest = ephy_sync_crypto_decode_hex (digest_hex);
  mac = g_base64_encode (digest, g_checksum_type_get_length (G_CHECKSUM_SHA256));

  g_free (normalized);
  g_free (digest_hex);
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

static guint8 *
concatenate_bytes (guint8 *first_data,
                   gsize   first_length,
                   ...)
{
  va_list args;
  guint8 *data;
  guint8 *out;
  gsize length;
  gsize out_length;

  out_length = first_length;
  out = g_malloc (out_length);
  memcpy (out, first_data, out_length);

  va_start (args, first_length);

  while ((data = va_arg (args, guint8 *)) != NULL) {
    length = va_arg (args, gsize);
    out = g_realloc (out, out_length + length);
    memcpy (out + out_length, data, length);
    out_length += length;
  }

  va_end (args);

  return out;
}

/*
 * HMAC-based Extract-and-Expand Key Derivation Function.
 * Uses sha256 as hash function.
 * https://tools.ietf.org/html/rfc5869
 */
static void
hkdf (guint8 *in,
      gsize   in_length,
      guint8 *salt,
      gsize   salt_length,
      guint8 *info,
      gsize   info_length,
      guint8 *out,
      gsize   out_length)
{
  gchar *prk_hex;
  gchar *tmp_hex;
  guint8 *tmp;
  guint8 *prk;
  guint8 *out_full;
  guint8 *data;
  guint8 counter;
  gsize hash_length;
  gsize data_length;
  gsize n;

  hash_length = g_checksum_type_get_length (G_CHECKSUM_SHA256);
  g_assert (out_length <= hash_length * 255);

  /* If salt value was not provided, use an array of hash_length zeros */
  if (salt == NULL) {
    salt = g_malloc0 (hash_length);
    salt_length = hash_length;
  }

  /* Step 1: Extract */
  prk_hex = g_compute_hmac_for_data (G_CHECKSUM_SHA256,
                                     salt, salt_length,
                                     in, in_length);
  prk = ephy_sync_crypto_decode_hex (prk_hex);

  /* Step 2: Expand */
  counter = 1;
  n = (out_length + hash_length - 1) / hash_length;
  out_full = g_malloc (n * hash_length);

  for (gsize i = 0; i < n; i++, counter++) {
    if (i == 0) {
      data = concatenate_bytes (info, info_length,
                                &counter, 1,
                                NULL);
      data_length = info_length + 1;
    } else {
      data = concatenate_bytes (out_full + (i - 1) * hash_length, hash_length,
                                info, info_length,
                                &counter, 1,
                                NULL);
      data_length = hash_length + info_length + 1;
    }

    tmp_hex = g_compute_hmac_for_data (G_CHECKSUM_SHA256,
                                       prk, hash_length,
                                       data, data_length);
    tmp = ephy_sync_crypto_decode_hex (tmp_hex);
    memcpy (out_full + i * hash_length, tmp, hash_length);

    g_free (data);
    g_free (tmp);
    g_free (tmp_hex);
  }

  memcpy (out, out_full, out_length);

  g_free (prk_hex);
  g_free (salt);
  g_free (prk);
  g_free (out_full);
}

static void
random_func (void   *ctx,
             gsize   length,
             guint8 *dst)
{
  for (gsize i = 0; i < length; i++)
    dst[i] = g_random_int ();
}

static void
base64_to_base64_urlsafe (gchar *text)
{
  /* Replace '+' with '-' and '/' with '_' */
  g_strcanon (text, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789=/", '-');
  g_strcanon (text, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789=-", '_');
}

static void
base64_urlsafe_to_base64 (gchar *text)
{
  /* Replace '-' with '+' and '_' with '/' */
  g_strcanon (text, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789=_", '+');
  g_strcanon (text, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789=+", '/');
}

EphySyncCryptoProcessedKFT *
ephy_sync_crypto_process_key_fetch_token (const gchar *keyFetchToken)
{
  guint8 *kft;
  guint8 *out1;
  guint8 *out2;
  guint8 *tokenID;
  guint8 *reqHMACkey;
  guint8 *respHMACkey;
  guint8 *respXORkey;
  guint8 *keyRequestKey;
  gchar *info_kft;
  gchar *info_keys;

  kft = ephy_sync_crypto_decode_hex (keyFetchToken);
  info_kft = kw ("keyFetchToken");
  info_keys = kw ("account/keys");
  out1 = g_malloc (3 * EPHY_SYNC_TOKEN_LENGTH);
  out2 = g_malloc (3 * EPHY_SYNC_TOKEN_LENGTH);

  hkdf (kft, EPHY_SYNC_TOKEN_LENGTH,
        NULL, 0,
        (guint8 *) info_kft, strlen (info_kft),
        out1, 3 * EPHY_SYNC_TOKEN_LENGTH);

  tokenID = g_malloc (EPHY_SYNC_TOKEN_LENGTH);
  reqHMACkey = g_malloc (EPHY_SYNC_TOKEN_LENGTH);
  keyRequestKey = g_malloc (EPHY_SYNC_TOKEN_LENGTH);
  memcpy (tokenID, out1, EPHY_SYNC_TOKEN_LENGTH);
  memcpy (reqHMACkey, out1 + EPHY_SYNC_TOKEN_LENGTH, EPHY_SYNC_TOKEN_LENGTH);
  memcpy (keyRequestKey, out1 + 2 * EPHY_SYNC_TOKEN_LENGTH, EPHY_SYNC_TOKEN_LENGTH);

  hkdf (keyRequestKey, EPHY_SYNC_TOKEN_LENGTH,
        NULL, 0,
        (guint8 *) info_keys, strlen (info_keys),
        out2, 3 * EPHY_SYNC_TOKEN_LENGTH);

  respHMACkey = g_malloc (EPHY_SYNC_TOKEN_LENGTH);
  respXORkey = g_malloc (2 * EPHY_SYNC_TOKEN_LENGTH);
  memcpy (respHMACkey, out2, EPHY_SYNC_TOKEN_LENGTH);
  memcpy (respXORkey, out2 + EPHY_SYNC_TOKEN_LENGTH, 2 * EPHY_SYNC_TOKEN_LENGTH);

  g_free (kft);
  g_free (out1);
  g_free (out2);
  g_free (info_kft);
  g_free (info_keys);
  g_free (keyRequestKey);

  return ephy_sync_crypto_processed_kft_new (tokenID,
                                             reqHMACkey,
                                             respHMACkey,
                                             respXORkey);
}

EphySyncCryptoProcessedST *
ephy_sync_crypto_process_session_token (const gchar *sessionToken)
{
  guint8 *st;
  guint8 *out;
  guint8 *tokenID;
  guint8 *reqHMACkey;
  guint8 *requestKey;
  gchar *info;

  st = ephy_sync_crypto_decode_hex (sessionToken);
  info = kw ("sessionToken");
  out = g_malloc (3 * EPHY_SYNC_TOKEN_LENGTH);

  hkdf (st, EPHY_SYNC_TOKEN_LENGTH,
        NULL, 0,
        (guint8 *) info, strlen (info),
        out, 3 * EPHY_SYNC_TOKEN_LENGTH);

  tokenID = g_malloc (EPHY_SYNC_TOKEN_LENGTH);
  reqHMACkey = g_malloc (EPHY_SYNC_TOKEN_LENGTH);
  requestKey = g_malloc (EPHY_SYNC_TOKEN_LENGTH);
  memcpy (tokenID, out, EPHY_SYNC_TOKEN_LENGTH);
  memcpy (reqHMACkey, out + EPHY_SYNC_TOKEN_LENGTH, EPHY_SYNC_TOKEN_LENGTH);
  memcpy (requestKey, out + 2 * EPHY_SYNC_TOKEN_LENGTH, EPHY_SYNC_TOKEN_LENGTH);

  g_free (st);
  g_free (out);
  g_free (info);

  return ephy_sync_crypto_processed_st_new (tokenID,
                                            reqHMACkey,
                                            requestKey);
}

EphySyncCryptoSyncKeys *
ephy_sync_crypto_retrieve_sync_keys (const gchar *bundle,
                                     guint8      *respHMACkey,
                                     guint8      *respXORkey,
                                     guint8      *unwrapBKey)
{
  guint8 *bdl;
  guint8 *ciphertext;
  guint8 *respMAC;
  guint8 *respMAC2;
  guint8 *xored;
  guint8 *wrapKB;
  guint8 *kA;
  guint8 *kB;
  gchar *respMAC2_hex;
  EphySyncCryptoSyncKeys *retval = NULL;

  bdl = ephy_sync_crypto_decode_hex (bundle);
  ciphertext = g_malloc (2 * EPHY_SYNC_TOKEN_LENGTH);
  respMAC = g_malloc (EPHY_SYNC_TOKEN_LENGTH);
  wrapKB = g_malloc (EPHY_SYNC_TOKEN_LENGTH);
  kA = g_malloc (EPHY_SYNC_TOKEN_LENGTH);

  memcpy (ciphertext, bdl, 2 * EPHY_SYNC_TOKEN_LENGTH);
  memcpy (respMAC, bdl + 2 * EPHY_SYNC_TOKEN_LENGTH, EPHY_SYNC_TOKEN_LENGTH);
  respMAC2_hex = g_compute_hmac_for_data (G_CHECKSUM_SHA256,
                                          respHMACkey, EPHY_SYNC_TOKEN_LENGTH,
                                          ciphertext, 2 * EPHY_SYNC_TOKEN_LENGTH);
  respMAC2 = ephy_sync_crypto_decode_hex (respMAC2_hex);

  if (are_equal (respMAC, respMAC2) == FALSE) {
    g_warning ("respMAC and respMAC2 differ");
    goto out;
  }

  xored = xor (ciphertext, respXORkey, 2 * EPHY_SYNC_TOKEN_LENGTH);
  memcpy (kA, xored, EPHY_SYNC_TOKEN_LENGTH);
  memcpy (wrapKB, xored + EPHY_SYNC_TOKEN_LENGTH, EPHY_SYNC_TOKEN_LENGTH);
  kB = xor (unwrapBKey, wrapKB, EPHY_SYNC_TOKEN_LENGTH);
  retval = ephy_sync_crypto_sync_keys_new (kA, kB, wrapKB);

out:
  g_free (bdl);
  g_free (ciphertext);
  g_free (respMAC);
  g_free (respMAC2);
  g_free (respMAC2_hex);
  g_free (xored);

  return retval;
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
  nonce = nonce ? nonce : ephy_sync_crypto_generate_random_string (6);
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

  soup_uri_free (uri);

  return ephy_sync_crypto_hawk_header_new (header, artifacts);
}

EphySyncCryptoRSAKeyPair *
ephy_sync_crypto_generate_rsa_key_pair (void)
{
  struct rsa_public_key public;
  struct rsa_private_key private;
  gint retval;

  rsa_public_key_init (&public);
  rsa_private_key_init (&private);

  /* The public exponent, usually one of the small Fermat primes 3, 5, 17, 257, 65537 */
  mpz_set_ui (public.e, 65537);

  /* Key sizes below 2048 are considered breakable and should not be used */
  retval = rsa_generate_keypair (&public, &private,
                                 NULL, random_func,
                                 NULL, NULL, 2048, 0);
  if (retval == 0) {
    g_warning ("Failed to generate RSA key pair");
    rsa_public_key_clear (&public);
    rsa_private_key_clear (&private);
    return NULL;
  }

  return ephy_sync_crypto_rsa_key_pair_new (public, private);
}

gchar *
ephy_sync_crypto_create_assertion (const gchar              *certificate,
                                   const gchar              *audience,
                                   guint64                   duration,
                                   EphySyncCryptoRSAKeyPair *keypair)
{
  mpz_t signature;
  const gchar *header = "{\"alg\": \"RS256\"}";
  gchar *body;
  gchar *body_b64;
  gchar *header_b64;
  gchar *to_sign;
  gchar *sig_b64 = NULL;
  gchar *assertion = NULL;
  gchar *digest_hex;
  guint8 *digest;
  guint8 *sig = NULL;
  guint64 expires_at;
  gsize expected_size;
  gsize count;

  expires_at = g_get_real_time () / 1000 + duration * 1000;
  body = g_strdup_printf ("{\"exp\": %lu, \"aud\": \"%s\"}", expires_at, audience);
  body_b64 = ephy_sync_crypto_base64_urlsafe_encode ((guint8 *) body, strlen (body), TRUE);
  header_b64 = ephy_sync_crypto_base64_urlsafe_encode ((guint8 *) header, strlen (header), TRUE);
  to_sign = g_strdup_printf ("%s.%s", header_b64, body_b64);

  mpz_init (signature);
  digest_hex = g_compute_checksum_for_string (G_CHECKSUM_SHA256, to_sign, -1);
  digest = ephy_sync_crypto_decode_hex (digest_hex);

  if (rsa_sha256_sign_digest_tr (&keypair->public, &keypair->private,
                                 NULL, random_func,
                                 digest, signature) == 0) {
    g_warning ("Failed to sign the message. Giving up.");
    goto out;
  }

  expected_size = (mpz_sizeinbase (signature, 2) + 7) / 8;
  sig = g_malloc (expected_size);
  mpz_export (sig, &count, 1, sizeof (guint8), 0, 0, signature);

  if (count != expected_size) {
    g_warning ("Expected %lu bytes, got %lu. Giving up.", count, expected_size);
    goto out;
  }

  sig_b64 = ephy_sync_crypto_base64_urlsafe_encode (sig, count, TRUE);
  assertion = g_strdup_printf ("%s~%s.%s.%s",
                               certificate, header_b64, body_b64, sig_b64);

out:
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

gchar *
ephy_sync_crypto_generate_random_string (gsize length)
{
  guchar *bytes;
  gchar *base64_string;
  gchar *string;

  bytes = g_malloc (length);
  for (gsize i = 0; i < length; i++)
    bytes[i] = g_random_int ();

  base64_string = g_base64_encode (bytes, length);
  base64_to_base64_urlsafe (base64_string);
  string = g_strndup (base64_string, length);

  g_free (bytes);
  g_free (base64_string);

  return string;
}

gchar *
ephy_sync_crypto_base64_urlsafe_encode (guint8   *data,
                                        gsize     data_length,
                                        gboolean  strip)
{
  gchar *encoded;
  gchar *base64;
  gsize start;
  gssize end;

  base64 = g_base64_encode (data, data_length);

  if (strip == FALSE) {
    base64_to_base64_urlsafe (base64);
    return base64;
  }

  /* Strip all the '=' */
  start = 0;
  while (start < strlen (base64) && base64[start] == '=')
    start++;

  end = strlen (base64) - 1;
  while (end >= 0 && base64[end] == '=')
    end--;

  encoded = g_strndup (base64 + start, end - start + 1);
  base64_to_base64_urlsafe (encoded);

  g_free (base64);

  return encoded;
}

guint8 *
ephy_sync_crypto_base64_urlsafe_decode (gchar *text,
                                        gsize *out_length)
{
  guint8 *decoded;
  gchar *text_copy;

  text_copy = g_strdup (text);
  base64_urlsafe_to_base64 (text_copy);
  decoded = g_base64_decode (text_copy, out_length);

  g_free (text_copy);

  return decoded;
}

guint8 *
ephy_sync_crypto_aes_256 (EphySyncCryptoAES256Mode  mode,
                          const guint8             *key,
                          const guint8             *data,
                          gsize                     data_length,
                          gsize                    *out_length)
{
  struct aes256_ctx aes;
  gsize padded_length;
  guint8 *padded_data;
  guint8 *out;

  g_return_val_if_fail (key != NULL, NULL);
  g_return_val_if_fail (data != NULL, NULL);

  if (mode == AES_256_MODE_DECRYPT)
    g_assert (data_length % AES_BLOCK_SIZE == 0);

  padded_length = data_length;
  if (data_length % AES_BLOCK_SIZE != 0)
    padded_length = data_length + (AES_BLOCK_SIZE - data_length % AES_BLOCK_SIZE);

  out = g_malloc (padded_length);
  padded_data = g_malloc0 (padded_length);
  memcpy (padded_data, data, data_length);

  switch (mode) {
    case AES_256_MODE_ENCRYPT:
      aes256_set_encrypt_key (&aes, key);
      aes256_encrypt (&aes, padded_length, out, padded_data);
      break;
    case AES_256_MODE_DECRYPT:
      aes256_set_decrypt_key (&aes, key);
      aes256_decrypt (&aes, padded_length, out, padded_data);
      break;
    default:
      g_assert_not_reached ();
  }

  if (out_length != NULL)
    *out_length = padded_length;

  g_free (padded_data);

  return out;
}

gchar *
ephy_sync_crypto_encode_hex (guint8 *data,
                             gsize   data_length)
{
  gchar *retval;
  gsize length;

  length = data_length == 0 ? EPHY_SYNC_TOKEN_LENGTH : data_length;
  retval = g_malloc (length * 2 + 1);

  for (gsize i = 0; i < length; i++) {
    guint8 byte = data[i];

    retval[2 * i] = hex_digits[byte >> 4];
    retval[2 * i + 1] = hex_digits[byte & 0xf];
  }

  retval[length * 2] = 0;

  return retval;
}

guint8 *
ephy_sync_crypto_decode_hex (const gchar *hex_string)
{
  guint8 *retval;
  gsize hex_length;

  hex_length = strlen (hex_string);
  g_return_val_if_fail (hex_length % 2 == 0, NULL);

  retval = g_malloc (hex_length / 2);

  for (gsize i = 0, j = 0; i < hex_length; i += 2, j++) {
    sscanf(hex_string + i, "%2hhx", retval + j);
  }

  return retval;
}
