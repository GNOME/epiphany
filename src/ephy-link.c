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

#include "ephy-link.h"

#include "ephy-type-builtins.h"
#include "ephy-marshal.h"
#include "ephy-signal-accumulator.h"
#include "ephy-gui.h"
#include "ephy-debug.h"

enum
{
	OPEN_LINK,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
ephy_link_base_init (gpointer g_class)
{
	static gboolean initialised = FALSE;

	if (!initialised)
	{
		signals[OPEN_LINK] = g_signal_new
			("open-link",
			 EPHY_TYPE_LINK,
			 G_SIGNAL_RUN_LAST,
			 G_STRUCT_OFFSET (EphyLinkIface, open_link),
			 ephy_signal_accumulator_object, ephy_tab_get_type,
			 ephy_marshal_OBJECT__STRING_OBJECT_FLAGS,
			 EPHY_TYPE_TAB,
			 3,
			 G_TYPE_STRING,
			 EPHY_TYPE_TAB,
			 EPHY_TYPE_LINK_FLAGS);

		initialised = TRUE;
	}
}

GType
ephy_link_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyLinkIface),
			ephy_link_base_init,
			NULL,
		};

		type = g_type_register_static (G_TYPE_INTERFACE,
					       "EphyLink",
					       &our_info, (GTypeFlags)0);
	}

	return type;
}

EphyTab	*
ephy_link_open (EphyLink *link,
		const char *address,
		EphyTab *tab,
		EphyLinkFlags flags)
{
	EphyTab *new_tab = NULL;

	LOG ("ephy_link_open address \"%s\" parent-tab %p flags %u", address, tab, flags);

	g_signal_emit (link, signals[OPEN_LINK], 0,
		       address, tab, flags,
		       &new_tab);

	return new_tab;
}

EphyLinkFlags
ephy_link_flags_from_current_event (void)
{
	GdkEventType type = GDK_NOTHING;
	guint state = 0, button = (guint) -1;
	EphyLinkFlags flags = 0;

	ephy_gui_get_current_event (&type, &state, &button);

	if (button == 2 && type == GDK_BUTTON_PRESS)
	{
		if (state == GDK_SHIFT_MASK)
		{
			flags = EPHY_LINK_NEW_WINDOW;
		}
		else
		{
			flags = EPHY_LINK_NEW_TAB;
		}
	}
	else
	{
		if (state == (GDK_CONTROL_MASK | GDK_SHIFT_MASK))
		{
			flags = EPHY_LINK_NEW_WINDOW;
		}
		else if (state == GDK_CONTROL_MASK)
		{
			flags = EPHY_LINK_NEW_TAB;
		}
	}

	return flags;
}
