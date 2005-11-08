
/*
 *  Copyright (C) 2002-2004 Marco Pesenti Gritti <mpeseng@tin.it>
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
 *  $Id$
 */

#include "config.h"

#include "ephy-topics-selector.h"
#include "ephy-debug.h"

#include <glib/gi18n.h>
#include <gtk/gtktreeselection.h>

static void ephy_topics_selector_class_init (EphyTopicsSelectorClass *klass);
static void ephy_topics_selector_init (EphyTopicsSelector *editor);

#define EPHY_TOPICS_SELECTOR_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_TOPICS_SELECTOR, EphyTopicsSelectorPrivate))

struct _EphyTopicsSelectorPrivate
{
	EphyBookmarks *bookmarks;
	EphyNode *bookmark;
	EphyNodeFilter *filter;
	GList *topics;
};

enum
{
	PROP_0,
	PROP_BOOKMARKS,
	PROP_BOOKMARK
};

static GObjectClass *parent_class = NULL;

GType
ephy_topics_selector_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
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

		type = g_type_register_static (EPHY_TYPE_NODE_VIEW,
					       "EphyTopicsSelector",
					       &our_info, 0);
	}

	return type;
}

static void
ephy_topics_selector_set_bookmark (EphyTopicsSelector *selector,
				   EphyNode *bookmark)
{
	LOG ("Set bookmark");

	selector->priv->bookmark = bookmark;

	g_object_notify (G_OBJECT (selector), "bookmark");
}

static void
ephy_topics_selector_set_bookmarks (EphyTopicsSelector *selector,
				    EphyBookmarks *bookmarks)
{
	selector->priv->bookmarks = bookmarks;
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
		ephy_topics_selector_set_bookmarks
			(selector, g_value_get_object (value));
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

void
ephy_topics_selector_apply (EphyTopicsSelector *selector,
			    EphyNode *bookmark)
{
	GList *l;

	for (l = selector->priv->topics; l != NULL; l = l->next)
	{
		EphyNode *node = l->data;

		ephy_bookmarks_set_keyword (selector->priv->bookmarks,
					    node, bookmark);
	}
}

static void
provide_toggle (EphyNode *node,
		GValue *value,
		gpointer data)
{
	EphyTopicsSelector *selector = EPHY_TOPICS_SELECTOR (data);
	gboolean result = FALSE;

	g_value_init (value, G_TYPE_BOOLEAN);

	if (selector->priv->bookmark)
	{
		result = ephy_node_has_child (node, selector->priv->bookmark);
	}
	else
	{
		result = (g_list_find (selector->priv->topics, node) != NULL);
	}

	g_value_set_boolean (value, result);
}

static GObject *
ephy_topics_selector_constructor (GType type,
				  guint n_construct_properties,
                                  GObjectConstructParam *construct_params)

{
	GObject *object;
	EphyTopicsSelector *selector;
	EphyTopicsSelectorPrivate *priv;
	GtkTreeSelection *selection;

	object = parent_class->constructor (type, n_construct_properties,
                                            construct_params);
	selector = EPHY_TOPICS_SELECTOR (object);
	priv = EPHY_TOPICS_SELECTOR_GET_PRIVATE (object);

	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (selector), FALSE);

	ephy_node_view_add_toggle (EPHY_NODE_VIEW (selector),
				   provide_toggle, selector);
	ephy_node_view_add_column (EPHY_NODE_VIEW (selector), _("Topics"),
				   G_TYPE_STRING,
				   EPHY_NODE_KEYWORD_PROP_NAME,
				   EPHY_NODE_VIEW_SHOW_PRIORITY |
				   EPHY_NODE_VIEW_EDITABLE |
				   EPHY_NODE_VIEW_SEARCHABLE, NULL, NULL);
	ephy_node_view_set_sort (EPHY_NODE_VIEW (selector), G_TYPE_STRING,
				 EPHY_NODE_KEYWORD_PROP_NAME, GTK_SORT_ASCENDING);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (selector));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);

	return object;
}

static void
topic_destroy_cb (EphyNode *node,
		  EphyTopicsSelector *selector)
{
	selector->priv->topics = g_list_remove
				(selector->priv->topics, node);
}

static void
toggle_topic (EphyTopicsSelector *selector, EphyNode *node, gboolean checked)
{
	if (selector->priv->bookmark)
	{
		if (checked)
		{
			ephy_bookmarks_set_keyword (selector->priv->bookmarks,
						    node,
						    selector->priv->bookmark);
		}
		else
		{
			ephy_bookmarks_unset_keyword (selector->priv->bookmarks,
						      node,
						      selector->priv->bookmark);
		}
	}
	else
	{
		if (checked)
		{
			selector->priv->topics = g_list_prepend
					(selector->priv->topics, node);
			ephy_node_signal_connect_object (node, EPHY_NODE_DESTROY,
				 			 (EphyNodeCallback) topic_destroy_cb,
							 G_OBJECT (selector));
		}
		else
		{
			selector->priv->topics = g_list_remove
					(selector->priv->topics, node);
		}
	}
}

static void
node_toggled_cb (EphyTopicsSelector *selector, EphyNode *node,
		 gboolean checked, gpointer data)
{
	toggle_topic (selector, node, checked);
}

static void
ephy_topics_selector_init (EphyTopicsSelector *selector)
{
	selector->priv = EPHY_TOPICS_SELECTOR_GET_PRIVATE (selector);

	selector->priv->filter = ephy_node_filter_new ();
	ephy_node_filter_add_expression (selector->priv->filter,
				         ephy_node_filter_expression_new (EPHY_NODE_FILTER_EXPRESSION_INT_PROP_EQUALS,
								          EPHY_NODE_KEYWORD_PROP_PRIORITY,
									  EPHY_NODE_VIEW_NORMAL_PRIORITY),
				         0);
	g_object_set (selector, "filter", selector->priv->filter, NULL);

	g_signal_connect (selector, "node_toggled",
			  G_CALLBACK (node_toggled_cb), NULL);
}

static void
ephy_topics_selector_finalize (GObject *object)
{
	EphyTopicsSelector *selector = EPHY_TOPICS_SELECTOR (object);

	g_list_free (selector->priv->topics);

	g_object_unref (selector->priv->filter);

	parent_class->finalize (object);
}

GtkWidget *
ephy_topics_selector_new (EphyBookmarks *bookmarks,
			  EphyNode *bookmark)
{
	EphyTopicsSelector *editor;
	EphyNode *root;

	g_assert (bookmarks != NULL);

	root = ephy_bookmarks_get_keywords (bookmarks);
	editor = EPHY_TOPICS_SELECTOR (g_object_new
			(EPHY_TYPE_TOPICS_SELECTOR,
			 "bookmarks", bookmarks,
			 "bookmark", bookmark,
			 "root", root,
			 NULL));

	return GTK_WIDGET (editor);
}

void
ephy_topics_selector_new_topic (EphyTopicsSelector *selector)
{
	EphyNode *node;

	node = ephy_bookmarks_add_keyword
			(selector->priv->bookmarks, _("Type a topic"));
	toggle_topic (selector, node, TRUE);
	ephy_node_view_select_node (EPHY_NODE_VIEW (selector), node);
	ephy_node_view_edit (EPHY_NODE_VIEW (selector), TRUE);
}

static void
ephy_topics_selector_class_init (EphyTopicsSelectorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->set_property = ephy_topics_selector_set_property;
	object_class->get_property = ephy_topics_selector_get_property;
	object_class->constructor = ephy_topics_selector_constructor;
	object_class->finalize = ephy_topics_selector_finalize;

	g_object_class_install_property (object_class,
					 PROP_BOOKMARKS,
					 g_param_spec_object ("bookmarks",
							      "Bookmarks set",
							      "Bookmarks set",
							      EPHY_TYPE_BOOKMARKS,
							      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class,
					 PROP_BOOKMARK,
					 g_param_spec_pointer ("bookmark",
							       "Bookmark",
							       "Bookmark",
							       G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class, sizeof(EphyTopicsSelectorPrivate));
}
