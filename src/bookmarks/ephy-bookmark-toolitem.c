/*
 *  Copyright (C) 2003  Christian Persch
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ephy-bookmark-toolitem.h"
#include "ephy-marshal.h"
#include "ephy-debug.h"

#include <gtk/gtk.h>
#include <gtk/gtkimagemenuitem.h>

static GObjectClass *parent_class = NULL;

static void	ephy_bookmark_toolitem_class_init	(EphyBookmarkToolitemClass *klass);
static void	ephy_bookmark_toolitem_init		(EphyBookmarkToolitem *item);

#define MENU_ID "ephy-bookmark-toolitem-menu-id"

/**
 * EphyBookmarkToolitem object
 */

GType
ephy_bookmark_toolitem_get_type (void)
{
        static GType ephy_bookmark_toolitem_type = 0;

        if (ephy_bookmark_toolitem_type == 0)
        {
                static const GTypeInfo our_info =
			{
				sizeof (EphyBookmarkToolitemClass),
				NULL, /* base_init */
				NULL, /* base_finalize */
				(GClassInitFunc) ephy_bookmark_toolitem_class_init,
				NULL,
				NULL, /* class_data */
				sizeof (EphyBookmarkToolitem),
				0, /* n_preallocs */
				(GInstanceInitFunc) ephy_bookmark_toolitem_init,
			};

                ephy_bookmark_toolitem_type = g_type_register_static (EGG_TYPE_TOOL_ITEM,
								      "EphyBookmarkToolitem",
								       &our_info, 0);
        }

        return ephy_bookmark_toolitem_type;
}

static gboolean
ephy_bookmark_toolitem_create_menu_proxy (EggToolItem *item)
{
	GtkWidget *menu_item, *label, *icon, *image;
	GdkPixbuf *pixbuf;
	const char *text;

	LOG ("create menu proxy for %p", item)
	
	icon = g_object_get_data (G_OBJECT (item), "icon");
	pixbuf = gtk_image_get_pixbuf (GTK_IMAGE (icon));
	image = gtk_image_new_from_pixbuf (pixbuf);

	label = g_object_get_data (G_OBJECT (item), "label");
	text = gtk_label_get_label (GTK_LABEL (label));

	menu_item = gtk_image_menu_item_new_with_mnemonic (text);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item), image);

	egg_tool_item_set_proxy_menu_item (item, MENU_ID, menu_item);

	return TRUE;
}

static void
ephy_bookmark_toolitem_init (EphyBookmarkToolitem *item)
{
	GtkWidget *button, *hbox, *label, *icon, *entry;

	LOG ("Initialising bookmark toolitem %p", item)

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox);
	gtk_container_add (GTK_CONTAINER (item), hbox);

	button = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
	gtk_widget_show (button);
	gtk_container_add (GTK_CONTAINER (hbox), button);
	g_object_set_data (G_OBJECT (item), "button", button);

	entry = gtk_entry_new ();
	gtk_widget_set_size_request (entry, 120, -1);
	gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
	g_object_set_data (G_OBJECT (item), "entry", entry);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox);
	gtk_container_add (GTK_CONTAINER (button), hbox);

	icon = gtk_image_new ();
	gtk_widget_show (icon);
	gtk_box_pack_start (GTK_BOX (hbox), icon, TRUE, TRUE, 0);
	g_object_set_data (G_OBJECT (item), "icon", icon);

	label = gtk_label_new (NULL);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
	g_object_set_data (G_OBJECT (item), "label", label);
}

static void
ephy_bookmark_toolitem_class_init (EphyBookmarkToolitemClass *klass)
{
	EggToolItemClass *tool_item_class;

	parent_class = g_type_class_peek_parent (klass);

	tool_item_class = (EggToolItemClass *)klass;

	tool_item_class->create_menu_proxy = ephy_bookmark_toolitem_create_menu_proxy;
}
