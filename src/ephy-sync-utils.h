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

#ifndef EPHY_SYNC_UTILS_H
#define EPHY_SYNC_UTILS_H

#include <glib-object.h>

typedef enum {
  TOKEN_UID,
  TOKEN_SESSIONTOKEN,
  TOKEN_KEYFETCHTOKEN,
  TOKEN_UNWRAPBKEY,
  TOKEN_KA,
  TOKEN_KB
} EphySyncTokenType;

G_BEGIN_DECLS

char       *ephy_sync_utils_build_json_string    (const char *key,
                                                  const char *value,
                                                  ...) G_GNUC_NULL_TERMINATED;
char       *ephy_sync_utils_create_bso_json      (const char *id,
                                                  const char *payload);
char       *ephy_sync_utils_make_audience        (const char *url);
const char *ephy_sync_utils_token_name_from_type (EphySyncTokenType type);
char       *ephy_sync_utils_find_and_replace     (const char *src,
                                                  const char *find,
                                                  const char *repl);
guint8     *ephy_sync_utils_concatenate_bytes    (guint8 *bytes,
                                                  gsize   bytes_len,
                                                  ...) G_GNUC_NULL_TERMINATED;
gint64      ephy_sync_utils_current_time_seconds  (void);

G_END_DECLS

#endif
