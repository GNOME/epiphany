/*
 *  Copyright © 2000-2004 Marco Pesenti Gritti
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

#ifndef WEBKIT_EMBED_FIND_H
#define WEBKIT_EMBED_FIND_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_EMBED_FIND		(webkit_embed_find_get_type ())
#define WEBKIT_EMBED_FIND(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), WEBKIT_TYPE_EMBED_FIND, WebkitEmbedFind))
#define WEBKIT_EMBED_FIND_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), WEBKIT_TYPE_EMBED_FIND, WebkitEmbedFindClass))
#define WEBKIT_IS_EMBED_FIND(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), WEBKIT_TYPE_EMBED_FIND))
#define WEBKIT_IS_EMBED_FIND_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), WEBKIT_TYPE_EMBED_FIND))
#define WEBKIT_EMBED_FIND_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), WEBKIT_TYPE_EMBED_FIND, WebkitEmbedFindClass))

typedef struct _WebkitEmbedFindClass	WebkitEmbedFindClass;
typedef struct _WebkitEmbedFind	WebkitEmbedFind;
typedef struct _WebkitEmbedFindPrivate	WebkitEmbedFindPrivate;

struct _WebkitEmbedFind
{
	GObject parent_instance;

	/*< private >*/
        WebkitEmbedFindPrivate *priv;
};

struct _WebkitEmbedFindClass
{
	GObjectClass parent_class;
};

GType	webkit_embed_find_get_type	(void);

G_END_DECLS

#endif
