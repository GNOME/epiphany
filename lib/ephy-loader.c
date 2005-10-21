/*
 *  Copyright (C) 2003 Marco Pesenti Gritti
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

#include "config.h"

#include "ephy-loader.h"

GType
ephy_loader_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyLoaderIface),
			NULL,
			NULL,
		};
	
		type = g_type_register_static (G_TYPE_INTERFACE,
					       "EphyLoader",
					       &our_info, 0);
	}

	return type;
}

const char *
ephy_loader_type (const EphyLoader *loader)
{
	EphyLoaderIface *iface = EPHY_LOADER_GET_IFACE (loader);
	return iface->type;
}

GObject *
ephy_loader_get_object (EphyLoader *loader,
			GKeyFile *keyfile)
{
	EphyLoaderIface *iface = EPHY_LOADER_GET_IFACE (loader);
	return iface->get_object (loader, keyfile);
}

void
ephy_loader_release_object (EphyLoader *loader,
			     GObject *object)
{
	EphyLoaderIface *iface = EPHY_LOADER_GET_IFACE (loader);
	iface->release_object (loader, object);
}
