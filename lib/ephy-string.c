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

/* copied from egg-toolbar-editor.c */
char *
ephy_string_elide_underscores (const char *original)
{
	char *q, *result;
	const char *p;
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

char *
ephy_string_double_underscores (const char *string)
{
	int underscores;
	const char *p;
	char *q;
	char *escaped;

	if (string == NULL)
	{
		return NULL;
	}

	underscores = 0;
	for (p = string; *p != '\0'; p++)
	{
		underscores += (*p == '_');
	}
        
	if (underscores == 0)
	{
		return g_strdup (string);
	}

	escaped = g_new (char, strlen (string) + underscores + 1);
	for (p = string, q = escaped; *p != '\0'; p++, q++)
	{
		/* Add an extra underscore. */
		if (*p == '_') {
			*q++ = '_';
		}
		*q = *p;
	}
	*q = '\0';
                                                                                                                             
	return escaped;
}
