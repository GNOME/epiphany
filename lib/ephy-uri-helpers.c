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

/* QueryItem holds a query parameter name/value pair. The name is unescaped in
 * query_decode() with form_decode(), the value is not altered. */
typedef struct {
  char *name;
  char *value;
} QueryItem;

static void
query_item_free (QueryItem *item)
{
  g_free (item->name);
  /* value is actually part of the name allocation,
   * see query_decode() */
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

static void
append_form_encoded (GString *str, const char *in)
{
  const unsigned char *s = (const unsigned char *)in;

  while (*s) {
    if (*s == ' ') {
      g_string_append_c (str, '+');
      s++;
    } else if (!g_ascii_isalnum (*s))
      g_string_append_printf (str, "%%%02X", (int)*s++);
    else
      g_string_append_c (str, *s++);
  }
}

static void
encode_pair (GString *str, const char *name, const char *value)
{
  g_return_if_fail (name != NULL);
  g_return_if_fail (value != NULL);

  if (str->len)
    g_string_append_c (str, '&');
  append_form_encoded (str, name);
  g_string_append_c (str, '=');
  g_string_append (str, value);
}

/* Adapted from soup_form_decode in libsoup */
static GList *
query_decode (const char *query)
{
  GList *items;
  char **pairs, *eq, *name, *value;
  int i;

  items = NULL;
  pairs = g_strsplit (query, "&", -1);
  for (i = 0; pairs[i]; i++) {
    QueryItem *item;

    name = pairs[i];
    eq = strchr (name, '=');
    if (eq) {
      *eq = '\0';
      value = eq + 1;
    } else
      value = NULL;
    if (!value || !form_decode (name)) {
      g_free (name);
      continue;
    }

    item = g_slice_new0 (QueryItem);
    item->name = name;
    item->value = value;
    items = g_list_prepend (items, item);
  }
  g_free (pairs);

  return g_list_reverse (items);
}

static char *
query_encode (GList *items)
{
  GList *l;
  GString *str;

  if (!items)
    return NULL;

  str = g_string_new (NULL);
  for (l = items; l != NULL; l = l->next) {
    QueryItem *item = l->data;

    encode_pair (str, item->name, item->value);
  }

  return g_string_free (str, FALSE);
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
 * @uri: a uri
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
  char *new_query;
  gboolean has_garbage = FALSE;
  char *ret = NULL;

  uri = soup_uri_new (uri_string);
  if (!uri)
    return ret;

  host = soup_uri_get_host (uri);
  query = soup_uri_get_query (uri);
  if (!query)
    goto bail;

  items = query_decode (query);
  if (!items)
    goto bail;

  new_items = NULL;
  for (l = items; l != NULL; l = l->next) {
    QueryItem *item = l->data;

    if (!is_garbage (item->name, host))
      new_items = g_list_prepend (new_items, item);
    else
      has_garbage = TRUE;
  }

  if (has_garbage) {
    new_items = g_list_reverse (new_items);
    new_query = query_encode (new_items);

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
/* vim: set sw=2 ts=2 sts=2 et: */
