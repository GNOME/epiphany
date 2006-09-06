/*
 *  Copyright (C) 2003 Marco Pesenti Gritti
 *  Copyright (C) 2003 David Bordoley
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

#include "ephy-go-action.h"
#include "ephy-debug.h"

#include <glib/gi18n.h>
#include <gtk/gtktoolitem.h>
#include <gtk/gtkbutton.h>

static void ephy_go_action_class_init (EphyGoActionClass *class);

static GObjectClass *parent_class = NULL;

GType
ephy_go_action_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		const GTypeInfo type_info =
		{
			sizeof (EphyGoActionClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) ephy_go_action_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,
			sizeof (EphyGoAction),
			0, /* n_preallocs */
			(GInstanceInitFunc) NULL,
		};

		type = g_type_register_static (EPHY_TYPE_LINK_ACTION,
					       "EphyGoAction",
					       &type_info, 0);
	}

	return type;
}

static GtkWidget *
create_tool_item (GtkAction *action)
{
	GtkWidget *button;
	GtkWidget *item;

	item = GTK_WIDGET (gtk_tool_item_new ());

	button = gtk_button_new_with_label (_("Go"));
	gtk_button_set_relief(GTK_BUTTON (button), GTK_RELIEF_NONE);
	gtk_button_set_focus_on_click (GTK_BUTTON (button), FALSE);

	gtk_container_add (GTK_CONTAINER (item), button);
	gtk_widget_show (button);

	return item;
}

static void
connect_proxy (GtkAction *action,
	       GtkWidget *proxy)
{      
	GTK_ACTION_CLASS (parent_class)->connect_proxy (action, proxy);

	if (GTK_IS_TOOL_ITEM (proxy))
	{
		g_signal_connect_object (GTK_BIN (proxy)->child, "clicked",
					 G_CALLBACK (gtk_action_activate), action,
					 G_CONNECT_SWAPPED);
	}
}

static void
disconnect_proxy (GtkAction *action,
		  GtkWidget *proxy)
{
	g_signal_handlers_disconnect_by_func
		(proxy, G_CALLBACK (gtk_action_activate), action);

	GTK_ACTION_CLASS (parent_class)->disconnect_proxy (action, proxy);
}

static void
ephy_go_action_class_init (EphyGoActionClass *class)
{
	GtkActionClass *action_class = GTK_ACTION_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	action_class->create_tool_item = create_tool_item;
	action_class->connect_proxy = connect_proxy;
	action_class->disconnect_proxy = disconnect_proxy;
}
