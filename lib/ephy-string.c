/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2002 Marco Pesenti Gritti
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
#include "ephy-string.h"

#include <errno.h>
#include <glib.h>
#include <libsoup/soup.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define ELLIPSIS "\xe2\x80\xa6"

gboolean
ephy_string_to_int (const char *string, gulong *integer)
{
  gulong result;
  char *parse_end;

  /* Check for the case of an empty string. */
  if (string == NULL || *string == '\0')
    return FALSE;

  /* Call the standard library routine to do the conversion. */
  errno = 0;
  result = strtol (string, &parse_end, 0);

  /* Check that the result is in range. */
  if (errno == ERANGE)
    return FALSE;

  /* Check that all the trailing characters are spaces. */
  while (*parse_end != '\0') {
    if (!g_ascii_isspace (*parse_end++))
      return FALSE;
  }

  /* Return the result. */
  *integer = result;
  return TRUE;
}

char *
ephy_string_blank_chr (char *source)
{
  char *p;

  if (source == NULL)
    return NULL;

  p = source;
  while (*p != '\0') {
    if ((guchar) * p < 0x20)
      *p = ' ';

    p++;
  }

  return source;
}

/**
 * ephy_string_shorten: shortens a string
 * @str: the string to shorten, in UTF-8
 * @target_length: the length of the shortened string (in characters)
 *
 * If @str is already short enough, it is returned. Otherwise a new string
 * is allocated and @str is consumed.
 *
 * Return value: a newly allocated string, not longer than target_length
 * characters.
 */
char *
ephy_string_shorten (char *str,
                     gsize target_length)
{
  char *new_str;
  glong actual_length;
  gulong bytes;

  g_return_val_if_fail (target_length > 0, NULL);

  if (!str)
    return NULL;

  /* FIXME: this function is a big mess. While it is utf-8 safe now,
   * it can still split a sequence of combining characters.
   */
  actual_length = g_utf8_strlen (str, -1);

  /* if the string is already short enough, or if it's too short for
   * us to shorten it, return a new copy */
  if ((gsize)actual_length <= target_length)
    return str;

  /* create string */
  bytes = GPOINTER_TO_UINT (g_utf8_offset_to_pointer (str, target_length - 1) - str);

  new_str = g_new (gchar, bytes + strlen (ELLIPSIS) + 1);

  strncpy (new_str, str, bytes);
  strncpy (new_str + bytes, ELLIPSIS, strlen (ELLIPSIS));
  new_str[bytes + strlen (ELLIPSIS)] = '\0';

  g_free (str);

  return new_str;
}

/* This is a collation key that is very very likely to sort before any
   collation key that libc strxfrm generates. We use this before any
   special case (dot or number) to make sure that its sorted before
   anything else.
 */
#define COLLATION_SENTINEL "\1\1\1"

/**
 * ephy_string_collate_key_for_domain:
 * @host:
 * @len: the length of @host, or -1 to use the entire null-terminated @host string
 *
 * Return value: a collation key for @host.
 */
char *
ephy_string_collate_key_for_domain (const char *str,
                                    gssize      len)
{
  GString *result;
  const char *dot;
  gssize newlen;

  if (len < 0)
    len = strlen (str);

  result = g_string_sized_new (len + 6 * strlen (COLLATION_SENTINEL));

  /* Note that we could do even better by using
   * g_utf8_collate_key_for_filename on the dot-separated
   * components, but this seems good enough for now.
   */
  while ((dot = g_strrstr_len (str, len, ".")) != NULL) {
    newlen = dot - str;

    g_string_append_len (result, dot + 1, len - newlen - 1);
    g_string_append (result, COLLATION_SENTINEL);

    len = newlen;
  }

  if (len > 0)
    g_string_append_len (result, str, len);

  return g_string_free (result, FALSE);
}

char *
ephy_string_get_host_name (const char *url)
{
  SoupURI *uri;
  char *ret;

  if (url == NULL ||
      g_str_has_prefix (url, "file://") ||
      g_str_has_prefix (url, "about:") ||
      g_str_has_prefix (url, "ephy-about:"))
    return NULL;

  uri = soup_uri_new (url);
  /* If uri is NULL it's very possible that we just got
   * something without a scheme, let's try to prepend
   * 'http://' */
  if (uri == NULL) {
    char *effective_url = g_strconcat ("http://", url, NULL);
    uri = soup_uri_new (effective_url);
    g_free (effective_url);
  }

  if (uri == NULL) return NULL;

  ret = g_strdup (uri->host);
  soup_uri_free (uri);

  return ret;
}

/**
 * ephy_string_commandline_args_to_uris:
 * @arguments: a %NULL-terminated array of chars.
 *
 * Transform commandline arguments to URIs if they are native,
 * otherwise simply transform them to UTF-8.
 *
 * Returns: a newly allocated array with the URIs and
 * UTF-8 strings.
 **/
char **
ephy_string_commandline_args_to_uris (char **arguments, GError **error)
{
  gchar **args;
  GFile *file;
  guint i;

  if (arguments == NULL)
    return NULL;

  args = g_malloc0 (sizeof (gchar *) * (g_strv_length (arguments) + 1));

  for (i = 0; arguments[i] != NULL; ++i) {
    file = g_file_new_for_commandline_arg (arguments [i]);
    if (g_file_is_native (file) && g_file_query_exists (file, NULL)) {
      args[i] = g_file_get_uri (file);
    } else {
      args[i] = g_locale_to_utf8 (arguments [i], -1,
                                  NULL, NULL, error);
      if (error && *error) {
        g_strfreev (args);
        return NULL;
      }
    }
    g_object_unref (file);
  }

  return args;
}

char *
ephy_string_find_and_replace (const char *string,
                              const char *to_find,
                              const char *to_repl)
{
  const char *haystack = string;
  const char *needle = NULL;
  char *out;
  gsize haystack_len;
  gsize to_find_len;
  gsize to_repl_len;
  gsize new_len = 0;
  gsize skip_len = 0;

  g_return_val_if_fail (string, NULL);
  g_return_val_if_fail (to_find, NULL);
  g_return_val_if_fail (to_repl, NULL);

  haystack_len = strlen (string);
  to_find_len = strlen (to_find);
  to_repl_len = strlen (to_repl);
  out = g_malloc (haystack_len + 1);

  while ((needle = g_strstr_len (haystack, -1, to_find)) != NULL) {
    haystack_len += to_find_len - to_repl_len;
    out = g_realloc (out, haystack_len + 1);
    skip_len = needle - haystack;
    memcpy (out + new_len, haystack, skip_len);
    memcpy (out + new_len + skip_len, to_repl, to_repl_len);
    new_len += skip_len + to_repl_len;
    haystack = needle + to_find_len;
  }
  strcpy (out + new_len, haystack);

  return out;
}

char **
ephy_strv_append (const char * const *strv,
                  const char         *str)
{
  char **new_strv;
  char **n;
  const char * const *s;
  guint len;

  if (g_strv_contains (strv, str))
    return g_strdupv ((char **)strv);

  /* Needs room for one more string than before, plus one for trailing NULL. */
  len = g_strv_length ((char **)strv);
  new_strv = g_malloc ((len + 1 + 1) * sizeof (char *));
  n = new_strv;
  s = strv;

  while (*s != NULL) {
    *n = g_strdup (*s);
    n++;
    s++;
  }
  new_strv[len] = g_strdup (str);
  new_strv[len + 1] = NULL;

  return new_strv;
}

char **
ephy_strv_remove (const char * const *strv,
                  const char         *str)
{
  char **new_strv;
  char **n;
  const char * const *s;
  guint len;

  if (!g_strv_contains (strv, str))
    return g_strdupv ((char **)strv);

  /* Needs room for one fewer string than before, plus one for trailing NULL. */
  len = g_strv_length ((char **)strv);
  new_strv = g_malloc ((len - 1 + 1) * sizeof (char *));
  n = new_strv;
  s = strv;

  while (*s != NULL) {
    if (strcmp (*s, str) != 0) {
      *n = g_strdup (*s);
      n++;
    }
    s++;
  }
  new_strv[len - 1] = NULL;

  return new_strv;
}
