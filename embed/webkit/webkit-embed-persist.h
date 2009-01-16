/*
 *  Copyright © 2007 Xan Lopez <xan@gnome.org>
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

#ifndef WEBKIT_EMBED_PERSIST_H
#define WEBKIT_EMBED_PERSIST_H

#include <glib.h>
#include <glib-object.h>

#include "ephy-embed-persist.h"

G_BEGIN_DECLS

#define WEBKIT_TYPE_EMBED_PERSIST		(webkit_embed_persist_get_type ())
#define WEBKIT_EMBED_PERSIST(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), WEBKIT_TYPE_EMBED_PERSIST, WebKitEmbedPersist))
#define WEBKIT_EMBED_PERSIST_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), WEBKIT_TYPE_EMBED_PERSIST, WebKitEmbedPersistClass))
#define WEBKIT_IS_EMBED_PERSIST(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), WEBKIT_TYPE_EMBED_PERSIST))
#define WEBKIT_IS_EMBED_PERSIST_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), WEBKIT_TYPE_EMBED_PERSIST))
#define WEBKIT_EMBED_PERSIST_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), WEBKIT_TYPE_EMBED_PERSIST, WebKitEmbedPersistClass))

typedef struct WebKitEmbedPersistClass WebKitEmbedPersistClass;
typedef struct WebKitEmbedPersist WebKitEmbedPersist;
typedef struct WebKitEmbedPersistPrivate WebKitEmbedPersistPrivate;

struct WebKitEmbedPersist
{
	EphyEmbedPersist parent;
};

struct WebKitEmbedPersistClass
{
	EphyEmbedPersistClass parent_class;
};

GType	webkit_embed_persist_get_type	(void);

void	webkit_embed_persist_completed	(WebKitEmbedPersist *persist);

void	webkit_embed_persist_cancelled	(WebKitEmbedPersist *persist);

G_END_DECLS

#endif
