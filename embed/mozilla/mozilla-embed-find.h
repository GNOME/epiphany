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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#ifndef MOZILLA_EMBED_FIND_H
#define MOZILLA_EMBED_FIND_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define MOZILLA_TYPE_EMBED_FIND		(mozilla_embed_find_get_type ())
#define MOZILLA_EMBED_FIND(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), MOZILLA_TYPE_EMBED_FIND, MozillaEmbedFind))
#define MOZILLA_EMBED_FIND_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), MOZILLA_TYPE_EMBED_FIND, MozillaEmbedFindClass))
#define MOZILLA_IS_EMBED_FIND(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), MOZILLA_TYPE_EMBED_FIND))
#define MOZILLA_IS_EMBED_FIND_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), MOZILLA_TYPE_EMBED_FIND))
#define MOZILLA_EMBED_FIND_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), MOZILLA_TYPE_EMBED_FIND, MozillaEmbedFindClass))

typedef struct _MozillaEmbedFindClass	MozillaEmbedFindClass;
typedef struct _MozillaEmbedFind	MozillaEmbedFind;
typedef struct _MozillaEmbedFindPrivate	MozillaEmbedFindPrivate;

struct _MozillaEmbedFind
{
	GObject parent_instance;

	/*< private >*/
        MozillaEmbedFindPrivate *priv;
};

struct _MozillaEmbedFindClass
{
	GObjectClass parent_class;
};

GType	mozilla_embed_find_get_type	(void);

G_END_DECLS

#endif
