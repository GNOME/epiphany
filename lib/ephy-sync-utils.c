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

#include "config.h"
#include "ephy-sync-utils.h"

#include "ephy-settings.h"

#include <libsoup/soup.h>
#include <stdio.h>
#include <string.h>

#define SYNC_ID_LEN 12

static const char hex_digits[] = "0123456789abcdef";

const SecretSchema *
ephy_sync_utils_get_secret_schema (void)
{
  static const SecretSchema schema = {
    "org.epiphany.SyncSecrets", SECRET_SCHEMA_NONE,
    {
      { ACCOUNT_KEY, SECRET_SCHEMA_ATTRIBUTE_STRING },
      { "NULL", 0 },
    }
  };

  return &schema;
}

char *
ephy_sync_utils_encode_hex (const guint8 *data,
                            gsize         data_len)
{
  char *encoded;

  g_assert (data);

  encoded = g_malloc (data_len * 2 + 1);
  for (gsize i = 0; i < data_len; i++) {
    guint8 byte = data[i];

    encoded[2 * i] = hex_digits[byte >> 4];
    encoded[2 * i + 1] = hex_digits[byte & 0xf];
  }
  encoded[data_len * 2] = 0;

  return encoded;
}

guint8 *
ephy_sync_utils_decode_hex (const char *hex)
{
  guint8 *decoded;

  g_assert (hex);

  decoded = g_malloc (strlen (hex) / 2);
  for (gsize i = 0, j = 0; i < strlen (hex); i += 2, j++)
    sscanf (hex + i, "%2hhx", decoded + j);

  return decoded;
}

static void
base64_to_base64_urlsafe (char *text)
{
  g_assert (text);

  /* / and + are inappropriate for URLs and file systems paths, so they have to
   * be omitted to make the base64 string safe. / is replaced with _ and + is
   * replaced with -.
   */
  g_strcanon (text, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789=/", '-');
  g_strcanon (text, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789=-", '_');
}

char *
ephy_sync_utils_base64_urlsafe_encode (const guint8 *data,
                                       gsize         data_len,
                                       gboolean      should_strip)
{
  char *base64;
  char *out;
  gsize start = 0;
  gssize end;

  g_assert (data);

  base64 = g_base64_encode (data, data_len);
  end = strlen (base64) - 1;

  /* Strip the data of any leading or trailing '=' characters. */
  if (should_strip) {
    while (start < strlen (base64) && base64[start] == '=')
      start++;

    while (end >= 0 && base64[end] == '=')
      end--;
  }

  out = g_strndup (base64 + start, end - start + 1);
  base64_to_base64_urlsafe (out);

  g_free (base64);

  return out;
}

static void
base64_urlsafe_to_base64 (char *text)
{
  g_assert (text);

  /* Replace '-' with '+' and '_' with '/' */
  g_strcanon (text, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789=_", '+');
  g_strcanon (text, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789=+", '/');
}

guint8 *
ephy_sync_utils_base64_urlsafe_decode (const char   *text,
                                       gsize        *out_len,
                                       gboolean      should_fill)
{
  guint8 *out;
  char *to_decode;
  char *suffix = NULL;

  g_assert (text);
  g_assert (out_len);

  /* Fill the text with trailing '=' characters up to the proper length. */
  if (should_fill)
    suffix = g_strnfill ((4 - strlen (text) % 4) % 4, '=');

  to_decode = g_strconcat (text, suffix, NULL);
  base64_urlsafe_to_base64 (to_decode);
  out = g_base64_decode (to_decode, out_len);

  g_free (suffix);
  g_free (to_decode);

  return out;
}

/*
 * This is mainly required by Nettle's RSA support.
 * From Nettle's documentation: random_ctx and random is a randomness generator.
 * random(random_ctx, length, dst) should generate length random octets and store them at dst.
 * We don't really use random_ctx, since we have /dev/urandom available.
 */
void
ephy_sync_utils_generate_random_bytes (void   *random_ctx,
                                       gsize   num_bytes,
                                       guint8 *out)
{
  FILE *fp;

  g_assert (num_bytes > 0);
  g_assert (out);

  fp = fopen ("/dev/urandom", "r");
  fread (out, sizeof (guint8), num_bytes, fp);
  fclose (fp);
}

char *
ephy_sync_utils_get_audience (const char *url)
{
  SoupURI *uri;
  const char *scheme;
  const char *host;
  char *audience;
  char *port;

  g_assert (url);

  uri = soup_uri_new (url);
  scheme = soup_uri_get_scheme (uri);
  host = soup_uri_get_host (uri);
  /* soup_uri_get_port returns the default port if URI does not have any port. */
  port = g_strdup_printf (":%u", soup_uri_get_port (uri));

  if (g_strstr_len (url, -1, port))
    audience = g_strdup_printf ("%s://%s%s", scheme, host, port);
  else
    audience = g_strdup_printf ("%s://%s", scheme, host);

  g_free (port);
  soup_uri_free (uri);

  return audience;
}

char *
ephy_sync_utils_get_random_sync_id (void)
{
  char *id;
  char *base64;
  guint8 *bytes;
  gsize bytes_len;

  /* The sync id is a base64-urlsafe string. Base64 uses 4 chars to represent 3 bytes,
   * therefore we need ceil(len * 3 / 4) bytes to cover the requested length. */
  bytes_len = (SYNC_ID_LEN + 3) / 4 * 3;
  bytes = g_malloc (bytes_len);

  ephy_sync_utils_generate_random_bytes (NULL, bytes_len, bytes);
  base64 = ephy_sync_utils_base64_urlsafe_encode (bytes, bytes_len, FALSE);
  id = g_strndup (base64, SYNC_ID_LEN);

  g_free (base64);
  g_free (bytes);

  return id;
}

void
ephy_sync_utils_set_device_id (const char *id)
{
  id = id ? id : "";
  g_settings_set_string (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_DEVICE_ID, id);
}

char *
ephy_sync_utils_get_device_id (void)
{
  char *id;

  id = g_settings_get_string (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_DEVICE_ID);
  if (!g_strcmp0 (id, "")) {
    g_free (id);
    id = ephy_sync_utils_get_random_sync_id ();
  }

  return id;
}

void
ephy_sync_utils_set_device_name (const char *name)
{
  name = name ? name : "";
  g_settings_set_string (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_DEVICE_NAME, name);
}

char *
ephy_sync_utils_get_device_name (void)
{
  char *name;

  name = g_settings_get_string (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_DEVICE_NAME);
  if (!g_strcmp0 (name, "")) {
    g_free (name);
    name = g_strdup_printf ("%s's GNOME Web on %s",
                            g_get_user_name (), g_get_host_name ());
  }

  return name;
}

void
ephy_sync_utils_set_sync_user (const char *user)
{
  user = user ? user : "";
  g_settings_set_string (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_USER, user);
}

char *
ephy_sync_utils_get_sync_user (void)
{
  char *user = g_settings_get_string (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_USER);

  if (!g_strcmp0 (user, "")) {
    g_free (user);
    return NULL;
  }

  return user;
}

gboolean
ephy_sync_utils_user_is_signed_in (void)
{
  char *user = ephy_sync_utils_get_sync_user ();

  if (user) {
    g_free (user);
    return TRUE;
  }

  return FALSE;
}

void
ephy_sync_utils_set_sync_time (gint64 time)
{
  time = time > 0 ? time : 0;
  g_settings_set_int64 (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_TIME, time);
}

gint64
ephy_sync_utils_get_sync_time (void)
{
  return g_settings_get_int64 (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_TIME);
}

guint
ephy_sync_utils_get_sync_frequency (void)
{
  /* Minutes. */
  return g_settings_get_uint (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_FREQUENCY);
}

gboolean
ephy_sync_utils_sync_with_firefox (void)
{
  return g_settings_get_boolean (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_WITH_FIREFOX);
}

gboolean
ephy_sync_utils_bookmarks_sync_is_enabled (void)
{
  return g_settings_get_boolean (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_BOOKMARKS_ENABLED);
}

void
ephy_sync_utils_set_bookmarks_sync_time (double time)
{
  g_settings_set_double (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_BOOKMARKS_TIME, time);
}

double
ephy_sync_utils_get_bookmarks_sync_time (void)
{
  return g_settings_get_double (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_BOOKMARKS_TIME);
}

void
ephy_sync_utils_set_bookmarks_sync_is_initial (gboolean is_initial)
{
  g_settings_set_boolean (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_BOOKMARKS_INITIAL, is_initial);
}

gboolean
ephy_sync_utils_get_bookmarks_sync_is_initial (void)
{
  return g_settings_get_boolean (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_BOOKMARKS_INITIAL);
}

gboolean
ephy_sync_utils_passwords_sync_is_enabled (void)
{
  return g_settings_get_boolean (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_PASSWORDS_ENABLED);
}

void
ephy_sync_utils_set_passwords_sync_time (double time)
{
  g_settings_set_double (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_PASSWORDS_TIME, time);
}

double
ephy_sync_utils_get_passwords_sync_time (void)
{
  return g_settings_get_double (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_PASSWORDS_TIME);
}

void
ephy_sync_utils_set_passwords_sync_is_initial (gboolean is_initial)
{
  g_settings_set_boolean (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_PASSWORDS_INITIAL, is_initial);
}

gboolean
ephy_sync_utils_get_passwords_sync_is_initial (void)
{
  return g_settings_get_boolean (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_PASSWORDS_INITIAL);
}

gboolean
ephy_sync_utils_history_sync_is_enabled (void)
{
  return g_settings_get_boolean (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_HISTORY_ENABLED);
}

void
ephy_sync_utils_set_history_sync_time (double time)
{
  g_settings_set_double (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_HISTORY_TIME, time);
}

double
ephy_sync_utils_get_history_sync_time (void)
{
  return g_settings_get_double (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_HISTORY_TIME);
}

void
ephy_sync_utils_set_history_sync_is_initial (gboolean is_initial)
{
  g_settings_set_boolean (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_HISTORY_INITIAL, is_initial);
}

gboolean
ephy_sync_utils_get_history_sync_is_initial (void)
{
  return g_settings_get_boolean (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_HISTORY_INITIAL);
}

gboolean
ephy_sync_utils_open_tabs_sync_is_enabled (void)
{
  return g_settings_get_boolean (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_OPEN_TABS_ENABLED);
}

void
ephy_sync_utils_set_open_tabs_sync_time (double time)
{
  g_settings_set_double (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_OPEN_TABS_TIME, time);
}

double
ephy_sync_utils_get_open_tabs_sync_time (void)
{
  return g_settings_get_double (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_OPEN_TABS_TIME);
}
