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

#ifndef MOZILLA_EMBED_SINGLE_H
#define MOZILLA_EMBED_SINGLE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define MOZILLA_TYPE_EMBED_SINGLE		(mozilla_embed_single_get_type ())
#define MOZILLA_EMBED_SINGLE(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), MOZILLA_TYPE_EMBED_SINGLE, MozillaEmbedSingle))
#define MOZILLA_EMBED_SINGLE_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), MOZILLA_TYPE_EMBED_SINGLE, MozillaEmbedSingleClass))
#define MOZILLA_IS_EMBED_SINGLE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), MOZILLA_TYPE_EMBED_SINGLE))
#define MOZILLA_IS_EMBED_SINGLE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), MOZILLA_TYPE_EMBED_SINGLE))
#define MOZILLA_EMBED_SINGLE_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), MOZILLA_TYPE_EMBED_SINGLE, MozillaEmbedSingleClass))

typedef struct MozillaEmbedSingle		MozillaEmbedSingle;
typedef struct MozillaEmbedSingleClass		MozillaEmbedSingleClass;
typedef struct MozillaEmbedSinglePrivate	MozillaEmbedSinglePrivate;

struct MozillaEmbedSingle
{
	GObject parent;

	/*< private >*/
	MozillaEmbedSinglePrivate *priv;
};

struct MozillaEmbedSingleClass
{
	GObjectClass parent_class;
};

GType	mozilla_embed_single_get_type	(void);

G_END_DECLS

#endif
