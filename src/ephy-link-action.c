/*
 *  Copyright © 2004 Christian Persch
 *  Copyright © 2005 Philip Langdale
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

#include "config.h"

#include "ephy-link-action.h"
#include "ephy-link.h"

#include "ephy-debug.h"
#include "ephy-gui.h"

#include <gtk/gtk.h>

G_DEFINE_TYPE_WITH_CODE (EphyLinkAction, ephy_link_action, GTK_TYPE_ACTION,
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_LINK,
                                                NULL))

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
	/*
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

	/*
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
		/*
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

	GTK_ACTION_CLASS (ephy_link_action_parent_class)->connect_proxy (action, proxy);
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

	GTK_ACTION_CLASS (ephy_link_action_parent_class)->disconnect_proxy (action, proxy);
}

static void
ephy_link_action_init (EphyLinkAction *action)
{
        /* Empty, needed for G_DEFINE_TYPE macro */
}

static void
ephy_link_action_class_init (EphyLinkActionClass *class)
{
	GtkActionClass *action_class = GTK_ACTION_CLASS (class);

	action_class->connect_proxy = ephy_link_action_connect_proxy;
	action_class->disconnect_proxy = ephy_link_action_disconnect_proxy;
}

static void
ephy_link_action_group_class_init (EphyLinkActionGroupClass *klass)
{
        /* Empty, needed for G_DEFINE_TYPE macro */
}

static void
ephy_link_action_group_init (EphyLinkActionGroup *action_group)
{
        /* Empty, needed for G_DEFINE_TYPE macro */
}

G_DEFINE_TYPE_WITH_CODE (EphyLinkActionGroup, ephy_link_action_group, GTK_TYPE_ACTION_GROUP,
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_LINK,
                                                NULL))

EphyLinkActionGroup *
ephy_link_action_group_new (const char * name)
{
	return g_object_new (EPHY_TYPE_LINK_ACTION_GROUP,
			     "name", name,
			     NULL);
}
