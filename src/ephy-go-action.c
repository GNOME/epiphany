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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ephy-go-action.h"
#include "ephy-debug.h"

#include <bonobo/bonobo-i18n.h>
#include <gtk/gtktoolitem.h>
#include <gtk/gtkbutton.h>

#define MENU_ID "ephy-go-action-menu-id"

static void ephy_go_action_init       (EphyGoAction *action);
static void ephy_go_action_class_init (EphyGoActionClass *class);
static void ephy_go_action_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

GType
ephy_go_action_get_type (void)
{
	static GType type = 0;

	if (!type)
	{
		static const GTypeInfo type_info =
		{
			sizeof (EphyGoActionClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) ephy_go_action_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,
			sizeof (EphyGoAction),
			0, /* n_preallocs */
			(GInstanceInitFunc) ephy_go_action_init,
		};

		type = g_type_register_static (GTK_TYPE_ACTION,
					       "EphyGoAction",
					       &type_info, 0);
	}
	return type;
}

static void
activate_cb (GtkWidget *widget, GtkAction *action)
{
	g_signal_emit_by_name (action, "activate");
}

static GtkWidget *
create_tool_item (GtkAction *action)
{
	GtkWidget *button;
	GtkWidget *item;

	item = GTK_WIDGET (gtk_tool_item_new ());

	button = gtk_button_new_with_label (_("Go"));
	gtk_button_set_relief(GTK_BUTTON (button), GTK_RELIEF_NONE);
	
	g_signal_connect (G_OBJECT (button), "clicked",
                          G_CALLBACK (activate_cb), action);

	gtk_container_add (GTK_CONTAINER (item), button);
	gtk_widget_show (button);

	return item;
}

static GtkWidget *
create_menu_item (GtkAction *action)
{
	GtkWidget *menu_item;

	menu_item = gtk_menu_item_new_with_label (_("Go"));
	g_signal_connect (G_OBJECT (menu_item), "activate",
			  G_CALLBACK (activate_cb), action);

	return menu_item;
}

static gboolean
create_menu_proxy_cb (GtkToolItem *item, GtkAction *action)
{
	GtkWidget *menu_item;

	menu_item = GTK_ACTION_GET_CLASS (action)->create_menu_item (action);

	GTK_ACTION_GET_CLASS (action)->connect_proxy (action, menu_item);

	gtk_tool_item_set_proxy_menu_item (item, MENU_ID, menu_item);

	return TRUE;
}

static void
connect_proxy (GtkAction *action, GtkWidget *proxy)
{	
	(* GTK_ACTION_CLASS (parent_class)->connect_proxy) (action, proxy);

	g_return_if_fail (EPHY_IS_GO_ACTION (action));

	if (GTK_IS_TOOL_ITEM (proxy))
	{
		g_signal_connect (proxy, "create_menu_proxy",
				  G_CALLBACK (create_menu_proxy_cb), action);
	}
}

static void
ephy_go_action_class_init (EphyGoActionClass *class)
{
	GtkActionClass *action_class;
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->finalize = ephy_go_action_finalize;

	parent_class = g_type_class_peek_parent (class);
	action_class = GTK_ACTION_CLASS (class);

	action_class->create_tool_item = create_tool_item;
	action_class->menu_item_type = GTK_TYPE_MENU_ITEM;
	action_class->create_menu_item = create_menu_item;
	action_class->connect_proxy = connect_proxy;
}

static void
ephy_go_action_init (EphyGoAction *action)
{
}

static void
ephy_go_action_finalize (GObject *object)
{
	g_return_if_fail (EPHY_IS_GO_ACTION (object));

	LOG ("Go action finalized")

	G_OBJECT_CLASS (parent_class)->finalize (object);
}
