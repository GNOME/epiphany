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

#include <gtk/gtktoolitem.h>
#include <glib/gi18n.h>

#include "ephy-bookmark-factory-action.h"
#include "ephy-bookmark-action.h"
#include "ephy-node-common.h"
#include "ephy-shell.h"
#include "ephy-stock-icons.h"
#include "egg-editable-toolbar.h"

static void ephy_bookmark_factory_action_class_init (EphyBookmarkFactoryActionClass *class);

#define EPHY_BOOKMARK_FACTORY_ACTION_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_BOOKMARK_FACTORY_ACTION, EphyBookmarkActionPrivate))
#define EGG_TOOLBARS_MODEL_DATA "ephy-bookmark-factory-menu"

static GObjectClass *parent_class = NULL;

GType
ephy_bookmark_factory_action_get_type (void)
{
	static GType type = 0;

	if (!type)
	{
		static const GTypeInfo type_info =
		{
			sizeof (EphyBookmarkFactoryActionClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) ephy_bookmark_factory_action_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,
			sizeof (EphyBookmarkFactoryAction),
			0, /* n_preallocs */
			NULL
		};

		type = g_type_register_static (GTK_TYPE_ACTION,
					       "EphyBookmarkFactoryAction",
					       &type_info, 0);
	}
	return type;
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
build_menu_for_topic (GtkWidget *placeholder, EggToolbarsModel *model, EphyNode *topic)
{
	GtkWidget *menu, *item;
	EphyNode *node;
	GPtrArray *children, *bookmarks;
	const char *name, *action;
	gint i;
	
	children = ephy_node_get_children (topic);
	bookmarks = g_ptr_array_sized_new (children->len);
	for (i = 0; i < children->len; i++)
	  g_ptr_array_add (bookmarks, g_ptr_array_index (children, i));
	g_ptr_array_sort (bookmarks, (GCompareFunc)ephy_bookmarks_compare_bookmark_pointers);
	
	menu = NULL;
	
	for (i = 0; i < bookmarks->len; i++)
	{
		node = g_ptr_array_index (bookmarks, i);
		
		action = ephy_bookmark_action_name (node);
		if (egg_toolbars_model_get_n_avail (model, action) < 0)
		  continue;
		
		name = ephy_node_get_property_string (node, EPHY_NODE_BMK_PROP_TITLE);
		item = gtk_menu_item_new_with_label (name);

		g_object_set_data_full (G_OBJECT (item), "ephy-action", (gpointer) action, g_free);
		g_signal_connect (item, "activate", G_CALLBACK (activate_item_cb), placeholder);
		gtk_widget_show (item);
		
		if (menu == NULL) menu = gtk_menu_new ();
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	}
	g_ptr_array_free (bookmarks, TRUE);
	
	return menu;
}

static GtkWidget *
build_menu (GtkWidget *placeholder, EggToolbarsModel *model)
{
	GtkWidget *menu, *submenu, *item;
	
	EphyBookmarks *eb;
	EphyNode *node;
	GPtrArray *children, *topics;
	
	const char *name;
	gint i, priority = -1, ptmp;
	
	/* Get a sorted list of topics. */
	eb = ephy_shell_get_bookmarks (ephy_shell);
	node = ephy_bookmarks_get_keywords (eb);
	children = ephy_node_get_children (node);
	topics = g_ptr_array_sized_new (children->len);
	for (i = 0; i < children->len; i++)
	  g_ptr_array_add (topics, g_ptr_array_index (children, i));
	g_ptr_array_sort (topics, (GCompareFunc)ephy_bookmarks_compare_topic_pointers);
	
	menu = gtk_menu_new ();
	for (i = 0; i < topics->len; i++)
	{
		node = g_ptr_array_index (topics, i);
		
		ptmp = ephy_node_get_property_int (node, EPHY_NODE_KEYWORD_PROP_PRIORITY);
		if (ptmp == EPHY_NODE_ALL_PRIORITY)
		  continue;
		
		submenu = build_menu_for_topic (placeholder, model, node);
		if (submenu == NULL)
		  continue;
		
		if (ptmp != priority && priority >= 0)
		{
			item = gtk_separator_menu_item_new ();
			gtk_widget_show (item);
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		}
		priority = ptmp;
		
		name = ephy_node_get_property_string (node, EPHY_NODE_KEYWORD_PROP_NAME);
		item = gtk_menu_item_new_with_label (name);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);

		gtk_widget_show (item);
		
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	}
	g_ptr_array_free (topics, TRUE);
	
	return menu;
}

static void
remove_placeholder_cb (GtkMenuShell *menushell,
		       GtkWidget *placeholder)
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
	
	g_signal_connect (widget, "realize",
			  G_CALLBACK (realize_placeholder_cb),
			  action);
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (clicked_placeholder_cb),
			  action);
}

static void
ephy_bookmark_factory_action_class_init (EphyBookmarkFactoryActionClass *class)
{
	GtkActionClass *action_class = GTK_ACTION_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	action_class->toolbar_item_type = GTK_TYPE_TOOL_ITEM;
	action_class->connect_proxy = connect_proxy;
	action_class->create_tool_item = create_tool_item;
}

GtkAction *
ephy_bookmark_factory_action_new (const char *name)
{
	return GTK_ACTION (g_object_new (EPHY_TYPE_BOOKMARK_FACTORY_ACTION,
					 "name", name,
					 "label", _("Quick Bookmark"),
					 "stock-id", GTK_STOCK_ADD,
					 NULL));
}
