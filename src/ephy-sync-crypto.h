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

#ifndef EPHY_SYNC_CRYPTO_H
#define EPHY_SYNC_CRYPTO_H

#include <glib-object.h>
#include <nettle/rsa.h>

G_BEGIN_DECLS

#define EPHY_SYNC_TOKEN_LENGTH 32

typedef enum {
  AES_256_MODE_ENCRYPT,
  AES_256_MODE_DECRYPT
} EphySyncCryptoAES256Mode;

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
} EphySyncCryptoHawkOptions;

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
} EphySyncCryptoHawkArtifacts;

typedef struct {
  char *header;
  EphySyncCryptoHawkArtifacts *artifacts;
} EphySyncCryptoHawkHeader;

typedef struct {
  struct rsa_public_key public;
  struct rsa_private_key private;
} EphySyncCryptoRSAKeyPair;

EphySyncCryptoHawkOptions *ephy_sync_crypto_hawk_options_new        (const char *app,
                                                                     const char *dlg,
                                                                     const char *ext,
                                                                     const char *content_type,
                                                                     const char *hash,
                                                                     const char *local_time_offset,
                                                                     const char *nonce,
                                                                     const char *payload,
                                                                     const char *timestamp);
void                       ephy_sync_crypto_hawk_options_free       (EphySyncCryptoHawkOptions *options);
void                       ephy_sync_crypto_hawk_header_free        (EphySyncCryptoHawkHeader *header);
void                       ephy_sync_crypto_rsa_key_pair_free       (EphySyncCryptoRSAKeyPair *keypair);
void                       ephy_sync_crypto_process_key_fetch_token (const char  *keyFetchToken,
                                                                     guint8     **tokenID,
                                                                     guint8     **reqHMACkey,
                                                                     guint8     **respHMACkey,
                                                                     guint8     **respXORkey);
void                       ephy_sync_crypto_process_session_token   (const char  *sessionToken,
                                                                     guint8     **tokenID,
                                                                     guint8     **reqHMACkey,
                                                                     guint8     **requestKey);
void                       ephy_sync_crypto_compute_sync_keys       (const char  *bundle,
                                                                     guint8      *respHMACkey,
                                                                     guint8      *respXORkey,
                                                                     guint8      *unwrapBKey,
                                                                     guint8     **kA,
                                                                     guint8     **kB);
EphySyncCryptoHawkHeader  *ephy_sync_crypto_compute_hawk_header     (const char                *url,
                                                                     const char                *method,
                                                                     const char                *id,
                                                                     guint8                    *key,
                                                                     gsize                      key_len,
                                                                     EphySyncCryptoHawkOptions *options);
EphySyncCryptoRSAKeyPair  *ephy_sync_crypto_generate_rsa_key_pair   (void);
char                      *ephy_sync_crypto_create_assertion        (const char               *certificate,
                                                                     const char               *audience,
                                                                     guint64                   duration,
                                                                     EphySyncCryptoRSAKeyPair *keypair);
char                      *ephy_sync_crypto_generate_random_hex     (gsize length);
char                      *ephy_sync_crypto_base64_urlsafe_encode   (guint8   *data,
                                                                     gsize     data_len,
                                                                     gboolean  strip);
guint8                    *ephy_sync_crypto_base64_urlsafe_decode   (const char *text,
                                                                     gsize      *out_len,
                                                                     gboolean    fill);
guint8                    *ephy_sync_crypto_aes_256                 (EphySyncCryptoAES256Mode  mode,
                                                                     const guint8             *key,
                                                                     const guint8             *data,
                                                                     gsize                     data_len,
                                                                     gsize                    *out_len);
char                      *ephy_sync_crypto_encode_hex              (guint8 *data,
                                                                     gsize   data_len);
guint8                    *ephy_sync_crypto_decode_hex              (const char *hex);

G_END_DECLS

#endif
