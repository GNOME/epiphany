/*
 *  Copyright © 2004 Peter Harvey
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtktoolitem.h>
#include <glib/gi18n.h>

#include "ephy-topic-factory-action.h"
#include "ephy-topic-action.h"
#include "ephy-bookmarks-ui.h"
#include "ephy-shell.h"
#include "ephy-stock-icons.h"
#include "egg-editable-toolbar.h"

static void ephy_topic_factory_action_class_init (EphyTopicFactoryActionClass *class);

#define EPHY_TOPIC_FACTORY_ACTION_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_TOPIC_FACTORY_ACTION, EphyTopicActionPrivate))
#define EGG_TOOLBARS_MODEL_DATA "ephy-topic-factory-menu"

static GObjectClass *parent_class = NULL;

GType
ephy_topic_factory_action_get_type (void)
{
	static GType type = 0;

	if (!type)
	{
		const GTypeInfo type_info =
		{
			sizeof (EphyTopicFactoryActionClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) ephy_topic_factory_action_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,
			sizeof (EphyTopicFactoryAction),
			0, /* n_preallocs */
			NULL
		};

		type = g_type_register_static (GTK_TYPE_ACTION,
					       "EphyTopicFactoryAction",
					       &type_info, 0);
	}
	return type;
}

static int
sort_topics (gconstpointer a, gconstpointer b)
{
	EphyNode *node_a = *(EphyNode **)a;
	EphyNode *node_b = *(EphyNode **)b;
	const char *title1, *title2;
	int priority1, priority2;
  
	priority1 = ephy_node_get_property_int (node_a, EPHY_NODE_KEYWORD_PROP_PRIORITY);
	priority2 = ephy_node_get_property_int (node_b, EPHY_NODE_KEYWORD_PROP_PRIORITY);
	
	if (priority1 > priority2)
	{
		return 1;
	}
	else if (priority1 < priority2)
	{
		return -1;
	}
	else
	{
		title1 = ephy_node_get_property_string (node_a, EPHY_NODE_KEYWORD_PROP_NAME);
		title2 = ephy_node_get_property_string (node_b, EPHY_NODE_KEYWORD_PROP_NAME);

		if (title1 == NULL)
		{
			return -1;
		}
		else if (title2 == NULL)
		{
			return 1;
		}
		else
		{
			return g_utf8_collate (title1, title2);
		}
	}
	
	return 0;
}

static void
activate_item_cb (GtkWidget *menuitem, GtkWidget *placeholder)
{
	GtkWidget *toolbar, *etoolbar, *item;
	EggToolbarsModel *model;
	GList *children;
	gint index, pos;
	char *id;

	item = gtk_widget_get_ancestor (placeholder, GTK_TYPE_TOOL_ITEM);
	g_return_if_fail (item);
	toolbar = gtk_widget_get_ancestor (item, GTK_TYPE_TOOLBAR);
	g_return_if_fail (toolbar);
	etoolbar = gtk_widget_get_ancestor (toolbar, EGG_TYPE_EDITABLE_TOOLBAR);
	g_return_if_fail (etoolbar);
	model = egg_editable_toolbar_get_model (EGG_EDITABLE_TOOLBAR (etoolbar));
	g_return_if_fail (model);
	
	children = gtk_container_get_children (GTK_CONTAINER (etoolbar));
	pos = g_list_index (children, toolbar->parent);
	index = gtk_toolbar_get_item_index (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (item));
	g_list_free (children);
	
	id = g_object_get_data (G_OBJECT (menuitem), "ephy-action");
	egg_toolbars_model_add_item (model, pos, index, id);
}

static GtkWidget *
build_menu (GtkWidget *placeholder, EggToolbarsModel *model)
{
	GtkWidget *menu, *item;
	
	EphyBookmarks *eb;
	EphyNode *node;
	GPtrArray *children, *topics;

	const char *name;
	char action[EPHY_TOPIC_ACTION_NAME_BUFFER_SIZE];
	gint i, priority = -1, ptmp, flags;
	
	/* Get a sorted list of topics. */
	eb = ephy_shell_get_bookmarks (ephy_shell);
	node = ephy_bookmarks_get_keywords (eb);
	children = ephy_node_get_children (node);
	topics = g_ptr_array_sized_new (children->len);
	for (i = 0; i < children->len; i++)
	  g_ptr_array_add (topics, g_ptr_array_index (children, i));
	g_ptr_array_sort (topics, (GCompareFunc)sort_topics);
	
	menu = gtk_menu_new ();
	for (i = 0; i < topics->len; i++)
	{
		node = g_ptr_array_index (topics, i);

		EPHY_TOPIC_ACTION_NAME_PRINTF (action, node);

		flags = egg_toolbars_model_get_name_flags (model, action);
		if (flags & EGG_TB_MODEL_NAME_USED)
		  continue;
		
		ptmp = ephy_node_get_property_int (node, EPHY_NODE_KEYWORD_PROP_PRIORITY);
		if (ptmp != priority && priority >= 0)
		{
			item = gtk_separator_menu_item_new ();
			gtk_widget_show (item);
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		}
		priority = ptmp;
		
		name = ephy_node_get_property_string (node, EPHY_NODE_KEYWORD_PROP_NAME);
		item = gtk_menu_item_new_with_label (name);

		/* FIXME: set the |node| instead here! */
		g_object_set_data_full (G_OBJECT (item), "ephy-action",
					g_strdup (action), g_free);
		g_signal_connect (item, "activate", G_CALLBACK (activate_item_cb), placeholder);
		gtk_widget_show (item);
		
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	}
	g_ptr_array_free (topics, TRUE);
	
	return menu;
}

static void
remove_placeholder_cb (GtkMenuShell *menushell, GtkWidget *placeholder)
{
	GtkWidget *toolbar, *etoolbar, *item;
	EggToolbarsModel *model;
	GList *children;
	gint index, pos;
	
	item = gtk_widget_get_ancestor (placeholder, GTK_TYPE_TOOL_ITEM);
	g_return_if_fail (item);
	toolbar = gtk_widget_get_ancestor (item, GTK_TYPE_TOOLBAR);
	g_return_if_fail (toolbar);
	etoolbar = gtk_widget_get_ancestor (toolbar, EGG_TYPE_EDITABLE_TOOLBAR);
	g_return_if_fail (etoolbar);
	model = egg_editable_toolbar_get_model (EGG_EDITABLE_TOOLBAR (etoolbar));
	g_return_if_fail (model);

	g_object_set_data (G_OBJECT (model), EGG_TOOLBARS_MODEL_DATA, NULL);

	children = gtk_container_get_children (GTK_CONTAINER (etoolbar));
	pos = g_list_index (children, toolbar->parent);
	index = gtk_toolbar_get_item_index (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (item));
	g_list_free (children);
	
	egg_toolbars_model_remove_item (model, pos, index);
}

static gboolean
activate_placeholder_cb (GtkWidget *placeholder)
{
	GtkWidget *toolbar, *etoolbar, *item, *menu;
	EggToolbarsModel *model;
	gint index;
	
	/* Get our position on a toolbar. */
	item = gtk_widget_get_ancestor (placeholder, GTK_TYPE_TOOL_ITEM);
	g_return_val_if_fail (item, FALSE);
	toolbar = gtk_widget_get_ancestor (item, GTK_TYPE_TOOLBAR);
	g_return_val_if_fail (toolbar, FALSE);
	etoolbar = gtk_widget_get_ancestor (toolbar, EGG_TYPE_EDITABLE_TOOLBAR);
	g_return_val_if_fail (etoolbar, FALSE);
	model = egg_editable_toolbar_get_model (EGG_EDITABLE_TOOLBAR (etoolbar));
	g_return_val_if_fail (model, FALSE);
	
	/* If we are not yet on the toolbar, abort. */
	index = gtk_toolbar_get_item_index (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (item));
	if (index < 0 || index >= gtk_toolbar_get_n_items (GTK_TOOLBAR (toolbar)))
	  return FALSE;
	
	/* If there is already a popup menu, abort. */
	menu = g_object_get_data (G_OBJECT (model), EGG_TOOLBARS_MODEL_DATA);
	if (menu != NULL) return FALSE;
	
	/* Create the menu and store it's pointer to ensure noone else creates a menu. */
	menu = build_menu (placeholder, model);
	g_object_set_data (G_OBJECT (model), EGG_TOOLBARS_MODEL_DATA, menu);

	g_signal_connect (menu, "selection-done",
			  G_CALLBACK (remove_placeholder_cb), placeholder);
		
	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, 0,
			gtk_get_current_event_time ());
	
	return FALSE;
}

static void
clicked_placeholder_cb (GtkWidget *placeholder, GdkEventButton *event, gpointer user)
{
	activate_placeholder_cb (placeholder);
}

static void
realize_placeholder_cb (GtkWidget *placeholder, gpointer user)
{
	g_idle_add ((GSourceFunc) activate_placeholder_cb, placeholder);
}

static GtkWidget *
create_tool_item (GtkAction *action)
{
	GtkWidget *item = GTK_WIDGET (gtk_tool_item_new ());
	GtkWidget *widget = gtk_button_new_with_label ("  ?  ");
	gtk_button_set_relief (GTK_BUTTON (widget), GTK_RELIEF_NONE);
	
	gtk_container_add (GTK_CONTAINER (item), widget);
	gtk_widget_show (widget);
	
	return item;
}

static void
connect_proxy (GtkAction *action, GtkWidget *proxy)
{
	GtkWidget *widget;
	
	(* GTK_ACTION_CLASS (parent_class)->connect_proxy) (action, proxy);

	g_return_if_fail (GTK_IS_TOOL_ITEM (proxy));
	
	widget = gtk_bin_get_child (GTK_BIN (proxy));
	
	g_signal_connect (G_OBJECT (widget), "realize",
			  G_CALLBACK (realize_placeholder_cb),
			  G_OBJECT (action));
	g_signal_connect (G_OBJECT (widget), "clicked",
			  G_CALLBACK (clicked_placeholder_cb),
			  G_OBJECT (action));
}

static void
ephy_topic_factory_action_class_init (EphyTopicFactoryActionClass *class)
{
	GtkActionClass *action_class = GTK_ACTION_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	action_class->toolbar_item_type = GTK_TYPE_TOOL_ITEM;
	action_class->connect_proxy = connect_proxy;
	action_class->create_tool_item = create_tool_item;
}

GtkAction *
ephy_topic_factory_action_new (const char *name)
{
	return GTK_ACTION (g_object_new (EPHY_TYPE_TOPIC_FACTORY_ACTION,
					 "name", name,
					 "label", _("Quick Topic"),
					 "stock-id", GTK_STOCK_ADD,
					 NULL));
}
