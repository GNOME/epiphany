/*
 *  Copyright (C) 2002 Marco Pesenti Gritti <mpeseng@tin.it>
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "ephy-topics-selector.h"
#include "ephy-debug.h"
#include "ephy-node-view.h"

#include <gtk/gtkliststore.h>
#include <gtk/gtkcellrenderertoggle.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtkwindow.h>
#include <gdk/gdkkeysyms.h>

static void ephy_topics_selector_class_init (EphyTopicsSelectorClass *klass);
static void ephy_topics_selector_init (EphyTopicsSelector *editor);
static void ephy_topics_selector_set_property (GObject *object,
		                               guint prop_id,
		                               const GValue *value,
		                               GParamSpec *pspec);
static void ephy_topics_selector_get_property (GObject *object,
		                               guint prop_id,
		                               GValue *value,
		                               GParamSpec *pspec);

#define EPHY_TOPICS_SELECTOR_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_TOPICS_SELECTOR, EphyTopicsSelectorPrivate))

struct EphyTopicsSelectorPrivate
{
	EphyBookmarks *bookmarks;
	GtkTreeModel *model;
	EphyNode *bookmark;
};

enum
{
	PROP_0,
	PROP_BOOKMARKS,
	PROP_BOOKMARK
};

enum
{
	COL_HAS_TOPIC,
	COL_TOPIC,
	COL_NODE
};

static GObjectClass *parent_class = NULL;

GType
ephy_topics_selector_get_type (void)
{
	static GType ephy_topics_selector_type = 0;

	if (ephy_topics_selector_type == 0)
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyTopicsSelectorClass),
			NULL,
			NULL,
			(GClassInitFunc) ephy_topics_selector_class_init,
			NULL,
			NULL,
			sizeof (EphyTopicsSelector),
			0,
			(GInstanceInitFunc) ephy_topics_selector_init
		};

		ephy_topics_selector_type = g_type_register_static (GTK_TYPE_TREE_VIEW,
							            "EphyTopicsSelector",
							            &our_info, 0);
	}

	return ephy_topics_selector_type;
}

static void
ephy_topics_selector_class_init (EphyTopicsSelectorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->set_property = ephy_topics_selector_set_property;
	object_class->get_property = ephy_topics_selector_get_property;

	g_object_class_install_property (object_class,
					 PROP_BOOKMARKS,
					 g_param_spec_object ("bookmarks",
							      "Bookmarks set",
							      "Bookmarks set",
							      EPHY_TYPE_BOOKMARKS,
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class,
					 PROP_BOOKMARK,
					 g_param_spec_pointer ("bookmark",
							       "Bookmark",
							       "Bookmark",
							       G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof(EphyTopicsSelectorPrivate));
}

void
ephy_topics_selector_set_bookmark (EphyTopicsSelector *selector,
				   EphyNode *bookmark)
{
	LOG ("Set bookmark")

	selector->priv->bookmark = bookmark;

	g_object_notify (G_OBJECT (selector), "bookmark");
}

static void
ephy_topics_selector_set_property (GObject *object,
		                   guint prop_id,
		                   const GValue *value,
		                   GParamSpec *pspec)
{
	EphyTopicsSelector *selector = EPHY_TOPICS_SELECTOR (object);

	switch (prop_id)
	{
	case PROP_BOOKMARKS:
		selector->priv->bookmarks = g_value_get_object (value);
		break;
	case PROP_BOOKMARK:
		ephy_topics_selector_set_bookmark
			(selector, g_value_get_pointer (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
ephy_topics_selector_get_property (GObject *object,
		                   guint prop_id,
		                   GValue *value,
		                   GParamSpec *pspec)
{
	EphyTopicsSelector *selector = EPHY_TOPICS_SELECTOR (object);

	switch (prop_id)
	{
	case PROP_BOOKMARK:
		g_value_set_pointer (value, selector);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
fill_model (EphyTopicsSelector *editor)
{
	GPtrArray *children;
	int i;
	EphyNode *keywords;
	GtkListStore *model = GTK_LIST_STORE (editor->priv->model);

	keywords = ephy_bookmarks_get_keywords (editor->priv->bookmarks);

	children = ephy_node_get_children (keywords);
	for (i = 0; i < children->len; i++)
	{
		EphyNode *kid;
		const char *name;
		gboolean has_keyword = FALSE;
		int priority;
		GtkTreeIter iter;

		kid = g_ptr_array_index (children, i);

		name = ephy_node_get_property_string
			(kid, EPHY_NODE_KEYWORD_PROP_NAME);

		if (editor->priv->bookmark != NULL)
		{
			has_keyword = ephy_bookmarks_has_keyword
				(editor->priv->bookmarks, kid,
				 editor->priv->bookmark);
		}

		priority = ephy_node_get_property_int
			(kid, EPHY_NODE_KEYWORD_PROP_PRIORITY);
		if (priority == -1) priority = EPHY_NODE_VIEW_NORMAL_PRIORITY;

		if (priority == EPHY_NODE_VIEW_NORMAL_PRIORITY)
		{
			gtk_list_store_append (model, &iter);
			gtk_list_store_set (model, &iter,
					    COL_HAS_TOPIC, has_keyword,
					    COL_TOPIC, name,
					    COL_NODE, kid,
					    -1);
		}
	}
}

static void
topic_toggled (GtkTreePath *path,
               EphyTopicsSelector *selector)
{
	GtkTreeModel *model = selector->priv->model;
	GtkTreeIter iter;
	gboolean has_topic;

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter, COL_HAS_TOPIC, &has_topic, -1);
	has_topic = !has_topic;

	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			    COL_HAS_TOPIC, has_topic, -1);
	ephy_topics_selector_apply (selector);
}

void
ephy_topics_selector_apply (EphyTopicsSelector *editor)
{
	GtkTreeIter iter;
	GtkTreeModel *model = editor->priv->model;

	LOG ("Update topics")

	if (editor->priv->bookmark == NULL) return;

	if (!gtk_tree_model_get_iter_first (model, &iter))
	{
		return;
	}

	do
	{
		GValue value = { 0, };
		gboolean has_topic;
		EphyNode *node;

		gtk_tree_model_get_value (model, &iter, COL_HAS_TOPIC, &value);
		has_topic = g_value_get_boolean (&value);
		g_value_unset (&value);

		gtk_tree_model_get_value (model, &iter, COL_NODE, &value);
		node = g_value_get_pointer (&value);
		g_value_unset (&value);

		if (has_topic)
		{
			ephy_bookmarks_set_keyword (editor->priv->bookmarks,
						    node,
						    editor->priv->bookmark);
		}
		else
		{
			ephy_bookmarks_unset_keyword (editor->priv->bookmarks,
						      node,
						      editor->priv->bookmark);
		}
	}
	while (gtk_tree_model_iter_next (model, &iter));
}

static gboolean
topic_clicked (GtkTreeView *tree_view,
	       GdkEventButton *event,
	       EphyTopicsSelector *selector)
{
	GtkTreePath *path = NULL;

	if (event->window != gtk_tree_view_get_bin_window (tree_view))
		return FALSE;

	if (gtk_tree_view_get_path_at_pos (tree_view,
					   (gint) event->x,
					   (gint) event->y,
					   &path, NULL,
					   NULL, NULL))
	{
		topic_toggled (path, selector);

		gtk_tree_path_free (path);
	}

	return FALSE;
}

static gboolean
topic_key_pressed (GtkTreeView *tree_view,
		   GdkEventKey *event,
		   EphyTopicsSelector *selector)
{
	GtkTreeSelection *sel = NULL;
	GtkTreeIter iter;
	GtkTreePath *path;

	switch (event->keyval)
	{
	case GDK_space:
	case GDK_Return:
	case GDK_KP_Enter:
		sel = gtk_tree_view_get_selection (tree_view);

		if (gtk_tree_selection_get_selected (sel, NULL, &iter))
		{
			path = gtk_tree_model_get_path (selector->priv->model, &iter);

			topic_toggled (path, selector);

			gtk_tree_path_free (path);
		}
		return TRUE;

	default:
		break;
	}

	return FALSE;
}

static void
ephy_topics_build_ui (EphyTopicsSelector *editor)
{
	GtkListStore *model;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	model = gtk_list_store_new (3, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_POINTER);
	editor->priv->model = GTK_TREE_MODEL (model);

	gtk_tree_view_set_model (GTK_TREE_VIEW (editor), GTK_TREE_MODEL (model));
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (editor), FALSE);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model),
					      COL_TOPIC,
					      GTK_SORT_ASCENDING);
	g_object_unref (model);

	/* Has topic column */
	renderer = gtk_cell_renderer_toggle_new ();
	column = gtk_tree_view_column_new_with_attributes
		("", renderer, "active", COL_HAS_TOPIC, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (editor), column);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes
		("Description", renderer, "text", COL_TOPIC, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (editor), column);

	g_signal_connect (G_OBJECT (editor), "key_press_event",
			  G_CALLBACK (topic_key_pressed), editor);
	g_signal_connect (G_OBJECT (editor), "button_press_event",
			  G_CALLBACK (topic_clicked), editor);
	fill_model (editor);
}

static void
ephy_topics_selector_init (EphyTopicsSelector *editor)
{
	editor->priv = EPHY_TOPICS_SELECTOR_GET_PRIVATE (editor);

	editor->priv->bookmark = NULL;
}

GtkWidget *
ephy_topics_selector_new (EphyBookmarks *bookmarks,
			  EphyNode *bookmark)
{
	EphyTopicsSelector *editor;

	g_assert (bookmarks != NULL);

	editor = EPHY_TOPICS_SELECTOR (g_object_new
			(EPHY_TYPE_TOPICS_SELECTOR,
			 "bookmarks", bookmarks,
			 "bookmark", bookmark,
			 NULL));

	ephy_topics_build_ui (editor);

	return GTK_WIDGET (editor);
}
