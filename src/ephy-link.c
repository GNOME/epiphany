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

#include "ephy-link.h"

#include "ephy-embed-utils.h"
#include "ephy-type-builtins.h"
#include "ephy-signal-accumulator.h"
#include "ephy-gui.h"
#include "ephy-debug.h"

enum
{
	OPEN_LINK,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
ephy_link_base_init (gpointer g_class)
{
	static gboolean initialised = FALSE;

	if (!initialised)
	{
		/**
		 * EphyLink::open-link:
		 * @address: the address of @link
		 * @embed: #EphyEmbed associated with @link
		 * @flags: flags for @link
		 *
		 * The ::open-link signal is emitted when @link is requested to
		 * open it's associated @address.
		 *
		 * Returns: (transfer none): the #EphyEmbed where @address has
		 * been handled.
		 **/
		signals[OPEN_LINK] = g_signal_new
			("open-link",
			 EPHY_TYPE_LINK,
			 G_SIGNAL_RUN_LAST,
			 G_STRUCT_OFFSET (EphyLinkIface, open_link),
			 ephy_signal_accumulator_object, ephy_embed_get_type,
			 g_cclosure_marshal_generic,
			 GTK_TYPE_WIDGET /* Can't use an interface type here */,
			 3,
			 G_TYPE_STRING,
			 GTK_TYPE_WIDGET /* Can't use an interface type here */,
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
		const GTypeInfo our_info =
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

/**
 * ephy_link_open:
 * @link: an #EphyLink object
 * @address: the address of @link
 * @embed: #EphyEmbed associated with @link
 * @flags: flags for @link
 *
 * Triggers @link open action.
 *
 * Returns: (transfer none): the #EphyEmbed where @link opened.
 */
EphyEmbed *
ephy_link_open (EphyLink *link,
		const char *address,
		EphyEmbed *embed,
		EphyLinkFlags flags)
{
	EphyEmbed *new_embed = NULL;

	LOG ("ephy_link_open address \"%s\" parent-embed %p flags %u", address, embed, flags);

	g_signal_emit (link, signals[OPEN_LINK], 0,
		       address, embed, flags,
		       &new_embed);

	return new_embed;
}

EphyLinkFlags
ephy_link_flags_from_current_event (void)
{
	GdkEventType type = GDK_NOTHING;
	guint state = 0, button = (guint) -1;
	EphyLinkFlags flags = 0;

	ephy_gui_get_current_event (&type, &state, &button);

	if (button == 2 && (type == GDK_BUTTON_PRESS || type == GDK_BUTTON_RELEASE))
	{
		if (state == GDK_SHIFT_MASK)
		{
			flags = EPHY_LINK_NEW_WINDOW;
		}
		else if (state == 0 || state == GDK_CONTROL_MASK)
		{
			flags = EPHY_LINK_NEW_TAB | EPHY_LINK_NEW_TAB_APPEND_AFTER;
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
			flags = EPHY_LINK_NEW_TAB | EPHY_LINK_NEW_TAB_APPEND_AFTER;
		}
	}

	return flags;
}
