/*
 *  Copyright (C) 2003 Marco Pesenti Gritti
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
 */


#include <gmodule.h>
#include <glib-object.h>

#include "ephy-shell.h"

static void
bmks_changed (EphyBookmarks *bookmarks)
{
	g_print ("Bookmarks changed !\n");
}

G_MODULE_EXPORT void
plugin_init (GTypeModule *module)
{
	EphyBookmarks *bookmarks;

	g_print ("plugin init\n");

	bookmarks = ephy_shell_get_bookmarks (ephy_shell);
	g_signal_connect (bookmarks, "tree_changed", G_CALLBACK (bmks_changed), NULL);
}

G_MODULE_EXPORT void
plugin_exit (void)
{
	g_print ("plugin exit\n");
}
