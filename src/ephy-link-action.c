/*
 *  Copyright (C) 2004 Christian Persch
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

#include "ephy-link-action.h"
#include "ephy-link.h"

GType
ephy_link_action_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyLinkActionClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			NULL, /* class_init */
			NULL,
			NULL, /* class_data */
			sizeof (EphyLinkAction),
			0,   /* n_preallocs */
			NULL /* instance_init */
		};
		static const GInterfaceInfo link_info = 
		{
			NULL,
			NULL,
			NULL
		};

		type = g_type_register_static (GTK_TYPE_ACTION,
					       "EphyLinkAction",
					       &our_info, G_TYPE_FLAG_ABSTRACT);
		g_type_add_interface_static (type,
					     EPHY_TYPE_LINK,
					     &link_info);
	}

	return type;
}
