/*
 *  Copyright (C) 2005 Christian Persch
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

#include "ephy-embed-find.h"

void
ephy_embed_find_set_embed (EphyEmbedFind *find,
			   EphyEmbed *embed)
{
	EphyEmbedFindIface *iface = EPHY_EMBED_FIND_GET_IFACE (find);
	iface->set_embed (find, embed);
}

/**
 * ephy_embed_find_set_properties:
 * @find: an #EphyEmbedFind
 * @case_sensitive: %TRUE for "case sensitive" to be set
 *
 * Sets the properties of @find
 **/
void
ephy_embed_find_set_properties (EphyEmbedFind *find,
				const char *search_string,
				gboolean case_sensitive)
{
	EphyEmbedFindIface *iface = EPHY_EMBED_FIND_GET_IFACE (find);
	iface->set_properties (find, search_string, case_sensitive);
}

/**
 * ephy_embed_find_find:
 * @embed: an #EphyEmbedFind
 * @search_string: the text to search for
 * @links_only: whether to only search the text in links
 *
 * Return value: whether a match was found
 **/
EphyEmbedFindResult
ephy_embed_find_find (EphyEmbedFind *find,
		      const char *search_string,
		      gboolean links_only)
{
	EphyEmbedFindIface *iface = EPHY_EMBED_FIND_GET_IFACE (find);
	return iface->find (find, search_string, links_only);
}

/**
 * ephy_embed_find_find_again:
 * @embed: an #EphyEmbedFind
 * @forward %TRUE to search forwards in the document
 *
 * Return value: whether a match was found
 **/
EphyEmbedFindResult
ephy_embed_find_find_again (EphyEmbedFind *find,
			    gboolean forward)
{
	EphyEmbedFindIface *iface = EPHY_EMBED_FIND_GET_IFACE (find);
	return iface->find_again (find, forward);
}

/**
 * ephy_embed_find_activate_link:
 * @embed: an #EphyEmbedFind
 * @mask:
 * 
 * Activates the currently focused link, if there is any.
 * 
 * Return value: %TRUE if a link was activated
 **/
gboolean
ephy_embed_find_activate_link (EphyEmbedFind *find,
			       GdkModifierType mask)
{
	EphyEmbedFindIface *iface = EPHY_EMBED_FIND_GET_IFACE (find);
	return iface->activate_link (find, mask);
}

GType
ephy_embed_find_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyEmbedFindIface),
			NULL,
			NULL,
		};

		type = g_type_register_static (G_TYPE_INTERFACE,
					       "EphyEmbedFind",
					       &our_info, (GTypeFlags) 0);
	}

	return type;
}
