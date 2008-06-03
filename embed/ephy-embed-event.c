/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
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

#include "ephy-embed-event.h"
#include "ephy-embed-type-builtins.h"

#include <glib.h>
#include <gtk/gtk.h>

static void ephy_embed_event_base_init (gpointer g_class);

GType
ephy_embed_event_get_type (void)
{
       static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		const GTypeInfo our_info =
		{
			sizeof (EphyEmbedEventIface),
			ephy_embed_event_base_init,
			NULL,
		};

		type = g_type_register_static (G_TYPE_INTERFACE,
					       "EphyEmbedEvent",
					       &our_info,
					       (GTypeFlags) 0);
	}

	return type;
}

static void
ephy_embed_event_base_init (gpointer g_class)
{
	static gboolean initialised = FALSE;

	initialised = TRUE;
}

EphyEmbedEventContext
ephy_embed_event_get_context (EphyEmbedEvent *event)
{
	EphyEmbedEventIface *iface = EPHY_EMBED_EVENT_GET_IFACE (event);
	return iface->get_context (event);
}

guint
ephy_embed_event_get_button (EphyEmbedEvent *event)
{
	EphyEmbedEventIface *iface = EPHY_EMBED_EVENT_GET_IFACE (event);
	return iface->get_button (event);
}

guint
ephy_embed_event_get_modifier (EphyEmbedEvent *event)
{
	EphyEmbedEventIface *iface = EPHY_EMBED_EVENT_GET_IFACE (event);
	return iface->get_modifier (event);
}

void
ephy_embed_event_get_coords (EphyEmbedEvent *event,
			     guint *x, guint *y)
{
	EphyEmbedEventIface *iface = EPHY_EMBED_EVENT_GET_IFACE (event);
	iface->get_coordinates (event, x, y);
}

const GValue*
ephy_embed_event_get_property	(EphyEmbedEvent *event,
				 const char *name)
{
	EphyEmbedEventIface *iface = EPHY_EMBED_EVENT_GET_IFACE (event);
	return iface->get_property (event, name);
}

gboolean
ephy_embed_event_has_property	(EphyEmbedEvent *event,
				 const char *name)
{
	EphyEmbedEventIface *iface = EPHY_EMBED_EVENT_GET_IFACE (event);
	return iface->has_property (event, name);
}

gpointer
ephy_embed_event_get_dom_event (EphyEmbedEvent *event)
{
	EphyEmbedEventIface *iface = EPHY_EMBED_EVENT_GET_IFACE (event);
	return iface->get_dom_event (event);
}
