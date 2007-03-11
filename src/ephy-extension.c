/*
 *  Copyright © 2003 Marco Pesenti Gritti
 *  Copyright © 2003 Christian Persch
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

#include "ephy-extension.h"

GType
ephy_extension_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		const GTypeInfo our_info =
		{
			sizeof (EphyExtensionIface),
			NULL,
			NULL,
		};
	
		type = g_type_register_static (G_TYPE_INTERFACE,
					       "EphyExtension",
					       &our_info, 0);
	}

	return type;
}

void
ephy_extension_attach_window (EphyExtension *extension,
			      EphyWindow *window)
{
	EphyExtensionIface *iface = EPHY_EXTENSION_GET_IFACE (extension);
	if (iface->attach_window)
	{
		iface->attach_window (extension, window);
	}
}

void
ephy_extension_detach_window (EphyExtension *extension,
			      EphyWindow *window)
{
	EphyExtensionIface *iface = EPHY_EXTENSION_GET_IFACE (extension);
	if (iface->detach_window)
	{
		iface->detach_window (extension, window);
	}
}

void
ephy_extension_attach_tab (EphyExtension *extension,
			   EphyWindow *window,
			   EphyTab *tab)
{
	EphyExtensionIface *iface = EPHY_EXTENSION_GET_IFACE (extension);
	if (iface->attach_tab)
	{
		iface->attach_tab (extension, window, tab);
	}
}

void
ephy_extension_detach_tab (EphyExtension *extension,
			   EphyWindow *window,
			   EphyTab *tab)
{
	EphyExtensionIface *iface = EPHY_EXTENSION_GET_IFACE (extension);
	if (iface->detach_tab)
	{
		iface->detach_tab (extension, window, tab);
	}
}
