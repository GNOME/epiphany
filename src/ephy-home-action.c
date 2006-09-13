/*
*  Copyright © 2004 Christian Persch
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

#include "ephy-home-action.h"
#include "ephy-link.h"
#include "ephy-prefs.h"
#include "ephy-gui.h"
#include "eel-gconf-extensions.h"

static void
ephy_home_action_activate (GtkAction *action)
{
	char *address;

	address = eel_gconf_get_string (CONF_GENERAL_HOMEPAGE);

	ephy_link_open (EPHY_LINK (action),
			address != NULL && address[0] != '\0' ? address : "about:blank",
			NULL,
			ephy_link_flags_from_current_event ());

	g_free (address);
}

static void
ephy_home_action_class_init (EphyHomeActionClass *class)
{
	GtkActionClass *action_class = GTK_ACTION_CLASS (class);

	action_class->activate = ephy_home_action_activate;
}

GType
ephy_home_action_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		const GTypeInfo type_info =
		{
			sizeof (EphyHomeActionClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) ephy_home_action_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,
			sizeof (EphyHomeAction),
			0, /* n_preallocs */
			(GInstanceInitFunc) NULL,
		};

		type = g_type_register_static (EPHY_TYPE_LINK_ACTION,
					       "EphyHomeAction",
					       &type_info, 0);
	}

	return type;
}
