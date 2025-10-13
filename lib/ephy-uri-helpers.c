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
#include <webkit/webkit.h>

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

  uri = g_uri_parse (uri_string, G_URI_FLAGS_PARSE_RELAXED | G_URI_FLAGS_ENCODED, NULL);
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

char *
ephy_uri_get_base_domain (const char *hostname)
{
  g_autofree char *lowercase_hostname = NULL;
  const char *base_domain;

  if (!hostname)
    return NULL;

  lowercase_hostname = g_utf8_strdown (hostname, -1);
  if (!lowercase_hostname)
    return NULL;

  base_domain = soup_tld_get_base_domain (lowercase_hostname, NULL);
  if (!base_domain)
    return g_steal_pointer (&lowercase_hostname);

  return g_strdup (base_domain);
}

char *
ephy_uri_get_decoded_host (const char *decoded_uri)
{
  g_autofree char *copy = g_strdup (decoded_uri);
  const char *protocol_end;
  const char *authority_start;
  const char *first_colon;
  const char *first_slash;
  const char *first_question_mark;
  const char *first_octothorpe;
  const char *first_hostname_terminator;
  const char *last_at_sign;

  /* Get host component without using GUri, such that the original URL is not
   * modified in any way and the return value is guaranteed to be a substring of
   * the input. Notably, this avoids GUri performing punycode encoding.
   *
   * The algorithm is based on WebKit's applyHostNameFunctionToURLString in
   * URLHelpers.cpp (License: BSD-3-Clause).
   */

  protocol_end = strstr (copy, "://");
  if (!protocol_end)
    return NULL;

  authority_start = protocol_end + strlen ("://");

  first_colon = g_utf8_strchr (authority_start, -1, ':');
  first_slash = g_utf8_strchr (authority_start, -1, '/');
  first_question_mark = g_utf8_strchr (authority_start, -1, '?');
  first_octothorpe = g_utf8_strchr (authority_start, -1, '#');

  first_hostname_terminator = first_colon;
  if (!first_hostname_terminator || (first_slash && first_slash < first_hostname_terminator))
    first_hostname_terminator = first_slash;
  if (!first_hostname_terminator || (first_question_mark && first_question_mark < first_hostname_terminator))
    first_hostname_terminator = first_question_mark;
  if (!first_hostname_terminator || (first_octothorpe && first_octothorpe < first_hostname_terminator))
    first_hostname_terminator = first_octothorpe;

  if (first_hostname_terminator)
    ((char *)first_hostname_terminator)[0] = '\0';

  last_at_sign = g_utf8_strrchr (authority_start, -1, '@');
  if (last_at_sign)
    return g_strdup (last_at_sign + 1);

  return g_strdup (authority_start);
}
