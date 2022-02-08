/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © Red Hat Inc.
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
#include "ephy-output-encoding.h"

#include <glib.h>

#if !GLIB_CHECK_VERSION(2, 68, 0)
static guint
g_string_replace (GString     *string,
                  const gchar *find,
                  const gchar *replace,
                  guint        limit)
{
  gsize f_len, r_len, pos;
  gchar *cur, *next;
  guint n = 0;

  g_return_val_if_fail (string != NULL, 0);
  g_return_val_if_fail (find != NULL, 0);
  g_return_val_if_fail (replace != NULL, 0);

  f_len = strlen (find);
  r_len = strlen (replace);
  cur = string->str;

  while ((next = strstr (cur, find)) != NULL)
    {
      pos = next - string->str;
      g_string_erase (string, pos, f_len);
      g_string_insert (string, pos, replace);
      cur = string->str + pos + r_len;
      n++;
      /* Only match the empty string once at any given position, to
       * avoid infinite loops */
      if (f_len == 0)
        {
          if (cur[0] == '\0')
            break;
          else
            cur++;
        }
      if (n == limit)
        break;
    }

  return n;
}
#endif

char *
ephy_encode_for_html_entity (const char *input)
{
  GString *str = g_string_new (input);

  g_string_replace (str, "&", "&amp;", 0);
  g_string_replace (str, "<", "&lt;", 0);
  g_string_replace (str, ">", "&gt;", 0);
  g_string_replace (str, "\"", "&quot;", 0);
  g_string_replace (str, "'", "&#x27;", 0);
  g_string_replace (str, "/", "&#x2F;", 0);

  return g_string_free (str, FALSE);
}

char *
ephy_encode_for_html_attribute (const char *input)
{
  GString *str;
  const char *c = input;

  if (!g_utf8_validate (input, -1, NULL))
    return g_strdup ("");

  str = g_string_new (NULL);
  do {
    gunichar u = g_utf8_get_char (c);
    if (g_unichar_isalnum (u))
      g_string_append_unichar (str, u);
    else
      g_string_append_printf (str, "&#x%02x;", u);
    c = g_utf8_next_char (c);
  } while (*c);

  return g_string_free (str, FALSE);
}
