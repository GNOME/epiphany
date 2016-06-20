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

#include "ephy-sync-crypto.h"

#include <nettle/hmac.h>
#include <nettle/pbkdf2.h>
#include <string.h>

/*
 * Runs 1000 iterations of PBKDF2.
 * Uses sha256 as hash function.
 */
void
ephy_sync_crypto_pbkdf2_1k (guint8 *key,
                            gsize   key_length,
                            guint8 *salt,
                            gsize   salt_length,
                            guint8 *out,
                            gsize   out_length)
{
  pbkdf2_hmac_sha256 (key_length, key, 1000, salt_length, salt, out_length, out);
}

/*
 * HMAC-based Extract-and-Expand Key Derivation Function.
 * Uses sha256 as hash function.
 * https://tools.ietf.org/html/rfc5869
 */
void
ephy_sync_crypto_hkdf (guint8 *in,
                       gsize   in_length,
                       guint8 *salt,
                       gsize   salt_length,
                       guint8 *info,
                       gsize   info_length,
                       guint8 *out,
                       gsize   out_length)
{
  struct hmac_sha256_ctx ctx;
  const gsize hash_length = 32;
  gsize i, offset = 0;
  guint8 *tmp, *prk;
  guint8 counter;

  if (out_length > hash_length * 255)
    return;

  /* If salt value was not provided, use an array of hash_length zeros */
  if (salt == NULL) {
    salt = g_malloc0 (hash_length);
    salt_length = hash_length;
  }

  tmp = g_malloc0 (hash_length + info_length + 1);
  prk = g_malloc0 (hash_length);

  /* Step 1: Extract */
  hmac_sha256_set_key (&ctx, salt_length, salt);
  hmac_sha256_update (&ctx, in_length, in);
  hmac_sha256_digest (&ctx, hash_length, prk);

  /* Step 2: Expand */
  hmac_sha256_set_key (&ctx, hash_length, prk);

  for (i = 0, counter = 1; i < out_length; i += hash_length, counter++) {
    memcpy (tmp + offset, info, info_length);
    tmp[offset + info_length] = counter;

    hmac_sha256_update (&ctx, offset + info_length + 1, tmp);
    hmac_sha256_digest (&ctx, hash_length, tmp);

    offset = hash_length;

    memcpy (out + i, tmp, hash_length);
  }

  g_free (salt);
  g_free (tmp);
  g_free (prk);
}
