/*
 *  Copyright © 2000-2004 Marco Pesenti Gritti
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

#include "ephy-embed-factory.h"
#include "webkit-embed-persist.h"
#include "ephy-embed.h"
#include "ephy-embed-persist.h"
#include "ephy-embed-single.h"

/**
 * ephy_embed_factory_new_object:
 * @type: a #GType specifying which object to create
 * 
 * Create an instance of an object implementing the @type interface.
 *
 * Return value: the object instance
 **/
GObject	*
ephy_embed_factory_new_object (GType type)
{
	GObject *object = NULL;

	if (type == EPHY_TYPE_EMBED)
	{
                object = g_object_new (EPHY_TYPE_EMBED, NULL);
	}
	else if (type == EPHY_TYPE_EMBED_PERSIST)
	{
                object = g_object_new (WEBKIT_TYPE_EMBED_PERSIST, NULL);
	}
	else if (type == EPHY_TYPE_EMBED_SINGLE)
	{
		object = g_object_new (EPHY_TYPE_EMBED_SINGLE, NULL);
	}
	else
	{
		g_assert_not_reached ();
	}

	return object;
}
