/*
 *  Copyright (C) 2000-2003 Marco Pesenti Gritti
 *  Copyright (C) 2003 Christian Persch
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

#ifndef EPHY_EMBED_PERSIST_H
#define EPHY_EMBED_PERSIST_H

#include "ephy-embed.h"

#include <glib-object.h>
#include <glib.h>

#include <gtk/gtkwindow.h>

G_BEGIN_DECLS

#define EPHY_TYPE_EMBED_PERSIST		(ephy_embed_persist_get_type ())
#define EPHY_EMBED_PERSIST(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_EMBED_PERSIST, EphyEmbedPersist))
#define EPHY_EMBED_PERSIST_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_EMBED_PERSIST, EphyEmbedPersistClass))
#define EPHY_IS_EMBED_PERSIST(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_EMBED_PERSIST))
#define EPHY_IS_EMBED_PERSIST_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_EMBED_PERSIST))
#define EPHY_EMBED_PERSIST_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_EMBED_PERSIST, EphyEmbedPersistClass))

typedef struct EphyEmbedPersistClass EphyEmbedPersistClass;
typedef struct EphyEmbedPersist EphyEmbedPersist;
typedef struct EphyEmbedPersistPrivate EphyEmbedPersistPrivate;

typedef enum
{
	EMBED_PERSIST_COPY_PAGE		= 1 << 0,
	EMBED_PERSIST_MAINDOC		= 1 << 1,
	EMBED_PERSIST_NO_VIEW		= 1 << 2,
	EMBED_PERSIST_ASK_DESTINATION	= 1 << 3
} EmbedPersistFlags;

struct EphyEmbedPersist
{
	GObject parent;

	/*< private >*/
	EphyEmbedPersistPrivate *priv;
};

struct EphyEmbedPersistClass
{
	GObjectClass parent_class;

	void	 (* completed)	(EphyEmbedPersist *persist);

	/* Methods */

	gboolean (* save)	(EphyEmbedPersist *persist);

	void	 (* cancel)	(EphyEmbedPersist *persist);
};

GType			 ephy_embed_persist_get_type	(void);

EphyEmbedPersist	*ephy_embed_persist_new			(EphyEmbed *embed);

gboolean		 ephy_embed_persist_save		(EphyEmbedPersist *persist);

void			 ephy_embed_persist_cancel		(EphyEmbedPersist *persist);

void			 ephy_embed_persist_set_dest		(EphyEmbedPersist *persist,
								 const char *value);

void			 ephy_embed_persist_set_embed		(EphyEmbedPersist *persist,
								 EphyEmbed *value);

void			 ephy_embed_persist_set_fc_title	(EphyEmbedPersist *persist,
								 const char *value);

void			 ephy_embed_persist_set_fc_parent	(EphyEmbedPersist *persist,
								 GtkWindow *value);

void			 ephy_embed_persist_set_flags		(EphyEmbedPersist *persist,
								 EmbedPersistFlags value);

void			 ephy_embed_persist_set_max_size	(EphyEmbedPersist *persist,
								 int value);

void			 ephy_embed_persist_set_persist_key	(EphyEmbedPersist *persist,
								 const char *value);

void			 ephy_embed_persist_set_source		(EphyEmbedPersist *persist,
								 const char *value);

const char 		*ephy_embed_persist_get_dest		(EphyEmbedPersist *persist);

EphyEmbed		*ephy_embed_persist_get_embed		(EphyEmbedPersist *persist);

const char 		*ephy_embed_persist_get_fc_title	(EphyEmbedPersist *persist);

GtkWindow		*ephy_embed_persist_get_fc_parent	(EphyEmbedPersist *persist);

EmbedPersistFlags	 ephy_embed_persist_get_flags		(EphyEmbedPersist *persist);

int			 ephy_embed_persist_get_max_size	(EphyEmbedPersist *persist);

const char 		*ephy_embed_persist_get_persist_key	(EphyEmbedPersist *persist);

const char 		*ephy_embed_persist_get_source		(EphyEmbedPersist *persist);

G_END_DECLS

#endif
