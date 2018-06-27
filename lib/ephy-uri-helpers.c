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
#include <unicode/uidna.h>
#include <webkit2/webkit2.h>

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

  ret = g_strjoinv ("&", (char **)array->pdata);
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
    { "utm_source", NULL },
    { "utm_medium", NULL },
    { "utm_term", NULL },
    { "utm_content", NULL },
    { "utm_campaign", NULL },
    { "utm_reader", NULL },
    /* metrika.yandex.ru */
    { "yclid", NULL },
    /* youtube.com */
    { "feature", "youtube.com" },
    /* facebook.com */
    { "fb_action_ids", NULL },
    { "fb_action_types", NULL },
    { "fb_ref", NULL },
    { "fb_source", NULL },
    { "action_object_map", NULL },
    { "action_type_map", NULL },
    { "action_ref_map", NULL },
    { "ref", "facebook.com" },
    { "fref", "facebook.com" },
    { "hc_location", "facebook.com" },
    /* imdb.com */
    { "ref_", "imdb.com" },
    /* addons.mozilla.org */
    { "src", "addons.mozilla.org" }
  };
  guint i;

  if (name == NULL)
    return FALSE;

  for (i = 0; i < G_N_ELEMENTS (fields); i++) {
    if (fields[i].host != NULL &&
        !g_str_has_suffix (host, fields[i].host))
      continue;
    if (!g_strcmp0 (fields[i].field, name))
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

  g_list_free_full (items, (GDestroyNotify)query_item_free);
  g_list_free (new_items);

 bail:
  soup_uri_free (uri);
  return ret;
}

static inline void
script_table_update (GHashTable     *table,
                     GUnicodeScript  script)
{
  gpointer value;
  gpointer new_value;

  value = g_hash_table_lookup (table, GINT_TO_POINTER (script));
  new_value = GINT_TO_POINTER (GPOINTER_TO_INT (value) + 1);
  g_hash_table_replace (table, GINT_TO_POINTER (script), new_value);
}

static inline int
script_table_get (GHashTable     *table,
                  GUnicodeScript  script)
{
  gpointer value;

  value = g_hash_table_lookup (table, GINT_TO_POINTER (script));
  return GPOINTER_TO_INT (value);
}

/**
 * validate_unicode_label:
 * @label: a domain label, UTF-8 encoded
 *
 * Verifies whether @label is safe to be displayed as Unicode characters, as per
 * this algorithm: https://wiki.mozilla.org/IDN_Display_Algorithm#Algorithm. If
 * %FALSE is returned, then @label should be displayed as Punycode text.
 *
 * Return value: %TRUE if @label is considered safe, %FALSE otherwise
 **/
static gboolean
validate_unicode_label (const char *label)
{
  GHashTable *table;
  GUnicodeScript script;
  gunichar *unichars;
  gunichar saved_zero_char = 0;
  gboolean retval = FALSE;
  long num;

  g_assert (label);

  if (!g_utf8_validate (label, -1, NULL))
    return FALSE;

  /* Use a hash table to count the occurrences of every script,
   * except Common and Inherited. */
  table = g_hash_table_new (g_direct_hash, g_direct_equal);
  unichars = g_utf8_to_ucs4_fast (label, -1, &num);

  for (gunichar *u = unichars; u && *u; u++) {
    script = g_unichar_get_script (*u);

    if (script != G_UNICODE_SCRIPT_COMMON && script != G_UNICODE_SCRIPT_INHERITED)
      script_table_update (table, script);
    else
      num--;

    /* Check for mixed numbering systems. */
    if (g_unichar_isdigit (*u)) {
      gunichar zero_char = *u - g_unichar_digit_value (*u);
      if (saved_zero_char == 0)
        saved_zero_char = zero_char;
      else if (zero_char != saved_zero_char)
        goto out;
    }
  }

  /* Single script, allow. */
  if (g_hash_table_size (table) < 2) {
    retval = TRUE;
    goto out;
  }

  /* Chinese scripts. */
  if (script_table_get (table, G_UNICODE_SCRIPT_LATIN) +
      script_table_get (table, G_UNICODE_SCRIPT_HAN) +
      script_table_get (table, G_UNICODE_SCRIPT_BOPOMOFO) == num) {
    retval = TRUE;
    goto out;
  }

  /* Korean scripts. */
  if (script_table_get (table, G_UNICODE_SCRIPT_LATIN) +
      script_table_get (table, G_UNICODE_SCRIPT_HAN) +
      script_table_get (table, G_UNICODE_SCRIPT_HANGUL) == num) {
    retval = TRUE;
    goto out;
  }

  /* Japanese scripts. */
  if (script_table_get (table, G_UNICODE_SCRIPT_LATIN) +
      script_table_get (table, G_UNICODE_SCRIPT_HAN) +
      script_table_get (table, G_UNICODE_SCRIPT_HIRAGANA) +
      script_table_get (table, G_UNICODE_SCRIPT_KATAKANA) == num) {
    retval = TRUE;
    goto out;
  }

  /* Ban mixes of more than two scripts. */
  if (g_hash_table_size (table) > 2)
    goto out;

  /* Ban any mix of two scrips that doesn't contain Latin. */
  if (script_table_get (table, G_UNICODE_SCRIPT_LATIN) == 0)
    goto out;

  /* Ban Latin + Cyrillic or Latin + Greek. */
  if (script_table_get (table, G_UNICODE_SCRIPT_CYRILLIC) > 0 ||
      script_table_get (table, G_UNICODE_SCRIPT_GREEK) > 0)
    goto out;

  /* Allow Latin + any other single script. */
  retval = TRUE;

out:
  g_hash_table_unref (table);
  g_free (unichars);

  return retval;
}

static char *
evaluate_host_for_display (const char *original_host,
                           const char *unicode_host)
{
  char **original_labels;
  char **unicode_labels;
  char *retval;

  g_assert (original_host);
  g_assert (unicode_host);

  /* These arrays will have the same length. */
  original_labels = g_strsplit (original_host, ".", -1);
  unicode_labels = g_strsplit (unicode_host, ".", -1);

  for (guint i = 0; i < g_strv_length (unicode_labels); i++) {
    if (!validate_unicode_label (unicode_labels[i])) {
      g_free (unicode_labels[i]);
      unicode_labels[i] = g_strdup (original_labels[i]);
    }
  }

  retval = g_strjoinv (".", unicode_labels);
  g_strfreev (original_labels);
  g_strfreev (unicode_labels);

  return retval;
}


/* Use this function to format a URI for display. The URIs used
 * internally by WebKit may contain percent-encoded characters or
 * punycode, which we do not want the user to see.
 *
 * Note this should probably be handled by WebKit instead.
 */
char *
ephy_uri_decode (const char *uri_string)
{
  static const guint MAX_DOMAIN_LENGTH = 255; /* RFC 1034, section 3.1 */
  static UIDNA *idna = NULL;
  static GMutex idna_creation_mutex;
  SoupURI *uri;
  char *percent_encoded_uri;
  char *percent_decoded_host;
  char *idna_decoded_name;
  char *fully_decoded_uri;
  UIDNAInfo info = UIDNA_INFO_INITIALIZER;
  UErrorCode error = U_ZERO_ERROR;

  /* This function is not null-safe since it is mostly used in scenarios where
   * passing or returning null would typically lead to a security issue. */
  g_assert (uri_string);

  /* This object is threadsafe to *use*, but need to create it exactly once. */
  g_mutex_lock (&idna_creation_mutex);
  if (idna == NULL) {
    /* These flags should be synced with URLParser::internationalDomainNameTranscoder
     * in WebKit's URLParser.cpp. */
    idna = uidna_openUTS46 (UIDNA_CHECK_BIDI | UIDNA_CHECK_CONTEXTJ | UIDNA_NONTRANSITIONAL_TO_UNICODE | UIDNA_NONTRANSITIONAL_TO_ASCII, &error);
    if (U_FAILURE (error))
      g_error ("ICU error opening UTS #46 context: %d", error);
  }
  g_mutex_unlock (&idna_creation_mutex);

  uri = soup_uri_new (uri_string);
  if (uri == NULL)
    return g_strdup (uri_string);

  /* Process any punycode in the host portion of the URI. */
  if (uri->host != NULL) {
    /* +1 so there is space for the trailing NUL with the longest-possible
     * domain name. +2 because ICU has this rather terrible behavior of
     * sometimes returning a result that's not NUL-terminated if the buffer
     * capacity exactly matches the output length, indicating that with a
     * warning code that's not caught by U_FAILURE. Our buffer is large enough
     * for any valid domain, but this function may receive invalid domains as
     * input. */
    idna_decoded_name = g_malloc0 (MAX_DOMAIN_LENGTH + 2);
    uidna_nameToUnicodeUTF8 (idna, uri->host, -1, idna_decoded_name, MAX_DOMAIN_LENGTH + 1, &info, &error);

    if (U_FAILURE (error)) {
      g_warning ("ICU error converting domain %s for display: %d", uri->host, error);
      soup_uri_free (uri);
      g_free (idna_decoded_name);
      return g_strdup (uri_string);
    }

    percent_decoded_host = soup_uri_decode (uri->host);
    g_free (uri->host);
    uri->host = evaluate_host_for_display (percent_decoded_host, idna_decoded_name);
    g_free (percent_decoded_host);
    g_free (idna_decoded_name);
  }

  /* Note: this also strips passwords from the display URI. */
  percent_encoded_uri = soup_uri_to_string (uri, FALSE);
  soup_uri_free (uri);

  /* Now, decode any percent-encoded characters in the URI. If there are null
   * characters or escaped slashes, this returns NULL, so just display the
   * encoded URI in that case. */
  fully_decoded_uri = g_uri_unescape_string (percent_encoded_uri, "/");
  if (fully_decoded_uri == NULL)
    return percent_encoded_uri;
  g_free (percent_encoded_uri);
  return fully_decoded_uri;
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
  const char *result;

  /* Convert to URI containing only protocol, host, and port. */
  origin = webkit_security_origin_new_for_uri (uri_string);
  result = webkit_security_origin_to_string (origin);
  webkit_security_origin_unref (origin);

  /* May be NULL. */
  return result;
}
