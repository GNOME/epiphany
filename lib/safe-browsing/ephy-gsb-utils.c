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
#include "ephy-gsb-utils.h"

#include "ephy-debug.h"
#include "ephy-string.h"

#include <arpa/inet.h>
#include <json-glib/json-glib.h>
#include <libsoup/soup.h>
#include <string.h>

#define MAX_UNESCAPE_STEP 1024

EphyGSBThreatList *
ephy_gsb_threat_list_new (const char *threat_type,
                          const char *platform_type,
                          const char *threat_entry_type,
                          const char *client_state,
                          gint64      timestamp)
{
  EphyGSBThreatList *list;

  g_assert (threat_type);
  g_assert (platform_type);
  g_assert (threat_entry_type);

  list = g_slice_new (EphyGSBThreatList);
  list->threat_type = g_strdup (threat_type);
  list->platform_type = g_strdup (platform_type);
  list->threat_entry_type = g_strdup (threat_entry_type);
  list->client_state = g_strdup (client_state);
  list->timestamp = timestamp;

  return list;
}
void
ephy_gsb_threat_list_free (EphyGSBThreatList *list)
{
  g_assert (list);

  g_free (list->threat_type);
  g_free (list->platform_type);
  g_free (list->threat_entry_type);
  g_free (list->client_state);
  g_slice_free (EphyGSBThreatList, list);
}

static JsonObject *
ephy_gsb_utils_make_client_info (void)
{
  JsonObject *client_info;

  client_info = json_object_new ();
  json_object_set_string_member (client_info, "clientId", "Epiphany");
  json_object_set_string_member (client_info, "clientVersion", VERSION);

  return client_info;
}

static JsonObject *
ephy_gsb_utils_make_contraints (void)
{
  JsonObject *constraints;
  JsonArray *compressions;

  compressions = json_array_new ();
  json_array_add_string_element (compressions, "RAW");

  constraints = json_object_new ();
  /* No restriction for the number of update entries. */
  json_object_set_int_member (constraints, "maxUpdateEntries", 0);
  /* No restriction for the number of database entries. */
  json_object_set_int_member (constraints, "maxDatabaseEntries", 0);
  /* Let the server pick the geographic region automatically. */
  json_object_set_null_member (constraints, "region");
  json_object_set_array_member (constraints, "supportedCompressions", compressions);

  return constraints;
}

char *
ephy_gsb_utils_make_list_updates_request (GList *threat_lists)
{
  JsonArray *requests;
  JsonObject *body_obj;
  JsonNode *body_node;
  char *retval;

  g_assert (threat_lists);

  requests = json_array_new ();
  for (GList *l = threat_lists; l && l->data; l = l->next) {
    EphyGSBThreatList *list = (EphyGSBThreatList *)l->data;
    JsonObject *request = json_object_new ();

    json_object_set_string_member (request, "threatType", list->threat_type);
    json_object_set_string_member (request, "platformType", list->platform_type);
    json_object_set_string_member (request, "threatEntryType", list->threat_entry_type);
    json_object_set_string_member (request, "state", list->client_state);
    json_object_set_object_member (request, "constraints", ephy_gsb_utils_make_contraints ());
    json_array_add_object_element (requests, request);
  }

  body_obj = json_object_new ();
  json_object_set_object_member (body_obj, "client", ephy_gsb_utils_make_client_info ());
  json_object_set_array_member (body_obj, "listUpdateRequests", requests);

  body_node = json_node_new (JSON_NODE_OBJECT);
  json_node_set_object (body_node, body_obj);
  retval = json_to_string (body_node, FALSE);

  json_object_unref (body_obj);
  json_node_unref (body_node);

  return retval;
}

static char *
ephy_gsb_utils_full_unescape (const char *part)
{
  char *prev;
  char *prev_prev;
  char *retval;
  int attempts = 0;

  g_assert (part);

  prev = g_strdup (part);
  retval = soup_uri_decode (part);

  /* Iteratively unescape the string until it cannot be unescaped anymore.
   * This is useful for strings that have been escaped multiple times.
   */
  while (g_strcmp0 (prev, retval) != 0 && attempts++ < MAX_UNESCAPE_STEP) {
    prev_prev = prev;
    prev = retval;
    retval = soup_uri_decode (retval);
    g_free (prev_prev);
  }

  g_free (prev);

  return retval;
}

static char *
ephy_gsb_utils_escape (const char *part)
{
  const guchar *s = (const guchar *)part;
  GString *str;

  g_assert (part);

  str = g_string_new (NULL);

  /* Use this instead of soup_uri_encode() because that escapes other
   * characters that we don't want to be escaped.
   */
  while (*s) {
    if (*s < 0x20 || *s >= 0x7f || *s == ' ' || *s == '#' || *s == '%')
      g_string_append_printf (str, "%%%02X", *s++);
    else
      g_string_append_c (str, *s++);
  }

  return g_string_free (str, FALSE);
}

static char *
ephy_gsb_utils_normalize_escape (const char *part)
{
  char *tmp;
  char *retval;

  g_assert (part);

  /* Perform a full unescape and then escape the string exactly once. */
  tmp = ephy_gsb_utils_full_unescape (part);
  retval = ephy_gsb_utils_escape (tmp);

  g_free (tmp);

  return retval;
}

static char *
ephy_gsb_utils_canonicalize_host (const char *host)
{
  struct in_addr addr;
  char *tmp;
  char *trimmed;
  char *retval;

  g_assert (host);

  trimmed = g_strdup (host);
  ephy_string_remove_leading (trimmed, '.');
  ephy_string_remove_trailing (trimmed, '.');

  /* This actually replaces groups of consecutive dots with a single dot. */
  tmp = ephy_string_find_and_replace (trimmed, "..", ".");

  /* If host is as an IP address, normalize it to 4 dot-separated decimal values.
   * If host is not an IP address, then it's a string and needs to be lowercased.
   *
   * inet_aton() handles octal, hex and fewer than 4 components addresses.
   * See https://linux.die.net/man/3/inet_network
   */
  if (inet_aton (tmp, &addr) != 0) {
    retval = g_strdup (inet_ntoa (addr));
  } else {
    retval = g_ascii_strdown (tmp, -1);
  }

  g_free (trimmed);
  g_free (tmp);

  return retval;
}

/*
 * https://developers.google.com/safe-browsing/v4/urls-hashing#canonicalization
 */
char *
ephy_gsb_utils_canonicalize (const char *url)
{
  SoupURI *uri;
  char *tmp;
  char *host;
  char *path;
  char *host_canonical;
  char *path_canonical;
  char *retval;
  const char *query;

  g_assert (url);

  /* Handle URLs with no scheme. */
  if (g_str_has_prefix (url, "//"))
    tmp = g_strdup_printf ("http:%s", url);
  else if (g_str_has_prefix (url, "://"))
    tmp = g_strdup_printf ("http%s", url);
  else if (!strstr (url, "://"))
    tmp = g_strdup_printf ("http://%s", url);
  else
    tmp = g_strdup (url);

  /* soup_uri_new() prepares the URL for us:
   * 1. Strips trailing and leading whitespaces.
   * 2. Includes the path component if missing.
   * 3. Removes tab (0x09), CR (0x0d), LF (0x0a) characters.
   */
  uri = soup_uri_new (tmp);
  g_free (tmp);
  if (!uri) {
    LOG ("Cannot make SoupURI from URL %s", url);
    return NULL;
  }

  /* Remove fragment. */
  soup_uri_set_fragment (uri, NULL);

  /* Canonicalize host. */
  host = ephy_gsb_utils_normalize_escape (soup_uri_get_host (uri));
  host_canonical = ephy_gsb_utils_canonicalize_host (host);

  /* Canonicalize path. "/../" and "/./" have already been resolved by soup_uri_new(). */
  path = ephy_gsb_utils_normalize_escape (soup_uri_get_path (uri));
  path_canonical = ephy_string_find_and_replace (path, "//", "/");

  /* Combine all parts. */
  query = soup_uri_get_query (uri);
  if (query) {
    retval = g_strdup_printf ("%s://%s%s?%s",
                              soup_uri_get_scheme (uri),
                              host_canonical, path_canonical,
                              query);
  } else {
    retval = g_strdup_printf ("%s://%s%s",
                              soup_uri_get_scheme (uri),
                              host_canonical, path_canonical);
  }

  g_free (host);
  g_free (path);
  g_free (host_canonical);
  g_free (path_canonical);
  soup_uri_free (uri);

  return retval;
}
