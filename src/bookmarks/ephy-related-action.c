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
 */

#include "config.h"

#include <gtk/gtk.h>
#include <gtk/gtkaction.h>
#include <gtk/gtkactiongroup.h>
#include <glib/gi18n.h>

#include "ephy-window.h"
#include "ephy-bookmarks.h"
#include "ephy-shell.h"
#include "ephy-node-common.h"
#include "ephy-stock-icons.h"
#include "ephy-related-action.h"

static void
node_changed (EphyNode *node, guint propertyid, GObject *object)
{
	ephy_topic_action_updated (EPHY_TOPIC_ACTION (object));
}

static void
node_destroyed (EphyNode *node, GObject *object)
{
	EphyBookmarks *eb = ephy_shell_get_bookmarks (ephy_shell);
	
	ephy_topic_action_set_topic (EPHY_TOPIC_ACTION (object),
				     ephy_bookmarks_get_favorites (eb));
}

static EphyTab *
open_link (EphyLink *link,
	   const char *address,
	   EphyTab *tab,
	   EphyLinkFlags flags)
{
	EphyBookmarks *eb = ephy_shell_get_bookmarks (ephy_shell);
	EphyNode *bookmark = ephy_bookmarks_find_bookmark (eb, address);
	EphyNode *topic, *chosen = NULL;
	
	if (bookmark != NULL)
	{
		GPtrArray *topics;
		gint i, tmp, best = 0;
		
		topic = ephy_topic_action_get_topic (EPHY_TOPIC_ACTION (link));
		tmp = ephy_node_get_property_int (topic, EPHY_NODE_KEYWORD_PROP_PRIORITY);                
		if (tmp == EPHY_NODE_NORMAL_PRIORITY &&
		    ephy_node_has_child (topic, bookmark))
		{
			return NULL;
		}                
		ephy_node_signal_disconnect_object (topic, EPHY_NODE_CHANGED,
						    (EphyNodeCallback) node_changed,
						    G_OBJECT (link));
		ephy_node_signal_disconnect_object (topic, EPHY_NODE_DESTROY,
						    (EphyNodeCallback) node_destroyed,
						    G_OBJECT (link));
		
		topics = ephy_node_get_children (ephy_bookmarks_get_keywords (eb));                
		for (i = 0; i < topics->len; i++)
		{
			topic = g_ptr_array_index (topics, i);
			tmp = ephy_node_get_property_int (topic, EPHY_NODE_KEYWORD_PROP_PRIORITY);
			if (tmp == EPHY_NODE_NORMAL_PRIORITY &&
			    ephy_node_has_child (topic, bookmark))
			{
				tmp = ephy_node_get_n_children (topic);
				if (chosen == NULL || (tmp >= 10 && tmp <= best))
				{
					chosen = topic;
					best = tmp;
				}
			}
		}
		
		if (chosen == NULL) chosen = ephy_bookmarks_get_favorites (eb);
		
		ephy_topic_action_set_topic (EPHY_TOPIC_ACTION (link), chosen);
		ephy_node_signal_connect_object (chosen, EPHY_NODE_CHANGED,
						 (EphyNodeCallback) node_changed,
						 G_OBJECT (link));
		ephy_node_signal_connect_object (chosen, EPHY_NODE_DESTROY,
						 (EphyNodeCallback) node_destroyed,
						 G_OBJECT (link));
	}

	return NULL;
}

static void
iface_init (EphyLinkIface *iface)
{
	iface->open_link = open_link;
}

GType
ephy_related_action_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyRelatedActionClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			NULL, /* class_init */
			NULL,
			NULL, /* class_data */
			sizeof (EphyRelatedAction),
			0,   /* n_preallocs */
			NULL /* instance_init */
		};
		static const GInterfaceInfo link_info = 
		{
			(GInterfaceInitFunc) iface_init,
			NULL,
			NULL
		};

		type = g_type_register_static (EPHY_TYPE_TOPIC_ACTION,
					       "EphyRelatedAction",
					       &our_info, 0);
		g_type_add_interface_static (type,
					     EPHY_TYPE_LINK,
					     &link_info);
	}

	return type;
}

GtkAction *
ephy_related_action_new (EphyLink *link, GtkUIManager *manager, char * name)
{
	EphyBookmarks *eb = ephy_shell_get_bookmarks (ephy_shell);
	EphyNode *favorites = ephy_bookmarks_get_favorites (eb);

	EphyRelatedAction *action = 
	  EPHY_RELATED_ACTION (g_object_new (EPHY_TYPE_RELATED_ACTION,
					     "name", name,
					     "topic", favorites,
					     "short_label", _("Related"),
					     "stock-id", GTK_STOCK_INDEX,
					     "manager", manager,
					     NULL));
	
	g_signal_connect_object (G_OBJECT (link), "open-link",
				 G_CALLBACK (ephy_link_open), action,
				 G_CONNECT_SWAPPED);
	
	return GTK_ACTION (action);
}

