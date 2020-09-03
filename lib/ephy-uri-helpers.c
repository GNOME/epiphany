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
#include <libsoup/soup.h>
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
  SoupURI *uri;
  char *encoded_uri;

  if (!uri_string || !*uri_string)
    return NULL;

  uri = soup_uri_new (uri_string);
  if (!uri)
    return g_strdup (uri_string);

  encoded_uri = soup_uri_normalize (uri_string, NULL);
  soup_uri_free (uri);

  return encoded_uri;
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
