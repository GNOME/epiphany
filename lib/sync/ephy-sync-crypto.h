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

#pragma once

#include <glib-object.h>
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

SyncCryptoHawkOptions *ephy_sync_crypto_hawk_options_new        (const char *app,
                                                                 const char *dlg,
                                                                 const char *ext,
                                                                 const char *content_type,
                                                                 const char *hash,
                                                                 const char *local_time_offset,
                                                                 const char *nonce,
                                                                 const char *payload,
                                                                 const char *timestamp);
void                   ephy_sync_crypto_hawk_options_free       (SyncCryptoHawkOptions *options);

SyncCryptoHawkHeader  *ephy_sync_crypto_hawk_header_new         (const char            *url,
                                                                 const char            *method,
                                                                 const char            *id,
                                                                 const guint8          *key,
                                                                 gsize                  key_len,
                                                                 SyncCryptoHawkOptions *options);
void                   ephy_sync_crypto_hawk_header_free        (SyncCryptoHawkHeader *header);

SyncCryptoRSAKeyPair  *ephy_sync_crypto_rsa_key_pair_new        (void);
void                   ephy_sync_crypto_rsa_key_pair_free       (SyncCryptoRSAKeyPair *key_pair);

SyncCryptoKeyBundle   *ephy_sync_crypto_key_bundle_new          (const char *aes_key_b64,
                                                                 const char *hmac_key_b64);
void                   ephy_sync_crypto_key_bundle_free         (SyncCryptoKeyBundle *bundle);

void                   ephy_sync_crypto_derive_session_token    (const char  *session_token,
                                                                 guint8     **token_id,
                                                                 guint8     **req_hmac_key,
                                                                 guint8     **requestKey);
void                   ephy_sync_crypto_derive_key_fetch_token  (const char  *key_fetch_token,
                                                                 guint8     **token_id,
                                                                 guint8     **req_hmac_key,
                                                                 guint8     **resp_hmac_key,
                                                                 guint8     **resp_xor_key);
gboolean               ephy_sync_crypto_derive_master_keys      (const char    *bundle_hex,
                                                                 const guint8  *resp_hmac_key,
                                                                 const guint8  *resp_xor_key,
                                                                 const guint8  *unwrap_kb,
                                                                 guint8       **ka,
                                                                 guint8       **kb);
SyncCryptoKeyBundle   *ephy_sync_crypto_derive_master_bundle    (const guint8 *kb);

char                  *ephy_sync_crypto_generate_crypto_keys    (void);
char                  *ephy_sync_crypto_create_assertion        (const char           *certificate,
                                                                 const char           *audience,
                                                                 guint64               duration,
                                                                 SyncCryptoRSAKeyPair *key_pair);

char                  *ephy_sync_crypto_encrypt_record          (const char          *cleartext,
                                                                 SyncCryptoKeyBundle *bundle);
char                  *ephy_sync_crypto_decrypt_record          (const char          *payload,
                                                                 SyncCryptoKeyBundle *bundle);

G_END_DECLS
