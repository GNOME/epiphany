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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ephy-string.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>

#define ELLIPSIS "\xe2\x80\xa6"

/**
 * ephy_string_shorten: returns a newly allocated shortened version of str.
 * The input must be valid utf-8.
 * @str: the string to shorten
 * @target_length: the length of the shortened string (in characters)
 *
 * FIXME: this function is a big mess. While it is utf-8 safe now,
 * it can still split a sequence of combining characters
 */
char *
ephy_string_shorten (const gchar *str, gint target_length)
{
	gchar *new_str;
	glong actual_length;
	gulong bytes;

	if (!str) return NULL;

	actual_length = g_utf8_strlen (str, -1);

	/* if the string is already short enough, or if it's too short for
	 * us to shorten it, return a new copy */
	if (actual_length <= target_length) return g_strdup (str);

	/* create string */
	bytes = GPOINTER_TO_UINT (g_utf8_offset_to_pointer (str, target_length - 1) - str);

	new_str = g_new0 (gchar, bytes + strlen(ELLIPSIS) + 1);

	strncpy (new_str, str, bytes);
	strncpy (new_str + bytes, ELLIPSIS, strlen (ELLIPSIS));

	return new_str;
}

gboolean
ephy_string_to_int (const char *string, gulong *integer)
{
	gulong result;
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
ephy_string_strip_chr (const char *source, char remove_this)
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

/* copied from egg-toolbar-editor.c */
char *
ephy_string_elide_underscores (const gchar *original)
{
	gchar *q, *result;
	const gchar *p;
	gboolean last_underscore;

	q = result = g_malloc (strlen (original) + 1);
	last_underscore = FALSE;

	for (p = original; *p; p++)
	{
		if (!last_underscore && *p == '_')
		{
			last_underscore = TRUE;
		}
		else
		{
			last_underscore = FALSE;
			*q++ = *p;
		}
	}

	*q = '\0';

	return result;
}
