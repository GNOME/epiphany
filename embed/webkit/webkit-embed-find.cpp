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

#include "config.h"

#include "ephy-debug.h"
#include "ephy-embed-find.h"
#include "ephy-embed-shell.h"

#include "webkit-embed-find.h"

#define WEBKIT_EMBED_FIND_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), WEBKIT_TYPE_EMBED_FIND, WebKitEmbedFindPrivate))

struct _WebKitEmbedFindPrivate
{

};

static void
impl_set_embed (EphyEmbedFind *efind,
		EphyEmbed *embed)
{
}

static void
impl_set_properties (EphyEmbedFind *efind,
		     const char *find_string,
		     gboolean case_sensitive)
{
}

static EphyEmbedFindResult
impl_find (EphyEmbedFind *efind,
	     const char *find_string,
	     gboolean links_only)
{
  return EPHY_EMBED_FIND_FOUND;
}

static EphyEmbedFindResult
impl_find_again (EphyEmbedFind *efind,
		 gboolean forward,
		 gboolean links_only)
{
  return EPHY_EMBED_FIND_FOUND;
}

static void
impl_set_selection (EphyEmbedFind *efind,
		    gboolean attention)
{
}

static gboolean
impl_activate_link (EphyEmbedFind *efind,
		    GdkModifierType mask)
{
  return FALSE;
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
webkit_embed_find_init (WebKitEmbedFind *find)
{
}

static void
webkit_embed_find_class_init (WebKitEmbedFindClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (object_class, sizeof (WebKitEmbedFindPrivate));
}

G_DEFINE_TYPE_WITH_CODE (WebKitEmbedFind, webkit_embed_find, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_EMBED_FIND,
                                                ephy_find_iface_init))
