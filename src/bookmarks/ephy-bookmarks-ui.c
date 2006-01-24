/*
 *  Copyright (C) 2005 Peter Harvey
 *  Copyright (C) 2006 Christian Persch
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

#include "ephy-bookmarks.h"
#include "ephy-bookmarks-ui.h"
#include "ephy-bookmarks-menu.h"
#include "ephy-bookmark-action.h"
#include "ephy-topic-action.h"
#include "ephy-bookmark-action-group.h"
#include "ephy-topic-action-group.h"
#include "ephy-related-action.h"
#include "ephy-open-tabs-action.h"
#include "ephy-topic-factory-action.h"
#include "ephy-bookmark-factory-action.h"
#include "ephy-bookmark-properties.h"
#include "ephy-node-common.h"
#include "ephy-link.h"
#include "ephy-dnd.h"
#include "ephy-history.h"
#include "ephy-shell.h"
#include "ephy-string.h"
#include "ephy-debug.h"
#include "ephy-file-helpers.h"
#include "ephy-gui.h"
#include "ephy-stock-icons.h"
#include "egg-editable-toolbar.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtkmain.h>

#define BM_WINDOW_DATA_KEY "bookmarks-window-data"

typedef struct
{
	guint bookmarks_menu;
	guint toolbar_menu;
} BookmarksWindowData;

enum
{
	RESPONSE_SHOW_PROPERTIES = 1,
	RESPONSE_NEW_BOOKMARK = 2
};

static GString * bookmarks_menu_string = 0;
static GHashTable *properties_dialogs = 0;

static GtkAction *
find_action (GtkUIManager *manager, const char *name)
{
	GList *l = gtk_ui_manager_get_action_groups (manager);
	GtkAction *action;
	
	while (l != NULL)
	{
		action = gtk_action_group_get_action (GTK_ACTION_GROUP (l->data), name);
		if (action) return action;
		l = l->next;
	}

	return NULL;
}

static void
activate_bookmarks_menu (GtkAction *action, EphyWindow *window)
{
	BookmarksWindowData *data = g_object_get_data (G_OBJECT (window), BM_WINDOW_DATA_KEY);
	if (data && !data->bookmarks_menu)
	{
		GtkUIManager *manager = GTK_UI_MANAGER (ephy_window_get_ui_manager (window));
		gtk_ui_manager_ensure_update (manager);

		if (!bookmarks_menu_string->len)
		{
			g_string_append (bookmarks_menu_string,
					 "<ui><menubar><menu name=\"BookmarksMenu\" action=\"Bookmarks\">");
			ephy_bookmarks_menu_build (bookmarks_menu_string, 0);
			g_string_append (bookmarks_menu_string, "</menu></menubar></ui>");
		}

		data->bookmarks_menu = gtk_ui_manager_add_ui_from_string
		  (manager, bookmarks_menu_string->str, bookmarks_menu_string->len, 0);
		
		gtk_ui_manager_ensure_update (manager);
	}
}

static void
activate_bookmark_properties (GtkAction *action,
			      EggEditableToolbar *etoolbar)
{
	GtkAction *baction;
	GtkWidget *widget;

	widget = gtk_widget_get_ancestor (egg_editable_toolbar_get_selected (etoolbar),
					  GTK_TYPE_TOOL_ITEM);
	baction = widget ? g_object_get_data (G_OBJECT (widget), "gtk-action") : NULL;
	g_return_if_fail (EPHY_IS_BOOKMARK_ACTION (baction));

	ephy_bookmarks_ui_show_bookmark (ephy_bookmark_action_get_bookmark
						 (EPHY_BOOKMARK_ACTION (baction)));
}

static void
activate_bookmark_open_tab (GtkAction *action,
			    EggEditableToolbar *etoolbar)
{
	GtkAction *baction;
	GtkWidget *widget;

	widget = gtk_widget_get_ancestor (egg_editable_toolbar_get_selected (etoolbar),
					  GTK_TYPE_TOOL_ITEM);
	baction = widget ? g_object_get_data (G_OBJECT (widget), "gtk-action") : NULL;
	g_return_if_fail (EPHY_IS_BOOKMARK_ACTION (baction));

	ephy_bookmark_action_activate (EPHY_BOOKMARK_ACTION (baction), widget,
				       EPHY_LINK_NEW_TAB);
}

static void
activate_bookmark_open_window (GtkAction *action,
			       EggEditableToolbar *etoolbar)
{
	GtkAction *baction;
	GtkWidget *widget;

	widget = gtk_widget_get_ancestor (egg_editable_toolbar_get_selected (etoolbar),
					  GTK_TYPE_TOOL_ITEM);
	baction = widget ? g_object_get_data (G_OBJECT (widget), "gtk-action") : NULL;
	g_return_if_fail (EPHY_IS_BOOKMARK_ACTION (baction));

	ephy_bookmark_action_activate (EPHY_BOOKMARK_ACTION (baction), widget,
				       EPHY_LINK_NEW_WINDOW);
}

static void
selected_bookmark_action (EggEditableToolbar *etoolbar,
			  GParamSpec *pspec,
			  GtkAction *action)
{
	GtkAction *baction;
	GtkWidget *widget;

	widget = gtk_widget_get_ancestor (egg_editable_toolbar_get_selected (etoolbar),
					  GTK_TYPE_TOOL_ITEM);
	baction = widget ? g_object_get_data (G_OBJECT (widget), "gtk-action") : NULL;

	gtk_action_set_visible (action, EPHY_IS_BOOKMARK_ACTION (baction));
}

static void
erase_bookmarks_menu (EphyWindow *window)
{
	BookmarksWindowData *data;
	GtkUIManager *manager;

	manager = GTK_UI_MANAGER (ephy_window_get_ui_manager (window));
	data = g_object_get_data (G_OBJECT (window), BM_WINDOW_DATA_KEY);

	if (data != NULL && data->bookmarks_menu != 0)
	{
		gtk_ui_manager_remove_ui (manager, data->bookmarks_menu);
		data->bookmarks_menu = 0;
	}

	g_string_truncate (bookmarks_menu_string, 0);
}

static void
tree_changed_cb (EphyBookmarks *bookmarks,
		 EphyWindow *window)
{
	erase_bookmarks_menu (window);
}

static void
node_added_cb (EphyNode *parent,
	       EphyNode *child,
	       EphyWindow *window)
{
	erase_bookmarks_menu (window);
}

static void
node_changed_cb (EphyNode *parent,
		 EphyNode *child,
		 guint property_id,
		 EphyWindow *window)
{
	if (property_id == EPHY_NODE_KEYWORD_PROP_NAME ||
	    property_id == EPHY_NODE_BMK_PROP_TITLE)
	{
		erase_bookmarks_menu (window);
	}
}

static void
node_removed_cb (EphyNode *parent,
		 EphyNode *child,
		 guint index,
		 EphyWindow *window)
{
	erase_bookmarks_menu (window);
}

void
ephy_bookmarks_ui_attach_window (EphyWindow *window)
{
	EphyBookmarks *eb;
	EphyNode *bookmarks;
	EphyNode *topics;
	BookmarksWindowData *data;
	GtkUIManager *manager;
	EggEditableToolbar *etoolbar;
	GtkActionGroup *actions;
	GtkAction *action;

	eb = ephy_shell_get_bookmarks (ephy_shell);
	bookmarks = ephy_bookmarks_get_bookmarks (eb);
	topics = ephy_bookmarks_get_keywords (eb);
	data = g_object_get_data (G_OBJECT (window), BM_WINDOW_DATA_KEY);
	g_return_if_fail (data == NULL);

	manager = GTK_UI_MANAGER (ephy_window_get_ui_manager (window));
	etoolbar = EGG_EDITABLE_TOOLBAR (ephy_window_get_toolbar (window));

	data = g_new0 (BookmarksWindowData, 1);
	g_object_set_data_full (G_OBJECT (window), BM_WINDOW_DATA_KEY, data, g_free);

	/* Create the self-maintaining action groups for bookmarks and topics */
	actions = ephy_bookmark_group_new (bookmarks);
	gtk_ui_manager_insert_action_group (manager, actions, -1);
	g_signal_connect_object (G_OBJECT (actions), "open-link",
				 G_CALLBACK (ephy_link_open), G_OBJECT (window),
				 G_CONNECT_SWAPPED | G_CONNECT_AFTER);
	g_object_unref (G_OBJECT (actions));
	
	actions = ephy_topic_group_new (topics, manager);
	gtk_ui_manager_insert_action_group (manager, actions, -1);
	g_object_unref (G_OBJECT (actions));

	actions = ephy_open_tabs_group_new (topics);
	gtk_ui_manager_insert_action_group (manager, actions, -1);
	g_signal_connect_object (actions, "open-link",
				 G_CALLBACK (ephy_link_open), G_OBJECT (window),
				 G_CONNECT_SWAPPED | G_CONNECT_AFTER);
	g_object_unref (actions);
	
	/* Create and add an action group specifically foor bookmarks on the toolbar */
	actions = gtk_action_group_new ("BookmarkToolbarActions");
	gtk_ui_manager_insert_action_group (manager, actions, 0);	
	g_object_unref (actions);

	/* Add factory actions */
	action = ephy_topic_factory_action_new ("AddTopicToToolbar");
	gtk_action_group_add_action (actions, action);
	g_object_unref (action);
	
	action = ephy_bookmark_factory_action_new ("AddBookmarkToToolbar");
	gtk_action_group_add_action (actions, action);
	g_object_unref (action);

	/* Add the dynamic 'related topic' action */
	action = ephy_related_action_new (EPHY_LINK (window), manager, "RelatedTopic");
	gtk_action_group_add_action (actions, action);
	g_object_unref (action);

	/* Add popup menu actions that are specific to the bookmark widgets */
	action = gtk_action_new ("ToolbarBookmarkProperties", _("Properties"), 
				 _("Show properties for this bookmark"), GTK_STOCK_PROPERTIES);
	g_signal_connect_object (action, "activate",
				 G_CALLBACK (activate_bookmark_properties), 
				 G_OBJECT (etoolbar), 0);
	g_signal_connect_object (etoolbar, "notify::selected",
				 G_CALLBACK (selected_bookmark_action),
				 G_OBJECT (action), 0);
	gtk_action_group_add_action (actions, action);
	g_object_unref (action);

	/* FIXME ngettext */
	action = gtk_action_new ("ToolbarBookmarkOpenInTab", _("Open in New _Tab"),
				 _("Open this bookmark in a new tab"), NULL);
	g_signal_connect_object (action, "activate",
				 G_CALLBACK (activate_bookmark_open_tab), 
				 G_OBJECT (etoolbar), 0);
	g_signal_connect_object (etoolbar, "notify::selected",
				 G_CALLBACK (selected_bookmark_action),
				 G_OBJECT (action), 0);
	gtk_action_group_add_action (actions, action);
	g_object_unref (action);

	/* FIXME ngettext */
	action = gtk_action_new ("ToolbarBookmarkOpenInWindow", _("Open in New _Window"), 
				 _("Open this bookmark in a new window"), NULL);
	g_signal_connect_object (action, "activate",
				 G_CALLBACK (activate_bookmark_open_window),
				 G_OBJECT (etoolbar), 0);
	g_signal_connect_object (etoolbar, "notify::selected",
				 G_CALLBACK (selected_bookmark_action),
				 G_OBJECT (action), 0);
	gtk_action_group_add_action (actions, action);
	g_object_unref (action);

	data->toolbar_menu = gtk_ui_manager_add_ui_from_string (manager,
	   "<popup name=\"ToolbarPopup\">"
	   "<separator/>"
	   "<menuitem action=\"ToolbarBookmarkOpenInTab\"/>"
	   "<menuitem action=\"ToolbarBookmarkOpenInWindow\"/>"
	   "<separator/>"
	   "<menuitem action=\"ToolbarBookmarkProperties\"/>"
	   "</popup>", -1, NULL);  

	/* Add signal handlers for the bookmark database */
	ephy_node_signal_connect_object (bookmarks, EPHY_NODE_CHILD_ADDED,
					 (EphyNodeCallback)node_added_cb,
					 G_OBJECT (window));
	ephy_node_signal_connect_object (topics, EPHY_NODE_CHILD_ADDED,
					 (EphyNodeCallback)node_added_cb,
					 G_OBJECT (window));

	ephy_node_signal_connect_object (bookmarks, EPHY_NODE_CHILD_REMOVED,
					 (EphyNodeCallback)node_removed_cb,
					 G_OBJECT (window));
	ephy_node_signal_connect_object (topics, EPHY_NODE_CHILD_REMOVED,
					 (EphyNodeCallback)node_removed_cb,
					 G_OBJECT (window));

	ephy_node_signal_connect_object (bookmarks, EPHY_NODE_CHILD_CHANGED,
					 (EphyNodeCallback)node_changed_cb,
					 G_OBJECT (window));        
	ephy_node_signal_connect_object (topics, EPHY_NODE_CHILD_CHANGED,
					 (EphyNodeCallback)node_changed_cb,
					 G_OBJECT (window));

	g_signal_connect_object (eb, "tree_changed",
				 G_CALLBACK (tree_changed_cb),
				 G_OBJECT (window), 0);

	/* Setup empty menu strings and add signal handlers to build the menus on demand */
	if (!bookmarks_menu_string) bookmarks_menu_string = g_string_new ("");
	action = find_action (manager, "Bookmarks");
	g_signal_connect_object (action, "activate",
				 G_CALLBACK (activate_bookmarks_menu),
				 G_OBJECT (window), 0);
}

void
ephy_bookmarks_ui_detach_window (EphyWindow *window)
{
	EphyBookmarks *eb = ephy_shell_get_bookmarks (ephy_shell);
	EphyNode *bookmarks = ephy_bookmarks_get_bookmarks (eb);
	EphyNode *topics = ephy_bookmarks_get_keywords (eb);

	BookmarksWindowData *data = g_object_get_data (G_OBJECT (window), BM_WINDOW_DATA_KEY);
	GtkUIManager *manager = GTK_UI_MANAGER (ephy_window_get_ui_manager (window));
	GtkAction *action;

	g_return_if_fail (data != 0);
	if (data->bookmarks_menu) gtk_ui_manager_remove_ui (manager, data->bookmarks_menu);
	if (data->toolbar_menu) gtk_ui_manager_remove_ui (manager, data->toolbar_menu);
	g_object_set_data (G_OBJECT (window), BM_WINDOW_DATA_KEY, 0);
	
	ephy_node_signal_disconnect_object (bookmarks, EPHY_NODE_CHILD_ADDED,
					    (EphyNodeCallback)node_added_cb,
					    G_OBJECT (window));
	ephy_node_signal_disconnect_object (topics, EPHY_NODE_CHILD_ADDED,
					    (EphyNodeCallback)node_added_cb,
					    G_OBJECT (window));
	
	ephy_node_signal_disconnect_object (bookmarks, EPHY_NODE_CHILD_REMOVED,
					    (EphyNodeCallback)node_removed_cb,
					    G_OBJECT (window));
	ephy_node_signal_disconnect_object (topics, EPHY_NODE_CHILD_REMOVED,
					    (EphyNodeCallback)node_removed_cb,
					    G_OBJECT (window));
	
	ephy_node_signal_disconnect_object (bookmarks, EPHY_NODE_CHILD_CHANGED,
					    (EphyNodeCallback)node_changed_cb,
					    G_OBJECT (window));        
	ephy_node_signal_disconnect_object (topics, EPHY_NODE_CHILD_CHANGED,
					    (EphyNodeCallback)node_changed_cb,
					    G_OBJECT (window));
	
	g_signal_handlers_disconnect_by_func
	  (G_OBJECT (eb), G_CALLBACK (tree_changed_cb), G_OBJECT (window));
	
	action = find_action (manager, "Bookmarks");
	g_signal_handlers_disconnect_by_func
	  (G_OBJECT (action), G_CALLBACK (activate_bookmarks_menu), G_OBJECT (window));
}

static void
properties_dialog_destroy_cb (EphyBookmarkProperties *dialog,
			      gpointer user_data)
{
	g_hash_table_remove (properties_dialogs,
			     ephy_bookmark_properties_get_node (dialog));
}

static void
add_bookmark (const char *location, 
	      const char *title)
{
	EphyBookmarks *bookmarks;
	EphyNode *bookmark;
	
	bookmarks = ephy_shell_get_bookmarks (ephy_shell_get_default ());
	bookmark = ephy_bookmarks_add (bookmarks, title, location);
	
	if (properties_dialogs == 0)
	{
		properties_dialogs = g_hash_table_new (g_direct_hash, g_direct_equal);
	}

	ephy_bookmarks_ui_show_bookmark (bookmark);
}

static void
duplicate_bookmark_response_cb (GtkWidget *dialog,
				int response,
				EphyNode *node)
{
	if (response == RESPONSE_SHOW_PROPERTIES)
	{
		ephy_bookmarks_ui_show_bookmark (node);
	}
        else if (response == RESPONSE_NEW_BOOKMARK)
	{
		const char *location;
		const char *title;

		location = g_object_get_data (G_OBJECT (dialog), "location");
		title = g_object_get_data (G_OBJECT (dialog), "title");

		add_bookmark (location, title);
	}

	gtk_widget_destroy (dialog);
}

static void
dialog_node_destroy_cb (EphyNode *node,
			GtkWidget *dialog)
{
	gtk_widget_destroy (dialog);
}

void
ephy_bookmarks_ui_add_bookmark (GtkWidget *parent,
				const char *location, 
				const char *title)
{
	EphyBookmarks *bookmarks;
	EphyNode *bookmark;
	
	bookmarks = ephy_shell_get_bookmarks (ephy_shell);
	bookmark = location ? ephy_bookmarks_find_bookmark (bookmarks, location) : NULL;
	
	if (bookmark != NULL)
	{
		GtkWidget *button, *dialog;

		dialog = gtk_message_dialog_new
			(GTK_WINDOW (parent),
			 GTK_DIALOG_DESTROY_WITH_PARENT,
			 GTK_MESSAGE_INFO, GTK_BUTTONS_NONE, 
			 _("Bookmark exists"));

		gtk_message_dialog_format_secondary_text
			(GTK_MESSAGE_DIALOG (dialog),
			 _("You already have a bookmark titled “%s” for this page."),
			 ephy_node_get_property_string (bookmark, EPHY_NODE_BMK_PROP_TITLE));

		if (parent != NULL)
		{
			gtk_window_group_add_window (ephy_gui_ensure_window_group (GTK_WINDOW (parent)),
						     GTK_WINDOW (dialog));
		}

		button = gtk_dialog_add_button (GTK_DIALOG (dialog),
						_("_Create New"),
						RESPONSE_NEW_BOOKMARK);
		gtk_button_set_image (GTK_BUTTON (button),
				      gtk_image_new_from_stock (STOCK_BOOKMARK,
								GTK_ICON_SIZE_BUTTON));

		button = gtk_dialog_add_button (GTK_DIALOG (dialog),
						_("_View Properties"),
						RESPONSE_SHOW_PROPERTIES);
		gtk_button_set_image (GTK_BUTTON (button), 
				      gtk_image_new_from_stock (GTK_STOCK_PROPERTIES,
						      		GTK_ICON_SIZE_BUTTON));

		gtk_dialog_add_button (GTK_DIALOG (dialog),
				       GTK_STOCK_OK,
				       GTK_RESPONSE_OK);
		gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);

		gtk_window_set_title (GTK_WINDOW (dialog), _("Bookmark Exists"));
		gtk_window_set_icon_name (GTK_WINDOW (dialog), "web-browser");

		g_object_set_data_full (G_OBJECT (dialog), "location",
					g_strdup (location), g_free);
		g_object_set_data_full (G_OBJECT (dialog), "title",
					g_strdup (title), g_free);

		g_signal_connect (dialog, "response",
			  	  G_CALLBACK (duplicate_bookmark_response_cb), bookmark);
		
		ephy_node_signal_connect_object (bookmark, EPHY_NODE_DESTROY,
						 (EphyNodeCallback) dialog_node_destroy_cb,
						 G_OBJECT (dialog));

		gtk_window_present_with_time (GTK_WINDOW (dialog),
					      gtk_get_current_event_time ());
	}
	else
	{
		add_bookmark (location, title);
	}
}

static EphyNode *
ephy_bookmarks_ui_find_topic (const char *name)
{
	EphyBookmarks *bookmarks;
	GPtrArray *children;
	EphyNode *node;
	int i;
	
	bookmarks = ephy_shell_get_bookmarks (ephy_shell);
	node = ephy_bookmarks_get_keywords (bookmarks);
	children = ephy_node_get_children (node);
	node = NULL;
	for (i = 0; i < children->len; i++)
	{
		const char *title;
		
		node = g_ptr_array_index (children, i);
		title = ephy_node_get_property_string (node, EPHY_NODE_KEYWORD_PROP_NAME);
		
		if (g_utf8_collate (title, name) == 0)
		{
			return node;
		}
	}
	
	return NULL;
}

static void
add_topic_changed_cb (GtkEntry *entry,
		      GtkDialog *dialog)
{
	const char *title;

	title = gtk_entry_get_text (entry);

	gtk_dialog_set_response_sensitive (dialog, GTK_RESPONSE_ACCEPT,
					   title[0] != '\0');
}

static void 
add_topic_response_cb (GtkWidget *dialog,
		       int response,
		       EphyNode *bookmark)
{
	EphyBookmarks *bookmarks;
	GtkEntry *entry;
	EphyNode *topic;
	const char *name;

	if (response != GTK_RESPONSE_ACCEPT)
	{
		gtk_widget_destroy (dialog);
		return;
	}

	entry = g_object_get_data (G_OBJECT (dialog), "name");
	name = gtk_entry_get_text (entry);
	g_return_if_fail (name != NULL && name[0] != '\0');
	
	topic = ephy_bookmarks_ui_find_topic (name);
	if (topic != NULL)
	{
		GtkWidget *message;

		message = gtk_message_dialog_new
			(GTK_WINDOW (dialog),
			 GTK_DIALOG_DESTROY_WITH_PARENT,
			 GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
			 _("You already have a topic named “%s”"),
			 ephy_node_get_property_string (topic, EPHY_NODE_KEYWORD_PROP_NAME));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
							  "%s",
							  _("Please use a different topic name."));

		g_signal_connect (message, "response",
				  G_CALLBACK (gtk_widget_destroy), NULL);

		gtk_window_group_add_window 
			(ephy_gui_ensure_window_group (GTK_WINDOW (dialog)),
						       GTK_WINDOW (message));

		gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
		gtk_window_present_with_time (GTK_WINDOW (message),
					      gtk_get_current_event_time ());
		
		return;
	}

	bookmarks = ephy_shell_get_bookmarks (ephy_shell);
	topic = ephy_bookmarks_add_keyword (bookmarks, name);
	ephy_bookmarks_set_keyword (bookmarks, topic, bookmark);				    
	
	gtk_widget_destroy (dialog);
}

void
ephy_bookmarks_ui_add_topic (GtkWidget *parent,
			     EphyNode *bookmark)
{
	GtkWidget *dialog, *hbox, *entry, *label;
	GtkContainer *container;
	GList *children;

	g_assert (parent != NULL);

	if (bookmark != NULL)
	{
		dialog = gtk_message_dialog_new
			(GTK_WINDOW (parent),
			 GTK_DIALOG_DESTROY_WITH_PARENT,
			 GTK_MESSAGE_QUESTION,
			 GTK_BUTTONS_NONE,
			 _("New topic for “%s”"),
			 ephy_node_get_property_string (bookmark, EPHY_NODE_BMK_PROP_TITLE));
	}
	else
	{
		dialog = gtk_message_dialog_new
			(GTK_WINDOW (parent),
			 GTK_DIALOG_DESTROY_WITH_PARENT,
			 GTK_MESSAGE_QUESTION,
			 GTK_BUTTONS_NONE,
			 _("New topic"));
	}
	
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  "%s",
						  _("Enter a unique name for the topic."));
			
	hbox = gtk_hbox_new (FALSE, 12);
	gtk_widget_show (hbox);
	
	entry = gtk_entry_new ();
	gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
	gtk_widget_show (entry);
	
	label = gtk_label_new_with_mnemonic ("_Name:");
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
	gtk_widget_show (label);
	
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
	
	/* Get the hbox which is the first child of the main vbox */
	children = gtk_container_get_children (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox));
	container = GTK_CONTAINER (children->data);
	g_list_free (children);

	/* Get the vbox which is the second child of the hbox */
	children = gtk_container_get_children (container);
	container = GTK_CONTAINER (children->next->data);
	g_list_free (children);
	gtk_box_pack_start (GTK_BOX (container), hbox, FALSE, FALSE, 0);

	g_object_set_data (G_OBJECT (dialog), "name", entry);

	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       _("Create"), GTK_RESPONSE_ACCEPT);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);

	gtk_window_set_title (GTK_WINDOW (dialog), _("New Topic"));
	gtk_window_set_icon_name (GTK_WINDOW (dialog), "web-browser");

	g_signal_connect (dialog, "response",
			  G_CALLBACK (add_topic_response_cb), bookmark);

	add_topic_changed_cb (GTK_ENTRY (entry), GTK_DIALOG (dialog));
	g_signal_connect (entry, "changed",
			  G_CALLBACK (add_topic_changed_cb), dialog);

	ephy_node_signal_connect_object (bookmark, EPHY_NODE_DESTROY,
					 (EphyNodeCallback) dialog_node_destroy_cb,
					 G_OBJECT (dialog));

	gtk_window_group_add_window 
		(ephy_gui_ensure_window_group (GTK_WINDOW (parent)),
		 GTK_WINDOW (dialog));

	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_window_present_with_time (GTK_WINDOW (dialog),
				      gtk_get_current_event_time ());
}

void
ephy_bookmarks_ui_show_bookmark (EphyNode *bookmark)
{
	EphyBookmarks *bookmarks;
	GtkWidget *dialog;

	bookmarks = ephy_shell_get_bookmarks (ephy_shell_get_default ());

	g_return_if_fail (EPHY_IS_BOOKMARKS (bookmarks));
	g_return_if_fail (EPHY_IS_NODE (bookmark));

	if (properties_dialogs == 0)
	{
		properties_dialogs = g_hash_table_new (g_direct_hash, g_direct_equal);
	}
	
	dialog = g_hash_table_lookup (properties_dialogs, bookmark);

	if (dialog == NULL)
	{
		dialog = ephy_bookmark_properties_new (bookmarks, bookmark, FALSE);

		g_signal_connect (dialog, "destroy",
				  G_CALLBACK (properties_dialog_destroy_cb), bookmarks);
		g_hash_table_insert (properties_dialogs,
				     bookmark, dialog);
	}

	gtk_window_present_with_time (GTK_WINDOW (dialog),
				      gtk_get_current_event_time ());
}

/* Below this line we have functions relating to toolbar code */

static EggToolbarsItemType bookmark_type;
static EggToolbarsItemType topic_type;
static EphyBookmarks *eb;

static gboolean
topic_has_data (EggToolbarsItemType *type,
		const char *name)
{
	EphyNode *node, *topics;
	guint node_id;
	
	if (sscanf (name, "OpenTopic%u", &node_id) != 1 &&
	    sscanf (name, "Tpc%u", &node_id) != 1) return FALSE;
	node = ephy_bookmarks_get_from_id (eb, node_id);
	if (!node) return FALSE;
	topics = ephy_bookmarks_get_keywords (eb);
	return ephy_node_has_child (topics, node);
}

static char *
topic_get_data (EggToolbarsItemType *type,
		const char *name)
{
	EphyNode *node;
	guint node_id;
	
	if (sscanf (name, "OpenTopic%u", &node_id) != 1 &&
	    sscanf (name, "Tpc%u", &node_id) != 1) return NULL;
	node = ephy_bookmarks_get_from_id (eb, node_id);
	if (!node) return NULL;
	return ephy_bookmarks_get_topic_uri (eb, node);
}

static char *
topic_get_name (EggToolbarsItemType *type,
		const char *name)
{
	EphyNode *topic = ephy_bookmarks_find_keyword (eb, name, FALSE);
	if (topic == NULL) return NULL;
	return ephy_topic_action_name (topic);
}


static gboolean
bookmark_has_data (EggToolbarsItemType *type,
		   const char *name)
{
	EphyNode *node;
	guint node_id;
	
	if (sscanf (name, "OpenBmk%u", &node_id) != 1 &&
	    sscanf (name, "Bmk%u", &node_id) != 1) return FALSE;
	node = ephy_bookmarks_get_from_id (eb, node_id);
	if (!node) return FALSE;
	
	return (ephy_node_get_property_string (node, EPHY_NODE_BMK_PROP_LOCATION) != NULL);
}

static char *
bookmark_get_data (EggToolbarsItemType *type,
		   const char *name)
{
	EphyNode *node;
	guint node_id;
	
	if (sscanf (name, "OpenBmk%u", &node_id) != 1 &&
	    sscanf (name, "Bmk%u", &node_id) != 1) return NULL;
	node = ephy_bookmarks_get_from_id (eb, node_id);
	if (!node) return NULL;
	
	return g_strdup (ephy_node_get_property_string (node, EPHY_NODE_BMK_PROP_LOCATION));
}

static char *
bookmark_get_name (EggToolbarsItemType *type,
		   const char *data)
{
	EphyNode *node;
	gchar **netscape_url;

	netscape_url = g_strsplit (data, "\n", 2);
	if (!netscape_url || !netscape_url[0])
	{
		g_strfreev (netscape_url);
		return NULL;
	}

	node = ephy_bookmarks_find_bookmark (eb, netscape_url[0]);
	g_strfreev (netscape_url);

	if (!node) return NULL;
	return ephy_bookmark_action_name (node);
}

static char *
bookmark_new_name (EggToolbarsItemType *type,
		   const char *data)
{
	EphyNode *node;
	gchar **netscape_url;

	netscape_url = g_strsplit (data, "\n", 2);
	if (!netscape_url || !netscape_url[0])
	{
		g_strfreev (netscape_url);
		return NULL;
	}

	node = ephy_bookmarks_add (eb, netscape_url[1], netscape_url[0]);

	g_strfreev (netscape_url);

	return ephy_bookmark_action_name (node);
}

static void
toolbar_node_removed_cb (EphyNode *parent, EphyNode *child, guint index, EggToolbarsModel *model)
{
	gint i, j;
	char *nid = NULL;
	const char *id;
	
	switch (ephy_node_get_id (parent))
	{
		case BOOKMARKS_NODE_ID:
			nid = ephy_bookmark_action_name (child);
			break;
		case KEYWORDS_NODE_ID:
			nid = ephy_topic_action_name (child);
			break;
	 	default:
			return;
	}

	for (i=egg_toolbars_model_n_toolbars(model)-1; i>=0; i--)
	  for (j=egg_toolbars_model_n_items(model, i)-1; j>=0; j--)
	  {
		  id = egg_toolbars_model_item_nth (model, i, j);
		  if (!strcmp (id, nid))
		  {
			  egg_toolbars_model_remove_item (model, i, j);
		  }
	  }
	
	free (nid);
}

void
ephy_bookmarks_ui_attach_toolbar_model (EggToolbarsModel *model)
{
	EphyNode *bookmarks;
	EphyNode *topics;
	GList *types;

	eb = ephy_shell_get_bookmarks (ephy_shell);        
	bookmarks = ephy_bookmarks_get_bookmarks (eb);
	topics = ephy_bookmarks_get_keywords (eb);
	types = egg_toolbars_model_get_types (model);

	topic_type.type = gdk_atom_intern (EPHY_DND_TOPIC_TYPE, TRUE);
	topic_type.has_data = topic_has_data;
	topic_type.get_data = topic_get_data;
	topic_type.new_name = NULL;
	topic_type.get_name = topic_get_name;

	bookmark_type.type = gdk_atom_intern (EPHY_DND_URL_TYPE, TRUE);
	bookmark_type.has_data = bookmark_has_data;
	bookmark_type.get_data = bookmark_get_data;
	bookmark_type.new_name = bookmark_new_name;
	bookmark_type.get_name = bookmark_get_name;

	types = g_list_prepend (types, &bookmark_type);
	types = g_list_prepend (types, &topic_type);
	egg_toolbars_model_set_types (model, types);

	ephy_node_signal_connect_object (bookmarks, EPHY_NODE_CHILD_REMOVED,
					 (EphyNodeCallback)toolbar_node_removed_cb,
					 G_OBJECT (model));
	ephy_node_signal_connect_object (topics, EPHY_NODE_CHILD_REMOVED,
					 (EphyNodeCallback)toolbar_node_removed_cb,
					 G_OBJECT (model));

	egg_toolbars_model_set_n_avail (model, "AddTopicToToolbar", G_MAXINT);
	egg_toolbars_model_set_n_avail (model, "AddBookmarkToToolbar", G_MAXINT);
	egg_toolbars_model_set_n_avail (model, "RelatedTopic", 1);
}


void
ephy_bookmarks_ui_detach_toolbar_model (EggToolbarsModel *model)
{
	EphyBookmarks *eb = ephy_shell_get_bookmarks (ephy_shell);        
	EphyNode *bookmarks = ephy_bookmarks_get_bookmarks (eb);
	EphyNode *topics = ephy_bookmarks_get_keywords (eb);
	
	ephy_node_signal_disconnect_object (bookmarks, EPHY_NODE_CHILD_REMOVED,
					    (EphyNodeCallback)toolbar_node_removed_cb,
					    G_OBJECT (model));
	ephy_node_signal_disconnect_object (topics, EPHY_NODE_CHILD_REMOVED,
					    (EphyNodeCallback)toolbar_node_removed_cb,
					    G_OBJECT (model));
}
