/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
 *  Copyright © 2004 Christian Persch
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

#ifndef MOZILLA_EMBED_EVENT_H
#define MOZILLA_EMBED_EVENT_H

#include <glib.h>
#include <glib-object.h>

#include "ephy-embed-event.h"

G_BEGIN_DECLS

#define MOZILLA_TYPE_EMBED_EVENT		(mozilla_embed_event_get_type ())
#define MOZILLA_EMBED_EVENT(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), MOZILLA_TYPE_EMBED_EVENT, MozillaEmbedEvent))
#define MOZILLA_EMBED_EVENT_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), MOZILLA_TYPE_EMBED_EVENT, MozillaEmbedEventClass))
#define MOZILLA_IS_EMBED_EVENT(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), MOZILLA_TYPE_EMBED_EVENT))
#define MOZILLA_IS_EMBED_EVENT_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), MOZILLA_TYPE_EMBED_EVENT))
#define MOZILLA_EMBED_EVENT_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), MOZILLA_TYPE_EMBED_EVENT, MozillaEmbedEventClass))

typedef struct _MozillaEmbedEventClass		MozillaEmbedEventClass;
typedef struct _MozillaEmbedEvent		MozillaEmbedEvent;
typedef struct _MozillaEmbedEventPrivate	MozillaEmbedEventPrivate;

struct _MozillaEmbedEventClass
{
        GObjectClass parent_class;
};

struct _MozillaEmbedEvent
{
        GObject parent;

	/*< private >*/
        MozillaEmbedEventPrivate *priv;

	/*< private >*/ /* public to the embed implementation */
	guint button;
	guint context;
	guint modifier;
	guint x;
	guint y;
	guint keycode;
};

GType		  mozilla_embed_event_get_type		(void);

MozillaEmbedEvent *mozilla_embed_event_new		(gpointer dom_event);

void		  mozilla_embed_event_set_property	(MozillaEmbedEvent *event,
							 const char *name,
							 GValue *value);

G_END_DECLS

#endif
