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

#ifndef MOZILLA_EMBED_H
#define MOZILLA_EMBED_H

#include <glib.h>
#include <glib-object.h>

#include <gtkmozembed.h>

#include "ephy-embed.h"

G_BEGIN_DECLS

#define MOZILLA_TYPE_EMBED		(mozilla_embed_get_type ())
#define MOZILLA_EMBED(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), MOZILLA_TYPE_EMBED, MozillaEmbed))
#define MOZILLA_EMBED_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), MOZILLA_TYPE_EMBED, MozillaEmbedClass))
#define MOZILLA_IS_EMBED(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), MOZILLA_TYPE_EMBED))
#define MOZILLA_IS_EMBED_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), MOZILLA_TYPE_EMBED))
#define MOZILLA_EMBED_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), MOZILLA_TYPE_EMBED, MozillaEmbedClass))

typedef struct MozillaEmbedClass	MozillaEmbedClass;
typedef struct MozillaEmbed		MozillaEmbed;
typedef struct MozillaEmbedPrivate	MozillaEmbedPrivate;

struct MozillaEmbed
{
        GtkMozEmbed parent;

	/*< private >*/
        MozillaEmbedPrivate *priv;
};

struct MozillaEmbedClass
{
        GtkMozEmbedClass parent_class;
};

GType	         mozilla_embed_get_type         (void);

/* The following are private to the embed implementation */

gpointer        _mozilla_embed_get_ephy_browser (MozillaEmbed *embed);

GtkMozEmbed    *_mozilla_embed_new_xul_dialog   (void);

EphyEmbedChrome _mozilla_embed_translate_chrome	(GtkMozEmbedChromeFlags flags);

G_END_DECLS

#endif
