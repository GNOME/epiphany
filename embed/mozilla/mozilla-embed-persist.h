/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
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

#ifndef MOZILLA_EMBED_PERSIST_H
#define MOZILLA_EMBED_PERSIST_H

#include <glib.h>
#include <glib-object.h>

#include "ephy-embed-persist.h"

G_BEGIN_DECLS

#define MOZILLA_TYPE_EMBED_PERSIST		(mozilla_embed_persist_get_type ())
#define MOZILLA_EMBED_PERSIST(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), MOZILLA_TYPE_EMBED_PERSIST, MozillaEmbedPersist))
#define MOZILLA_EMBED_PERSIST_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), MOZILLA_TYPE_EMBED_PERSIST, MozillaEmbedPersistClass))
#define MOZILLA_IS_EMBED_PERSIST(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), MOZILLA_TYPE_EMBED_PERSIST))
#define MOZILLA_IS_EMBED_PERSIST_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), MOZILLA_TYPE_EMBED_PERSIST))
#define MOZILLA_EMBED_PERSIST_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), MOZILLA_TYPE_EMBED_PERSIST, MozillaEmbedPersistClass))

typedef struct MozillaEmbedPersistClass MozillaEmbedPersistClass;
typedef struct MozillaEmbedPersist MozillaEmbedPersist;
typedef struct MozillaEmbedPersistPrivate MozillaEmbedPersistPrivate;

struct MozillaEmbedPersist
{
	EphyEmbedPersist parent;

	/*< private >*/
	MozillaEmbedPersistPrivate *priv;
};

struct MozillaEmbedPersistClass
{
	EphyEmbedPersistClass parent_class;
};

GType	mozilla_embed_persist_get_type	(void);

void	mozilla_embed_persist_completed	(MozillaEmbedPersist *persist);

void	mozilla_embed_persist_cancelled	(MozillaEmbedPersist *persist);

G_END_DECLS

#endif
