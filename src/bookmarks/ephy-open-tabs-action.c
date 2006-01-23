/*
 *  Copyright (C) 2004 Peter Harvey <pah06@uow.edu.au>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtktoolitem.h>
#include <glib/gi18n.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <string.h>

#include "ephy-open-tabs-action.h"
#include "ephy-bookmarks.h"
#include "ephy-node-common.h"
#include "ephy-link-action.h"
#include "ephy-link.h"

static void
activate_cb (GtkAction *action, gpointer dummy)
{
	EphyLink *link = g_object_get_data (G_OBJECT (action), "ephy-link");
	EphyNode *node = g_object_get_data (G_OBJECT (action), "ephy-node");
	GPtrArray *children = ephy_node_get_children (node);
	EphyTab *tab = 0;

	gint i;
	const char *url;

	for (i = 0; i < children->len; i++)
	{
		node = g_ptr_array_index (children, i);
		url = ephy_node_get_property_string (node, EPHY_NODE_BMK_PROP_LOCATION);
		tab = ephy_link_open (link, url, tab, EPHY_LINK_NEW_TAB);
	}
}

static void
node_added_cb (EphyNode *parent, EphyNode *child, GObject *object)
{
	GtkActionGroup *actions = GTK_ACTION_GROUP (object);
	GtkAction *action;
	char *name, *accel;
	
	name = ephy_open_tabs_action_name (child);
	g_return_if_fail (name);

	action = gtk_action_new (name, _("Open in New _Tabs"), "Open this topic in tabs", NULL);
	
	g_object_set_data (G_OBJECT (action), "ephy-node", child);
	g_object_set_data (G_OBJECT (action), "ephy-link", EPHY_LINK (object));
	
	g_signal_connect (G_OBJECT (action), "activate",
			  G_CALLBACK (activate_cb), NULL);
	
	accel = g_strjoin ("/", "<Actions>",
			   gtk_action_group_get_name (actions),
			   name,
			   NULL);
	gtk_action_set_accel_path (action, accel);
	gtk_action_group_add_action (actions, action);
	g_object_unref (action);
	g_free (accel);
	g_free (name);
}

static void
node_removed_cb (EphyNode *parent, EphyNode *child, guint index, GObject *object)
{
	char *name = ephy_open_tabs_action_name (child);
	if (name)
	{
		GtkActionGroup *actions = GTK_ACTION_GROUP (object);
		GtkAction *action = gtk_action_group_get_action (actions, name);
		if (action) gtk_action_group_remove_action (actions, action);
		g_free (name);
	}
}

GtkActionGroup *
ephy_open_tabs_group_new (EphyNode *node)
{
	GPtrArray *children = ephy_node_get_children (node);
	GObject *actions = G_OBJECT (ephy_link_action_group_new ("OpenTabsActions"));
	gint i;
	
	for (i = 0; i < children->len; i++)
	  node_added_cb (node, g_ptr_array_index (children, i), actions);
	
	ephy_node_signal_connect_object (node, EPHY_NODE_CHILD_ADDED,
					 (EphyNodeCallback)node_added_cb,
					 actions);
	ephy_node_signal_connect_object (node, EPHY_NODE_CHILD_REMOVED,
					 (EphyNodeCallback)node_removed_cb,
					 actions);

	return GTK_ACTION_GROUP (actions);
}

char *
ephy_open_tabs_action_name (EphyNode *node)
{
	return g_strdup_printf("OpenTabs%u", ephy_node_get_id (node));
}
