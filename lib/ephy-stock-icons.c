/*
 *  Copyright (C) 2002 Jorn Baayen
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

#include <gtk/gtk.h>
#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "ephy-file-helpers.h"
#include "ephy-stock-icons.h"

void
ephy_stock_icons_init (void)
{
	GtkIconFactory *factory;
	int i;

	static const char *items[] =
	{
		EPHY_STOCK_SECURE,
		EPHY_STOCK_UNSECURE,
		EPHY_STOCK_HISTORY,
		EPHY_STOCK_BOOKMARKS,
		EPHY_STOCK_FULLSCREEN,
		EPHY_STOCK_NEW_TAB,
		EPHY_STOCK_VIEWSOURCE,
		EPHY_STOCK_SEND_LINK
	};

	factory = gtk_icon_factory_new ();
	gtk_icon_factory_add_default (factory);

	for (i = 0; i < (int) G_N_ELEMENTS (items); i++)
	{
		GtkIconSet *icon_set;
		GdkPixbuf *pixbuf;
		char *fn;

		fn = g_strconcat (items[i], ".png", NULL);
		pixbuf = gdk_pixbuf_new_from_file (ephy_file (fn), NULL);
		g_free (fn);

		icon_set = gtk_icon_set_new_from_pixbuf (pixbuf);
		gtk_icon_factory_add (factory, items[i], icon_set);
		gtk_icon_set_unref (icon_set);

		g_object_unref (G_OBJECT (pixbuf));
	}

	g_object_unref (G_OBJECT (factory));
}
