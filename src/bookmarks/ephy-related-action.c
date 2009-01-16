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

#include "ephy-related-action.h"

#include "ephy-window.h"
#include "ephy-bookmarks.h"
#include "ephy-shell.h"
#include "ephy-node-common.h"
#include "ephy-stock-icons.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

static void
node_changed (EphyNode *node,
	      guint propertyid,
	      EphyTopicAction *action)
{
	ephy_topic_action_updated (action);
}

static void
node_destroyed (EphyNode *node,
		EphyTopicAction *action)
{
	EphyBookmarks *eb;
	EphyNode *favorites;

	eb = ephy_shell_get_bookmarks (ephy_shell_get_default ());
	favorites = ephy_bookmarks_get_favorites (eb);

	ephy_topic_action_set_topic (action, favorites);
}

static EphyEmbed *
open_link (EphyLink *link,
	   const char *address,
	   EphyEmbed *embed,
	   EphyLinkFlags flags)
{
	EphyBookmarks *eb;
	EphyNode *bookmark;
	EphyNode *topic, *chosen = NULL;
	GPtrArray *topics;
	gint i, tmp, best = 0;
	
	eb = ephy_shell_get_bookmarks (ephy_shell_get_default ());
	bookmark = ephy_bookmarks_find_bookmark (eb, address);
	if (bookmark == NULL) return NULL;

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
			if (chosen == NULL ||
			    (tmp >= 10 && tmp <= best))
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

	return NULL;
}

static void
iface_init (EphyLinkIface *iface)
{
	iface->open_link = open_link;
}

static void
ephy_related_action_class_init (EphyRelatedActionClass *klass)
{
        /* Empty, needed for G_DEFINE_TYPE macro */
}

static void
ephy_related_action_init (EphyRelatedAction *action)
{
        /* Empty, needed for G_DEFINE_TYPE macro */
}

G_DEFINE_TYPE_WITH_CODE (EphyRelatedAction, ephy_related_action, EPHY_TYPE_TOPIC_ACTION,
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_LINK,
                                                iface_init))

GtkAction *
ephy_related_action_new (EphyLink *link,
			 GtkUIManager *manager,
			 char * name)
{
	EphyBookmarks *eb;
	EphyNode *favorites;
	EphyRelatedAction *action;
 
	eb = ephy_shell_get_bookmarks (ephy_shell);
	favorites = ephy_bookmarks_get_favorites (eb);

	action = (EphyRelatedAction *) (g_object_new (EPHY_TYPE_RELATED_ACTION,
						      "name", name,
						      "topic", favorites,
						      "short_label", _("Related"),
						      "stock-id", GTK_STOCK_INDEX,
						      "manager", manager,
						      NULL));
	
	g_signal_connect_object (link, "open-link",
				 G_CALLBACK (ephy_link_open), action,
				 G_CONNECT_SWAPPED);
	
	return (GtkAction *) action;
}
