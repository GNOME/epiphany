/*
 *  Copyright (C) 2003 Marco Pesenti Gritti
 *  Copyright (C) 2003 Christian Persch
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

#include "ephy-location-action.h"
#include "ephy-location-entry.h"
#include "ephy-shell.h"
#include "ephy-completion-model.h"
#include "ephy-debug.h"

#include <gtk/gtkentry.h>
#include <gtk/gtkentrycompletion.h>

#define EPHY_LOCATION_ACTION_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_LOCATION_ACTION, EphyLocationActionPrivate))

struct _EphyLocationActionPrivate
{
	GList *actions;
	char *address;
	EphyNode *smart_bmks;
	EphyBookmarks *bookmarks;
};

static void ephy_location_action_init       (EphyLocationAction *action);
static void ephy_location_action_class_init (EphyLocationActionClass *class);
static void ephy_location_action_finalize   (GObject *object);
static void user_changed_cb		    (GtkWidget *proxy,
					     EphyLocationAction *action);
static void sync_address		    (GtkAction *action,
					     GParamSpec *pspec,
					     GtkWidget *proxy);

enum
{
	PROP_0,
	PROP_ADDRESS
};

enum
{
	GO_LOCATION,
	LAST_SIGNAL
};

static GObjectClass *parent_class = NULL;

static guint signals[LAST_SIGNAL] = { 0 };

GType
ephy_location_action_get_type (void)
{
	static GType type = 0;

	if (!type)
	{
		static const GTypeInfo type_info =
		{
			sizeof (EphyLocationActionClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) ephy_location_action_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,
			sizeof (EphyLocationAction),
			0, /* n_preallocs */
			(GInstanceInitFunc) ephy_location_action_init,
		};

		type = g_type_register_static (GTK_TYPE_ACTION,
					       "EphyLocationAction",
					       &type_info, 0);
	}
	return type;
}

static void
action_activated_cb (GtkEntryCompletion *completion,
                     gint index,
		     EphyLocationAction *action)
{
	GtkWidget *entry;
	char *content;

	entry = gtk_entry_completion_get_entry (completion);
	content = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
	if (content)
	{
		EphyNode *node;
		const char *smart_url;
		char *url;

		node = (EphyNode *)g_list_nth_data (action->priv->actions, index);
		smart_url = ephy_node_get_property_string
	                (node, EPHY_NODE_BMK_PROP_LOCATION);
		g_return_if_fail (smart_url != NULL);

		url = ephy_bookmarks_solve_smart_url
			(action->priv->bookmarks, smart_url, content);
		g_return_if_fail (url != NULL);

		g_signal_emit (action, signals[GO_LOCATION], 0, url);

		g_free (url);
		g_free (content);
	}
}

static void
location_url_activate_cb (EphyLocationEntry *entry,
			  EphyLocationAction *action)
{
	char *content;

	content = gtk_editable_get_chars (GTK_EDITABLE(entry), 0, -1);
	if (content)
	{
		g_signal_emit (action, signals[GO_LOCATION], 0, content);
		g_free (content);
	}
}

static void
user_changed_cb (GtkWidget *proxy, EphyLocationAction *action)
{
	const char *address;

	address = ephy_location_entry_get_location (EPHY_LOCATION_ENTRY (proxy));

	LOG ("user_changed_cb, new address %s", address)

	g_signal_handlers_block_by_func (action, G_CALLBACK (sync_address), proxy);
	ephy_location_action_set_address (action, address);
	g_signal_handlers_unblock_by_func (action, G_CALLBACK (sync_address), proxy);
}

static void
sync_address (GtkAction *act, GParamSpec *pspec, GtkWidget *proxy)
{
	EphyLocationAction *action = EPHY_LOCATION_ACTION (act);

	LOG ("sync_address")

	g_return_if_fail (EPHY_IS_LOCATION_ENTRY (proxy));
	g_signal_handlers_block_by_func (proxy, G_CALLBACK (user_changed_cb), action);
	ephy_location_entry_set_location (EPHY_LOCATION_ENTRY (proxy),
					  action->priv->address);
	g_signal_handlers_unblock_by_func (proxy, G_CALLBACK (user_changed_cb), action);
}

static void
remove_completion_actions (GtkAction *action, GtkWidget *proxy)
{
	GtkWidget *entry;
	GtkEntryCompletion *completion;
	EphyLocationAction *la = EPHY_LOCATION_ACTION (action);
	GList *l;

	entry = ephy_location_entry_get_entry (EPHY_LOCATION_ENTRY (proxy));
	completion = gtk_entry_get_completion (GTK_ENTRY (entry));

	for (l = la->priv->actions; l != NULL; l = l->next)
	{
		int index;

		index = g_list_position (la->priv->actions, l);
		gtk_entry_completion_delete_action (completion, index);
	}

	g_signal_handlers_disconnect_by_func
			(completion, G_CALLBACK (action_activated_cb), la);
}

static void
add_completion_actions (GtkAction *action, GtkWidget *proxy)
{
	GtkWidget *entry;
	GtkEntryCompletion *completion;
	EphyLocationAction *la = EPHY_LOCATION_ACTION (action);
	GList *l;

	entry = ephy_location_entry_get_entry (EPHY_LOCATION_ENTRY (proxy));
	completion = gtk_entry_get_completion (GTK_ENTRY (entry));

	for (l = la->priv->actions; l != NULL; l = l->next)
	{
		EphyNode *bmk = l->data;
		const char *title;
		int index;

		index = g_list_position (la->priv->actions, l);
		title = ephy_node_get_property_string
	                (bmk, EPHY_NODE_BMK_PROP_TITLE);
		gtk_entry_completion_insert_action_text (completion, index, (char*)title);
	}

	g_signal_connect (completion, "action_activated",
			  G_CALLBACK (action_activated_cb), la);
}

static void
connect_proxy (GtkAction *action, GtkWidget *proxy)
{
	LOG ("Connect proxy")

	if (EPHY_IS_LOCATION_ENTRY (proxy))
	{
		EphyCompletionModel *model;
		GtkWidget *entry;

		model = ephy_completion_model_new ();
		ephy_location_entry_set_completion (EPHY_LOCATION_ENTRY (proxy),
						    GTK_TREE_MODEL (model),
						    EPHY_COMPLETION_TEXT_COL,
						    EPHY_COMPLETION_ACTION_COL,
						    EPHY_COMPLETION_KEYWORDS_COL,
						    EPHY_COMPLETION_RELEVANCE_COL);

		add_completion_actions (action, proxy);

		sync_address (action, NULL, proxy);
		g_signal_connect_object (action, "notify::address",
					 G_CALLBACK (sync_address), proxy, 0);

		entry = ephy_location_entry_get_entry (EPHY_LOCATION_ENTRY (proxy));
		g_signal_connect_object (entry, "activate",
					 G_CALLBACK (location_url_activate_cb),
					 action, 0);
		g_signal_connect_object (proxy, "user_changed",
					 G_CALLBACK (user_changed_cb), action, 0);
	}

	(* GTK_ACTION_CLASS (parent_class)->connect_proxy) (action, proxy);
}

static void
disconnect_proxy (GtkAction *action, GtkWidget *proxy)
{
	LOG ("Disconnect proxy")

	if (EPHY_IS_LOCATION_ENTRY (proxy))
	{
		g_signal_handlers_disconnect_by_func
			(action, G_CALLBACK (sync_address), proxy);

		g_signal_handlers_disconnect_by_func
			(proxy, G_CALLBACK (location_url_activate_cb), action);

		g_signal_handlers_disconnect_by_func
			(proxy, G_CALLBACK (user_changed_cb), action);
	}

	(* GTK_ACTION_CLASS (parent_class)->disconnect_proxy) (action, proxy);
}

static void
ephy_location_action_set_property (GObject *object,
				   guint prop_id,
				   const GValue *value,
				   GParamSpec *pspec)
{
	EphyLocationAction *action = EPHY_LOCATION_ACTION (object);

	switch (prop_id)
	{
		case PROP_ADDRESS:
			ephy_location_action_set_address (action, g_value_get_string (value));
			break;
	}
}

static void
ephy_location_action_get_property (GObject *object,
				   guint prop_id,
				   GValue *value,
				   GParamSpec *pspec)
{
	EphyLocationAction *action = EPHY_LOCATION_ACTION (object);

	switch (prop_id)
	{
		case PROP_ADDRESS:
			g_value_set_string (value, ephy_location_action_get_address (action));
			break;
	}
}

static void
ephy_location_action_activate (GtkAction *action)
{
	GSList *proxies;

	/* Note: this makes sense only for a single proxy */
	proxies = gtk_action_get_proxies (action);

	if (proxies)
	{
		ephy_location_entry_activate (EPHY_LOCATION_ENTRY (proxies->data));
	}
}

static void
ephy_location_action_class_init (EphyLocationActionClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkActionClass *action_class = GTK_ACTION_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->finalize = ephy_location_action_finalize;
	object_class->get_property = ephy_location_action_get_property;
	object_class->set_property = ephy_location_action_set_property;

	action_class->toolbar_item_type = EPHY_TYPE_LOCATION_ENTRY;
	action_class->connect_proxy = connect_proxy;
	action_class->disconnect_proxy = disconnect_proxy;
	action_class->activate = ephy_location_action_activate;

	signals[GO_LOCATION] =
                g_signal_new ("go_location",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (EphyLocationActionClass, go_location),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__STRING,
                              G_TYPE_NONE,
                              1,
			      G_TYPE_STRING);

	g_object_class_install_property (object_class,
					 PROP_ADDRESS,
					 g_param_spec_string ("address",
							      "Address",
							      "The address",
							      "",
							      G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (EphyLocationActionPrivate));
}

static void
init_actions_list (EphyLocationAction *action)
{
	GPtrArray *children;
	int i;

	children = ephy_node_get_children (action->priv->smart_bmks);
	for (i = 0; i < children->len; i++)
	{
		EphyNode *kid;

		kid = g_ptr_array_index (children, i);

		action->priv->actions = g_list_append
			(action->priv->actions, kid);
	}
}

static void
update_actions_list (EphyLocationAction *la)
{
	GSList *l;
	GtkAction *action = GTK_ACTION (la);

	l = gtk_action_get_proxies (action);
	for (; l != NULL; l = l->next)
	{
		remove_completion_actions (action, GTK_WIDGET (l->data));
	}

	g_list_free (la->priv->actions);
	la->priv->actions = NULL;
	init_actions_list (la);

	l = gtk_action_get_proxies (action);
	for (; l != NULL; l = l->next)
	{
		add_completion_actions (action, l->data);
	}
}

static void
actions_child_removed_cb (EphyNode *node,
		          EphyNode *child,
		          guint old_index,
		          EphyLocationAction *action)
{
	update_actions_list (action);
}

static void
actions_child_added_cb (EphyNode *node,
		        EphyNode *child,
		        EphyLocationAction *action)
{
	update_actions_list (action);
}

static void
actions_child_changed_cb (EphyNode *node,
		          EphyNode *child,
		          EphyLocationAction *action)
{
	update_actions_list (action);
}

static void
ephy_location_action_init (EphyLocationAction *action)
{
	action->priv = EPHY_LOCATION_ACTION_GET_PRIVATE (action);

	action->priv->address = g_strdup ("");
	action->priv->actions = NULL;

	action->priv->bookmarks = ephy_shell_get_bookmarks (ephy_shell);
	action->priv->smart_bmks = ephy_bookmarks_get_smart_bookmarks
		(action->priv->bookmarks);

	init_actions_list (action);

	ephy_node_signal_connect_object (action->priv->smart_bmks,
			                 EPHY_NODE_CHILD_ADDED,
			                 (EphyNodeCallback)actions_child_added_cb,
			                 G_OBJECT (action));
	ephy_node_signal_connect_object (action->priv->smart_bmks,
			                 EPHY_NODE_CHILD_REMOVED,
			                 (EphyNodeCallback)actions_child_removed_cb,
			                 G_OBJECT (action));
	ephy_node_signal_connect_object (action->priv->smart_bmks,
			                 EPHY_NODE_CHILD_CHANGED,
			                 (EphyNodeCallback)actions_child_changed_cb,
			                 G_OBJECT (action));
}

static void
ephy_location_action_finalize (GObject *object)
{
	EphyLocationAction *action = EPHY_LOCATION_ACTION (object);

	g_list_free (action->priv->actions);

	g_free (action->priv->address);
}

const char *
ephy_location_action_get_address (EphyLocationAction *action)
{
	g_return_val_if_fail (EPHY_IS_LOCATION_ACTION (action), "");

	return action->priv->address;
}

void
ephy_location_action_set_address (EphyLocationAction *action,
				  const char *address)
{
	g_return_if_fail (EPHY_IS_LOCATION_ACTION (action));

	LOG ("set_address %s", address)

	g_free (action->priv->address);
	action->priv->address = g_strdup (address ? address : "");
	g_object_notify (G_OBJECT (action), "address");
}

static void
clear_history (GtkWidget *proxy, gpointer user_data)
{
	if (EPHY_IS_LOCATION_ENTRY (proxy))
	{
		ephy_location_entry_clear_history (EPHY_LOCATION_ENTRY (proxy));
	}
}

void
ephy_location_action_clear_history (EphyLocationAction *action)
{
	GSList *proxies;

	proxies = gtk_action_get_proxies (GTK_ACTION (action));

	g_slist_foreach (proxies, (GFunc) clear_history, NULL);
}
