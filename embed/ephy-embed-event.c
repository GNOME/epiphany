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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "ephy-embed-event.h"

struct _EphyEmbedEvent {
  GObject parent_instance;

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
  EphyEmbedEvent *event = EPHY_EMBED_EVENT (object);

  g_clear_object (&event->hit_test_result);

  G_OBJECT_CLASS (ephy_embed_event_parent_class)->dispose (object);
}

static void
ephy_embed_event_class_init (EphyEmbedEventClass *klass)
{
  GObjectClass *object_class = (GObjectClass *)klass;

  object_class->dispose = dispose;
}

static void
ephy_embed_event_init (EphyEmbedEvent *embed_event)
{
}

EphyEmbedEvent *
ephy_embed_event_new (GdkEventButton *event, WebKitHitTestResult *hit_test_result)
{
  EphyEmbedEvent *embed_event;

  embed_event = g_object_new (EPHY_TYPE_EMBED_EVENT, NULL);

  embed_event->hit_test_result = g_object_ref (hit_test_result);
  embed_event->button = event->button;
  embed_event->modifier = event->state;
  embed_event->x = event->x;
  embed_event->y = event->y;

  return embed_event;
}

guint
ephy_embed_event_get_context (EphyEmbedEvent *event)
{
  guint context;

  g_return_val_if_fail (EPHY_IS_EMBED_EVENT (event), 0);

  g_object_get (event->hit_test_result, "context", &context, NULL);
  return context;
}

guint
ephy_embed_event_get_button (EphyEmbedEvent *event)
{
  g_return_val_if_fail (EPHY_IS_EMBED_EVENT (event), 0);

  return event->button;
}

guint
ephy_embed_event_get_modifier (EphyEmbedEvent *event)
{
  g_return_val_if_fail (EPHY_IS_EMBED_EVENT (event), 0);

  return event->modifier;
}

void
ephy_embed_event_get_coords (EphyEmbedEvent *event,
                             guint *x, guint *y)
{
  g_return_if_fail (EPHY_IS_EMBED_EVENT (event));

  if (x)
    *x = event->x;

  if (y)
    *y = event->y;
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
  g_return_if_fail (EPHY_IS_EMBED_EVENT (event));
  g_return_if_fail (name);

  /* FIXME: ugly hack! This only works for now because all properties
     we have are strings */
  g_value_init (value, G_TYPE_STRING);

  g_object_get_property (G_OBJECT (event->hit_test_result), name, value);
}

gboolean
ephy_embed_event_has_property   (EphyEmbedEvent *event,
                                 const char *name)
{
  g_return_val_if_fail (EPHY_IS_EMBED_EVENT (event), FALSE);
  g_return_val_if_fail (name, FALSE);

  return g_object_class_find_property (G_OBJECT_GET_CLASS (event->hit_test_result),
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
  
  return event->hit_test_result;
}
