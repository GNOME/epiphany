/*
 *  Copyright (C) 2003 Marco Pesenti Gritti
 *  Copyright (C) 2003 Christian Persch
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

#include "ephy-extension.h"

GType
ephy_extension_get_type (void)
{
	static GType type = 0;

	if (type == 0)
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyExtensionClass),
			NULL,
			NULL,
		};
	
		type = g_type_register_static (G_TYPE_INTERFACE,
					       "EphyExtension",
					       &our_info,
					       G_TYPE_FLAG_ABSTRACT);
	}

	return type;
}

void
ephy_extension_attach_window (EphyExtension *extension,
			      EphyWindow *window)
{
	EphyExtensionClass *class = EPHY_EXTENSION_GET_CLASS (extension);
	class->attach_window (extension, window);
}

void
ephy_extension_detach_window (EphyExtension *extension,
			      EphyWindow *window)
{
	EphyExtensionClass *class = EPHY_EXTENSION_GET_CLASS (extension);
	class->detach_window (extension, window);
}
