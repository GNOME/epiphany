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
#include "ephy-node-common.h"
#include "ephy-link.h"
#include "ephy-dnd.h"
#include "ephy-history.h"
#include "ephy-shell.h"
#include "ephy-string.h"
#include "ephy-debug.h"
#include "ephy-file-helpers.h"

#include <string.h>
#include <glib/gi18n.h>

#define BM_WINDOW_DATA_KEY "bookmarks-window-data"

typedef struct
{
	guint bookmarks_menu;
	guint favorites_menu;
} BookmarksWindowData;



static GString * bookmarks_menu_string = 0;
static GString * favorites_menu_string = 0;

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
activate_favorites_menu (GtkAction *action, EphyWindow *window)
{
	BookmarksWindowData *data = g_object_get_data (G_OBJECT (window), BM_WINDOW_DATA_KEY);
	if (data && !data->favorites_menu)
	{
		GtkUIManager *manager = GTK_UI_MANAGER (ephy_window_get_ui_manager (window));
		gtk_ui_manager_ensure_update (manager);
		
		if (!favorites_menu_string->len)
		{
			EphyBookmarks *eb = ephy_shell_get_bookmarks (ephy_shell);
			EphyNode *favorites = ephy_bookmarks_get_favorites (eb);
		
			g_string_append (favorites_menu_string,
					 "<ui><menubar><menu name=\"GoMenu\" action=\"Go\">");
			ephy_bookmarks_menu_build (favorites_menu_string, favorites);
			g_string_append (favorites_menu_string, "</menu></menubar></ui>");
		}
	
		data->favorites_menu = gtk_ui_manager_add_ui_from_string
		  (manager, favorites_menu_string->str, favorites_menu_string->len, 0);
		
		gtk_ui_manager_ensure_update (manager);
	}
}

static void
erase_bookmarks_menu (EphyWindow *window)
{
	BookmarksWindowData *data = g_object_get_data (G_OBJECT (window), BM_WINDOW_DATA_KEY);
	GtkUIManager *manager = GTK_UI_MANAGER (ephy_window_get_ui_manager (window));
    
	if (data != NULL && data->bookmarks_menu != 0)
	{
		gtk_ui_manager_remove_ui (manager, data->bookmarks_menu);
		data->bookmarks_menu = 0;
	}
	g_string_truncate (bookmarks_menu_string, 0);
}

static void
erase_favorites_menu (EphyWindow *window)
{
	BookmarksWindowData *data = g_object_get_data (G_OBJECT (window), BM_WINDOW_DATA_KEY);
	GtkUIManager *manager = GTK_UI_MANAGER (ephy_window_get_ui_manager (window));
    
	if (data != NULL && data->favorites_menu != 0)
	{
		gtk_ui_manager_remove_ui (manager, data->favorites_menu);
		data->favorites_menu = 0;
	}
	g_string_truncate (favorites_menu_string, 0);
}

static void
tree_changed_cb (EphyBookmarks *bookmarks, EphyWindow *window)
{
	erase_bookmarks_menu (window);
}

static void
node_added_cb (EphyNode *parent, EphyNode *child, EphyWindow *window)
{
	erase_bookmarks_menu (window);
	erase_favorites_menu (window);
}

static void
node_changed_cb (EphyNode *parent, EphyNode *child, guint property_id, EphyWindow *window)
{
	if (property_id == EPHY_NODE_KEYWORD_PROP_NAME || property_id == EPHY_NODE_BMK_PROP_TITLE)
	{
		erase_bookmarks_menu (window);
		erase_favorites_menu (window);
	}
}

static void
node_removed_cb (EphyNode *parent, EphyNode *child, guint index, EphyWindow *window)
{
	erase_bookmarks_menu (window);
	erase_favorites_menu (window);
}

void
ephy_bookmarks_ui_attach_window (EphyWindow *window)
{
	EphyBookmarks *eb = ephy_shell_get_bookmarks (ephy_shell);
	EphyNode *bookmarks = ephy_bookmarks_get_bookmarks (eb);
	EphyNode *topics = ephy_bookmarks_get_keywords (eb);
	EphyNode *favorites = ephy_bookmarks_get_favorites (eb);
	
	BookmarksWindowData *data = g_object_get_data (G_OBJECT (window), BM_WINDOW_DATA_KEY);
	GtkUIManager *manager = GTK_UI_MANAGER (ephy_window_get_ui_manager (window));
	GtkActionGroup *actions;
	GtkAction *action;

	g_return_if_fail (data == 0);
	data = g_new0 (BookmarksWindowData, 1);
	g_object_set_data_full (G_OBJECT (window), BM_WINDOW_DATA_KEY, data, g_free);

	actions = ephy_bookmark_group_new (bookmarks);
	gtk_ui_manager_insert_action_group (manager, actions, 0);
	g_signal_connect_swapped (G_OBJECT (actions), "open-link",
				  G_CALLBACK (ephy_link_open), G_OBJECT (window));
	g_object_unref (G_OBJECT (actions));
	
	actions = ephy_topic_group_new (topics, manager);
	gtk_ui_manager_insert_action_group (manager, actions, 0);
	g_object_unref (G_OBJECT (actions));

	actions = ephy_open_tabs_group_new (topics);
	gtk_ui_manager_insert_action_group (manager, actions, 0);
	g_signal_connect_swapped (G_OBJECT (actions), "open-link",
				  G_CALLBACK (ephy_link_open), G_OBJECT (window));
	g_object_unref (G_OBJECT (actions));
	
	actions = gtk_action_group_new ("BookmarkToolbarActions");
	
	action = ephy_topic_factory_action_new ("AddTopicToToolbar");
	gtk_action_group_add_action (actions, action);
	g_object_unref (action);
	
	action = ephy_bookmark_factory_action_new ("AddBookmarkToToolbar");
	gtk_action_group_add_action (actions, action);
	g_object_unref (action);
	
	action = ephy_related_action_new (EPHY_LINK (window), manager, "RelatedTopic");
	gtk_action_group_add_action (actions, action);
	g_object_unref (action);
	
	gtk_ui_manager_insert_action_group (manager, actions, 0);
	g_object_unref (G_OBJECT (actions));
	

	ephy_node_signal_connect_object (bookmarks, EPHY_NODE_CHILD_ADDED,
					 (EphyNodeCallback)node_added_cb,
					 G_OBJECT (window));
	ephy_node_signal_connect_object (topics, EPHY_NODE_CHILD_ADDED,
					 (EphyNodeCallback)node_added_cb,
					 G_OBJECT (window));
	ephy_node_signal_connect_object (favorites, EPHY_NODE_CHILD_ADDED,
					 (EphyNodeCallback)node_added_cb,
					 G_OBJECT (window));
	
	ephy_node_signal_connect_object (bookmarks, EPHY_NODE_CHILD_REMOVED,
					 (EphyNodeCallback)node_removed_cb,
					 G_OBJECT (window));
	ephy_node_signal_connect_object (topics, EPHY_NODE_CHILD_REMOVED,
					 (EphyNodeCallback)node_removed_cb,
					 G_OBJECT (window));
	ephy_node_signal_connect_object (favorites, EPHY_NODE_CHILD_REMOVED,
					 (EphyNodeCallback)node_removed_cb,
					 G_OBJECT (window));
	
	ephy_node_signal_connect_object (bookmarks, EPHY_NODE_CHILD_CHANGED,
					 (EphyNodeCallback)node_changed_cb,
					 G_OBJECT (window));        
	ephy_node_signal_connect_object (topics, EPHY_NODE_CHILD_CHANGED,
					 (EphyNodeCallback)node_changed_cb,
					 G_OBJECT (window));

	g_signal_connect_object (G_OBJECT (eb), "tree_changed",
				 G_CALLBACK (tree_changed_cb),
				 G_OBJECT (window), 0);
    
	/* Build menus on demand. */
	if (!favorites_menu_string) favorites_menu_string = g_string_new ("");
	if (!bookmarks_menu_string) bookmarks_menu_string = g_string_new ("");
	action = find_action (manager, "Bookmarks");
	g_signal_connect_object (G_OBJECT (action), "activate",
				 G_CALLBACK (activate_bookmarks_menu),
				 G_OBJECT (window), 0);
	action = find_action (manager, "Go");
	g_signal_connect_object (G_OBJECT (action), "activate",
				 G_CALLBACK (activate_favorites_menu),
				 G_OBJECT (window), 0);
}

void
ephy_bookmarks_ui_detach_window (EphyWindow *window)
{
	EphyBookmarks *eb = ephy_shell_get_bookmarks (ephy_shell);
	EphyNode *bookmarks = ephy_bookmarks_get_bookmarks (eb);
	EphyNode *topics = ephy_bookmarks_get_keywords (eb);
	EphyNode *favorites = ephy_bookmarks_get_favorites (eb);

	BookmarksWindowData *data = g_object_get_data (G_OBJECT (window), BM_WINDOW_DATA_KEY);
	GtkUIManager *manager = GTK_UI_MANAGER (ephy_window_get_ui_manager (window));
	GtkAction *action;

	g_return_if_fail (data != 0);
	if (data->bookmarks_menu) gtk_ui_manager_remove_ui (manager, data->bookmarks_menu);
	if (data->favorites_menu) gtk_ui_manager_remove_ui (manager, data->favorites_menu);
	g_object_set_data (G_OBJECT (window), BM_WINDOW_DATA_KEY, 0);
	
	ephy_node_signal_disconnect_object (bookmarks, EPHY_NODE_CHILD_ADDED,
					    (EphyNodeCallback)node_added_cb,
					    G_OBJECT (window));
	ephy_node_signal_disconnect_object (topics, EPHY_NODE_CHILD_ADDED,
					    (EphyNodeCallback)node_added_cb,
					    G_OBJECT (window));
	ephy_node_signal_disconnect_object (favorites, EPHY_NODE_CHILD_ADDED,
					    (EphyNodeCallback)node_added_cb,
					    G_OBJECT (window));
	
	ephy_node_signal_disconnect_object (bookmarks, EPHY_NODE_CHILD_REMOVED,
					    (EphyNodeCallback)node_removed_cb,
					    G_OBJECT (window));
	ephy_node_signal_disconnect_object (topics, EPHY_NODE_CHILD_REMOVED,
					    (EphyNodeCallback)node_removed_cb,
					    G_OBJECT (window));
	ephy_node_signal_disconnect_object (favorites, EPHY_NODE_CHILD_REMOVED,
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
	
	action = find_action (manager, "Go");
	g_signal_handlers_disconnect_by_func
	  (G_OBJECT (action), G_CALLBACK (activate_favorites_menu), G_OBJECT (window));
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
	
	if (sscanf (name, "OpenTopic%u", &node_id) != 1 ||
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
	
	if (sscanf (name, "OpenTopic%u", &node_id) != 1 ||
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
	
	if (sscanf (name, "OpenBmk%u", &node_id) != 1 ||
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
	
	if (sscanf (name, "OpenBmk%u", &node_id) != 1 ||
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
	EphyHistory *gh;
	EphyNode *node;
	gchar **netscape_url;
	const char *icon;
	const char *title;

	netscape_url = g_strsplit (data, "\n", 2);
	if (!netscape_url || !netscape_url[0])
	{
		g_strfreev (netscape_url);
		return NULL;
	}

	title = netscape_url[1];
	if (title == NULL || title[0] == '\0')
	{
		title = _("Untitled");
	}

	node = ephy_bookmarks_add (eb, title, netscape_url[0]);

	if (node != NULL)
	{
		gh = EPHY_HISTORY (ephy_embed_shell_get_global_history (embed_shell));
		icon = ephy_history_get_icon (gh, netscape_url[0]);

		if (icon)
		{
			ephy_bookmarks_set_icon (eb, netscape_url[0], icon);
		}
	}

	g_strfreev (netscape_url);

	return ephy_bookmark_action_name (node);
}

void
ephy_bookmarks_ui_attach_toolbar_model (EggToolbarsModel *model)
{
  	eb = ephy_shell_get_bookmarks (ephy_shell);        
	EphyNode *bookmarks = ephy_bookmarks_get_bookmarks (eb);
	EphyNode *topics = ephy_bookmarks_get_keywords (eb);
	GList *types = egg_toolbars_model_get_types (model);

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
