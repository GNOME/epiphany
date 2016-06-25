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

#include "ephy-debug.h"
#include "ephy-sync-utils.h"

#include <glib/gstdio.h>
#include <string.h>

static const gchar hex_digits[] = "0123456789abcdef";

gchar *
ephy_sync_utils_kw (const gchar *name)
{
  return g_strconcat ("identity.mozilla.com/picl/v1/", name, NULL);
}

gchar *
ephy_sync_utils_kwe (const gchar *name,
                     const gchar *emailUTF8)
{
  return g_strconcat ("identity.mozilla.com/picl/v1/", name, ":", emailUTF8, NULL);
}

gchar *
ephy_sync_utils_encode_hex (guint8 *data,
                            gsize   data_length)
{
  gchar *retval = g_malloc (data_length * 2 + 1);

  for (gsize i = 0; i < data_length; i++) {
    guint8 byte = data[i];

    retval[2 * i] = hex_digits[byte >> 4];
    retval[2 * i + 1] = hex_digits[byte & 0xf];
  }

  retval[data_length * 2] = 0;

  return retval;
}

guint8 *
ephy_sync_utils_decode_hex (const gchar *hex_string)
{
  guint8 *retval;
  gsize hex_length = strlen (hex_string);

  g_return_val_if_fail (hex_length % 2 == 0, NULL);

  retval = g_malloc (hex_length / 2);

  for (gsize i = 0, j = 0; i < hex_length; i += 2, j++) {
    sscanf(hex_string + i, "%2hhx", retval + j);
  }

  return retval;
}

const gchar *
ephy_sync_utils_token_name_from_type (EphySyncTokenType token_type)
{
  switch (token_type) {
  case EPHY_SYNC_TOKEN_AUTHPW:
    return "authPw";
  case EPHY_SYNC_TOKEN_KEYFETCHTOKEN:
    return "keyFetchToken";
  case EPHY_SYNC_TOKEN_SESSIONTOKEN:
    return "sessionToken";
  case EPHY_SYNC_TOKEN_UID:
    return "uid";
  case EPHY_SYNC_TOKEN_UNWRAPBKEY:
    return "unwrapBKey";
  case EPHY_SYNC_TOKEN_QUICKSTRETCHEDPW:
    return "quickStretchedPW";
  default:
    g_assert_not_reached ();
  }
}

/* FIXME: Only for debugging, remove when no longer needed */
void
ephy_sync_utils_display_hex (const gchar *data_name,
                             guint8      *data,
                             gsize        data_length)
{
LOG ("%s:", data_name);
for (gsize i = 0; i < data_length; i++)
  LOG ("%02x", data[i]);
}
