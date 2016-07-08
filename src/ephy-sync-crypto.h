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

G_BEGIN_DECLS

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
  guint8 *quickStretchedPW;
  guint8 *authPW;
  guint8 *unwrapBKey;
} EphySyncCryptoStretchedCredentials;

typedef struct {
  guint8 *tokenID;
  guint8 *reqHMACkey;
  guint8 *respHMACkey;
  guint8 *respXORkey;
} EphySyncCryptoProcessedKFT;

typedef struct {
  guint8 *kA;
  guint8 *kB;
  guint8 *wrapKB;
} EphySyncCryptoSyncKeys;

EphySyncCryptoHawkOptions          *ephy_sync_crypto_hawk_options_new           (gchar *app,
                                                                                 gchar *dlg,
                                                                                 gchar *ext,
                                                                                 gchar *content_type,
                                                                                 gchar *hash,
                                                                                 gchar *local_time_offset,
                                                                                 gchar *nonce,
                                                                                 gchar *payload,
                                                                                 gchar *timestamp);

void                                ephy_sync_crypto_hawk_options_free          (EphySyncCryptoHawkOptions *hawk_options);

void                                ephy_sync_crypto_hawk_header_free           (EphySyncCryptoHawkHeader *hawk_header);

void                                ephy_sync_crypto_stretched_credentials_free (EphySyncCryptoStretchedCredentials *stretched_credentials);

void                                ephy_sync_crypto_processed_kft_free         (EphySyncCryptoProcessedKFT *processed_kft);

void                                ephy_sync_crypto_sync_keys_free             (EphySyncCryptoSyncKeys *sync_keys);

EphySyncCryptoStretchedCredentials *ephy_sync_crypto_stretch                    (const gchar *emailUTF8,
                                                                                 const gchar *passwordUTF8);

EphySyncCryptoProcessedKFT         *ephy_sync_crypto_process_key_fetch_token    (const gchar  *keyFetchToken);

EphySyncCryptoSyncKeys             *ephy_sync_crypto_retrieve_sync_keys         (const gchar *bundle,
                                                                                 guint8      *respHMACkey,
                                                                                 guint8      *respXORkey,
                                                                                 guint8      *unwrapBKey);

EphySyncCryptoHawkHeader           *ephy_sync_crypto_compute_hawk_header        (const gchar               *url,
                                                                                 const gchar               *method,
                                                                                 const gchar               *id,
                                                                                 guint8                    *key,
                                                                                 gsize                      key_length,
                                                                                 EphySyncCryptoHawkOptions *options);

G_END_DECLS

#endif
