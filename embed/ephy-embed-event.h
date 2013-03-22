/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
 *  Copyright © 2004 Christian Persch
 *  Copyright © 2009 Igalia S.L.
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

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef EPHY_EMBED_EVENT_H
#define EPHY_EMBED_EVENT_H

#include <webkit2/webkit2.h>

G_BEGIN_DECLS

#define EPHY_TYPE_EMBED_EVENT               (ephy_embed_event_get_type ())
#define EPHY_EMBED_EVENT(o)                 (G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_EMBED_EVENT, EphyEmbedEvent))
#define EPHY_EMBED_EVENT_CLASS(k)           (G_TYPE_CHECK_CLASS_CAST ((k), EPHY_TYPE_EMBED_EVENT, EphyEmbedEventClass))
#define EPHY_IS_EMBED_EVENT(o)              (G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_EMBED_EVENT))
#define EPHY_IS_EMBED_EVENT_CLASS(k)        (G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_EMBED_EVENT))
#define EPHY_EMBED_EVENT_GET_CLASS(o)       (G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_EMBED_EVENT, EphyEmbedEventClass))

typedef struct EphyEmbedEventClass EphyEmbedEventClass;
typedef struct EphyEmbedEvent EphyEmbedEvent;
typedef struct EphyEmbedEventPrivate EphyEmbedEventPrivate;

struct EphyEmbedEvent {
  GObject parent_instance;

  /*< private >*/
  EphyEmbedEventPrivate *priv;
};

struct EphyEmbedEventClass {
  GObjectClass parent_class;
};


GType                ephy_embed_event_get_type            (void);
EphyEmbedEvent *     ephy_embed_event_new                 (GdkEventButton      *event,
                                                           WebKitHitTestResult *hit_test_result);
guint                ephy_embed_event_get_context         (EphyEmbedEvent      *event);
guint                ephy_embed_event_get_button          (EphyEmbedEvent      *event);
guint                ephy_embed_event_get_modifier        (EphyEmbedEvent      *event);
void                 ephy_embed_event_get_coords          (EphyEmbedEvent      *event,
                                                           guint               *x,
                                                           guint               *y);
void                 ephy_embed_event_get_property        (EphyEmbedEvent      *event,
                                                           const char          *name,
                                                           GValue              *value);
gboolean             ephy_embed_event_has_property        (EphyEmbedEvent      *event,
                                                           const char          *name);
WebKitHitTestResult *ephy_embed_event_get_hit_test_result (EphyEmbedEvent      *event);

G_END_DECLS

#endif
