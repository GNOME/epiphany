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

#include "ephy-shell.h"
#include "ephy-bookmark-action-group.h"
#include "ephy-bookmark-action.h"
#include "ephy-bookmarks.h"
#include "ephy-bookmarks-ui.h"
#include "ephy-link.h"
#include "ephy-node.h"
#include "ephy-node-common.h"
#include "ephy-debug.h"

#include <gtk/gtk.h>
#include <string.h>

static void
smart_added_cb (EphyNode *parent, 
		EphyNode *child,
		GtkActionGroup *action_group)
{
	GtkAction *action;
	char name[EPHY_BOOKMARK_ACTION_NAME_BUFFER_SIZE];

	EPHY_BOOKMARK_ACTION_NAME_PRINTF (name, child);

	action = gtk_action_group_get_action (action_group, name);
	
	if (action != NULL)
	{
		ephy_bookmark_action_updated ((EphyBookmarkAction *) action);
	}
}

static void
smart_removed_cb (EphyNode *parent,
		  EphyNode *child,
		  guint index, 
		  GtkActionGroup *action_group)
{
	GtkAction *action;
	char name[EPHY_BOOKMARK_ACTION_NAME_BUFFER_SIZE];

	EPHY_BOOKMARK_ACTION_NAME_PRINTF (name, child);

	action = gtk_action_group_get_action (action_group, name);
	
	if (action != NULL)
	{
		ephy_bookmark_action_updated ((EphyBookmarkAction *) action);
	}
}

static void
node_changed_cb (EphyNode *parent,
		 EphyNode *child,
		 guint property_id,
		 GtkActionGroup *action_group)
{
	GtkAction *action;
	char name[EPHY_BOOKMARK_ACTION_NAME_BUFFER_SIZE];

	EPHY_BOOKMARK_ACTION_NAME_PRINTF (name, child);

	action = gtk_action_group_get_action (action_group, name);
	
	if (action != NULL)
	{
		ephy_bookmark_action_updated ((EphyBookmarkAction *) action);
	}
}

static void
node_added_cb (EphyNode *parent,
	       EphyNode *child,
	       GtkActionGroup *action_group)
{
	GtkAction *action;
	char name[EPHY_BOOKMARK_ACTION_NAME_BUFFER_SIZE];
	char accel[256];

	EPHY_BOOKMARK_ACTION_NAME_PRINTF (name, child);

	action = ephy_bookmark_action_new (child, name);

	g_signal_connect_swapped (action, "open-link",
				  G_CALLBACK (ephy_link_open), action_group);

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
	char name[EPHY_BOOKMARK_ACTION_NAME_BUFFER_SIZE];

	EPHY_BOOKMARK_ACTION_NAME_PRINTF (name, child);

	action = gtk_action_group_get_action (action_group, name);
	
	if (action != NULL)
	{
		gtk_action_group_remove_action (action_group, action);
	}
}

GtkActionGroup *
ephy_bookmark_group_new (EphyNode *node)
{
	EphyBookmarks *bookmarks;
	EphyNode *smart;
	GPtrArray *children;
	GtkActionGroup *action_group;
	guint i;
	
	bookmarks = ephy_shell_get_bookmarks (ephy_shell_get_default ());
	smart = ephy_bookmarks_get_smart_bookmarks (bookmarks);

	action_group = (GtkActionGroup *) ephy_link_action_group_new ("BA");

	children = ephy_node_get_children (node);
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
	
	ephy_node_signal_connect_object (smart, EPHY_NODE_CHILD_ADDED,
					 (EphyNodeCallback) smart_added_cb,
					 (GObject *) action_group);
	ephy_node_signal_connect_object (smart, EPHY_NODE_CHILD_REMOVED,
					 (EphyNodeCallback) smart_removed_cb,
					 (GObject *) action_group);
	
	return action_group;
}
