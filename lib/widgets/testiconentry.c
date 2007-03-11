/*
 * Copyright © 2005 Christian Persch
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
 *  $Id$
 */

#include <glib.h>
#include <gtk/gtk.h>
#include <gtk/gtkstock.h>

#include "ephy-icon-entry.h"
#include "ephy-icon-entry.c"

int main(int argc, char **argv)
{
	GtkWidget *window, *vbox, *entry, *image;
	GtkTooltips *tips;

	gtk_init (&argc, &argv);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	vbox = gtk_vbox_new (0, FALSE);
	gtk_container_add (GTK_CONTAINER (window), vbox);
	gtk_container_set_border_width (GTK_CONTAINER (window), 12);

	entry = ephy_icon_entry_new ();
	gtk_box_pack_start (GTK_BOX (vbox), entry, TRUE, TRUE, 10);
	gtk_entry_set_text (GTK_ENTRY (EPHY_ICON_ENTRY (entry)->entry), "Icon Entry");

	image = gtk_image_new_from_stock (GTK_STOCK_NEW, GTK_ICON_SIZE_MENU);
	ephy_icon_entry_pack_widget (EPHY_ICON_ENTRY (entry), image, TRUE);
	image = gtk_image_new_from_stock (GTK_STOCK_CLEAR, GTK_ICON_SIZE_MENU);
	ephy_icon_entry_pack_widget (EPHY_ICON_ENTRY (entry), image, TRUE);
	image = gtk_image_new_from_stock (GTK_STOCK_QUIT, GTK_ICON_SIZE_MENU);
	ephy_icon_entry_pack_widget (EPHY_ICON_ENTRY (entry), image, TRUE);
	image = gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_MENU);
	ephy_icon_entry_pack_widget (EPHY_ICON_ENTRY (entry), image, FALSE);
	image = gtk_image_new_from_stock (GTK_STOCK_CDROM, GTK_ICON_SIZE_MENU);
	ephy_icon_entry_pack_widget (EPHY_ICON_ENTRY (entry), image, FALSE);

	entry = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX (vbox), entry, TRUE, TRUE, 10);
	gtk_entry_set_text (GTK_ENTRY (entry), "Normal entry");

	gtk_widget_show_all (window);

	g_signal_connect (window, "delete-event", G_CALLBACK (gtk_main_quit), NULL);

	gtk_main ();

	return 0;
}
