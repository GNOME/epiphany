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
 */

#include "ephy-embed-event.h"

#include <glib/ghash.h>
#include <gtk/gtktypeutils.h>

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
        event->priv = g_new0 (EphyEmbedEventPrivate, 1);

	event->priv->props = g_hash_table_new_full (g_str_hash, g_str_equal,
						    g_free, free_g_value);
}

static void
ephy_embed_event_finalize (GObject *object)
{
        EphyEmbedEvent *event;

        g_return_if_fail (object != NULL);
        g_return_if_fail (IS_EPHY_EMBED_EVENT (object));

        event = EPHY_EMBED_EVENT (object);

        g_return_if_fail (event->priv != NULL);

	g_hash_table_destroy (event->priv->props);

	g_free (event->priv);

        G_OBJECT_CLASS (parent_class)->finalize (object);
}


EphyEmbedEvent *
ephy_embed_event_new (void)
{
	EphyEmbedEvent *event;

        event = EPHY_EMBED_EVENT (g_object_new (EPHY_EMBED_EVENT_TYPE, NULL));

        g_return_val_if_fail (event->priv != NULL, NULL);

        return event;
}

guint
ephy_embed_event_get_modifier (EphyEmbedEvent *event)
{
	return event->modifier;
}

gresult
ephy_embed_event_get_mouse_button (EphyEmbedEvent *event,
				   guint *mouse_button)
{
	*mouse_button = event->mouse_button;
	return G_OK;
}

gresult
ephy_embed_event_get_mouse_coords (EphyEmbedEvent *event,
				   guint *x, guint *y)
{
	*x = event->mouse_x;
	*y = event->mouse_y;
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
