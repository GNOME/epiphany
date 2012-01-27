/*
 *  Copyright © 2002 Jorn Baayen
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

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "ephy-stock-icons.h"

void
ephy_stock_icons_init (void)
{
	GtkIconFactory *factory;
	GtkIconSet *icon_set;
	GtkIconSource *icon_source;
	int i;

	static const GtkStockItem items[] =
	{
		{ EPHY_STOCK_HISTORY,	N_("History"),		0, 0, NULL },
		{ EPHY_STOCK_BOOKMARKS, N_("Bookmarks"),	0, 0, NULL }
	};

	factory = gtk_icon_factory_new ();

	for (i = 0; i < (int) G_N_ELEMENTS (items); i++)
	{
		icon_source = gtk_icon_source_new ();
		gtk_icon_source_set_icon_name (icon_source, items[i].stock_id);

		icon_set = gtk_icon_set_new ();
		gtk_icon_set_add_source (icon_set, icon_source);
		gtk_icon_source_free (icon_source);

		gtk_icon_factory_add (factory, items[i].stock_id, icon_set);
		gtk_icon_set_unref (icon_set);
	}

	gtk_stock_add_static (items, G_N_ELEMENTS (items));

	gtk_icon_factory_add_default (factory);
	g_object_unref (factory);

	/* GtkIconTheme will then look in Ephy custom hicolor dir
	 * for icons as well as the standard search paths
	 */
	/* FIXME: multi-head! */
	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
					   SHARE_DIR G_DIR_SEPARATOR_S "icons");
}
