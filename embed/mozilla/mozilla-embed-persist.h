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

#ifndef MOZILLA_EMBED_PERSIST_H
#define MOZILLA_EMBED_PERSIST_H

#include "ephy-embed-persist.h"

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

typedef struct MozillaEmbedPersistClass MozillaEmbedPersistClass;

#define MOZILLA_EMBED_PERSIST_TYPE             (mozilla_embed_persist_get_type ())
#define MOZILLA_EMBED_PERSIST(obj)             (GTK_CHECK_CAST ((obj), MOZILLA_EMBED_PERSIST_TYPE, MozillaEmbedPersist))
#define MOZILLA_EMBED_PERSIST_CLASS(klass)     (GTK_CHECK_CLASS_CAST ((klass), MOZILLA_EMBED_PERSIST_TYPE, MozillaEmbedPersistClass))
#define IS_MOZILLA_EMBED_PERSIST(obj)          (GTK_CHECK_TYPE ((obj), MOZILLA_EMBED_PERSIST_TYPE))
#define IS_MOZILLA_EMBED_PERSIST_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), MOZILLA_EMBED_PERSIST))
#define MOZILLA_EMBED_PERSIST_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), MOZILLA_EMBED_PERSIST_TYPE, MozillaEmbedPersistClass))

typedef struct MozillaEmbedPersist MozillaEmbedPersist;
typedef struct MozillaEmbedPersistPrivate MozillaEmbedPersistPrivate;

struct MozillaEmbedPersist 
{
        EphyEmbedPersist parent;
        MozillaEmbedPersistPrivate *priv;
};

struct MozillaEmbedPersistClass
{
        EphyEmbedPersistClass parent_class;
};

GType               mozilla_embed_persist_get_type   (void);

G_END_DECLS

#endif
