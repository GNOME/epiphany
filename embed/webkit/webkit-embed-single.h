/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
 *  Copyright © 2003 Christian Persch
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

#ifndef WEBKIT_EMBED_SINGLE_H
#define WEBKIT_EMBED_SINGLE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_EMBED_SINGLE		(webkit_embed_single_get_type ())
#define WEBKIT_EMBED_SINGLE(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), WEBKIT_TYPE_EMBED_SINGLE, WebKitEmbedSingle))
#define WEBKIT_EMBED_SINGLE_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), WEBKIT_TYPE_EMBED_SINGLE, WebKitEmbedSingleClass))
#define WEBKIT_IS_EMBED_SINGLE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), WEBKIT_TYPE_EMBED_SINGLE))
#define WEBKIT_IS_EMBED_SINGLE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), WEBKIT_TYPE_EMBED_SINGLE))
#define WEBKIT_EMBED_SINGLE_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), WEBKIT_TYPE_EMBED_SINGLE, WebKitEmbedSingleClass))

typedef struct WebKitEmbedSingle		WebKitEmbedSingle;
typedef struct WebKitEmbedSingleClass		WebKitEmbedSingleClass;
typedef struct WebKitEmbedSinglePrivate	WebKitEmbedSinglePrivate;

struct WebKitEmbedSingle
{
	GObject parent;

	/*< private >*/
	WebKitEmbedSinglePrivate *priv;
};

struct WebKitEmbedSingleClass
{
	GObjectClass parent_class;
};

GType	webkit_embed_single_get_type	(void);

G_END_DECLS

#endif
