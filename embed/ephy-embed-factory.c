/*
 *  Copyright (C) 2000-2003 Marco Pesenti Gritti
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
 */

#include "ephy-embed-factory.h"
#include "mozilla-embed.h"
#include "mozilla-embed-persist.h"
#include "mozilla-embed-single.h"

#include <string.h>

typedef enum
{
	EPHY_EMBED_OBJECT,
	EPHY_EMBED_PERSIST_OBJECT,
	EPHY_EMBED_SINGLE_OBJECT
} EmbedObjectType;

static EmbedObjectType
type_from_id (const char *object_id)
{
	EmbedObjectType result = 0;

	if (strcmp (object_id, "EphyEmbed") == 0)
	{
		result = EPHY_EMBED_OBJECT;
	}
	else if (strcmp (object_id, "EphyEmbedPersist") == 0)
	{
		result = EPHY_EMBED_PERSIST_OBJECT;
	}
	else if (strcmp (object_id, "EphyEmbedSingle") == 0)
	{
		result = EPHY_EMBED_SINGLE_OBJECT;
	}
	else
	{
		g_assert_not_reached ();
	}

	return result;
}

/**
 * ephy_embed_factory_new_object:
 * @object_id: identifier of the object to create
 * 
 * Create an instance of the object identified by
 * object_id string. Valid ids are EphyEmbed, EphyEmbedPersist,
 * EphyEmbedSingle.
 * We use a factory instead of creating instances directly
 * to keep the embed implementation abstract. All the embed
 * objects should be based on an interface and created by
 * this factory.
 * 
 * Return value: the object instance
 **/
GObject	*
ephy_embed_factory_new_object (const char *object_id)
{
	GObject *object;

	switch (type_from_id (object_id))
	{
		case EPHY_EMBED_OBJECT:
			object = g_object_new (MOZILLA_TYPE_EMBED, NULL);
			break;
		case EPHY_EMBED_PERSIST_OBJECT:
			object = g_object_new (MOZILLA_TYPE_EMBED_PERSIST, NULL);
			break;
		case EPHY_EMBED_SINGLE_OBJECT:
			object = g_object_new (MOZILLA_TYPE_EMBED_SINGLE, NULL);
			break;
		default:
			object = NULL;
	}

	return object;
}
