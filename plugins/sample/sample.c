/*
 *  Copyright (C) 2003 Marco Pesenti Gritti
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


#include <gmodule.h>
#include <glib-object.h>

#include "epiphany/ephy-shell.h"
#include "epiphany/session.h"

static void
bmk_added (EphyNode *node, EphyNode *child)
{
	g_print ("Bookmark added\n");
}

static void
bmk_removed (EphyNode *node, EphyNode *child)
{
	g_print ("Bookmark removed\n");
}

static void
bmk_changed (EphyNode *node, EphyNode *child)
{
	g_print ("Bookmark changed\n");
}

static void
switch_page_cb (GtkWidget *widget)
{
	GtkWidget *toplevel;
	EphyTab *tab;

	toplevel = gtk_widget_get_toplevel (widget);

	tab = ephy_window_get_active_tab (EPHY_WINDOW (toplevel));
	g_print ("New active tab is %p\n", tab);
}

static void
window_focus_in_cb (GtkWidget *widget, GdkEventFocus *event)
{
	EphyTab *tab;

	tab = ephy_window_get_active_tab (EPHY_WINDOW (widget));
	g_print ("New active tab is %p\n", tab);
}

static void
location_changed_cb (EphyEmbed *embed, char *location)
{
	g_print ("New location %s\n", location);
}

static void
tab_added_cb (GtkWidget *nb, GtkWidget *child)
{
	g_signal_connect (child, "ge_location",
			  G_CALLBACK (location_changed_cb), NULL);
}

static void
new_window_cb (Session *session, EphyWindow *window)
{
	GtkWidget *nb;

	nb = ephy_window_get_notebook (window);

	g_signal_connect (window, "focus_in_event",
			  G_CALLBACK (window_focus_in_cb), NULL);
	g_signal_connect (nb, "switch_page",
			  G_CALLBACK (switch_page_cb), NULL);
	g_signal_connect (nb, "tab_added",
			  G_CALLBACK (tab_added_cb), NULL);
}

G_MODULE_EXPORT void
plugin_init (GTypeModule *module)
{
	EphyBookmarks *bookmarks;
	Session *session;
	EphyNode *bmks;

	g_print ("plugin init\n");

	bookmarks = ephy_shell_get_bookmarks (ephy_shell);
	bmks = ephy_bookmarks_get_bookmarks (bookmarks);
	ephy_node_signal_connect_object (bmks, EPHY_NODE_CHILD_ADDED,
				         (EphyNodeCallback) bmk_added, NULL);
	ephy_node_signal_connect_object (bmks, EPHY_NODE_CHILD_CHANGED,
				         (EphyNodeCallback) bmk_changed, NULL);
	ephy_node_signal_connect_object (bmks, EPHY_NODE_CHILD_REMOVED,
				         (EphyNodeCallback) bmk_removed, NULL);

	session = SESSION (ephy_shell_get_session (ephy_shell));
	g_signal_connect (session, "new_window",
			  G_CALLBACK (new_window_cb), NULL);
}

G_MODULE_EXPORT void
plugin_exit (void)
{
	g_print ("plugin exit\n");
}
