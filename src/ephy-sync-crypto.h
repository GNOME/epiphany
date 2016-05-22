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

gchar *ephy_sync_crypto_kw          (const gchar *name);
gchar *ephy_sync_crypto_kwe         (const gchar *name,
                                     const gchar *emailUTF8);
gchar *ephy_sync_crypto_encode_hex  (guint8 *data,
                                     gsize   data_length);
void   ephy_sync_crypto_pbkdf2_1k   (guint8 *key,
                                     gsize   key_length,
                                     guint8 *salt,
                                     gsize   salt_length,
                                     guint8 *out,
                                     gsize   out_length);
void   ephy_sync_crypto_hkdf        (guint8 *in,
                                     gsize   in_length,
                                     guint8 *salt,
                                     gsize   salt_length,
                                     guint8 *info,
                                     gsize   info_length,
                                     guint8 *out,
                                     gsize   out_length);
/* FIXME: Only for debugging, remove when no longer needed */
void   ephy_sync_crypto_display_hex (guint8      *data,
                                     gsize        data_length,
                                     const gchar *data_name);

G_END_DECLS

#endif
