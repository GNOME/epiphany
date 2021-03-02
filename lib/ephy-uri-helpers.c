/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2013 Bastien Nocera <hadess@hadess.net>
 *  Copyright © 2016 Igalia S.L.
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
#include "ephy-uri-helpers.h"

#include <glib.h>
#include <string.h>
#include <webkit2/webkit2.h>

/* Use this function to format a URI for display. The URIs used
 * internally by WebKit may contain percent-encoded characters or
 * punycode, which we do not want the user to see.
 */
char *
ephy_uri_decode (const char *uri_string)
{
  char *decoded_uri;

  /* This function is not null-safe since it is mostly used in scenarios where
   * passing or returning null would typically lead to a security issue. */
  g_assert (uri_string);

  decoded_uri = webkit_uri_for_display (uri_string);
  return decoded_uri ? decoded_uri : g_strdup (uri_string);
}

char *
ephy_uri_normalize (const char *uri_string)
{
  g_autoptr (GUri) uri = NULL;

  if (!uri_string || !*uri_string)
    return NULL;

  uri = g_uri_parse (uri_string, G_URI_FLAGS_SCHEME_NORMALIZE, NULL);
  if (!uri)
    return g_strdup (uri_string);

  return g_uri_to_string (uri);
}

char *
ephy_uri_to_security_origin (const char *uri_string)
{
  WebKitSecurityOrigin *origin;
  char *result;

  /* Convert to URI containing only protocol, host, and port. */
  origin = webkit_security_origin_new_for_uri (uri_string);
  result = webkit_security_origin_to_string (origin);
  webkit_security_origin_unref (origin);

  /* May be NULL. */
  return result;
}

#define XDIGIT(c) ((c) <= '9' ? (c) - '0' : ((c) & 0x4F) - 'A' + 10)
#define HEXCHAR(s) ((XDIGIT (s[1]) << 4) + XDIGIT (s[2]))

char *
ephy_uri_unescape (const char *uri_string)
{
  unsigned char *s, *d;
  char *decoded;

  g_assert (uri_string);

  decoded = g_strdup (uri_string);
  s = d = (unsigned char *)decoded;
  do {
    if (*s == '%') {
      if (s[1] == '\0' ||
          s[2] == '\0' ||
          !g_ascii_isxdigit (s[1]) ||
          !g_ascii_isxdigit (s[2])) {
        *d++ = *s;
        continue;
      }
      *d++ = HEXCHAR (s);
      s += 2;
    } else
      *d++ = *s;
  } while (*s++);

  return decoded;
}
