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

#ifndef EPHY_EMBED_EVENT_H
#define EPHY_EMBED_EVENT_H

#include "ephy-embed-types.h"

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

typedef struct EphyEmbedEventClass EphyEmbedEventClass;

#define EPHY_EMBED_EVENT_TYPE             (ephy_embed_event_get_type ())
#define EPHY_EMBED_EVENT(obj)             (GTK_CHECK_CAST ((obj), EPHY_EMBED_EVENT_TYPE, EphyEmbedEvent))
#define EPHY_EMBED_EVENT_CLASS(klass)     (GTK_CHECK_CLASS_CAST ((klass), EPHY_PERSIST_SHELL, EphyEmbedEventClass))
#define IS_EPHY_EMBED_EVENT(obj)          (GTK_CHECK_TYPE ((obj), EPHY_EMBED_EVENT_TYPE))
#define IS_EPHY_EMBED_EVENT_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), EPHY_EMBED_EVENT))
#define EPHY_EMBED_EVENT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), EPHY_EMBED_SHELL_TYPE, EphyEmbedEventClass))

typedef struct EphyEmbedEvent EphyEmbedEvent;
typedef struct EphyEmbedEventPrivate EphyEmbedEventPrivate;

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

struct EphyEmbedEvent
{
        GObject parent;
        EphyEmbedEventPrivate *priv;

	/* Public to the embed implementations */
	guint modifier;
	guint mouse_button;
	guint context;
	guint x, y;
};

struct EphyEmbedEventClass
{
        GObjectClass parent_class;
};

GType             ephy_embed_event_get_type		(void);

EphyEmbedEvent   *ephy_embed_event_new			(void);

guint		  ephy_embed_event_get_modifier		(EphyEmbedEvent *event);

gresult		  ephy_embed_event_get_mouse_button	(EphyEmbedEvent *event,
							 guint *mouse_button);

gresult		  ephy_embed_event_get_coords		(EphyEmbedEvent *event,
							 guint *x, guint *y);

gresult		  ephy_embed_event_get_context		(EphyEmbedEvent *event,
							 EmbedEventContext *context);

void		  ephy_embed_event_set_property		(EphyEmbedEvent *event,
							 const char *name,
							 GValue *value);

void		  ephy_embed_event_get_property		(EphyEmbedEvent *event,
							 const char *name,
							 const GValue **value);

gboolean	  ephy_embed_event_has_property		(EphyEmbedEvent *event,
							 const char *name);


G_END_DECLS

#endif
