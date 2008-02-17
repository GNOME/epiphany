/*
 *  Copyright © 2000-2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
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

#include "mozilla-config.h"
#include "config.h"

#include "ephy-debug.h"
#include "ephy-embed-find.h"
#include "ephy-embed-shell.h"

#include "EphyFind.h"

#include "mozilla-embed-find.h"

static void mozilla_embed_find_class_init (MozillaEmbedFindClass *klass);
static void mozilla_embed_find_init       (MozillaEmbedFind *self);
static void ephy_find_iface_init          (EphyEmbedFindIface *iface);

#define MOZILLA_EMBED_FIND_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), MOZILLA_TYPE_EMBED_FIND, MozillaEmbedFindPrivate))

struct _MozillaEmbedFindPrivate
{
	EphyFind *find;
};

static void
impl_set_embed (EphyEmbedFind *efind,
		EphyEmbed *embed)
{
	MozillaEmbedFind *find = MOZILLA_EMBED_FIND (efind);
	MozillaEmbedFindPrivate *priv = find->priv;

	priv->find->SetEmbed (embed);
}

G_DEFINE_TYPE_WITH_CODE (MozillaEmbedFind, mozilla_embed_find, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_EMBED_FIND,
                                                ephy_find_iface_init))

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
		 gboolean forward,
		 gboolean links_only)
{
	MozillaEmbedFind *find = MOZILLA_EMBED_FIND (efind);
	MozillaEmbedFindPrivate *priv = find->priv;

	return priv->find->FindAgain (forward, links_only);
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

	return G_OBJECT_CLASS (mozilla_embed_find_parent_class)->constructor (type,
                                                                              n_construct_properties,
                                                                              construct_params);
}

static void
mozilla_embed_find_finalize (GObject *object)
{
	MozillaEmbedFind *find = MOZILLA_EMBED_FIND (object);

	delete find->priv->find;

	G_OBJECT_CLASS (mozilla_embed_find_parent_class)->finalize (object);

	g_object_unref (embed_shell);
}

static void
mozilla_embed_find_class_init (MozillaEmbedFindClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->constructor = mozilla_embed_find_constructor;
	object_class->finalize = mozilla_embed_find_finalize;

	g_type_class_add_private (object_class, sizeof (MozillaEmbedFindPrivate));
}

