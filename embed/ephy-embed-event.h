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

#ifndef EPHY_EMBED_EVENT_H
#define EPHY_EMBED_EVENT_H

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#define EPHY_TYPE_EMBED_EVENT			(ephy_embed_event_get_type ())
#define EPHY_EMBED_EVENT(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_EMBED_EVENT, EphyEmbedEvent))
#define EPHY_EMBED_EVENT_IFACE(k)		(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_EMBED_EVENT, EphyEmbedEventIFace))
#define EPHY_IS_EMBED_EVENT(o)			(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_EMBED_EVENT))
#define EPHY_IS_EMBED_EVENT_IFACE(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_EMBED_EVENT))
#define EPHY_EMBED_EVENT_GET_IFACE(inst)	(G_TYPE_INSTANCE_GET_INTERFACE ((inst), EPHY_TYPE_EMBED_EVENT, EphyEmbedEventIFace))

typedef struct EphyEmbedEventIFace	EphyEmbedEventIFace;
typedef struct EphyEmbedEvent		EphyEmbedEvent;

typedef enum
{
	EMBED_CONTEXT_NONE     = 0,
        EMBED_CONTEXT_DEFAULT  = 1 << 1,
        EMBED_CONTEXT_LINK     = 1 << 2,
        EMBED_CONTEXT_IMAGE    = 1 << 3,
        EMBED_CONTEXT_DOCUMENT = 1 << 4,
        EMBED_CONTEXT_INPUT    = 1 << 5,
        EMBED_CONTEXT_XUL      = 1 << 7,
	EMBED_CONTEXT_EMAIL_LINK = 1 << 8
} EmbedEventContext;

typedef enum
{
	EPHY_EMBED_EVENT_MOUSE_BUTTON1,
	EPHY_EMBED_EVENT_MOUSE_BUTTON2,
	EPHY_EMBED_EVENT_MOUSE_BUTTON3,
	EPHY_EMBED_EVENT_KEY
} EphyEmbedEventType;

struct EphyEmbedEventIFace
{
	GTypeInterface parent_iface;

	/* Methods */
	EphyEmbedEventType	(* get_type)		(EphyEmbedEvent *event);
	EmbedEventContext	(* get_context)		(EphyEmbedEvent *event);
	guint			(* get_modifier)	(EphyEmbedEvent *event);
	void			(* get_coordinates)	(EphyEmbedEvent *event,
							 guint *x,
							 guint *y);
	void			(* get_property)	(EphyEmbedEvent *event,
							 const char *name,
							 const GValue **value);
	gboolean		(* has_property)	(EphyEmbedEvent *event,
							 const char *name);
	gpointer		(* get_dom_event)	(EphyEmbedEvent *event);
};

GType			ephy_embed_event_get_type	(void);

EphyEmbedEventType	ephy_embed_event_get_event_type	(EphyEmbedEvent *event);

EmbedEventContext	ephy_embed_event_get_context	(EphyEmbedEvent *event);

guint			ephy_embed_event_get_modifier	(EphyEmbedEvent *event);


void			ephy_embed_event_get_coords	(EphyEmbedEvent *event,
							 guint *x, guint *y);


void			ephy_embed_event_get_property	(EphyEmbedEvent *event,
							 const char *name,
							 const GValue **value);

gboolean		ephy_embed_event_has_property	(EphyEmbedEvent *event,
							 const char *name);

gpointer		ephy_embed_event_get_dom_event	(EphyEmbedEvent *event);

G_END_DECLS

#endif
