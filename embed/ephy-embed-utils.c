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
 */

#include "config.h"

#include <string.h>

#include <glib/gi18n.h>
#include <libsoup/soup.h>

#include "ephy-string.h"
#include "ephy-embed-utils.h"
#include "ephy-request-about.h"

char *
ephy_embed_utils_link_message_parse (const char *message)
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
	int colonpos;

	if (address == NULL) return FALSE;

	colonpos = (int)((g_strstr_len (address, 11, ":")) - address);

	if (colonpos < 0) return FALSE;

	has_web_scheme = !(g_ascii_strncasecmp (address, "http", colonpos) &&
			   g_ascii_strncasecmp (address, "https", colonpos) &&
			   g_ascii_strncasecmp (address, "ftp", colonpos) &&
			   g_ascii_strncasecmp (address, "file", colonpos) &&
			   g_ascii_strncasecmp (address, "javascript", colonpos) &&
			   g_ascii_strncasecmp (address, "data", colonpos) &&
			   g_ascii_strncasecmp (address, "about", colonpos) &&
			   g_ascii_strncasecmp (address, "ephy-about", colonpos) &&
			   g_ascii_strncasecmp (address, "gopher", colonpos));

	return has_web_scheme;
}

gboolean
ephy_embed_utils_address_is_existing_absolute_filename (const char *address)
{
	return g_path_is_absolute (address) &&
	       g_file_test (address, G_FILE_TEST_EXISTS);
}

char*
ephy_embed_utils_normalize_address (const char *address)
{
	char *effective_address = NULL;
	SoupURI *uri;

	g_return_val_if_fail (address, NULL);

	if (ephy_embed_utils_address_is_existing_absolute_filename (address))
		return g_strconcat ("file://", address, NULL);

	uri = soup_uri_new (address);

	/* FIXME: if we are here we passed through the "should we
	 * auto-search this?" regex in EphyWebView, so we should be a
	 * valid-ish URL. Auto-prepend http:// to anything that is not
	 * one according to soup, because it probably will be
	 * something like "google.com". Special case localhost(:port)
	 * and IP(:port), because SoupURI, correctly, thinks it is a
	 * URI with scheme being localhost/IP and, optionally, path
	 * being the port. Ideally we should check if we have a
	 * handler for the scheme, and since we'll fail for localhost
	 * and IP, we'd fallback to loading it as a domain. */
	if (!uri ||
	    (uri && !g_strcmp0 (uri->scheme, "localhost")) ||
	    (uri && g_hostname_is_ip_address (uri->scheme)))
		effective_address = g_strconcat ("http://", address, NULL);
	else {
		/* Convert about: schemes to ephy-about: in order to
		 * force WebKit to delegate its handling to our
		 * EphyRequestAbout. In general about: schemes are
		 * handled internally by WebKit and mean "empty
		 * document".
		 */
		if (g_str_has_prefix (address, "about:") && !g_str_equal (address, "about:blank")) {
			effective_address = g_strconcat (EPHY_ABOUT_SCHEME, address + strlen ("about"), NULL);
		} else
			effective_address = g_strdup (address);
	}

	if (uri)
		soup_uri_free (uri);

	return effective_address;
}

gboolean
ephy_embed_utils_url_is_empty (const char *location)
{
	gboolean is_empty = FALSE;

        if (location == NULL || location[0] == '\0' ||
            strcmp (location, "about:blank") == 0)
        {
                is_empty = TRUE;
        }

        return is_empty;
}

