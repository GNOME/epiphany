/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2017 Gabriel Ivascu <ivascu.gabriel59@gmail.com>
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

#include <glib.h>
#include <libsecret/secret.h>

G_BEGIN_DECLS

const SecretSchema *ephy_sync_utils_get_secret_schema (void) G_GNUC_CONST;

#define ACCOUNT_KEY "firefox_account"
#define EPHY_SYNC_SECRET_SCHEMA (ephy_sync_utils_get_secret_schema ())

#define TOKEN_SERVER_URL "https://token.services.mozilla.com/1.0/sync/1.5"
#define FIREFOX_ACCOUNTS_SERVER_URL "https://api.accounts.firefox.com/v1"

char   *ephy_sync_utils_encode_hex                  (const guint8 *data,
                                                     gsize         data_len);
guint8 *ephy_sync_utils_decode_hex                  (const char   *hex);
char   *ephy_sync_utils_base64_urlsafe_encode       (const guint8 *data,
                                                     gsize         data_len,
                                                     gboolean      should_strip);
guint8 *ephy_sync_utils_base64_urlsafe_decode       (const char   *text,
                                                     gsize        *out_len,
                                                     gboolean      should_fill);
void    ephy_sync_utils_generate_random_bytes       (void         *random_ctx,
                                                     gsize         num_bytes,
                                                     guint8       *out);
char   *ephy_sync_utils_get_audience                (const char   *url);
char   *ephy_sync_utils_get_random_sync_id          (void);

G_END_DECLS
