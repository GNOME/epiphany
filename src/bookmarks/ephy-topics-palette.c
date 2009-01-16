/*
 *  Copyright © 2002-2004 Marco Pesenti Gritti <mpeseng@tin.it>
 *  Copyright © 2005 Peter Harvey <pah06@uow.edu.au>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#include "ephy-topics-palette.h"
#include "ephy-nodes-cover.h"
#include "ephy-node-common.h"
#include "ephy-bookmarks.h"
#include "ephy-debug.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

static void ephy_topics_palette_class_init (EphyTopicsPaletteClass *klass);
static void ephy_topics_palette_init (EphyTopicsPalette *editor);

#define EPHY_TOPICS_PALETTE_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_TOPICS_PALETTE, EphyTopicsPalettePrivate))

struct _EphyTopicsPalettePrivate
{
	EphyBookmarks *bookmarks;
	EphyNode *bookmark;
	GtkListStore *store;
	GtkTreeViewColumn *column;
	int mode;
};

enum
{
	PROP_0,
	PROP_BOOKMARKS,
	PROP_BOOKMARK,
	PROP_MODE
};

enum
{
	COLUMN_TITLE,
        COLUMN_NODE,
	COLUMN_SELECTED,
	COLUMNS
};

enum
{
	MODE_GROUPED,
	MODE_LIST,
	MODES
};

G_DEFINE_TYPE (EphyTopicsPalette, ephy_topics_palette, GTK_TYPE_TREE_VIEW)

static void
append_topics (EphyTopicsPalette *palette,
	       GtkTreeIter *iter,
	       gboolean *valid,
	       gboolean *first,
	       GPtrArray *topics)
{
	EphyNode *node;
	const char *title;
	gint i;
	
	if (topics->len == 0)
	{
		return;
	}
	 
	if (!*first)
	{
		if (!*valid) gtk_list_store_append (palette->priv->store, iter);
		gtk_list_store_set (palette->priv->store, iter, COLUMN_TITLE, NULL,
				    COLUMN_NODE, NULL, -1);
		*valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (palette->priv->store), iter);
	}

	for (i = 0; i < topics->len ; i++)
	{
		node = g_ptr_array_index (topics, i);
		title = ephy_node_get_property_string (node, EPHY_NODE_KEYWORD_PROP_NAME);
		
		if (!*valid) gtk_list_store_append (palette->priv->store, iter);
		gtk_list_store_set (palette->priv->store, iter, COLUMN_TITLE, title,
				    COLUMN_NODE, node, COLUMN_SELECTED,
				    ephy_node_has_child (node, palette->priv->bookmark), -1);
		*valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (palette->priv->store), iter);
		*first = FALSE;
	}
}

static void
update_list (EphyTopicsPalette *palette)
{
	GPtrArray *children, *bookmarks, *topics;
	EphyNode *node;
	GtkTreeIter iter;
	gint i, priority;
	gboolean valid, first;
	
	gtk_widget_queue_draw (GTK_WIDGET (palette));
	valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (palette->priv->store), &iter);
	first = TRUE;
	
	if (palette->priv->mode == MODE_LIST)
	{
		/* Allocate and fill the suggestions array. */
		node = ephy_bookmarks_get_keywords (palette->priv->bookmarks);
		children = ephy_node_get_children (node);
		topics = g_ptr_array_sized_new (children->len);
		for (i = 0; i < children->len; i++)
		{
			node = g_ptr_array_index (children, i);
			
			priority = ephy_node_get_property_int (node, EPHY_NODE_KEYWORD_PROP_PRIORITY);
			if (priority != EPHY_NODE_NORMAL_PRIORITY)
			  continue;

			g_ptr_array_add (topics, node);
		}

		g_ptr_array_sort (topics, ephy_bookmarks_compare_topic_pointers);		
		append_topics (palette, &iter, &valid, &first, topics);
		g_ptr_array_free (topics, TRUE);
	}
	else if (palette->priv->mode == MODE_GROUPED)
	{
		GPtrArray *suggested, *selected;
		
		/* Allocate and fill the bookmarks array. */
		node = ephy_bookmarks_get_bookmarks (palette->priv->bookmarks);
		children = ephy_node_get_children (node);
		bookmarks = g_ptr_array_sized_new (children->len);
		for (i = 0; i < children->len; i++)
		{
			g_ptr_array_add(bookmarks, g_ptr_array_index (children, i));
		}
		
		/* Allocate and fill the topics array. */
		node = ephy_bookmarks_get_keywords (palette->priv->bookmarks);
		children = ephy_node_get_children (node);
		topics = g_ptr_array_sized_new (children->len);
		suggested = g_ptr_array_sized_new (children->len);
		selected = g_ptr_array_sized_new (children->len);
		for (i = 0; i < children->len; i++)
		{
			node = g_ptr_array_index (children, i);
			
			priority = ephy_node_get_property_int (node, EPHY_NODE_KEYWORD_PROP_PRIORITY);
			if (priority != EPHY_NODE_NORMAL_PRIORITY)
			  continue;
			
			/* We'll consider only bookmarks covered by the same topics as our bookmark. */
			if (ephy_node_has_child (node, palette->priv->bookmark))
			{
				ephy_nodes_remove_not_covered (node, bookmarks);
				g_ptr_array_add (selected, node);
			}
			
			/* We'll onsider only topics that are not already selected for our bookmark. */
			else
			{
				g_ptr_array_add (topics, node);
			}
		}
	
		/* Get the minimum cover of topics for the bookmarks. */
		suggested = ephy_nodes_get_covering (topics, bookmarks, suggested, 0, 0);
		
		for (i = 0; i < suggested->len; i++)
		{
			g_ptr_array_remove_fast (topics, g_ptr_array_index (suggested, i));
		}
		
		/* Add any topics which cover the bookmarks completely in their own right, or 
		   have no bookmarks currently associated with it. */
		for (i = 0; i < topics->len ; i++)
		{
			node = g_ptr_array_index (topics, i);
			if (!ephy_node_has_child (node, palette->priv->bookmark) &&
			     ephy_nodes_covered (node, bookmarks))
			{
				g_ptr_array_add (suggested, node);
				g_ptr_array_remove_index_fast (topics, i);
				i--;
			}
		}
	

		g_ptr_array_sort (selected, ephy_bookmarks_compare_topic_pointers);
		g_ptr_array_sort (suggested, ephy_bookmarks_compare_topic_pointers);
		g_ptr_array_sort (topics, ephy_bookmarks_compare_topic_pointers);
		append_topics (palette, &iter, &valid, &first, selected);
		append_topics (palette, &iter, &valid, &first, suggested);
		append_topics (palette, &iter, &valid, &first, topics);
		g_ptr_array_free (selected, TRUE);
		g_ptr_array_free (suggested, TRUE);
		g_ptr_array_free (bookmarks, TRUE);
		g_ptr_array_free (topics, TRUE);
	}
	
	while (valid)
	{
		valid = gtk_list_store_remove (palette->priv->store, &iter);
	}
}

static void
tree_changed_cb (EphyBookmarks *bookmarks,
		 EphyTopicsPalette *palette)
{
	update_list (palette);
}

static void
node_added_cb (EphyNode *parent,
	       EphyNode *child,
	       EphyTopicsPalette *palette)
{
	update_list (palette);
}

static void
node_changed_cb (EphyNode *parent,
		 EphyNode *child,
		 guint property_id,
		 EphyTopicsPalette *palette)
{
	update_list (palette);
}

static void
node_removed_cb (EphyNode *parent,
		 EphyNode *child,
		 guint index,
		 EphyTopicsPalette *palette)
{
	update_list (palette);
}

static void
ephy_topics_palette_set_property (GObject *object,
		                   guint prop_id,
		                   const GValue *value,
		                   GParamSpec *pspec)
{
	EphyTopicsPalette *palette = EPHY_TOPICS_PALETTE (object);
	EphyNode *node;

	switch (prop_id)
	{
	case PROP_BOOKMARKS:
		palette->priv->bookmarks = g_value_get_object (value);
		node = ephy_bookmarks_get_keywords (palette->priv->bookmarks);
		ephy_node_signal_connect_object (node, EPHY_NODE_CHILD_ADDED,
				   (EphyNodeCallback) node_added_cb, object);
		ephy_node_signal_connect_object (node, EPHY_NODE_CHILD_CHANGED,
				   (EphyNodeCallback) node_changed_cb, object);
		ephy_node_signal_connect_object (node, EPHY_NODE_CHILD_REMOVED,
				   (EphyNodeCallback) node_removed_cb, object);
		g_signal_connect_object (palette->priv->bookmarks, "tree-changed",
					 G_CALLBACK (tree_changed_cb), palette,
					 G_CONNECT_AFTER);
		break;
	case PROP_BOOKMARK:
		palette->priv->bookmark = g_value_get_pointer (value);
		break;
	case PROP_MODE:
		palette->priv->mode = g_value_get_int (value);
		update_list (palette);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
cell_edited (GtkCellRendererText *renderer,
	     const char *path_str,
	     const char *new_text,
	     EphyTopicsPalette *palette)
{
	if (*new_text != 0)
	{
		EphyNode *node;
		node = ephy_bookmarks_add_keyword (palette->priv->bookmarks, new_text);
		ephy_bookmarks_set_keyword (palette->priv->bookmarks, node,
					    palette->priv->bookmark);
	}
	else
	{
		update_list (palette);
	}
}

static void
toggled (GtkCellRendererToggle *cell_renderer,
	 gchar *path,
	 EphyTopicsPalette *palette)
{
	EphyNode *topic;
	GtkTreeModel *model;
	GtkTreeIter iter;
	
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (palette));

	g_return_if_fail(gtk_tree_model_get_iter_from_string (model, &iter, path));

	gtk_tree_model_get (model, &iter, COLUMN_NODE, &topic, -1);
	
	/* Need to protect against toggling separators. */
	if (topic == NULL) return;
	
	if (ephy_node_has_child (topic, palette->priv->bookmark))
	{
		ephy_bookmarks_unset_keyword (palette->priv->bookmarks,
					      topic,
					      palette->priv->bookmark);
	}
	else
	{
		ephy_bookmarks_set_keyword (palette->priv->bookmarks,
					    topic,
					    palette->priv->bookmark);
	}
}

static gboolean
is_separator (GtkTreeModel *model,
	      GtkTreeIter *iter,
	      gpointer data)
{
	EphyNode *node;
	gtk_tree_model_get (model, iter, COLUMN_NODE, &node, -1);
	return (node == NULL);
}

static GObject *
ephy_topics_palette_constructor (GType type,
				 guint n_construct_properties,
				 GObjectConstructParam *construct_params)

{
	GObject *object;
	EphyTopicsPalette *palette;
	EphyTopicsPalettePrivate *priv;
	GtkCellRenderer *renderer;

	object = G_OBJECT_CLASS (ephy_topics_palette_parent_class)->constructor (type,
                                                                                 n_construct_properties,
                                                                                 construct_params);
	palette = EPHY_TOPICS_PALETTE (object);
	priv = EPHY_TOPICS_PALETTE_GET_PRIVATE (object);

	priv->store = gtk_list_store_new (COLUMNS, G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_BOOLEAN);
	gtk_tree_view_set_model (GTK_TREE_VIEW (object), GTK_TREE_MODEL (priv->store));
	g_object_unref (priv->store);

	priv->column = gtk_tree_view_column_new ();
	
	renderer = gtk_cell_renderer_toggle_new ();
	gtk_tree_view_column_pack_start (priv->column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (priv->column, renderer, "active", COLUMN_SELECTED);
	g_signal_connect (renderer, "toggled", G_CALLBACK (toggled), palette);
	
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (priv->column, renderer, TRUE);
	gtk_tree_view_column_add_attribute (priv->column, renderer, "text", COLUMN_TITLE);
	g_signal_connect (renderer, "edited", G_CALLBACK (cell_edited), palette);

	gtk_tree_view_append_column (GTK_TREE_VIEW (object), priv->column);

	gtk_tree_view_set_row_separator_func (GTK_TREE_VIEW (object), is_separator, NULL, NULL);
	gtk_tree_view_set_enable_search (GTK_TREE_VIEW (object), TRUE);
	gtk_tree_view_set_search_column (GTK_TREE_VIEW (object), COLUMN_TITLE);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (object), FALSE);
	gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (object)), GTK_SELECTION_NONE);
	
	update_list (palette);
    
	return object;
}

static void
ephy_topics_palette_init (EphyTopicsPalette *palette)
{
	palette->priv = EPHY_TOPICS_PALETTE_GET_PRIVATE (palette);
}

GtkWidget *
ephy_topics_palette_new (EphyBookmarks *bookmarks,
			 EphyNode *bookmark)
{
	EphyTopicsPalette *palette;

	g_assert (bookmarks != NULL);

	palette = EPHY_TOPICS_PALETTE (g_object_new
				       (EPHY_TYPE_TOPICS_PALETTE,
					"bookmarks", bookmarks,
					"bookmark", bookmark,
					NULL));

	return GTK_WIDGET (palette);
}

static void
ephy_topics_palette_class_init (EphyTopicsPaletteClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	object_class->set_property = ephy_topics_palette_set_property;
	object_class->constructor = ephy_topics_palette_constructor;
	
	g_object_class_install_property (object_class,
					 PROP_BOOKMARKS,
					 g_param_spec_object ("bookmarks",
							      "Bookmarks set",
							      "Bookmarks set",
							      EPHY_TYPE_BOOKMARKS,
							      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
							      G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | 
							      G_PARAM_STATIC_BLURB));

	g_object_class_install_property (object_class,
					 PROP_BOOKMARK,
					 g_param_spec_pointer ("bookmark",
							       "Bookmark",
							       "Bookmark",
							       G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
							       G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | 
							       G_PARAM_STATIC_BLURB));

	g_object_class_install_property (object_class,
					 PROP_MODE,
					 g_param_spec_int ("mode",
							   "Mode",
							   "Mode",
							   0, MODES-1, 0, 
							   G_PARAM_WRITABLE |
							   G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
							   G_PARAM_STATIC_BLURB));

	g_type_class_add_private (object_class, sizeof(EphyTopicsPalettePrivate));
}
