/*
 *  Copyright (C) 2005 Peter Harvey
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

#include "ephy-shell.h"
#include "ephy-bookmark-action-group.h"
#include "ephy-bookmark-action.h"
#include "ephy-bookmarks.h"
#include "ephy-link.h"
#include "ephy-node.h"
#include "ephy-node-common.h"
#include "ephy-debug.h"

#include <gtk/gtkaction.h>
#include <gtk/gtkactiongroup.h>
#include <string.h>

static void
smart_added_cb (EphyNode *parent, 
		EphyNode *child,
		GtkActionGroup *actions)
{
	GtkAction *action;
	char *name;
	
	name = ephy_bookmark_action_name (child);
	g_return_if_fail (name);
	action = gtk_action_group_get_action (actions, name);
	
	if (action)
	{
		ephy_bookmark_action_updated
		  (EPHY_BOOKMARK_ACTION (action));
	}
	
	g_free (name);
}


static void
smart_removed_cb (EphyNode *parent,
		  EphyNode *child,
		  guint index, 
		  GtkActionGroup *actions)
{
	GtkAction *action;
	char *name;
	
	name = ephy_bookmark_action_name (child);
	g_return_if_fail (name);
	action = gtk_action_group_get_action (actions, name);
	
	if (action)
	{
		ephy_bookmark_action_updated
		  (EPHY_BOOKMARK_ACTION (action));
	}
	
	g_free (name);
}

static void
node_changed_cb (EphyNode *parent,
		 EphyNode *child,
		 guint property_id,
		 GtkActionGroup *actions)
{
	GtkAction *action;
	char *name;
	
	name = ephy_bookmark_action_name (child);
	g_assert (name != NULL);

	action = gtk_action_group_get_action (actions, name);
	
	if (action)
	{
		ephy_bookmark_action_updated
			(EPHY_BOOKMARK_ACTION (action));
	}
	
	g_free (name);
}

static void
node_added_cb (EphyNode *parent,
	       EphyNode *child,
	       GtkActionGroup *action_group)
{
	GtkAction *action;
	char *name, *accel;
		
	name = ephy_bookmark_action_name (child);
	action = ephy_bookmark_action_new (child, name);
	accel = g_strjoin ("/", "<Actions>",
			   gtk_action_group_get_name (action_group),
			   name, NULL);
	gtk_action_set_accel_path (action, accel);
	gtk_action_group_add_action (action_group, action);
	g_object_unref (action);

	g_free (accel);
	g_free (name);

	ephy_bookmark_action_updated (EPHY_BOOKMARK_ACTION (action));
	
	g_signal_connect_swapped (action, "open-link",
				  G_CALLBACK (ephy_link_open), action_group);
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
	
	if (action)
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
	
	bookmarks = ephy_shell_get_bookmarks (ephy_shell);
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
