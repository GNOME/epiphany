/*
 *  Copyright (C) 2005 Christian Persch
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

#ifndef EPHY_EMBED_FIND_H
#define EPHY_EMBED_FIND_H

#include <glib-object.h>
#include <glib.h>

#include "ephy-embed.h"
#include <gdk/gdktypes.h>

G_BEGIN_DECLS

#define EPHY_TYPE_EMBED_FIND		(ephy_embed_find_get_type ())
#define EPHY_EMBED_FIND(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_EMBED_FIND, EphyEmbedFind))
#define EPHY_EMBED_FIND_IFACE(k)	(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_EMBED_FIND, EphyEmbedFindIface))
#define EPHY_IS_EMBED_FIND(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_EMBED_FIND))
#define EPHY_IS_EMBED_FIND_IFACE(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_EMBED_FIND))
#define EPHY_EMBED_FIND_GET_IFACE(inst)	(G_TYPE_INSTANCE_GET_INTERFACE ((inst), EPHY_TYPE_EMBED_FIND, EphyEmbedFindIface))

typedef struct _EphyEmbedFind		EphyEmbedFind;
typedef struct _EphyEmbedFindIface	EphyEmbedFindIface;

/* Keep these the same as in nsITypeAheadFind */
typedef enum
{
	EPHY_EMBED_FIND_FOUND		= 0,
	EPHY_EMBED_FIND_NOTFOUND	= 1,
	EPHY_EMBED_FIND_FOUNDWRAPPED	= 2
} EphyEmbedFindResult;

struct _EphyEmbedFindIface
{
	GTypeInterface base_iface;

	/* Methods */
	void	 (* set_embed)		(EphyEmbedFind *find,
					 EphyEmbed *embed);
	void	 (* set_properties)	(EphyEmbedFind *find,
					 const char *search_string,
					 gboolean case_sensitive);
	EphyEmbedFindResult (* find)		(EphyEmbedFind *find,
						 const char *search_string,
						 gboolean links_only);
	EphyEmbedFindResult (* find_again)	(EphyEmbedFind *find,
						 gboolean forward);
	void	 (* set_selection)	(EphyEmbedFind *find,
					 gboolean attention);
	gboolean (* activate_link)	(EphyEmbedFind *find,
					 GdkModifierType mask);
};

GType	 ephy_embed_find_get_type		(void);

void	 ephy_embed_find_set_embed		(EphyEmbedFind *find,
						 EphyEmbed *embed);

void	 ephy_embed_find_set_properties		(EphyEmbedFind *find,
						 const char *search_string,
						 gboolean case_sensitive);

EphyEmbedFindResult	ephy_embed_find_find		(EphyEmbedFind *find,
							 const char *search_string,
							 gboolean links_only);

EphyEmbedFindResult	ephy_embed_find_find_again	(EphyEmbedFind *find,
							 gboolean forward);

void	 ephy_embed_find_set_selection		(EphyEmbedFind *find,
						 gboolean attention);

gboolean ephy_embed_find_activate_link		(EphyEmbedFind *find,
						 GdkModifierType mask);

G_END_DECLS

#endif
