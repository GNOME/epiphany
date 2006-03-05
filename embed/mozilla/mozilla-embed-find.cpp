/*
 *  Copyright (C) 2000-2004 Marco Pesenti Gritti
 *  Copyright (C) 2003, 2004 Christian Persch
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

#include "mozilla-config.h"

#include "config.h"

#include "EphyFind.h"

#include "mozilla-embed-find.h"
#include "ephy-embed-find.h"
#include "ephy-embed-shell.h"
#include "ephy-debug.h"

#define MOZILLA_EMBED_FIND_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), MOZILLA_TYPE_EMBED_FIND, MozillaEmbedFindPrivate))

struct _MozillaEmbedFindPrivate
{
	EphyFind *find;
};

static GObjectClass *parent_class = NULL;

static void
impl_set_embed (EphyEmbedFind *efind,
		EphyEmbed *embed)
{
	MozillaEmbedFind *find = MOZILLA_EMBED_FIND (efind);
	MozillaEmbedFindPrivate *priv = find->priv;

	priv->find->SetEmbed (embed);
}

static void
impl_set_properties (EphyEmbedFind *efind,
		     const char *find_string,
		     gboolean case_sensitive)
{
	MozillaEmbedFind *find = MOZILLA_EMBED_FIND (efind);
	MozillaEmbedFindPrivate *priv = find->priv;

	priv->find->SetFindProperties (find_string, case_sensitive);
}

static EphyEmbedFindResult
impl_find (EphyEmbedFind *efind,
	     const char *find_string,
	     gboolean links_only)
{
	MozillaEmbedFind *find = MOZILLA_EMBED_FIND (efind);
	MozillaEmbedFindPrivate *priv = find->priv;

	return priv->find->Find (find_string, links_only);
}

static EphyEmbedFindResult
impl_find_again (EphyEmbedFind *efind,
		   gboolean forward)
{
	MozillaEmbedFind *find = MOZILLA_EMBED_FIND (efind);
	MozillaEmbedFindPrivate *priv = find->priv;

	return priv->find->FindAgain (forward);
}

static void
impl_set_selection (EphyEmbedFind *efind,
		    gboolean attention)
{
	MozillaEmbedFind *find = MOZILLA_EMBED_FIND (efind);
	MozillaEmbedFindPrivate *priv = find->priv;

	priv->find->SetSelectionAttention (attention);
}

static gboolean
impl_activate_link (EphyEmbedFind *efind,
		    GdkModifierType mask)
{
	MozillaEmbedFind *find = MOZILLA_EMBED_FIND (efind);
	MozillaEmbedFindPrivate *priv = find->priv;

	return priv->find->ActivateLink (mask);
}

static void
ephy_find_iface_init (EphyEmbedFindIface *iface)
{
	iface->set_embed = impl_set_embed;
	iface->set_properties = impl_set_properties;
	iface->find = impl_find;
	iface->find_again = impl_find_again;
	iface->set_selection = impl_set_selection;
	iface->activate_link = impl_activate_link;
}

static void
mozilla_embed_find_init (MozillaEmbedFind *find)
{
	find->priv = MOZILLA_EMBED_FIND_GET_PRIVATE (find);
	find->priv->find = new EphyFind ();
}

static GObject *
mozilla_embed_find_constructor (GType type, guint n_construct_properties,
				GObjectConstructParam *construct_params)
{
	g_object_ref (embed_shell);

	/* we depend on single because of mozilla initialization */
	ephy_embed_shell_get_embed_single (embed_shell);

	return parent_class->constructor (type, n_construct_properties,
					  construct_params);
}

static void
mozilla_embed_find_finalize (GObject *object)
{
	MozillaEmbedFind *find = MOZILLA_EMBED_FIND (object);

	delete find->priv->find;

	parent_class->finalize (object);

	g_object_unref (embed_shell);
}

static void
mozilla_embed_find_class_init (MozillaEmbedFindClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = (GObjectClass *) g_type_class_peek_parent (klass);

	object_class->constructor = mozilla_embed_find_constructor;
	object_class->finalize = mozilla_embed_find_finalize;

	g_type_class_add_private (object_class, sizeof (MozillaEmbedFindPrivate));
}

GType 
mozilla_embed_find_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GTypeInfo our_info =
		{
			sizeof (MozillaEmbedFindClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) mozilla_embed_find_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (MozillaEmbedFind),
			0, /* n_preallocs */
			(GInstanceInitFunc) mozilla_embed_find_init
		};

		static const GInterfaceInfo find_info =
		{
			(GInterfaceInitFunc) ephy_find_iface_init,
			NULL,
			NULL
		};
	
		type = g_type_register_static (G_TYPE_OBJECT,
					       "MozillaEmbedFind",
					       &our_info, 
					       (GTypeFlags)0);
		g_type_add_interface_static (type,
					     EPHY_TYPE_EMBED_FIND,
					     &find_info);
	}

	return type;
}
