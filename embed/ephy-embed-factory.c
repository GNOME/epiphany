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
 *  $Id$
 */

#include "config.h"

#include "ephy-embed-factory.h"
#if defined(WITH_GECKO_ENGINE)
#include "mozilla-embed.h"
#include "mozilla-embed-find.h"
#include "mozilla-embed-persist.h"
#include "mozilla-embed-single.h"
#elif defined(WITH_WEBKIT_ENGINE)
#include "webkit-embed.h"
#include "webkit-embed-find.h"
#include "webkit-embed-persist.h"
#include "webkit-embed-single.h"
#endif
#include "ephy-embed.h"
#include "ephy-embed-find.h"
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
#if defined(WITH_GECKO_ENGINE)
		object = g_object_new (MOZILLA_TYPE_EMBED, NULL);
#elif defined(WITH_WEBKIT_ENGINE)
	object = g_object_new (WEBKIT_TYPE_EMBED, NULL);
#endif
	}
	else if (type == EPHY_TYPE_EMBED_PERSIST)
	{
#if defined(WITH_GECKO_ENGINE)
		object = g_object_new (MOZILLA_TYPE_EMBED_PERSIST, NULL);
#elif defined(WITH_WEBKIT_ENGINE)
	object = g_object_new (WEBKIT_TYPE_EMBED_PERSIST, NULL);
#endif
	}
	else if (type == EPHY_TYPE_EMBED_FIND)
	{
#if defined(WITH_GECKO_ENGINE)
		object = g_object_new (MOZILLA_TYPE_EMBED_FIND, NULL);
#elif defined(WITH_WEBKIT_ENGINE)
		object = g_object_new (WEBKIT_TYPE_EMBED_FIND, NULL);
#endif
	}
	else if (type == EPHY_TYPE_EMBED_SINGLE)
	{
#if defined(WITH_GECKO_ENGINE)
		object = g_object_new (MOZILLA_TYPE_EMBED_SINGLE, NULL);
#elif defined(WITH_WEBKIT_ENGINE)
		object = g_object_new (WEBKIT_TYPE_EMBED_SINGLE, NULL);
#endif
	}
	else
	{
		g_assert_not_reached ();
	}

	return object;
}
