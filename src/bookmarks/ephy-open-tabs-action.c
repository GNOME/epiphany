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
 *  $Id$
 */

#include "config.h"

#include "ephy-open-tabs-action.h"

#include "ephy-bookmarks.h"
#include "ephy-bookmarks-ui.h"
#include "ephy-node-common.h"
#include "ephy-link-action.h"
#include "ephy-link.h"

#include <glib/gi18n.h>
#include <gtk/gtktoolitem.h>

#include <libgnomevfs/gnome-vfs-uri.h>

#include <string.h>

static void
activate_cb (GtkAction *action,
	    gpointer dummy)
{
	GObject *object = G_OBJECT (action);
	EphyLink *link;
	EphyNode *node;
	GPtrArray *children;
	EphyTab *tab = 0;
	const char *url;
	guint i;

	link = g_object_get_data (object, "ephy-link");
	node = g_object_get_data (object, "ephy-node");

	children = ephy_node_get_children (node);
	for (i = 0; i < children->len; ++i)
	{
		node = g_ptr_array_index (children, i);

		url = ephy_node_get_property_string (node, EPHY_NODE_BMK_PROP_LOCATION);
		tab = ephy_link_open (link, url, tab,
				      EPHY_LINK_NEW_TAB | ephy_link_flags_from_current_event ());
	}
}

static void
node_added_cb (EphyNode *parent,
	       EphyNode *child,
	       GtkActionGroup *action_group)
{
	GObject *action_object;
	GtkAction *action;
	char name[EPHY_TOPIC_ACTION_NAME_BUFFER_SIZE];
	char accel[256];

	EPHY_TOPIC_ACTION_NAME_PRINTF (name, child);
	
	/* FIXME !!!! */
	action = gtk_action_new (name, _("Open in New _Tabs"), "Open this topic in tabs", NULL);
	action_object = (GObject *) action;

	g_object_set_data (action_object, "ephy-node", child);
	g_object_set_data (action_object, "ephy-link", EPHY_LINK (action_group));
	
	g_signal_connect (action, "activate",
			  G_CALLBACK (activate_cb), NULL);

	g_snprintf (accel, sizeof (accel), "<Actions>/%s/%s",
		    gtk_action_group_get_name (action_group),
		    name);

	gtk_action_set_accel_path (action, accel);
	gtk_action_group_add_action (action_group, action);
	g_object_unref (action);
}

static void
node_removed_cb (EphyNode *parent,
		 EphyNode *child,
		 guint index,
		 GtkActionGroup *action_group)
{
	GtkAction *action;
	char name[EPHY_TOPIC_ACTION_NAME_BUFFER_SIZE];

	EPHY_TOPIC_ACTION_NAME_PRINTF (name, child);
		
	action = gtk_action_group_get_action (action_group, name);

	if (action != NULL)
	{
		gtk_action_group_remove_action (action_group, action);
	}
}

GtkActionGroup *
ephy_open_tabs_group_new (EphyNode *node)
{
	GPtrArray *children;
	GtkActionGroup *action_group;
	guint i;
	
	children = ephy_node_get_children (node);
	action_group = (GtkActionGroup *) ephy_link_action_group_new ("OpenTabsActions");

	for (i = 0; i < children->len; i++)
	{
		  node_added_cb (node, g_ptr_array_index (children, i),
				 action_group);
	}

	ephy_node_signal_connect_object (node, EPHY_NODE_CHILD_ADDED,
					 (EphyNodeCallback) node_added_cb,
					 (GObject *) action_group);
	ephy_node_signal_connect_object (node, EPHY_NODE_CHILD_REMOVED,
					 (EphyNodeCallback) node_removed_cb,
					 (GObject *) action_group);

	return action_group;
}
