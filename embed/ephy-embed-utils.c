/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2005 Christian Persch
 *  Copyright © 2004 Crispin Flowerday
 *  Copyright © 2004 Adam Hooper
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
 *  $Id: 
 */

#include "config.h"

#include <string.h>

#include <glib/gi18n.h>

#include "ephy-string.h"
#include "ephy-embed-utils.h"

char*
ephy_embed_utils_link_message_parse (char *message)
{
	
	char *status_message;
	char **splitted_message;

	status_message = ephy_string_blank_chr (g_strdup (message));
	
	if (status_message && g_str_has_prefix (status_message, "mailto:"))
	{
		int i = 1;
		char *p;
		GString *tmp;
		
		/* We first want to eliminate all the things after "?", like
		 * cc, subject and alike.
		 */
		
		p = strchr (status_message, '?');
		if (p != NULL) *p = '\0';
		
		/* Then we also want to check if there is more than an email address
		 * in the mailto: list.
		 */
		
		splitted_message = g_strsplit_set (status_message, ";", -1);
		tmp = g_string_new (g_strdup_printf (_("Send an email message to “%s”"),
						     (splitted_message[0] + 7)));
		
		while (splitted_message [i] != NULL)
		{
			g_string_append_printf (tmp, ", “%s”", splitted_message[i]);
			i++;
		}

		g_free (status_message);
		g_strfreev (splitted_message);

		return g_string_free (tmp, FALSE);
	}
	else
	{
		return status_message;
	}
}

gboolean
ephy_embed_utils_address_has_web_scheme (const char *address)
{
	gboolean has_web_scheme;

	if (address == NULL) return FALSE;

	has_web_scheme = (g_str_has_prefix (address, "http:") ||
			  g_str_has_prefix (address, "https:") ||
			  g_str_has_prefix (address, "ftp:") ||
			  g_str_has_prefix (address, "file:") ||
			  g_str_has_prefix (address, "data:") ||
			  g_str_has_prefix (address, "about:") ||
			  g_str_has_prefix (address, "javascript:") ||
			  g_str_has_prefix (address, "gopher:"));

	return has_web_scheme;
}

char*
ephy_embed_utils_normalize_address (const char *address)
{
	char *effective_address;

	g_return_val_if_fail (address, NULL);

	if (ephy_embed_utils_address_has_web_scheme (address) == FALSE)
		effective_address = g_strconcat ("http://", address, NULL);
	else
		effective_address = g_strdup (address);
	
	return effective_address;
}
