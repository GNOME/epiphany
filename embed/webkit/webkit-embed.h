/*
 *  Copyright © 2007 Xan Lopez
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

#ifndef WEBKIT_EMBED_H
#define WEBKIT_EMBED_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include "ephy-embed.h"

G_BEGIN_DECLS

#define WEBKIT_TYPE_EMBED		(webkit_embed_get_type ())
#define WEBKIT_EMBED(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), WEBKIT_TYPE_EMBED, WebkitEmbed))
#define WEBKIT_EMBED_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), WEBKIT_TYPE_EMBED, WebkitEmbedClass))
#define WEBKIT_IS_EMBED(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), WEBKIT_TYPE_EMBED))
#define WEBKIT_IS_EMBED_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), WEBKIT_TYPE_EMBED))
#define WEBKIT_EMBED_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), WEBKIT_TYPE_EMBED, WebkitEmbedClass))

typedef struct WebkitEmbedClass	WebkitEmbedClass;
typedef struct WebkitEmbed		WebkitEmbed;
typedef struct WebkitEmbedPrivate	WebkitEmbedPrivate;

struct WebkitEmbed
{
        GtkScrolledWindow parent;

	/*< private >*/
        WebkitEmbedPrivate *priv;
};

struct WebkitEmbedClass
{
        GtkScrolledWindowClass parent_class;
};

GType	         webkit_embed_get_type         (void);

G_END_DECLS

#endif
