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

#ifndef MOZILLA_EMBED_SINGLE_H
#define MOZILLA_EMBED_SINGLE_H

#include "glib.h"
#include "ephy-embed-shell.h"
#include <glib-object.h>

G_BEGIN_DECLS

#define MOZILLA_TYPE_EMBED_SINGLE		(mozilla_embed_single_get_type ())
#define MOZILLA_EMBED_SINGLE(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), MOZILLA_TYPE_EMBED_SINGLE, MozillaEmbedSingle))
#define MOZILLA_EMBED_SINGLE_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), MOZILLA_TYPE_EMBED_SINGLE, MozillaEmbedSingleClass))
#define MOZILLA_IS_EMBED_SINGLE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), MOZILLA_TYPE_EMBED_SINGLE))
#define MOZILLA_IS_EMBED_SINGLE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), MOZILLA_TYPE_EMBED_SINGLE))
#define MOZILLA_EMBED_SINGLE_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), MOZILLA_TYPE_EMBED_SINGLE, MozillaEmbedSingleClass))

typedef struct MozillaEmbedSingle MozillaEmbedSingle;
typedef struct MozillaEmbedSinglePrivate MozillaEmbedSinglePrivate;

struct MozillaEmbedSingle
{
	EphyEmbedSingle parent;
        MozillaEmbedSinglePrivate *priv;
};

struct MozillaEmbedSingleClass
{
        EphyEmbedSingleClass parent_class;
};

GType             mozilla_embed_single_get_type	     (void);

EphyEmbedSingle  *mozilla_embed_single_new	     (void);

gboolean	  mozilla_embed_single_init_services (MozillaEmbedSingle *single);

G_END_DECLS

#endif
