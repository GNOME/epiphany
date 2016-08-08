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

#include "config.h"
#include "ephy-sync-utils.h"

#include <libsoup/soup.h>
#include <string.h>

gchar *
ephy_sync_utils_build_json_string (const gchar *key,
                                   const gchar *value,
                                   ...)
{
  va_list args;
  gchar *json;
  gchar *next_key;
  gchar *next_value;
  gchar *tmp;

  json = g_strconcat ("{\"", key, "\": \"", value, "\"", NULL);
  va_start (args, value);

  while ((next_key = va_arg (args, gchar *)) != NULL) {
    next_value = va_arg (args, gchar *);
    tmp = json;
    json = g_strconcat (json, ", \"", next_key, "\": \"", next_value, "\"", NULL);
    g_free (tmp);
  }

  va_end (args);
  tmp = json;
  json = g_strconcat (json, "}", NULL);
  g_free (tmp);

  return json;
}

gchar *
ephy_sync_utils_create_bso_json (const gchar *id,
                                 const gchar *payload)
{
  return ephy_sync_utils_build_json_string ("id", id, "payload", payload, NULL);
}

gchar *
ephy_sync_utils_make_audience (const gchar *url)
{
  SoupURI *uri;
  const gchar *scheme;
  const gchar *host;
  gchar *audience;
  gchar *port;

  g_return_val_if_fail (url != NULL, NULL);

  uri = soup_uri_new (url);
  scheme = soup_uri_get_scheme (uri);
  host = soup_uri_get_host (uri);
  port = g_strdup_printf (":%u", soup_uri_get_port (uri));

  /* Even if the url doesn't contain the port, soup_uri_get_port() will return
   * the default port for the url's scheme so we need to check if the port was
   * really present in the url.
   */
  if (g_strstr_len (url, -1, port) != NULL)
    audience = g_strdup_printf ("%s://%s%s", scheme, host, port);
  else
    audience = g_strdup_printf ("%s://%s", scheme, host);

  g_free (port);
  soup_uri_free (uri);

  return audience;
}

const gchar *
ephy_sync_utils_token_name_from_type (EphySyncTokenType type)
{
  switch (type) {
    case TOKEN_UID:
      return "uid";
    case TOKEN_SESSIONTOKEN:
      return "sessionToken";
    case TOKEN_KEYFETCHTOKEN:
      return "keyFetchToken";
    case TOKEN_UNWRAPBKEY:
      return "unwrapBKey";
    case TOKEN_KA:
      return "kA";
    case TOKEN_KB:
      return "kB";
    default:
      g_assert_not_reached ();
  }
}

gchar *
ephy_sync_utils_find_and_replace (const gchar *src,
                                  const gchar *find,
                                  const gchar *repl)
{
  const gchar *haystack = src;
  const gchar *needle = NULL;
  gsize haystack_len = strlen (src);
  gsize find_len = strlen (find);
  gsize repl_len = strlen (repl);
  gsize new_len = 0;
  gsize skip_len = 0;
  gchar *out = g_malloc (haystack_len + 1);

  while ((needle = g_strstr_len (haystack, -1, find)) != NULL) {
    haystack_len += find_len - repl_len;
    out = g_realloc (out, haystack_len + 1);
    skip_len = needle - haystack;
    memcpy (out + new_len, haystack, skip_len);
    memcpy (out + new_len + skip_len, repl, repl_len);
    new_len += skip_len + repl_len;
    haystack = needle + find_len;
  }
  strcpy (out + new_len, haystack);

  return out;
}

guint8 *
ephy_sync_utils_concatenate_bytes (guint8 *bytes,
                                   gsize   bytes_len,
                                   ...)
{
  va_list args;
  guint8 *next;
  guint8 *out;
  gsize next_len;
  gsize out_len;

  out_len = bytes_len;
  out = g_malloc (out_len);
  memcpy (out, bytes, out_len);

  va_start (args, bytes_len);
  while ((next = va_arg (args, guint8 *)) != NULL) {
    next_len = va_arg (args, gsize);
    out = g_realloc (out, out_len + next_len);
    memcpy (out + out_len, next, next_len);
    out_len += next_len;
  }

  va_end (args);

  return out;
}

gint64
ephy_sync_utils_current_time_seconds (void)
{
  return g_get_real_time () / 1000000;
}
