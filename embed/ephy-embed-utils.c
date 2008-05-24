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

/**
 * ephy_embed_utils_get_title_composite:
 * @embed: an #EphyEmbed
 *
 * Returns the title of the web page loaded in @embed.
 * 
 * This differs from #ephy_embed_utils_get_title in that this function
 * will return a special title while the page is still loading.
 *
 * Return value: @embed's web page's title. Will never be %NULL.
 **/
const char *
ephy_embed_utils_get_title_composite (EphyEmbed *embed)
{
	const char *title = "";
	const char *loading_title;
	gboolean is_loading, is_blank;

	g_return_val_if_fail (EPHY_IS_EMBED (embed), NULL);

	is_loading = ephy_embed_get_load_status (embed);
	is_blank = ephy_embed_get_is_blank (embed);
	loading_title = ephy_embed_get_loading_title (embed);
	title = ephy_embed_get_title (embed);

	if (is_blank)
	{
		if (is_loading)
			title = loading_title;
		else
			title = _("Blank page");
	}

	return title != NULL ? title : "";
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
			  g_str_has_prefix (address, "gopher:"));

	return has_web_scheme;
}
