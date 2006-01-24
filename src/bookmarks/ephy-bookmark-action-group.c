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
node_added_cb (EphyNode *parent,
	       EphyNode *child,
	       GtkActionGroup *actions)
{
	GtkAction *action;
	char *name, *accel;
		
	name = ephy_bookmark_action_name (child);
	action = ephy_bookmark_action_new (child, name);
	accel = g_strjoin ("/", "<Actions>",
			   gtk_action_group_get_name (actions),
			   name, NULL);
	gtk_action_set_accel_path (action, accel);
	gtk_action_group_add_action (actions, action);
	g_object_unref (action);
	g_free (accel);
	g_free (name);

	ephy_bookmark_action_updated (EPHY_BOOKMARK_ACTION (action));
	
	g_signal_connect_swapped (G_OBJECT(action), "open-link",
				  G_CALLBACK (ephy_link_open), actions);
}

static void
node_removed_cb (EphyNode *parent,
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
		gtk_action_group_remove_action (actions, action);
	}
	
	g_free (name);
}

GtkActionGroup *
ephy_bookmark_group_new (EphyNode *node)
{
	EphyBookmarks *bookmarks = ephy_shell_get_bookmarks (ephy_shell);
	EphyNode *smart = ephy_bookmarks_get_smart_bookmarks (bookmarks);
	
	GPtrArray *children = ephy_node_get_children (node);
	GObject *actions = (GObject *) ephy_link_action_group_new ("BA");
	guint i;
	
	for (i = 0; i < children->len; i++)
	{
		node_added_cb (node, g_ptr_array_index (children, i), (GtkActionGroup *) actions);
	}
	
	ephy_node_signal_connect_object (node, EPHY_NODE_CHILD_ADDED,
					 (EphyNodeCallback)node_added_cb,
					 actions);
	ephy_node_signal_connect_object (node, EPHY_NODE_CHILD_REMOVED,
					 (EphyNodeCallback)node_removed_cb,
					 actions);
	ephy_node_signal_connect_object (node, EPHY_NODE_CHILD_CHANGED,
					 (EphyNodeCallback)node_changed_cb,
					 actions);
	
	ephy_node_signal_connect_object (smart, EPHY_NODE_CHILD_ADDED,
					 (EphyNodeCallback)smart_added_cb,
					 actions);
	ephy_node_signal_connect_object (smart, EPHY_NODE_CHILD_REMOVED,
					 (EphyNodeCallback)smart_removed_cb,
					 actions);
	
	return GTK_ACTION_GROUP (actions);
}
