/*
 *  Copyright (C) 2002 Marco Pesenti Gritti
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "ephy-string.h"

#include <string.h>
#include <glib.h>
#include <libgnome/libgnome.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs-mime.h>
#include <libxml/parser.h>

/**
 * ephy_string_shorten: returns a newly allocated shortened version of str.
 * the new string will be no longer than target_length characters, and will
 * be of the form "http://blahblah...blahblah.html".
 */
gchar *
ephy_string_shorten (const gchar *str, gint target_length)
{
	gchar *new_str;
	gint actual_length, first_length, second_length;

	if (!str) return NULL;

	actual_length = strlen (str);

	/* if the string is already short enough, or if it's too short for
	 * us to shorten it, return a new copy */
	if (actual_length <= target_length ||
	    actual_length <= 3)
		return g_strdup (str);

	/* allocate new string */
	new_str = g_new (gchar, target_length + 1);

	/* calc lengths to take from beginning and ending of str */
	second_length = (target_length - 3) / 2;
	first_length = target_length - 3 - second_length;

	/* create string */
	strncpy (new_str, str, first_length);
	strncpy (new_str + first_length, "...", 3);
	strncpy (new_str + first_length + 3,
		 str + actual_length - second_length, second_length);
	new_str[target_length] = '\0';

	return new_str;
}

char *
ephy_string_double_underscores (const char *string)
{
        int underscores;
        const char *p;
        char *q;
        char *escaped;

        if (string == NULL) {
                return NULL;
        }

        underscores = 0;
        for (p = string; *p != '\0'; p++) {
                underscores += (*p == '_');
        }

        if (underscores == 0) {
                return g_strdup (string);
        }

        escaped = g_new (char, strlen (string) + underscores + 1);
        for (p = string, q = escaped; *p != '\0'; p++, q++) {
                /* Add an extra underscore. */
                if (*p == '_') {
                        *q++ = '_';
                }
                *q = *p;
        }
        *q = '\0';

        return escaped;
}

/**
 * ephy_string_store_time_in_string:
 * NOTE: str must be at least 256 chars long
 */
void
ephy_string_store_time_in_string (GDate *t, gchar *str, const char *format)
{
	int length;

	if (t > 0)
	{
		/* format into string */
		/* this is used whenever a brief date is needed, like
		 * in the history (for last visited, first time visited) */
		length = g_date_strftime (str, 255,
				          format ? format : _("%Y-%m-%d"), t);
		str[length] = '\0';
	}
	else
	{
		str[0] = '\0';
	}
}

/**
 * ephy_string_time_to_string:
 */
gchar *
ephy_string_time_to_string (GDate *t,
			    const char *format)
{
	gchar str[256];

	/* write into stack string */
	ephy_string_store_time_in_string (t, str, format);

	/* copy in heap and return */
	return g_strdup (str);
}

gboolean
ephy_str_to_int (const char *string, int *integer)
{
	long result;
	char *parse_end;

	/* Check for the case of an empty string. */
	if (string == NULL || *string == '\0') {
		return FALSE;
	}

	/* Call the standard library routine to do the conversion. */
	errno = 0;
	result = strtol (string, &parse_end, 0);

	/* Check that the result is in range. */
	if ((result == G_MINLONG || result == G_MAXLONG) && errno == ERANGE) {
		return FALSE;
	}
	if (result < G_MININT || result > G_MAXINT) {
		return FALSE;
	}

	/* Check that all the trailing characters are spaces. */
	while (*parse_end != '\0') {
		if (!g_ascii_isspace (*parse_end++)) {
			return FALSE;
		}
	}

	/* Return the result. */
	*integer = result;
	return TRUE;
}

/**
 * ephy_str_strip_chr:
 * Remove all occurrences of a character from a string.
 *
 * @source: The string to be stripped.
 * @remove_this: The char to remove from @source
 *
 * Return value: A copy of @source, after removing all occurrences
 * of @remove_this.
 */
char *
ephy_str_strip_chr (const char *source, char remove_this)
{
	char *result, *out;
	const char *in;

        if (source == NULL) {
		return NULL;
	}

	result = g_new (char, strlen (source) + 1);
	in = source;
	out = result;
	do {
		if (*in != remove_this) {
			*out++ = *in;
		}
	} while (*in++ != '\0');

        return result;
}

int
ephy_strcasecmp (const char *string_a, const char *string_b)
{
        return g_ascii_strcasecmp (string_a == NULL ? "" : string_a,
                                   string_b == NULL ? "" : string_b);
}

int
ephy_strcasecmp_compare_func (gconstpointer string_a, gconstpointer string_b)
{
        return ephy_strcasecmp ((const char *) string_a,
			       (const char *) string_b);
}

/**
 * like strpbrk but ignores chars preceded by slashes, unless the
 * slash is also preceded by a slash unless that later slash is
 * preceded by another slash... ;-)
 */
static char *
ephy_strpbrk_unescaped (const char *s, const char *accept)
{
	gchar *ret = strpbrk (s, accept);

	if (!ret || ret == s || *(ret - 1) != '\\')
	{
		return ret;
	}
	else
	{
		gchar *c = ret - 1;
		g_assert (*c == '\\');

		while (c >= s && *c == '\\') c--;

		if ((ret - c) % 2 == 0)
		{
			return ephy_strpbrk_unescaped (ret + 1, accept);
		}
		else
		{
			return ret;
		}
	}
}

/**
 * like strstr but supports quoting, ignoring matches inside quoted text
 */
static char *
ephy_strstr_with_quotes (const char *haystack, const char *needle,
			 const char *quotes)
{
	gchar *quot = ephy_strpbrk_unescaped (haystack, quotes);
	gchar *ret = strstr (haystack, needle);

	if (!quot || !ret || ret < quot)
	{
		return ret;
	}

	quot = ephy_strpbrk_unescaped (quot + 1, quotes);

	if (quot)
	{
		return ephy_strstr_with_quotes (quot + 1, needle, quotes);
	}
	else
	{
		return NULL;
	}
}

/**
 * like strpbrk but supports quoting, ignoring matches inside quoted text
 */
static char *
ephy_strpbrk_with_quotes (const char *haystack, const char *needles,
			  const char *quotes)
{
	gchar *quot = ephy_strpbrk_unescaped (haystack, quotes);
	gchar *ret = strpbrk (haystack, needles);

	if (!quot || !ret || ret < quot)
	{
		return ret;
	}

	quot = ephy_strpbrk_unescaped (quot + 1, quotes);

	if (quot)
	{
		return ephy_strpbrk_with_quotes (quot + 1, needles, quotes);
	}
	else
	{
		return NULL;
	}
}

/**
 * Like g_strsplit, but does not split tokens betwen quotes. Ignores
 * quotes preceded by '\'.
 */
gchar **
ephy_strsplit_with_quotes (const gchar *string,
			   const gchar *delimiter,
			   gint max_tokens,
			   const gchar *quotes)
{
	GSList *string_list = NULL, *slist;
	gchar **str_array, *s;
	guint n = 0;
	const gchar *remainder;

	g_return_val_if_fail (string != NULL, NULL);
	g_return_val_if_fail (delimiter != NULL, NULL);
	g_return_val_if_fail (delimiter[0] != '\0', NULL);

	if (quotes == NULL)
	{
		return g_strsplit (string, delimiter, max_tokens);
	}

	if (max_tokens < 1)
	{
		max_tokens = G_MAXINT;
	}

	remainder = string;
	s = ephy_strstr_with_quotes (remainder, delimiter, quotes);
	if (s)
	{
		gsize delimiter_len = strlen (delimiter);

		while (--max_tokens && s)
		{
			gsize len;
			gchar *new_string;

			len = s - remainder;
			new_string = g_new (gchar, len + 1);
			strncpy (new_string, remainder, len);
			new_string[len] = 0;
			string_list = g_slist_prepend (string_list, new_string);
			n++;
			remainder = s + delimiter_len;
			s = ephy_strstr_with_quotes (remainder, delimiter, quotes);
		}
	}
	if (*string)
	{
		n++;
		string_list = g_slist_prepend (string_list, g_strdup (remainder));
	}

	str_array = g_new (gchar*, n + 1);

	str_array[n--] = NULL;
	for (slist = string_list; slist; slist = slist->next)
	{
		str_array[n--] = slist->data;
	}

	g_slist_free (string_list);

	return str_array;
}

/**
 * like ephy_strsplit_with_quotes, but matches any char in 'delimiters' as delimiter
 * and does not return empty tokens
 */
gchar **
ephy_strsplit_multiple_delimiters_with_quotes (const gchar *string,
					       const gchar *delimiters,
					       gint max_tokens,
					       const gchar *quotes)
{
	GSList *string_list = NULL, *slist;
	gchar **str_array, *s;
	guint n = 0;
	const gchar *remainder;

	g_return_val_if_fail (string != NULL, NULL);
	g_return_val_if_fail (delimiters != NULL, NULL);
	g_return_val_if_fail (delimiters[0] != '\0', NULL);

	if (quotes == NULL)
	{
		quotes = "";
	}

	if (max_tokens < 1)
	{
		max_tokens = G_MAXINT;
	}

	remainder = string;
	s = ephy_strpbrk_with_quotes (remainder, delimiters, quotes);
	if (s)
	{
		const gsize delimiter_len = 1; /* only chars */

		while (--max_tokens && s)
		{
			gsize len;
			gchar *new_string;

			len = s - remainder;
			if (len > 0) /* ignore empty strings */
			{
				new_string = g_new (gchar, len + 1);
				strncpy (new_string, remainder, len);
				new_string[len] = 0;
				string_list = g_slist_prepend (string_list, new_string);
				n++;
			}
			remainder = s + delimiter_len;
			s = ephy_strpbrk_with_quotes (remainder, delimiters, quotes);
		}
	}
	if (*string)
	{
		n++;
		string_list = g_slist_prepend (string_list, g_strdup (remainder));
	}

	str_array = g_new (gchar*, n + 1);

	str_array[n--] = NULL;
	for (slist = string_list; slist; slist = slist->next)
	{
		str_array[n--] = slist->data;
	}

	g_slist_free (string_list);

	return str_array;
}

char *
ephy_str_replace_substring (const char *string,
			    const char *substring,
			    const char *replacement)
{
	int substring_length, replacement_length, result_length, remaining_length;
	const char *p, *substring_position;
	char *result, *result_position;

	g_return_val_if_fail (substring != NULL, g_strdup (string));
	g_return_val_if_fail (substring[0] != '\0', g_strdup (string));

	if (string == NULL)
	{
		return NULL;
	}

	substring_length = strlen (substring);
	replacement_length = replacement == NULL ? 0 : strlen (replacement);

	result_length = strlen (string);
	for (p = string; ; p = substring_position + substring_length)
	{
		substring_position = strstr (p, substring);
		if (substring_position == NULL)
		{
			break;
		}
		result_length += replacement_length - substring_length;
	}

	result = g_malloc (result_length + 1);

	result_position = result;
	for (p = string; ; p = substring_position + substring_length)
	{
		substring_position = strstr (p, substring);
		if (substring_position == NULL)
		{
			remaining_length = strlen (p);
			memcpy (result_position, p, remaining_length);
			result_position += remaining_length;
			break;
		}
		memcpy (result_position, p, substring_position - p);
		result_position += substring_position - p;
		memcpy (result_position, replacement, replacement_length);
		result_position += replacement_length;
	}
	g_assert (result_position - result == result_length);
	result_position[0] = '\0';

	return result;
}
