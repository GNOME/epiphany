/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2013 Bastien Nocera <hadess@hadess.net>
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "ephy-uri-helpers.h"

#include <glib.h>
#include <libsoup/soup.h>
#include <string.h>

/**
 * SECTION:ephy-uri-helpers
 * @short_description: miscellaneous URI related utility functions
 *
 * URI related functions, including functions to clean up URI.
 */

/* QueryItem holds the decoded name for each parameter, as well as the untouched
 * name/value pair. The name is unescaped in query_decode() with form_decode(),
 * the pair is not altered. */
typedef struct {
  char *decoded_name;
  char *pair;
} QueryItem;

static void
query_item_free (QueryItem *item)
{
  g_free (item->decoded_name);
  g_free (item->pair);
  g_slice_free (QueryItem, item);
}

#define XDIGIT(c) ((c) <= '9' ? (c) - '0' : ((c) & 0x4F) - 'A' + 10)
#define HEXCHAR(s) ((XDIGIT (s[1]) << 4) + XDIGIT (s[2]))

/* From libsoup, in libsoup/soup-form.c */
static gboolean
form_decode (char *part)
{
  unsigned char *s, *d;

  s = d = (unsigned char *)part;
  do {
    if (*s == '%') {
      if (!g_ascii_isxdigit (s[1]) ||
          !g_ascii_isxdigit (s[2]))
        return FALSE;
      *d++ = HEXCHAR (s);
      s += 2;
    } else if (*s == '+')
      *d++ = ' ';
    else
      *d++ = *s;
  } while (*s++);

  return TRUE;
}

static GList *
query_split (const char *query)
{
  GList *items;
  char **pairs;
  int i;

  items = NULL;
  pairs = g_strsplit (query, "&", -1);
  for (i = 0; pairs[i]; i++) {
    QueryItem *item;
    char *decoded_name = NULL;
    char *pair, *eq;

    pair = pairs[i];
    eq = strchr (pair, '=');
    if (eq)
      decoded_name = g_strndup (pair, eq - pair);
    else
      decoded_name = g_strdup (pair);

    if (!form_decode (decoded_name)) {
      g_free (decoded_name);
      decoded_name = NULL;
    }

    item = g_slice_new0 (QueryItem);
    item->decoded_name = decoded_name;
    item->pair = pair;
    items = g_list_prepend (items, item);
  }
  g_free (pairs);

  return g_list_reverse (items);
}

static char *
query_concat (GList *items)
{
  GList *l;
  GPtrArray *array;
  char *ret;

  if (!items)
    return NULL;

  array = g_ptr_array_new ();

  for (l = items; l != NULL; l = l->next) {
    QueryItem *item = l->data;

    g_ptr_array_add (array, item->pair);
  }
  g_ptr_array_add (array, NULL);

  ret = g_strjoinv ("&", (char **) array->pdata);
  g_ptr_array_free (array, TRUE);

  return ret;
}

static gboolean
is_garbage (const char *name,
            const char *host)
{
  struct {
    const char *field;
    const char *host;
  } const fields[] = {
    /* analytics.google.com */
    { "utm_source",		NULL },
    { "utm_medium",		NULL },
    { "utm_term",		NULL },
    { "utm_content",	NULL },
    { "utm_campaign",	NULL },
    { "utm_reader",	NULL },
    /* metrika.yandex.ru */
    { "yclid",		NULL },
    /* youtube.com */
    { "feature",		"youtube.com" },
    /* facebook.com */
    { "fb_action_ids",	NULL},
    { "fb_action_types",	NULL },
    { "fb_ref",		NULL },
    { "fb_source",		NULL },
    { "action_object_map",	NULL },
    { "action_type_map",	NULL },
    { "action_ref_map",	NULL },
    { "ref",		"facebook.com" },
    { "fref",		"facebook.com" },
    { "hc_location",	"facebook.com" },
    /* imdb.com */
    { "ref_",		"imdb.com" },
    /* addons.mozilla.org */
    { "src",		"addons.mozilla.org" }
  };
  guint i;

  if (name == NULL)
    return FALSE;

  for (i = 0; i < G_N_ELEMENTS (fields); i++) {
    if (fields[i].host != NULL &&
        !g_str_has_suffix (host, fields[i].host))
      continue;
    if (g_str_equal (fields[i].field, name))
      return TRUE;
  }

  return FALSE;
}

/**
 * ephy_remove_tracking_from_uri:
 * @uri_string: a uri
 *
 * Sanitize @uri to make sure it does not contain analytics tracking
 * information. Inspired by the Firefox PureURL add-on:
 * https://addons.mozilla.org/fr/firefox/addon/pure-url/
 *
 * Returns: the sanitized uri, or %NULL on error or when the URI did
 * not change.
 */
char *
ephy_remove_tracking_from_uri (const char *uri_string)
{
  SoupURI *uri;
  GList *items, *new_items, *l;
  const char *query, *host;
  gboolean has_garbage = FALSE;
  char *ret = NULL;

  uri = soup_uri_new (uri_string);
  if (!uri)
    return ret;

  host = soup_uri_get_host (uri);
  query = soup_uri_get_query (uri);
  if (!query)
    goto bail;

  items = query_split (query);
  if (!items)
    goto bail;

  new_items = NULL;
  for (l = items; l != NULL; l = l->next) {
    QueryItem *item = l->data;

    if (!is_garbage (item->decoded_name, host))
      new_items = g_list_prepend (new_items, item);
    else
      has_garbage = TRUE;
  }

  if (has_garbage) {
    char *new_query;

    new_items = g_list_reverse (new_items);
    new_query = query_concat (new_items);

    soup_uri_set_query (uri, new_query);
    g_free (new_query);

    ret = soup_uri_to_string (uri, FALSE);
  }

  g_list_free_full (items, (GDestroyNotify) query_item_free);
  g_list_free (new_items);

bail:
  soup_uri_free (uri);
  return ret;
}

char *
ephy_uri_safe_unescape (const char *uri_string)
{
  char *decoded_uri;

  /* This function is not null-safe since it is mostly used in scenarios where
   * passing or returning null would typically lead to a security issue. */
  g_return_val_if_fail (uri_string, g_strdup (""));

  /* Protect against escaped null characters and escaped slashes. */
  decoded_uri = g_uri_unescape_string (uri_string, "/");
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

/* vim: set sw=2 ts=2 sts=2 et: */
