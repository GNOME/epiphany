/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
 *  Copyright © 2009-2012 Igalia S.L.
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
#include "ephy-embed-event.h"

#define EPHY_EMBED_EVENT_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_EMBED_EVENT, EphyEmbedEventPrivate))

struct EphyEmbedEventPrivate
{
  guint button;
  guint modifier;
  guint x;
  guint y;
  WebKitHitTestResult *hit_test_result;
};

G_DEFINE_TYPE (EphyEmbedEvent, ephy_embed_event, G_TYPE_OBJECT)

static void
dispose (GObject *object)
{
  EphyEmbedEventPrivate *priv = EPHY_EMBED_EVENT (object)->priv;

  g_clear_object (&priv->hit_test_result);

  G_OBJECT_CLASS (ephy_embed_event_parent_class)->dispose (object);
}

static void
ephy_embed_event_class_init (EphyEmbedEventClass *klass)
{
  GObjectClass *object_class = (GObjectClass *)klass;

  object_class->dispose = dispose;

  g_type_class_add_private (G_OBJECT_CLASS (klass), sizeof(EphyEmbedEventPrivate));
}

static void
ephy_embed_event_init (EphyEmbedEvent *embed_event)
{
  embed_event->priv = EPHY_EMBED_EVENT_GET_PRIVATE (embed_event);
}

EphyEmbedEvent *
ephy_embed_event_new (GdkEventButton *event, WebKitHitTestResult *hit_test_result)
{
  EphyEmbedEvent *embed_event;
  EphyEmbedEventPrivate *priv;

  embed_event = g_object_new (EPHY_TYPE_EMBED_EVENT, NULL);
  priv = embed_event->priv;

  priv->hit_test_result = g_object_ref (hit_test_result);
  priv->button = event->button;
  priv->modifier = event->state;
  priv->x = event->x;
  priv->y = event->y;

  return embed_event;
}

guint
ephy_embed_event_get_context (EphyEmbedEvent *event)
{
  EphyEmbedEventPrivate *priv;
  guint context;

  g_return_val_if_fail (EPHY_IS_EMBED_EVENT (event), 0);

  priv = event->priv;
  g_object_get (priv->hit_test_result, "context", &context, NULL);
  return context;
}

guint
ephy_embed_event_get_button (EphyEmbedEvent *event)
{
  EphyEmbedEventPrivate *priv;

  g_return_val_if_fail (EPHY_IS_EMBED_EVENT (event), 0);

  priv = event->priv;

  return priv->button;
}

guint
ephy_embed_event_get_modifier (EphyEmbedEvent *event)
{
  EphyEmbedEventPrivate *priv;

  g_return_val_if_fail (EPHY_IS_EMBED_EVENT (event), 0);

  priv = event->priv;

  return priv->modifier;
}

void
ephy_embed_event_get_coords (EphyEmbedEvent *event,
                             guint *x, guint *y)
{
  EphyEmbedEventPrivate *priv;

  g_return_if_fail (EPHY_IS_EMBED_EVENT (event));

  priv = event->priv;

  if (x)
    *x = priv->x;

  if (y)
    *y = priv->y;
}

/**
 * ephy_embed_event_get_property:
 * @name: the name of the property
 * @value: (out): a variable to hold its value
 */
void 
ephy_embed_event_get_property   (EphyEmbedEvent *event,
                                 const char *name,
                                 GValue *value)
{
  EphyEmbedEventPrivate *priv;

  g_return_if_fail (EPHY_IS_EMBED_EVENT (event));
  g_return_if_fail (name);

  priv = event->priv;

  /* FIXME: ugly hack! This only works for now because all properties
     we have are strings */
  g_value_init (value, G_TYPE_STRING);

  g_object_get_property (G_OBJECT (priv->hit_test_result), name, value);
}

gboolean
ephy_embed_event_has_property   (EphyEmbedEvent *event,
                                 const char *name)
{
  EphyEmbedEventPrivate *priv;

  g_return_val_if_fail (EPHY_IS_EMBED_EVENT (event), FALSE);
  g_return_val_if_fail (name, FALSE);

  priv = event->priv;

  return g_object_class_find_property (G_OBJECT_GET_CLASS (priv->hit_test_result),
                                       name) != NULL;
                                                           
}

/**
 * ephy_embed_event_get_hit_test_result:
 * @event: an #EphyEmbedEvent
 * 
 * Returns: (transfer none): returns the #WebKitHitTestResult associated with @event
 **/
WebKitHitTestResult *
ephy_embed_event_get_hit_test_result (EphyEmbedEvent *event)
{
  g_return_val_if_fail (EPHY_IS_EMBED_EVENT (event), NULL);
  
  return event->priv->hit_test_result;
}
