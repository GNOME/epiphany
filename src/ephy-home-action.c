/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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
*  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*/

#include "config.h"
#include "ephy-home-action.h"

#include "ephy-link.h"

G_DEFINE_TYPE (EphyHomeAction, ephy_home_action, EPHY_TYPE_LINK_ACTION)

static void
ephy_home_action_open (GtkAction *action, 
		       const char *address, 
		       EphyLinkFlags flags)
{
	ephy_link_open (EPHY_LINK (action),
			address != NULL && address[0] != '\0' ? address : "about:blank",
			NULL,
			flags);
}

static void
action_name_association (GtkAction *action,
			 char *action_name,
			 char *address)
{
	EphyLinkFlags flags = EPHY_LINK_HOME_PAGE;

	if (g_str_equal (action_name, "FileNewTab"))
		flags |= EPHY_LINK_NEW_TAB | EPHY_LINK_JUMP_TO;

	ephy_home_action_open (action, address, flags);
}	

static void
ephy_home_action_activate (GtkAction *action)
{
	char *action_name;

	g_object_get (G_OBJECT (action), "name", &action_name, NULL);
		
	action_name_association (action, action_name, "about:overview");

	g_free (action_name);
}

static void
ephy_home_action_class_init (EphyHomeActionClass *class)
{
	GtkActionClass *action_class = GTK_ACTION_CLASS (class);
	
	action_class->activate = ephy_home_action_activate;
}

static void
ephy_home_action_init (EphyHomeAction *action)
{
        /* Empty, needed for G_DEFINE_TYPE macro */
}
