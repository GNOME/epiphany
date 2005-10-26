/*
 *  Copyright (C) 2004 Christian Persch
 *  Copyright (C) 2005 Philip Langdale
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

#include "ephy-link-action.h"
#include "ephy-link.h"

#include "ephy-debug.h"
#include "ephy-gui.h"

#include <gtk/gtkbutton.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkmenutoolbutton.h>

static GObjectClass *parent_class = NULL;

static gboolean
proxy_button_press_event_cb (GtkButton *button,
			       GdkEventButton *event,
			       EphyLinkAction *action)
{
	if (event->button == 2)
	{
		gtk_button_pressed(button);
	}

	return FALSE;
}

static gboolean
proxy_button_release_event_cb (GtkButton *button,
			       GdkEventButton *event,
			       EphyLinkAction *action)
{
	/**
	 * We do not use ephy_gui_is_middle_click() here because
	 * that also catches ctrl + left_click which already
	 * triggers an activate event for all proxies.
	 */
	if (event->button == 2)
	{
		gtk_button_released(button);
	}

	return FALSE;
}

static GtkWidget *
get_event_widget (GtkWidget *proxy)
{
	GtkWidget *widget;

	/**
	 * Finding the interesting widget requires internal knowledge of
	 * the widgets in question. This can't be helped, but by keeping
	 * the sneaky code in one place, it can easily be updated.
	 */
	if (GTK_IS_MENU_ITEM (proxy))
	{
		/* Menu items already forward middle clicks */
		widget = NULL;
	}
	else if (GTK_IS_MENU_TOOL_BUTTON (proxy))
	{
		/**
		 * The menu tool button's button is the first child
		 * of the child hbox.
		 */
		GtkContainer *container =
			GTK_CONTAINER (gtk_bin_get_child (GTK_BIN (proxy)));
		widget = GTK_WIDGET (gtk_container_get_children (container)->data);
	}
	else if (GTK_IS_TOOL_BUTTON (proxy))
	{
		/* The tool button's button is the direct child */
		widget = gtk_bin_get_child (GTK_BIN (proxy));
	}
	else if (GTK_IS_BUTTON (proxy))
	{
		widget = proxy;
	}
	else
	{
		/* Don't touch anything we don't know about */
		widget = NULL;
	}

	return widget;
}

static void
ephy_link_action_connect_proxy (GtkAction *action, GtkWidget *proxy)
{
	GtkWidget *widget;

	LOG ("Connect link action proxy");

	widget = get_event_widget(proxy);
	if (widget)
	{
		g_signal_connect (widget, "button-press-event",
				  G_CALLBACK (proxy_button_press_event_cb),
				  action);
		g_signal_connect (widget, "button-release-event",
				  G_CALLBACK (proxy_button_release_event_cb),
				  action);
	}

	GTK_ACTION_CLASS (parent_class)->connect_proxy (action, proxy);
}

static void
ephy_link_action_disconnect_proxy (GtkAction *action, GtkWidget *proxy)
{
	GtkWidget *widget;

	LOG ("Disconnect link action proxy");

	widget = get_event_widget(proxy);
	if (widget)
	{
		g_signal_handlers_disconnect_by_func (widget,
						      G_CALLBACK (proxy_button_press_event_cb),
						      action);
		g_signal_handlers_disconnect_by_func (widget,
						      G_CALLBACK (proxy_button_release_event_cb),
						      action);
	}

	GTK_ACTION_CLASS (parent_class)->disconnect_proxy (action, proxy);
}

static void
ephy_link_action_class_init (EphyLinkActionClass *class)
{
	GtkActionClass *action_class = GTK_ACTION_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	action_class->connect_proxy = ephy_link_action_connect_proxy;
	action_class->disconnect_proxy = ephy_link_action_disconnect_proxy;
}

GType
ephy_link_action_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyLinkActionClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) ephy_link_action_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (EphyLinkAction),
			0,   /* n_preallocs */
			NULL /* instance_init */
		};
		static const GInterfaceInfo link_info = 
		{
			NULL,
			NULL,
			NULL
		};

		type = g_type_register_static (GTK_TYPE_ACTION,
					       "EphyLinkAction",
					       &our_info, G_TYPE_FLAG_ABSTRACT);
		g_type_add_interface_static (type,
					     EPHY_TYPE_LINK,
					     &link_info);
	}

	return type;
}

GType
ephy_link_action_group_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyLinkActionGroupClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			NULL, /* class_init */
			NULL,
			NULL, /* class_data */
			sizeof (EphyLinkActionGroup),
			0,   /* n_preallocs */
			NULL /* instance_init */
		};
		static const GInterfaceInfo link_info = 
		{
			NULL,
			NULL,
			NULL
		};

		type = g_type_register_static (GTK_TYPE_ACTION_GROUP,
					       "EphyLinkActionGroup",
					       &our_info, 0);
		g_type_add_interface_static (type,
					     EPHY_TYPE_LINK,
					     &link_info);
	}

	return type;
}

EphyLinkActionGroup *
ephy_link_action_group_new (char * name)
{
	return EPHY_LINK_ACTION_GROUP (g_object_new (EPHY_TYPE_LINK_ACTION_GROUP,
						     "name", "BookmarkActions", NULL));
}
