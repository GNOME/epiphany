/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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
 */

#include "config.h"
#include "ephy-link-action.h"

#include "ephy-debug.h"
#include "ephy-gui.h"
#include "ephy-link.h"
#include "ephy-window-action.h"

#include <gtk/gtk.h>

G_DEFINE_TYPE_WITH_CODE (EphyLinkAction, ephy_link_action, EPHY_TYPE_WINDOW_ACTION,
			 G_IMPLEMENT_INTERFACE (EPHY_TYPE_LINK,
						NULL))

#define EPHY_LINK_ACTION_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_LINK_ACTION, EphyLinkActionPrivate))

struct _EphyLinkActionPrivate
{
	guint button;
};

static gboolean
proxy_button_press_event_cb (GtkButton *button,
			     GdkEventButton *event,
			     EphyLinkAction *action)
{
	action->priv->button = event->button;

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

	widget = get_event_widget (proxy);
	if (widget)
	{
		g_signal_connect (widget, "button-press-event",
				  G_CALLBACK (proxy_button_press_event_cb),
				  action);
	}

	GTK_ACTION_CLASS (ephy_link_action_parent_class)->connect_proxy (action, proxy);
}

static void
ephy_link_action_disconnect_proxy (GtkAction *action, GtkWidget *proxy)
{
	GtkWidget *widget;

	LOG ("Disconnect link action proxy");

	widget = get_event_widget (proxy);
	if (widget)
	{
		g_signal_handlers_disconnect_by_func (widget,
						      G_CALLBACK (proxy_button_press_event_cb),
						      action);
	}

	GTK_ACTION_CLASS (ephy_link_action_parent_class)->disconnect_proxy (action, proxy);
}

static void
ephy_link_action_init (EphyLinkAction *action)
{
	action->priv = EPHY_LINK_ACTION_GET_PRIVATE (action);
}

static void
ephy_link_action_class_init (EphyLinkActionClass *class)
{
	GtkActionClass *action_class = GTK_ACTION_CLASS (class);

	action_class->connect_proxy = ephy_link_action_connect_proxy;
	action_class->disconnect_proxy = ephy_link_action_disconnect_proxy;

	g_type_class_add_private (G_OBJECT_CLASS (class), sizeof (EphyLinkActionPrivate));
}

/**
 * ephy_link_action_get_button:
 * @action: an #EphyLinkAction
 * 
 * This method stores the mouse button number that last activated, or
 * is activating, the @action. This is useful because #GtkButton's
 * cannot be clicked with a middle click by default, so inside
 * Epiphany we fake this by forwarding a left click (button 1) event
 * instead of a middle click (button 2) to the button. That makes the
 * EphyGUI methods like ephy_gui_is_middle_click not work here, so we
 * need to ask the @action directly about the button that activated
 * it.
 * 
 * Returns: the button number that last activated (or is activating) the @action
 **/
guint
ephy_link_action_get_button (EphyLinkAction *action)
{
	g_return_val_if_fail (EPHY_IS_LINK_ACTION (action), 0);

	return action->priv->button;
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
