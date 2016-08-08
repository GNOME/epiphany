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
  gchar *app;
  gchar *dlg;
  gchar *ext;
  gchar *content_type;
  gchar *hash;
  gchar *local_time_offset;
  gchar *nonce;
  gchar *payload;
  gchar *timestamp;
} EphySyncCryptoHawkOptions;

typedef struct {
  gchar *app;
  gchar *dlg;
  gchar *ext;
  gchar *hash;
  gchar *host;
  gchar *method;
  gchar *nonce;
  gchar *port;
  gchar *resource;
  gchar *ts;
} EphySyncCryptoHawkArtifacts;

typedef struct {
  gchar *header;
  EphySyncCryptoHawkArtifacts *artifacts;
} EphySyncCryptoHawkHeader;

typedef struct {
  struct rsa_public_key public;
  struct rsa_private_key private;
} EphySyncCryptoRSAKeyPair;

EphySyncCryptoHawkOptions *ephy_sync_crypto_hawk_options_new        (const gchar *app,
                                                                     const gchar *dlg,
                                                                     const gchar *ext,
                                                                     const gchar *content_type,
                                                                     const gchar *hash,
                                                                     const gchar *local_time_offset,
                                                                     const gchar *nonce,
                                                                     const gchar *payload,
                                                                     const gchar *timestamp);
void                       ephy_sync_crypto_hawk_options_free       (EphySyncCryptoHawkOptions *options);
void                       ephy_sync_crypto_hawk_header_free        (EphySyncCryptoHawkHeader *header);
void                       ephy_sync_crypto_rsa_key_pair_free       (EphySyncCryptoRSAKeyPair *keypair);
void                       ephy_sync_crypto_process_key_fetch_token (const gchar  *keyFetchToken,
                                                                     guint8      **tokenID,
                                                                     guint8      **reqHMACkey,
                                                                     guint8      **respHMACkey,
                                                                     guint8      **respXORkey);
void                       ephy_sync_crypto_process_session_token   (const gchar  *sessionToken,
                                                                     guint8      **tokenID,
                                                                     guint8      **reqHMACkey,
                                                                     guint8      **requestKey);
void                       ephy_sync_crypto_compute_sync_keys       (const gchar  *bundle,
                                                                     guint8       *respHMACkey,
                                                                     guint8       *respXORkey,
                                                                     guint8       *unwrapBKey,
                                                                     guint8      **kA,
                                                                     guint8      **kB);
EphySyncCryptoHawkHeader  *ephy_sync_crypto_compute_hawk_header     (const gchar               *url,
                                                                     const gchar               *method,
                                                                     const gchar               *id,
                                                                     guint8                    *key,
                                                                     gsize                      key_len,
                                                                     EphySyncCryptoHawkOptions *options);
EphySyncCryptoRSAKeyPair  *ephy_sync_crypto_generate_rsa_key_pair   (void);
gchar                     *ephy_sync_crypto_create_assertion        (const gchar              *certificate,
                                                                     const gchar              *audience,
                                                                     guint64                   duration,
                                                                     EphySyncCryptoRSAKeyPair *keypair);
gchar                     *ephy_sync_crypto_generate_random_hex     (gsize length);
gchar                     *ephy_sync_crypto_base64_urlsafe_encode   (guint8   *data,
                                                                     gsize     data_len,
                                                                     gboolean  strip);
guint8                    *ephy_sync_crypto_base64_urlsafe_decode   (const gchar *text,
                                                                     gsize       *out_len,
                                                                     gboolean     fill);
guint8                    *ephy_sync_crypto_aes_256                 (EphySyncCryptoAES256Mode  mode,
                                                                     const guint8             *key,
                                                                     const guint8             *data,
                                                                     gsize                     data_len,
                                                                     gsize                    *out_len);
gchar                     *ephy_sync_crypto_encode_hex              (guint8 *data,
                                                                     gsize   data_len);
guint8                    *ephy_sync_crypto_decode_hex              (const gchar *hex_string);

G_END_DECLS

#endif
