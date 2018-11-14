/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2003, 2004 Christian Persch
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
#include "ephy-langs.h"

#include <string.h>

/* sanitise the languages list according to the rules for HTTP accept-language
 * in RFC 2616, Sect. 14.4
 */
void
ephy_langs_sanitise (GArray *array)
{
  char *lang1, *lang2;
  int i, j;

  /* if we have 'xy-ab' in list but not 'xy', append 'xy' */
  for (i = 0; i < (int)array->len; i++) {
    gboolean found = FALSE;
    char *dash, *prefix;

    lang1 = (char *)g_array_index (array, char *, i);

    dash = strchr (lang1, '-');
    if (dash == NULL)
      continue;

    for (j = i + 1; j < (int)array->len; j++) {
      lang2 = (char *)g_array_index (array, char *, j);
      if (strchr (lang2, '-') == NULL &&
          g_str_has_prefix (lang1, lang2)) {
        found = TRUE;
      }
    }

    if (found == FALSE) {
      prefix = g_strndup (lang1, dash - lang1);
      g_array_append_val (array, prefix);
    }
  }

  /* uniquify */
  for (i = 0; i < (int)array->len - 1; i++) {
    for (j = (int)array->len - 1; j > i; j--) {
      lang1 = (char *)g_array_index (array, char *, i);
      lang2 = (char *)g_array_index (array, char *, j);

      if (strcmp (lang1, lang2) == 0) {
        g_array_remove_index (array, j);
        g_free (lang2);
      }
    }
  }

  /* move 'xy' code behind all 'xy-ab' codes */
  for (i = (int)array->len - 2; i >= 0; i--) {
    for (j = (int)array->len - 1; j > i; j--) {
      lang1 = (char *)g_array_index (array, char *, i);
      lang2 = (char *)g_array_index (array, char *, j);

      if (strchr (lang1, '-') == NULL &&
          strchr (lang2, '-') != NULL &&
          g_str_has_prefix (lang2, lang1)) {
        g_array_insert_val (array, j + 1, lang1);
        g_array_remove_index (array, i);
        break;
      }
    }
  }
}

void
ephy_langs_append_languages (GArray *array)
{
  const char * const *languages;
  char *lang;
  int i;

  languages = g_get_language_names ();
  g_assert (languages != NULL);

  for (i = 0; languages[i] != NULL; i++) {
    if (strstr (languages[i], ".") == 0 &&
        strstr (languages[i], "@") == 0 &&
        strcmp (languages[i], "C") != 0) {
      /* change '_' to '-' */
      lang = g_strdelimit (g_strdup (languages[i]), "_", '-');

      g_array_append_val (array, lang);
    }
  }

  /* Fallback: add "en" if list is empty */
  if (array->len == 0) {
    lang = g_strdup ("en");
    g_array_append_val (array, lang);
  }
}

char **
ephy_langs_get_languages (void)
{
  GArray *array;

  array = g_array_new (TRUE, FALSE, sizeof (char *));

  ephy_langs_append_languages (array);

  ephy_langs_sanitise (array);

  return (char **)(void *)g_array_free (array, FALSE);
}
