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
 *  $Id$
 */

#include "config.h"

#include "ephy-embed-event.h"

#include <glib/ghash.h>
#include <gtk/gtktypeutils.h>

GType
ephy_embed_event_context_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GFlagsValue values[] =
		{
		{ EPHY_EMBED_CONTEXT_NONE, "EPHY_EMBED_CONTEXT_NONE", "none" },
		{ EPHY_EMBED_CONTEXT_DEFAULT, "EPHY_EMBED_CONTEXT_DEFAULT", "default" },
		{ EPHY_EMBED_CONTEXT_LINK, "EPHY_EMBED_CONTEXT_LINK", "link" },
		{ EPHY_EMBED_CONTEXT_IMAGE, "EPHY_EMBED_CONTEXT_IMAGE", "image" },
		{ EPHY_EMBED_CONTEXT_DOCUMENT, "EPHY_EMBED_CONTEXT_DOCUMENT", "document" },
		{ EPHY_EMBED_CONTEXT_INPUT, "EPHY_EMBED_CONTEXT_INPUT", "input" },
		{ EPHY_EMBED_CONTEXT_XUL, "EPHY_EMBED_CONTEXT_XUL", "xul" },
		{ EPHY_EMBED_CONTEXT_EMAIL_LINK, "EPHY_EMBED_CONTEXT_EMAIL_LINK", "email-link" },
		{ 0, NULL, NULL }
		};

		type = g_flags_register_static ("EphyEmbedEventContext", values);
	}

	return type;
}

GType
ephy_embed_event_type_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GEnumValue values[] =
		{
		{ EPHY_EMBED_EVENT_MOUSE_BUTTON1, "EPHY_EMBED_EVENT_MOUSE_BUTTON1", "mouse-button-1" },
		{ EPHY_EMBED_EVENT_MOUSE_BUTTON2, "EPHY_EMBED_EVENT_MOUSE_BUTTON2", "mouse-button-2" },
		{ EPHY_EMBED_EVENT_MOUSE_BUTTON3, "EPHY_EMBED_EVENT_MOUSE_BUTTON3", "mouse-button-3" },
		{ EPHY_EMBED_EVENT_KEY, "EPHY_EMBED_EVENT_KEY", "key" },
		{ 0, NULL, NULL }
		};

		type = g_enum_register_static ("EphyEmbedEventType", values);
	}

	return type;
}

static void ephy_embed_event_base_init (gpointer g_class);

GType
ephy_embed_event_get_type (void)
{
       static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GTypeInfo our_info =
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

EphyEmbedEventType
ephy_embed_event_get_event_type (EphyEmbedEvent *event)
{
	EphyEmbedEventIface *iface = EPHY_EMBED_EVENT_GET_IFACE (event);
	return iface->get_type (event);
}

EphyEmbedEventContext
ephy_embed_event_get_context (EphyEmbedEvent *event)
{
	EphyEmbedEventIface *iface = EPHY_EMBED_EVENT_GET_IFACE (event);
	return iface->get_context (event);
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

void
ephy_embed_event_get_property	(EphyEmbedEvent *event,
				 const char *name,
				 const GValue **value)
{
	EphyEmbedEventIface *iface = EPHY_EMBED_EVENT_GET_IFACE (event);
	iface->get_property (event, name, value);
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
