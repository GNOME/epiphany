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
	GtkIconSet *icon_set;
	GtkIconSource *icon_source;
	int i;

	static const char *icon_theme_items[] =
	{
		STOCK_NEW_TAB,
		STOCK_FULLSCREEN,
		STOCK_VIEW_SOURCE,
		STOCK_SEND_MAIL,
		STOCK_ADD_BOOKMARK,
		STOCK_PRINT_SETUP
	};

	static const char *items[] =
	{
		EPHY_STOCK_SECURE,
		EPHY_STOCK_UNSECURE,
		EPHY_STOCK_HISTORY,
		EPHY_STOCK_BOOKMARKS,
		EPHY_STOCK_ENTRY,
		EPHY_STOCK_DOWNLOAD
	};

	factory = gtk_icon_factory_new ();
	gtk_icon_factory_add_default (factory);

	for (i = 0; i < (int) G_N_ELEMENTS (items); i++)
	{
		char *fn;

		icon_set = gtk_icon_set_new ();
		icon_source = gtk_icon_source_new ();

		fn = g_strconcat (items[i], ".png", NULL);
		gtk_icon_source_set_filename (icon_source, ephy_file (fn));
		g_free (fn);

		gtk_icon_set_add_source (icon_set, icon_source);
		gtk_icon_factory_add (factory, items[i], icon_set);
		gtk_icon_set_unref (icon_set);
		gtk_icon_source_free (icon_source);
	}

	for (i = 0; i < (int) G_N_ELEMENTS (icon_theme_items); i++)
	{
		icon_set = gtk_icon_set_new ();
		icon_source = gtk_icon_source_new ();
		gtk_icon_source_set_icon_name (icon_source, icon_theme_items[i]);
		gtk_icon_set_add_source (icon_set, icon_source);
		gtk_icon_factory_add (factory, icon_theme_items[i], icon_set);
		gtk_icon_set_unref (icon_set);
		gtk_icon_source_free (icon_source);
	}

	g_object_unref (G_OBJECT (factory));
}
