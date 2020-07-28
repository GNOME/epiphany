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

#pragma once

#include <glib.h>
#include <libsecret/secret.h>

G_BEGIN_DECLS

const SecretSchema *ephy_sync_utils_get_secret_schema (void) G_GNUC_CONST;

#define EPHY_SYNC_SECRET_SCHEMA       (ephy_sync_utils_get_secret_schema ())
#define EPHY_SYNC_SECRET_ACCOUNT_KEY  "firefox_account"

#define EPHY_SYNC_STORAGE_VERSION 5
#define EPHY_SYNC_DEVICE_ID_LEN   32
#define EPHY_SYNC_BSO_ID_LEN      12

#define EPHY_SYNC_BATCH_SIZE    80
#define EPHY_SYNC_MAX_BATCHES   80

char     *ephy_sync_utils_encode_hex                    (const guint8 *data,
                                                         gsize         data_len);
guint8   *ephy_sync_utils_decode_hex                    (const char   *hex);

char     *ephy_sync_utils_base64_urlsafe_encode         (const guint8 *data,
                                                         gsize         data_len,
                                                         gboolean      should_strip);
guint8   *ephy_sync_utils_base64_urlsafe_decode         (const char *text,
                                                         gsize      *out_len,
                                                         gboolean    should_fill);

void      ephy_sync_utils_generate_random_bytes         (void   *random_ctx,
                                                         gsize   num_bytes,
                                                         guint8 *out);
char     *ephy_sync_utils_get_audience                  (const char *url);
char     *ephy_sync_utils_get_random_sync_id            (void);

char     *ephy_sync_utils_make_client_record            (const char *device_bso_id,
                                                         const char *device_id,
                                                         const char *device_name);

void      ephy_sync_utils_set_device_id                 (const char *id);
char     *ephy_sync_utils_get_device_id                 (void);
char     *ephy_sync_utils_get_device_bso_id             (void);

void      ephy_sync_utils_set_device_name               (const char *name);
char     *ephy_sync_utils_get_device_name               (void);

void      ephy_sync_utils_set_sync_user                 (const char *user);
char     *ephy_sync_utils_get_sync_user                 (void);
gboolean  ephy_sync_utils_user_is_signed_in             (void);

void      ephy_sync_utils_set_sync_time                 (gint64 time);
gint64    ephy_sync_utils_get_sync_time                 (void);

guint     ephy_sync_utils_get_sync_frequency            (void);
gboolean  ephy_sync_utils_sync_with_firefox             (void);

gboolean  ephy_sync_utils_bookmarks_sync_is_enabled     (void);
void      ephy_sync_utils_set_bookmarks_sync_time       (gint64 time);
gint64    ephy_sync_utils_get_bookmarks_sync_time       (void);
void      ephy_sync_utils_set_bookmarks_sync_is_initial (gboolean is_initial);
gboolean  ephy_sync_utils_get_bookmarks_sync_is_initial (void);

gboolean  ephy_sync_utils_passwords_sync_is_enabled     (void);
void      ephy_sync_utils_set_passwords_sync_time       (gint64 time);
gint64    ephy_sync_utils_get_passwords_sync_time       (void);
void      ephy_sync_utils_set_passwords_sync_is_initial (gboolean is_initial);
gboolean  ephy_sync_utils_get_passwords_sync_is_initial (void);

gboolean  ephy_sync_utils_history_sync_is_enabled       (void);
void      ephy_sync_utils_set_history_sync_time         (gint64 time);
gint64    ephy_sync_utils_get_history_sync_time         (void);
void      ephy_sync_utils_set_history_sync_is_initial   (gboolean is_initial);
gboolean  ephy_sync_utils_get_history_sync_is_initial   (void);

gboolean  ephy_sync_utils_open_tabs_sync_is_enabled     (void);
void      ephy_sync_utils_set_open_tabs_sync_time       (gint64 time);
gint64    ephy_sync_utils_get_open_tabs_sync_time       (void);

char     *ephy_sync_utils_get_token_server              (void);
char     *ephy_sync_utils_get_accounts_server           (void);

G_END_DECLS
