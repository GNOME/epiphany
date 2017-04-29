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

#pragma once

#include <glib-object.h>
#include <json-glib/json-glib.h>
#include <nettle/rsa.h>

G_BEGIN_DECLS

typedef struct {
  char *app;
  char *dlg;
  char *ext;
  char *content_type;
  char *hash;
  char *local_time_offset;
  char *nonce;
  char *payload;
  char *timestamp;
} SyncCryptoHawkOptions;

typedef struct {
  char *app;
  char *dlg;
  char *ext;
  char *hash;
  char *host;
  char *method;
  char *nonce;
  char *port;
  char *resource;
  char *ts;
} SyncCryptoHawkArtifacts;

typedef struct {
  char *header;
  SyncCryptoHawkArtifacts *artifacts;
} SyncCryptoHawkHeader;

typedef struct {
  struct rsa_public_key public;
  struct rsa_private_key private;
} SyncCryptoRSAKeyPair;

typedef struct {
  char *aes_key_hex;
  char *hmac_key_hex;
} SyncCryptoKeyBundle;

SyncCryptoHawkOptions  *ephy_sync_crypto_hawk_options_new         (const char *app,
                                                                   const char *dlg,
                                                                   const char *ext,
                                                                   const char *content_type,
                                                                   const char *hash,
                                                                   const char *local_time_offset,
                                                                   const char *nonce,
                                                                   const char *payload,
                                                                   const char *timestamp);
void                    ephy_sync_crypto_hawk_options_free        (SyncCryptoHawkOptions *options);
void                    ephy_sync_crypto_hawk_header_free         (SyncCryptoHawkHeader *header);
void                    ephy_sync_crypto_rsa_key_pair_free        (SyncCryptoRSAKeyPair *rsa_key_pair);
SyncCryptoKeyBundle    *ephy_sync_crypto_key_bundle_from_array    (JsonArray *array);
void                    ephy_sync_crypto_key_bundle_free          (SyncCryptoKeyBundle *bundle);
void                    ephy_sync_crypto_process_key_fetch_token  (const char  *key_fetch_token,
                                                                   guint8     **token_id,
                                                                   guint8     **req_hmac_key,
                                                                   guint8     **resp_hmac_key,
                                                                   guint8     **resp_xor_key,
                                                                   gsize        token_len);
void                    ephy_sync_crypto_process_session_token    (const char  *session_token,
                                                                   guint8     **token_id,
                                                                   guint8     **req_hmac_key,
                                                                   guint8     **requestKey,
                                                                   gsize        token_len);
gboolean                ephy_sync_crypto_compute_sync_keys        (const char    *bundle_hex,
                                                                   const guint8  *resp_hmac_key,
                                                                   const guint8  *resp_xor_key,
                                                                   const guint8  *unwrap_b_key,
                                                                   guint8       **key_a,
                                                                   guint8       **key_b,
                                                                   gsize          key_len);
SyncCryptoKeyBundle    *ephy_sync_crypto_derive_key_bundle        (const guint8 *key,
                                                                   gsize         key_len);
char                   *ephy_sync_crypto_generate_crypto_keys     (gsize key_len);
char                   *ephy_sync_crypto_decrypt_record           (const char          *payload,
                                                                   SyncCryptoKeyBundle *bundle);
char                   *ephy_sync_crypto_encrypt_record           (const char          *cleartext,
                                                                   SyncCryptoKeyBundle *bundle);
SyncCryptoHawkHeader   *ephy_sync_crypto_compute_hawk_header      (const char            *url,
                                                                   const char            *method,
                                                                   const char            *id,
                                                                   const guint8          *key,
                                                                   gsize                  key_len,
                                                                   SyncCryptoHawkOptions *options);
SyncCryptoRSAKeyPair   *ephy_sync_crypto_generate_rsa_key_pair    (void);
char                   *ephy_sync_crypto_create_assertion         (const char           *certificate,
                                                                   const char           *audience,
                                                                   guint64               duration,
                                                                   SyncCryptoRSAKeyPair *rsa_key_pair);
char                   *ephy_sync_crypto_base64_urlsafe_encode    (const guint8 *data,
                                                                   gsize         data_len,
                                                                   gboolean      strip);
guint8                 *ephy_sync_crypto_base64_urlsafe_decode    (const char *text,
                                                                   gsize      *out_len,
                                                                   gboolean    fill);
char                   *ephy_sync_crypto_encode_hex               (const guint8 *data,
                                                                   gsize         data_len);
guint8                 *ephy_sync_crypto_decode_hex               (const char *hex);
char                   *ephy_sync_crypto_get_random_sync_id       (void);

G_END_DECLS
