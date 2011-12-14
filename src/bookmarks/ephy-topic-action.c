/*
 *  Copyright © 2003, 2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
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

#include "ephy-topic-action.h"
#include "ephy-node.h"
#include "ephy-node-common.h"
#include "ephy-nodes-cover.h"
#include "ephy-bookmarks.h"
#include "ephy-bookmarks-ui.h"
#include "ephy-bookmarks-menu.h"
#include "ephy-shell.h"
#include "ephy-gui.h"
#include "ephy-debug.h"
#include "ephy-dnd.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

#define EPHY_TOPIC_ACTION_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_TOPIC_ACTION, EphyTopicActionPrivate))

struct _EphyTopicActionPrivate
{
	EphyNode *node;
	GtkUIManager *manager;
	guint merge_id;
};

enum
{
	PROP_0,
	PROP_TOPIC,
	PROP_MANAGER
};

G_DEFINE_TYPE (EphyTopicAction, ephy_topic_action, GTK_TYPE_ACTION)

static void
ephy_topic_action_sync_label (GtkAction *action,
			      GParamSpec *pspec,
			      GtkWidget *proxy)
{
	GtkWidget *label = NULL;
	GValue value = { 0, };
	const char *label_text;

	g_value_init (&value, G_TYPE_STRING);
	g_object_get_property (G_OBJECT (action), "label", &value);

	label_text = g_value_get_string (&value);

	if (GTK_IS_MENU_ITEM (proxy))
	{
		label = gtk_bin_get_child (GTK_BIN (proxy));
	}
	else
	{
		g_warning ("Unknown widget");
		return;
	}

	g_return_if_fail (label != NULL);

	if (label_text)
	{
		gtk_label_set_label (GTK_LABEL (label), label_text);
	}
	
	g_value_unset (&value);
}

static GtkWidget *
get_popup (EphyTopicAction *action)
{
	EphyTopicActionPrivate *priv = action->priv;
	char path[40];

	g_snprintf (path, sizeof (path), "/PopupTopic%ld",
		    (long int) ephy_node_get_id (action->priv->node));

	if (priv->merge_id == 0)
	{
		GString *popup_menu_string;

		popup_menu_string = g_string_new (NULL);
		g_string_append_printf (popup_menu_string, "<ui><popup name=\"%s\">", path + 1);

		ephy_bookmarks_menu_build (popup_menu_string, priv->node);
		g_string_append (popup_menu_string, "</popup></ui>");

		priv->merge_id = gtk_ui_manager_add_ui_from_string
			(priv->manager, popup_menu_string->str,
			 popup_menu_string->len, 0);

		g_string_free (popup_menu_string, TRUE);
	}

	return gtk_ui_manager_get_widget (priv->manager, path);
}

static void
erase_popup (EphyTopicAction *action)
{
	EphyTopicActionPrivate *priv = action->priv;

	if (priv->merge_id != 0)
	{
		gtk_ui_manager_remove_ui (priv->manager, priv->merge_id);
		priv->merge_id = 0;
	}
}

static void
child_added_cb (EphyNode *node, EphyNode *child, GObject *object)
{
	EphyTopicAction *action = EPHY_TOPIC_ACTION (object);
	erase_popup (action);
}

static void
child_changed_cb (EphyNode *node,
		  EphyNode *child,
		  guint property,
		  GObject *object)
{
	EphyTopicAction *action = EPHY_TOPIC_ACTION (object);

	erase_popup (action);
}

static void
child_removed_cb (EphyNode *node,
		  EphyNode *child,
		  guint index,
		  GObject *object)
{
	EphyTopicAction *action = EPHY_TOPIC_ACTION (object);

	erase_popup (action);
}

static void
menu_destroy_cb (GtkWidget *menuitem,
		 gpointer user_data)
{
	/* Save the submenu from similar destruction,
	 * because it doesn't rightly belong to this menuitem. */
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), NULL);
}

static void
menu_init_cb (GtkWidget *menuitem,
	      EphyTopicAction *action)
{
	if (gtk_menu_item_get_submenu (GTK_MENU_ITEM (menuitem)) == NULL)
	{
		GtkWidget *popup;

		popup = get_popup (action);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), popup);
		g_signal_connect (menuitem, "destroy",
				  G_CALLBACK (menu_destroy_cb), NULL);
	}
}

static void
connect_proxy (GtkAction *action,
	       GtkWidget *proxy)
{
	GTK_ACTION_CLASS (ephy_topic_action_parent_class)->connect_proxy (action, proxy);
    
	ephy_topic_action_sync_label (action, NULL, proxy);
	g_signal_connect_object (action, "notify::label",
				 G_CALLBACK (ephy_topic_action_sync_label), proxy, 0);

	if (GTK_IS_MENU_ITEM (proxy))
	{
		g_signal_connect (proxy, "map",
				  G_CALLBACK (menu_init_cb), action);
	}
}

void
ephy_topic_action_updated (EphyTopicAction *action)
{
	EphyTopicActionPrivate *priv = action->priv;
	GValue value = { 0, };
	const char *title;
	int priority;
	
	g_return_if_fail (priv->node != NULL);
	
	priority = ephy_node_get_property_int 
		(priv->node, EPHY_NODE_KEYWORD_PROP_PRIORITY);
	
	if (priority == EPHY_NODE_ALL_PRIORITY)
	{
		title = _("Bookmarks");
	}
	else
	{
		title = ephy_node_get_property_string
			(priv->node, EPHY_NODE_KEYWORD_PROP_NAME);
	}
	
	g_value_init(&value, G_TYPE_STRING);
	g_value_set_static_string (&value, title);
	g_object_set_property (G_OBJECT (action), "label", &value);
	g_object_set_property (G_OBJECT (action), "tooltip", &value);
	g_value_unset (&value);
}

EphyNode *
ephy_topic_action_get_topic (EphyTopicAction *action)
{
	EphyTopicActionPrivate *priv = action->priv;

	return priv->node;
}

void
ephy_topic_action_set_topic (EphyTopicAction *action,
			     EphyNode *node)
{
	EphyTopicActionPrivate *priv = action->priv;
	GObject *object = G_OBJECT (action);

	g_return_if_fail (node != NULL);
	
	if (priv->node == node) return;

	if (priv->node != NULL)
	{
		ephy_node_signal_disconnect_object
			(priv->node, EPHY_NODE_CHILD_ADDED,
			 (EphyNodeCallback) child_added_cb, object);
		ephy_node_signal_disconnect_object
			(priv->node, EPHY_NODE_CHILD_CHANGED,
			 (EphyNodeCallback)child_changed_cb, object);
		ephy_node_signal_disconnect_object
			(priv->node, EPHY_NODE_CHILD_REMOVED,
			 (EphyNodeCallback)child_removed_cb, object);
	}

	ephy_node_signal_connect_object (node, EPHY_NODE_CHILD_ADDED,
					 (EphyNodeCallback) child_added_cb,
					 object);
	ephy_node_signal_connect_object (node, EPHY_NODE_CHILD_CHANGED,
					 (EphyNodeCallback) child_changed_cb,
					 object);
	ephy_node_signal_connect_object (node, EPHY_NODE_CHILD_REMOVED,
					 (EphyNodeCallback) child_removed_cb,
					 object);

	priv->node = node;
	
	erase_popup (action);
	
	g_object_freeze_notify (object);
	g_object_notify (object, "topic");
	ephy_topic_action_updated (action);
	g_object_thaw_notify (object);
}

static void
ephy_topic_action_set_property (GObject *object,
				guint prop_id,
				const GValue *value,
				GParamSpec *pspec)
{
	EphyTopicAction *action = EPHY_TOPIC_ACTION (object);
	EphyTopicActionPrivate *priv = action->priv;

	switch (prop_id)
	{
		case PROP_TOPIC:
			ephy_topic_action_set_topic (action, g_value_get_pointer (value));
			break;
		case PROP_MANAGER:
			priv->manager = g_value_get_object (value);
			break;
	}
}

static void
ephy_topic_action_get_property (GObject *object,
				guint prop_id,
				GValue *value,
				GParamSpec *pspec)
{
	EphyTopicAction *action = EPHY_TOPIC_ACTION (object);
	EphyTopicActionPrivate *priv = action->priv;

	switch (prop_id)
	{
		case PROP_TOPIC:
			g_value_set_pointer (value, priv->node);
			break;
		case PROP_MANAGER:
			g_value_set_object (value, priv->manager);
			break;
	}
}

static void
ephy_topic_action_init (EphyTopicAction *action)
{
	action->priv = EPHY_TOPIC_ACTION_GET_PRIVATE (action);
}

static void
ephy_topic_action_class_init (EphyTopicActionClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkActionClass *action_class = GTK_ACTION_CLASS (class);

	action_class->connect_proxy = connect_proxy;

	object_class->set_property = ephy_topic_action_set_property;
	object_class->get_property = ephy_topic_action_get_property;

	g_object_class_install_property (object_class,
					 PROP_TOPIC,
					 g_param_spec_pointer ("topic",
							       "Topic",
							       "Topic",
							       G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB |
							       G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class,
					 PROP_MANAGER,
					 g_param_spec_object ("manager",
							      "Manager",
							      "UI Manager",
							      GTK_TYPE_UI_MANAGER,
							      G_PARAM_WRITABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB |
							      G_PARAM_CONSTRUCT_ONLY));
	
	g_type_class_add_private (object_class, sizeof(EphyTopicActionPrivate));
}

GtkAction *
ephy_topic_action_new (EphyNode *node,
		       GtkUIManager *manager,
		       const char *name)
{
	g_assert (name != NULL);

	return GTK_ACTION (g_object_new (EPHY_TYPE_TOPIC_ACTION,
					 "name", name,
					 "topic", node,
					 "manager", manager,
					 NULL));
}
