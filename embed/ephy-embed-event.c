/*
 *  Copyright (C) 2000, 2001, 2002 Marco Pesenti Gritti
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

#include "ephy-embed-event.h"

#include <glib/ghash.h>
#include <gtk/gtktypeutils.h>

#define EPHY_EMBED_EVENT_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_EMBED_EVENT, EphyEmbedEventPrivate))

struct EphyEmbedEventPrivate
{
	GHashTable *props;
};

static void
ephy_embed_event_class_init (EphyEmbedEventClass *klass);
static void
ephy_embed_event_init (EphyEmbedEvent *ges);
static void
ephy_embed_event_finalize (GObject *object);

static GObjectClass *parent_class = NULL;

GType
ephy_embed_event_get_type (void)
{
       static GType ephy_embed_event_type = 0;

        if (ephy_embed_event_type == 0)
        {
                static const GTypeInfo our_info =
                {
                        sizeof (EphyEmbedEventClass),
                        NULL, /* base_init */
                        NULL, /* base_finalize */
                        (GClassInitFunc) ephy_embed_event_class_init,
                        NULL, /* class_finalize */
                        NULL, /* class_data */
                        sizeof (EphyEmbedEvent),
                        0,    /* n_preallocs */
                        (GInstanceInitFunc) ephy_embed_event_init
                };

                ephy_embed_event_type = g_type_register_static (G_TYPE_OBJECT,
                                                                  "EphyEmbedEvent",
                                                                  &our_info, 0);
        }

        return ephy_embed_event_type;
}

static void
ephy_embed_event_class_init (EphyEmbedEventClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = ephy_embed_event_finalize;

	g_type_class_add_private (object_class, sizeof(EphyEmbedEventPrivate));
}

static void
free_g_value (gpointer value)
{
	g_value_unset (value);
	g_free (value);
}

static void
ephy_embed_event_init (EphyEmbedEvent *event)
{
        event->priv = EPHY_EMBED_EVENT_GET_PRIVATE (event);

	event->priv->props = g_hash_table_new_full (g_str_hash, g_str_equal,
						    g_free, free_g_value);
}

static void
ephy_embed_event_finalize (GObject *object)
{
        EphyEmbedEvent *event = EPHY_EMBED_EVENT (object);

	g_hash_table_destroy (event->priv->props);

        G_OBJECT_CLASS (parent_class)->finalize (object);
}


EphyEmbedEvent *
ephy_embed_event_new (void)
{
	EphyEmbedEvent *event;

        event = EPHY_EMBED_EVENT (g_object_new (EPHY_TYPE_EMBED_EVENT, NULL));

        return event;
}

guint
ephy_embed_event_get_modifier (EphyEmbedEvent *event)
{
	return event->modifier;
}

gresult
ephy_embed_event_get_event_type (EphyEmbedEvent *event,
				 EphyEmbedEventType *type)
{
	*type = event->type;
	return G_OK;
}

gresult
ephy_embed_event_get_coords (EphyEmbedEvent *event,
			     guint *x, guint *y)
{
	*x = event->x;
	*y = event->y;
	return G_OK;
}

gresult
ephy_embed_event_get_context (EphyEmbedEvent *event,
			      EmbedEventContext *context)
{
	*context = event->context;
	return G_OK;
}

void
ephy_embed_event_set_property (EphyEmbedEvent *event,
			       const char *name,
			       GValue *value)
{
	g_hash_table_insert (event->priv->props,
                             g_strdup (name),
                             value);
}

void
ephy_embed_event_get_property	(EphyEmbedEvent *event,
				 const char *name,
				 const GValue **value)
{
	*value = g_hash_table_lookup (event->priv->props, name);
}

gboolean
ephy_embed_event_has_property	(EphyEmbedEvent *event,
				 const char *name)
{
	gpointer tmp;

	tmp = g_hash_table_lookup (event->priv->props,
				   name);

	return tmp != NULL;
}
