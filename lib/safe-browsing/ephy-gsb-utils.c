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

#define MAX_HOST_SUFFIXES 5
#define MAX_PATH_PREFIXES 6
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

EphyGSBHashPrefixLookup *
ephy_gsb_hash_prefix_lookup_new (const guint8 *prefix,
                                 gsize         length,
                                 const char   *threat_type,
                                 const char   *platform_type,
                                 const char   *threat_entry_type,
                                 gboolean      negative_expired)
{
  EphyGSBHashPrefixLookup *lookup;

  g_assert (prefix);
  g_assert (threat_type);
  g_assert (platform_type);
  g_assert (threat_entry_type);

  lookup = g_slice_new (EphyGSBHashPrefixLookup);
  lookup->prefix = g_bytes_new (prefix, length);
  lookup->threat_type = g_strdup (threat_type);
  lookup->platform_type = g_strdup (platform_type);
  lookup->threat_entry_type = g_strdup (threat_entry_type);
  lookup->negative_expired = negative_expired;

  return lookup;
}

void
ephy_gsb_hash_prefix_lookup_free (EphyGSBHashPrefixLookup *lookup)
{
  g_assert (lookup);

  g_bytes_unref (lookup->prefix);
  g_free (lookup->threat_type);
  g_free (lookup->platform_type);
  g_free (lookup->threat_entry_type);
  g_slice_free (EphyGSBHashPrefixLookup, lookup);
}

EphyGSBHashFullLookup *
ephy_gsb_hash_full_lookup_new (const guint8 *hash,
                               const char   *threat_type,
                               const char   *platform_type,
                               const char   *threat_entry_type,
                               gboolean      expired)
{
  EphyGSBHashFullLookup *lookup;

  g_assert (hash);
  g_assert (threat_type);
  g_assert (platform_type);
  g_assert (threat_entry_type);

  lookup = g_slice_new (EphyGSBHashFullLookup);
  lookup->hash = g_malloc (GSB_HASH_SIZE);
  memcpy (lookup->hash, hash, GSB_HASH_SIZE);
  lookup->threat_type = g_strdup (threat_type);
  lookup->platform_type = g_strdup (platform_type);
  lookup->threat_entry_type = g_strdup (threat_entry_type);
  lookup->expired = expired;

  return lookup;
}

void
ephy_gsb_hash_full_lookup_free (EphyGSBHashFullLookup *lookup)
{
  g_assert (lookup);

  g_free (lookup->hash);
  g_free (lookup->threat_type);
  g_free (lookup->platform_type);
  g_free (lookup->threat_entry_type);
  g_slice_free (EphyGSBHashFullLookup, lookup);
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

char *
ephy_gsb_utils_make_full_hashes_request (GList *threat_lists,
                                         GList *hash_prefixes)
{
  GHashTable *threat_types_set;
  GHashTable *platform_types_set;
  GHashTable *threat_entry_types_set;
  GList *threat_types_list;
  GList *platform_types_list;
  GList *threat_entry_types_list;
  JsonArray *threat_types;
  JsonArray *platform_types;
  JsonArray *threat_entry_types;
  JsonArray *threat_entries;
  JsonArray *client_states;
  JsonObject *threat_info;
  JsonObject *body_obj;
  JsonNode *body_node;
  char *body;

  g_assert (threat_lists);
  g_assert (hash_prefixes);

  client_states = json_array_new ();
  threat_types_set = g_hash_table_new (g_str_hash, g_str_equal);
  platform_types_set = g_hash_table_new (g_str_hash, g_str_equal);
  threat_entry_types_set = g_hash_table_new (g_str_hash, g_str_equal);

  for (GList *l = threat_lists; l && l->data; l = l->next) {
    EphyGSBThreatList *list = (EphyGSBThreatList *)l->data;

    if (!g_hash_table_contains (threat_types_set, list->threat_type))
      g_hash_table_add (threat_types_set, list->threat_type);
    if (!g_hash_table_contains (platform_types_set, list->platform_type))
      g_hash_table_add (platform_types_set, list->platform_type);
    if (!g_hash_table_contains (threat_entry_types_set, list->threat_entry_type))
      g_hash_table_add (threat_entry_types_set, list->threat_entry_type);

    json_array_add_string_element (client_states, list->client_state);
  }

  threat_types = json_array_new ();
  threat_types_list = g_hash_table_get_keys (threat_types_set);
  for (GList *l = threat_types_list; l && l->data; l = l->next)
    json_array_add_string_element (threat_types, (const char *)l->data);

  platform_types = json_array_new ();
  platform_types_list = g_hash_table_get_keys (platform_types_set);
  for (GList *l = platform_types_list; l && l->data; l = l->next)
    json_array_add_string_element (platform_types, (const char *)l->data);

  threat_entry_types = json_array_new ();
  threat_entry_types_list = g_hash_table_get_keys (threat_entry_types_set);
  for (GList *l = threat_entry_types_list; l && l->data; l = l->next)
    json_array_add_string_element (threat_entry_types, (const char *)l->data);

  threat_entries = json_array_new ();
  for (GList *l = hash_prefixes; l && l->data; l = l->next) {
    JsonObject *threat_entry = json_object_new ();
    char *hash = g_base64_encode (g_bytes_get_data (l->data, NULL),
                                  g_bytes_get_size (l->data));

    json_object_set_string_member (threat_entry, "hash", hash);
    json_array_add_object_element (threat_entries, threat_entry);

    g_free (hash);
  }

  threat_info = json_object_new ();
  json_object_set_array_member (threat_info, "threatTypes", threat_types);
  json_object_set_array_member (threat_info, "platformTypes", platform_types);
  json_object_set_array_member (threat_info, "threatEntryTypes", threat_entry_types);
  json_object_set_array_member (threat_info, "threatEntries", threat_entries);

  body_obj = json_object_new ();
  json_object_set_object_member (body_obj, "client", ephy_gsb_utils_make_client_info ());
  json_object_set_array_member (body_obj, "clientStates", client_states);
  json_object_set_object_member (body_obj, "threatInfo", threat_info);
  json_object_set_null_member (body_obj, "apiClient");

  body_node = json_node_new (JSON_NODE_OBJECT);
  json_node_set_object (body_node, body_obj);
  body = json_to_string (body_node, TRUE);

  g_list_free (threat_types_list);
  g_list_free (platform_types_list);
  g_list_free (threat_entry_types_list);
  g_hash_table_unref (threat_types_set);
  g_hash_table_unref (platform_types_set);
  g_hash_table_unref (threat_entry_types_set);
  json_object_unref (body_obj);
  json_node_unref (body_node);

  return body;
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
ephy_gsb_utils_canonicalize (const char  *url,
                             char       **host_out,
                             char       **path_out,
                             char       **query_out)
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

  if (host_out)
    *host_out = g_strdup (host_canonical);
  if (path_out)
    *path_out = g_strdup (path_canonical);
  if (query_out)
    *query_out = g_strdup (query);

  g_free (host);
  g_free (path);
  g_free (host_canonical);
  g_free (path_canonical);
  soup_uri_free (uri);

  return retval;
}

/*
 * https://developers.google.com/safe-browsing/v4/urls-hashing#suffixprefix-expressions
 */
static GList *
ephy_gsb_utils_compute_host_suffixes (const char *host)
{
  GList *retval = NULL;
  struct in_addr addr;
  char **tokens;
  int steps;
  int start;
  int num_tokens;

  g_assert (host);

  retval = g_list_prepend (retval, g_strdup (host));

  /* If host is an IP address, return immediately. */
  if (inet_aton (host, &addr) != 0)
    return retval;

  tokens = g_strsplit (host, ".", -1);
  num_tokens = g_strv_length (tokens);
  start = MAX (num_tokens - MAX_HOST_SUFFIXES, 1);
  steps = MIN (num_tokens - 1 - start, MAX_HOST_SUFFIXES - 1);

  for (int i = start; i < start + steps; i++)
    retval = g_list_prepend (retval, g_strjoinv (".", tokens + i));

  g_strfreev (tokens);

  return g_list_reverse (retval);
}

/*
 * https://developers.google.com/safe-browsing/v4/urls-hashing#suffixprefix-expressions
 */
static GList *
ephy_gsb_utils_compute_path_prefixes (const char *path,
                                      const char *query)
{
  GList *retval = NULL;
  char *no_trailing;
  char **tokens;
  int steps;
  int num_tokens;
  int no_trailing_len;
  gboolean has_trailing;

  g_assert (path);

  if (query)
    retval = g_list_prepend (retval, g_strjoin ("?", path, query, NULL));
  retval = g_list_prepend (retval, g_strdup (path));

  if (!g_strcmp0 (path, "/"))
    return retval;

  has_trailing = path[strlen (path) - 1] == '/';
  no_trailing = ephy_string_remove_trailing (g_strdup (path), '/');
  no_trailing_len = strlen (no_trailing);

  tokens = g_strsplit (no_trailing, "/", -1);
  num_tokens = g_strv_length (tokens);
  steps = MIN (num_tokens, MAX_PATH_PREFIXES - 2);

  for (int i = 0; i < steps; i++) {
    char *value = g_strconcat (i > 0 ? retval->data : "", tokens[i], "/", NULL);

    if ((has_trailing && !g_strcmp0 (value, path)) ||
        (!has_trailing && !strncmp (value, no_trailing, no_trailing_len))) {
      g_free (value);
      break;
    }

    retval = g_list_prepend (retval, value);
  }

  g_free (no_trailing);
  g_strfreev (tokens);

  return g_list_reverse (retval);
}

GList *
ephy_gsb_utils_compute_hashes (const char *url)
{
  GChecksum *checksum;
  GList *retval = NULL;
  GList *host_suffixes;
  GList *path_prefixes;
  char *url_canonical;
  char *host = NULL;
  char *path = NULL;
  char *query = NULL;
  gsize hash_len = GSB_HASH_SIZE;

  g_assert (url);

  url_canonical = ephy_gsb_utils_canonicalize (url, &host, &path, &query);
  if (!url_canonical)
    return NULL;

  host_suffixes = ephy_gsb_utils_compute_host_suffixes (host);
  path_prefixes = ephy_gsb_utils_compute_path_prefixes (path, query);
  checksum = g_checksum_new (G_CHECKSUM_SHA256);

  /* Get the hash of every host-path combination.
   * The maximum number of combinations is MAX_HOST_SUFFIXES * MAX_PATH_PREFIXES.
   */
  for (GList *h = host_suffixes; h && h->data; h = h->next) {
    for (GList *p = path_prefixes; p && p->data; p = p->next) {
      char *value = g_strconcat (h->data, p->data, NULL);
      guint8 *hash = g_malloc (hash_len);

      g_checksum_reset (checksum);
      g_checksum_update (checksum, (const guint8 *)value, strlen (value));
      g_checksum_get_digest (checksum, hash, &hash_len);
      retval = g_list_prepend (retval, hash);

      g_free (value);
    }
  }

  g_free (host);
  g_free (path);
  g_free (query);
  g_free (url_canonical);
  g_checksum_free (checksum);
  g_list_free_full (host_suffixes, g_free);
  g_list_free_full (path_prefixes, g_free);

  return g_list_reverse (retval);
}
