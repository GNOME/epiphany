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

#ifndef EPHY_EMBED_PERSIST_H
#define EPHY_EMBED_PERSIST_H

#include "ephy-embed.h"

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

typedef struct EphyEmbedPersistClass EphyEmbedPersistClass;

#define EPHY_EMBED_PERSIST_TYPE             (ephy_embed_persist_get_type ())
#define EPHY_EMBED_PERSIST(obj)             (GTK_CHECK_CAST ((obj), EPHY_EMBED_PERSIST_TYPE, EphyEmbedPersist))
#define EPHY_EMBED_PERSIST_CLASS(klass)     (GTK_CHECK_CLASS_CAST ((klass), EPHY_EMBED_PERSIST_TYPE, EphyEmbedPersistClass))
#define IS_EPHY_EMBED_PERSIST(obj)          (GTK_CHECK_TYPE ((obj), EPHY_EMBED_PERSIST_TYPE))
#define IS_EPHY_EMBED_PERSIST_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), EPHY_EMBED_PERSIST))
#define EPHY_EMBED_PERSIST_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), EPHY_EMBED_PERSIST_TYPE, EphyEmbedPersistClass))

typedef struct EphyEmbedPersist EphyEmbedPersist;
typedef struct EphyEmbedPersistPrivate EphyEmbedPersistPrivate;

typedef enum
{
	EMBED_PERSIST_SHOW_PROGRESS = 1 << 0,
	EMBED_PERSIST_SAVE_CONTENT = 1 << 1,
	EMBED_PERSIST_FROMCACHE = 1 << 2,
	EMBED_PERSIST_BYPASSCACHE = 1 << 3,
	EMBED_PERSIST_MAINDOC = 1 << 4
} EmbedPersistFlags;

typedef struct
{
	char *command;
	gboolean need_terminal;
} PersistHandlerInfo;

struct EphyEmbedPersist
{
        GObject parent;
        EphyEmbedPersistPrivate *priv;
};

struct EphyEmbedPersistClass
{
        GObjectClass parent_class;

	void (* completed) (EphyEmbedPersist *persist);

	/* Methods */

	gresult (* set_source)   (EphyEmbedPersist *persist,
				  const char *url);
	gresult (* set_dest)     (EphyEmbedPersist *persist,
				  const char *dir);
	gresult (* save)         (EphyEmbedPersist *persist);

	gresult (* set_max_size) (EphyEmbedPersist *persist,
				  int max_size);

	gresult (* set_embed)    (EphyEmbedPersist *persist,
				  EphyEmbed *embed);

	gresult (* set_flags)    (EphyEmbedPersist *persist,
				  EmbedPersistFlags flags);

	gresult (* set_handler)  (EphyEmbedPersist *persist,
				  const char *command,
				  gboolean need_terminal);
};

GType               ephy_embed_persist_get_type    (void);

EphyEmbedPersist   *ephy_embed_persist_new         (EphyEmbed *embed);

gresult             ephy_embed_persist_set_source  (EphyEmbedPersist *persist,
						    const char *url);

gresult             ephy_embed_persist_get_source  (EphyEmbedPersist *persist,
						    const char **url);

gresult		    ephy_embed_persist_set_dest    (EphyEmbedPersist *persist,
						    const char *dir);

gresult		    ephy_embed_persist_get_dest    (EphyEmbedPersist *persist,
						    const char **dir);

gresult		    ephy_embed_persist_set_handler (EphyEmbedPersist *persist,
						    const char *handler,
						    gboolean need_terminal);

gresult		    ephy_embed_persist_set_max_size (EphyEmbedPersist *persist,
						     int kb_size);

gresult             ephy_embed_persist_set_embed   (EphyEmbedPersist *persist,
		                                    EphyEmbed *embed);

gresult             ephy_embed_persist_get_embed   (EphyEmbedPersist *persist,
		                                    EphyEmbed **embed);

gresult		    ephy_embed_persist_set_flags   (EphyEmbedPersist *persist,
						    EmbedPersistFlags flags);

gresult		    ephy_embed_persist_get_flags   (EphyEmbedPersist *persist,
						    EmbedPersistFlags *flags);

gresult		    ephy_embed_persist_save        (EphyEmbedPersist *persist);

G_END_DECLS

#endif
