/*
 *  Copyright (C) 2000-2003 Marco Pesenti Gritti
 *  Copyright (C) 2004 Christian Persch
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

#include "mozilla-config.h"

#include "config.h"

#include "mozilla-embed-event.h"

#include "ephy-debug.h"

#include <nsCOMPtr.h>
#include <nsIDOMEvent.h>

#include <glib/ghash.h>
#include <gtk/gtktypeutils.h>

#define MOZILLA_EMBED_EVENT_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), MOZILLA_TYPE_EMBED_EVENT, MozillaEmbedEventPrivate))

struct _MozillaEmbedEventPrivate
{
	nsIDOMEvent* dom_event;
	GHashTable *props;
};

static void mozilla_embed_event_class_init	(MozillaEmbedEventClass *klass);
static void mozilla_embed_event_init		(MozillaEmbedEvent *event);
static void ephy_embed_event_iface_init		(EphyEmbedEventIface *iface);

static GObjectClass *parent_class = NULL;

GType
mozilla_embed_event_get_type (void)
{
       static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GTypeInfo our_info =
		{
			sizeof (MozillaEmbedEventClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) mozilla_embed_event_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (MozillaEmbedEvent),
			0,    /* n_preallocs */
			(GInstanceInitFunc) mozilla_embed_event_init
		};

		static const GInterfaceInfo embed_event_info =
		{
			(GInterfaceInitFunc) ephy_embed_event_iface_init,
        		NULL,
        		NULL
     		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "MozillaEmbedEvent",
					       &our_info, (GTypeFlags) 0);

		g_type_add_interface_static (type,
                                   	     EPHY_TYPE_EMBED_EVENT,
                                   	     &embed_event_info);
	}

	return type;
}

MozillaEmbedEvent *
mozilla_embed_event_new (gpointer dom_event)
{
	MozillaEmbedEvent *event;

	event = MOZILLA_EMBED_EVENT (g_object_new (MOZILLA_TYPE_EMBED_EVENT, NULL));

	event->priv->dom_event = static_cast<nsIDOMEvent*>(dom_event);
	NS_IF_ADDREF (event->priv->dom_event);

	return event;
}

void
mozilla_embed_event_set_property (MozillaEmbedEvent *event,
				  const char *name,
				  GValue *value)
{
	char *value_content = g_strdup_value_contents (value);
	LOG ("embed event %p set property \"%s\" to %s", event, name, value_content);
	g_free (value_content);

	g_hash_table_insert (event->priv->props,
			     g_strdup (name),
			     value);
}

static EphyEmbedEventContext
impl_get_context (EphyEmbedEvent *event)
{
	return (EphyEmbedEventContext) ((MozillaEmbedEvent *) event)->context;
}

static guint
impl_get_button (EphyEmbedEvent *event)
{
	return ((MozillaEmbedEvent *) event)->button;
}

static guint
impl_get_modifier (EphyEmbedEvent *event)
{
	return ((MozillaEmbedEvent *) event)->modifier;
}

static void
impl_get_coordinates (EphyEmbedEvent *event,
		      guint *x,
		      guint *y)
{
	*x = ((MozillaEmbedEvent *) event)->x;
	*y = ((MozillaEmbedEvent *) event)->y;
}

static const GValue*
impl_get_property (EphyEmbedEvent *event,
		   const char *name)
{
	return (const GValue *) g_hash_table_lookup (((MozillaEmbedEvent *) event)->priv->props, name);
}

static gboolean
impl_has_property (EphyEmbedEvent *event,
		   const char *name)
{
	gpointer tmp;

	tmp = g_hash_table_lookup (((MozillaEmbedEvent *) event)->priv->props, name);

	return tmp != NULL;
}

static gpointer
impl_get_dom_event (EphyEmbedEvent *event)
{
	return NS_STATIC_CAST (gpointer, ((MozillaEmbedEvent *) event)->priv->dom_event);
}

static void
free_g_value (gpointer value)
{
	g_value_unset ((GValue *) value);
	g_free (value);
}

static void
mozilla_embed_event_init (MozillaEmbedEvent *event)
{
	event->priv = MOZILLA_EMBED_EVENT_GET_PRIVATE (event);

	LOG ("MozillaEmbedEvent %p initialising", event);

	event->priv->dom_event = nsnull;
	event->priv->props = g_hash_table_new_full (g_str_hash, g_str_equal,
						    g_free, free_g_value);
}

static void
mozilla_embed_event_finalize (GObject *object)
{
	MozillaEmbedEvent *event = MOZILLA_EMBED_EVENT (object);

	g_hash_table_destroy (event->priv->props);

	NS_IF_RELEASE (event->priv->dom_event);
	event->priv->dom_event = nsnull;

	LOG ("MozillaEmbedEvent %p finalised", object);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
ephy_embed_event_iface_init (EphyEmbedEventIface *iface)
{
	iface->get_context = impl_get_context;
	iface->get_button = impl_get_button;
	iface->get_modifier = impl_get_modifier;
	iface->get_coordinates = impl_get_coordinates;
	iface->get_property = impl_get_property;
	iface->has_property = impl_has_property;
	iface->get_dom_event = impl_get_dom_event;
}

static void
mozilla_embed_event_class_init (MozillaEmbedEventClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = (GObjectClass *) g_type_class_peek_parent (klass);

	object_class->finalize = mozilla_embed_event_finalize;

	g_type_class_add_private (object_class, sizeof (MozillaEmbedEventPrivate));
}
