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
 *
 *  $Id$
 */

#include "config.h"

#include "ephy-string.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>

#define ELLIPSIS "\xe2\x80\xa6"

gboolean
ephy_string_to_int (const char *string, gulong *integer)
{
	gulong result;
	char *parse_end;

	/* Check for the case of an empty string. */
	if (string == NULL || *string == '\0')
	{
		return FALSE;
	}

	/* Call the standard library routine to do the conversion. */
	errno = 0;
	result = strtol (string, &parse_end, 0);

	/* Check that the result is in range. */
	if ((result == G_MINLONG || result == G_MAXLONG) && errno == ERANGE)
	{
		return FALSE;
	}

	/* Check that all the trailing characters are spaces. */
	while (*parse_end != '\0')
	{
		if (!g_ascii_isspace (*parse_end++))
		{
			return FALSE;
		}
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
	{
		return NULL;
	}

	p = source;
	while (*p != '\0')
	{
		if ((guchar) *p < 0x20)
		{
			*p = ' ';
		}
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

        if (!str) return NULL;

 	/* FIXME: this function is a big mess. While it is utf-8 safe now,
	 * it can still split a sequence of combining characters.
	 */

        actual_length = g_utf8_strlen (str, -1);

        /* if the string is already short enough, or if it's too short for
         * us to shorten it, return a new copy */
        if (actual_length <= target_length) return str;

        /* create string */
        bytes = GPOINTER_TO_UINT (g_utf8_offset_to_pointer (str, target_length - 1) - str);

        new_str = g_new (gchar, bytes + strlen(ELLIPSIS) + 1);

        strncpy (new_str, str, bytes);
        strncpy (new_str + bytes, ELLIPSIS, strlen (ELLIPSIS));
	new_str[bytes + strlen (ELLIPSIS)] = '\0';

	g_free (str);

        return new_str;
}
