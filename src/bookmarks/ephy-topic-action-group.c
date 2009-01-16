/*
 *  Copyright © 2005 Peter Harvey
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

#include "ephy-topic-action-group.h"
#include "ephy-topic-action.h"
#include "ephy-node.h"
#include "ephy-node-common.h"
#include "ephy-bookmarks.h"
#include "ephy-bookmarks-ui.h"
#include "ephy-debug.h"

#include <gtk/gtk.h>

static void
node_changed_cb (EphyNode *parent, 
		 EphyNode *child, 
		 guint property_id, 
		 GtkActionGroup *action_group)
{
	GtkAction *action;
	char name[EPHY_TOPIC_ACTION_NAME_BUFFER_SIZE];

	EPHY_TOPIC_ACTION_NAME_PRINTF (name, child);

	action = gtk_action_group_get_action (action_group, name);
	
	if (property_id == EPHY_NODE_KEYWORD_PROP_NAME)
	{
		ephy_topic_action_updated (EPHY_TOPIC_ACTION (action));
	}
}

static void
node_added_cb (EphyNode *parent,
	       EphyNode *child,
	       GtkActionGroup *action_group)
{
	GtkUIManager *manager;
	GtkAction *action;
	char name[EPHY_TOPIC_ACTION_NAME_BUFFER_SIZE];
	char accel[256];

	EPHY_TOPIC_ACTION_NAME_PRINTF (name, child);

	manager = g_object_get_data ((GObject *) action_group, "ui-manager");

	action = ephy_topic_action_new (child, manager, name);

	g_snprintf (accel, sizeof (accel), "<Actions>/%s/%s",
		    gtk_action_group_get_name (action_group),
		    name);
	gtk_action_set_accel_path (action, accel);

	gtk_action_group_add_action (action_group, action);
	g_object_unref (action);

	ephy_topic_action_updated ((EphyTopicAction *) action);
}

static void
node_removed_cb (EphyNode *parent,
		 EphyNode *child, guint index,
		 GtkActionGroup *action_group)
{
	GtkAction *action;
	char name[EPHY_TOPIC_ACTION_NAME_BUFFER_SIZE];

	EPHY_TOPIC_ACTION_NAME_PRINTF (name, child);
	
	action = gtk_action_group_get_action (action_group, name);
	
	if (action)
	{
		gtk_action_group_remove_action (action_group, action);
	}
}

GtkActionGroup *
ephy_topic_action_group_new (EphyNode *node,
			     GtkUIManager *manager)
{
	GPtrArray *children;
	GtkActionGroup *action_group;
	int i;
	
	children = ephy_node_get_children (node);
	action_group = gtk_action_group_new ("TpAc");

	g_object_set_data ((GObject *) action_group, "ui-manager", manager);
	
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
	ephy_node_signal_connect_object (node, EPHY_NODE_CHILD_CHANGED,
					 (EphyNodeCallback) node_changed_cb,
					 (GObject *) action_group);
	
	return (GtkActionGroup *) action_group;
}
