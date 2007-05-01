/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 *  Copyright © 2002 Jorn Baayen <jorn@nl.linux.org>
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
 *  $Id$
 */

#include "config.h"

#include "ephy-completion-model.h"
#include "ephy-favicon-cache.h"
#include "ephy-node.h"
#include "ephy-shell.h"
#include "ephy-history.h"

static void ephy_completion_model_class_init (EphyCompletionModelClass *klass);
static void ephy_completion_model_init (EphyCompletionModel *model);
static void ephy_completion_model_tree_model_init (GtkTreeModelIface *iface);

#define EPHY_COMPLETION_MODEL_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_COMPLETION_MODEL, EphyCompletionModelPrivate))

struct _EphyCompletionModelPrivate
{
	EphyNode *history;
	EphyNode *bookmarks;
	int stamp;
};

enum
{
	HISTORY_GROUP,
	BOOKMARKS_GROUP
};

static GObjectClass *parent_class = NULL;

GType
ephy_completion_model_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		const GTypeInfo our_info =
		{
			sizeof (EphyCompletionModelClass),
			NULL,
			NULL,
			(GClassInitFunc) ephy_completion_model_class_init,
			NULL,
			NULL,
			sizeof (EphyCompletionModel),
			0,
			(GInstanceInitFunc) ephy_completion_model_init
		};

		const GInterfaceInfo tree_model_info =
		{
			(GInterfaceInitFunc) ephy_completion_model_tree_model_init,
			NULL,
			NULL
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "EphyCompletionModel",
					       &our_info, 0);

		g_type_add_interface_static (type,
					     GTK_TYPE_TREE_MODEL,
					     &tree_model_info);
	}

	return type;
}

static void
ephy_completion_model_class_init (EphyCompletionModelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	g_type_class_add_private (object_class, sizeof (EphyCompletionModelPrivate));
}

static GtkTreePath *
get_path_real (EphyCompletionModel *model,
	       EphyNode *root,
	       EphyNode *child)
{
	GtkTreePath *retval;
	int index;

	retval = gtk_tree_path_new ();
	index = ephy_node_get_child_index (root, child);

	if (root == model->priv->bookmarks)
	{
		index += ephy_node_get_n_children (model->priv->history);
	}

	gtk_tree_path_append_index (retval, index);

	return retval;
}

static void
node_iter_from_node (EphyCompletionModel *model,
		     EphyNode *root,
		     EphyNode *child,
		     GtkTreeIter *iter)
{
	iter->stamp = model->priv->stamp;
	iter->user_data = child;
	iter->user_data2 = root;
}

static EphyNode *
get_index_root (EphyCompletionModel *model, int *index)
{
	int children;

	children = ephy_node_get_n_children (model->priv->history);

	if (*index >= children)
	{
		*index = *index - children;

		if (*index < ephy_node_get_n_children (model->priv->bookmarks))
		{
			return model->priv->bookmarks;
		}
		else
		{
			return NULL;
		}
	}
	else
	{
		return model->priv->history;
	}
}

static void
root_child_removed_cb (EphyNode *node,
		       EphyNode *child,
		       guint old_index,
		       EphyCompletionModel *model)
{
	GtkTreePath *path;
	guint index;

	path = gtk_tree_path_new ();

	index = old_index;
	if (node == model->priv->bookmarks)
	{
		index += ephy_node_get_n_children (model->priv->history);
	}
	gtk_tree_path_append_index (path, index);

	gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);
	gtk_tree_path_free (path);
}

static void
root_child_added_cb (EphyNode *node,
		     EphyNode *child,
		     EphyCompletionModel *model)
{
	GtkTreePath *path;
	GtkTreeIter iter;

	node_iter_from_node (model, node, child, &iter);

	path = get_path_real (model, node, child);
	gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), path, &iter);
	gtk_tree_path_free (path);
}

static void
root_child_changed_cb (EphyNode *node,
		       EphyNode *child,
		       guint property_id,
		       EphyCompletionModel *model)
{
	GtkTreePath *path;
	GtkTreeIter iter;

	node_iter_from_node (model, node, child, &iter);

	path = get_path_real (model, node, child);
	gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
	gtk_tree_path_free (path);
}

static void
connect_signals (EphyCompletionModel *model, EphyNode *root)
{
	ephy_node_signal_connect_object (root,
			                 EPHY_NODE_CHILD_ADDED,
			                 (EphyNodeCallback)root_child_added_cb,
			                 G_OBJECT (model));
	ephy_node_signal_connect_object (root,
			                 EPHY_NODE_CHILD_REMOVED,
			                 (EphyNodeCallback)root_child_removed_cb,
			                 G_OBJECT (model));
	ephy_node_signal_connect_object (root,
			                 EPHY_NODE_CHILD_CHANGED,
			                 (EphyNodeCallback)root_child_changed_cb,
			                 G_OBJECT (model));
}

static void
ephy_completion_model_init (EphyCompletionModel *model)
{
	EphyBookmarks *bookmarks;
	EphyHistory *history;

	model->priv = EPHY_COMPLETION_MODEL_GET_PRIVATE (model);
	model->priv->stamp = g_random_int ();

	history = EPHY_HISTORY (ephy_embed_shell_get_global_history (embed_shell));
	model->priv->history = ephy_history_get_pages (history);
	connect_signals (model, model->priv->history);

	bookmarks = ephy_shell_get_bookmarks (ephy_shell);
	model->priv->bookmarks = ephy_bookmarks_get_bookmarks (bookmarks);
	connect_signals (model, model->priv->bookmarks);
}

EphyCompletionModel *
ephy_completion_model_new (void)
{
	EphyCompletionModel *model;

	model = EPHY_COMPLETION_MODEL (g_object_new (EPHY_TYPE_COMPLETION_MODEL,
						    NULL));

	g_return_val_if_fail (model->priv != NULL, NULL);

	return model;
}

static int
ephy_completion_model_get_n_columns (GtkTreeModel *tree_model)
{
	return N_COL;
}

static GType
ephy_completion_model_get_column_type (GtkTreeModel *tree_model,
			               int index)
{
	GType type = 0;

	switch (index)
	{
		case EPHY_COMPLETION_TEXT_COL:
		case EPHY_COMPLETION_ACTION_COL:
		case EPHY_COMPLETION_KEYWORDS_COL:
		case EPHY_COMPLETION_EXTRA_COL:
			type =  G_TYPE_STRING;
			break;
		case EPHY_COMPLETION_FAVICON_COL:
			type = GDK_TYPE_PIXBUF;
			break;
		case EPHY_COMPLETION_RELEVANCE_COL:
			type = G_TYPE_INT;
			break;
	}

	return type;
}

static void
init_text_col (GValue *value, EphyNode *node, int group)
{
	const char *text;

	switch (group)
	{
		case BOOKMARKS_GROUP:
			text = ephy_node_get_property_string
				(node, EPHY_NODE_BMK_PROP_TITLE);
			break;
		case HISTORY_GROUP:
			text = ephy_node_get_property_string
				(node, EPHY_NODE_PAGE_PROP_LOCATION);
			break;

		default:
			text = "";
	}
	
	g_value_set_string (value, text);
}

static void
init_action_col (GValue *value, EphyNode *node, int group)
{
	const char *text;

	switch (group)
	{
		case BOOKMARKS_GROUP:
			text = ephy_node_get_property_string
				(node, EPHY_NODE_BMK_PROP_LOCATION);
			break;
		case HISTORY_GROUP:
			text = ephy_node_get_property_string
				(node, EPHY_NODE_PAGE_PROP_LOCATION);
			break;
		default:
			text = "";
	}
	
	g_value_set_string (value, text);
}

static void
init_keywords_col (GValue *value, EphyNode *node, int group)
{
	const char *text = NULL;

	switch (group)
	{
		case BOOKMARKS_GROUP:
			text = ephy_node_get_property_string
				(node, EPHY_NODE_BMK_PROP_KEYWORDS);
			break;
	}

	if (text == NULL)
	{
		text = "";
	}
	
	g_value_set_string (value, text);
}
static void
init_favicon_col (GValue *value, EphyNode *node, int group)
{
	EphyFaviconCache *cache;
	const char *icon_location;
	GdkPixbuf *pixbuf = NULL;

	cache = EPHY_FAVICON_CACHE
		(ephy_embed_shell_get_favicon_cache (EPHY_EMBED_SHELL (ephy_shell)));

	switch (group)
	{
		case BOOKMARKS_GROUP:
			icon_location = ephy_node_get_property_string
				(node, EPHY_NODE_BMK_PROP_ICON);
			break;
		case HISTORY_GROUP:
			icon_location = ephy_node_get_property_string
				(node, EPHY_NODE_PAGE_PROP_ICON);
			break;
		default:
			icon_location = NULL;
	}
	
	if (icon_location)
	{
		pixbuf = ephy_favicon_cache_get (cache, icon_location);
	}

	g_value_take_object (value, pixbuf);
}

static gboolean
is_base_address (const char *address)
{
	int slashes = 0;

	if (address == NULL) return FALSE;

	while (*address != '\0')
	{
		if (*address == '/') slashes++;

		address++;

		/* Base uris has 3 slashes like http://www.gnome.org/ */
		if (slashes == 3)
		{
			return (*address == '\0');
		}
	}

	return FALSE;
}

static void
init_relevance_col (GValue *value, EphyNode *node, int group)
{
	int relevance = 0;

	/* We have three ordered groups: history's base
	   addresses, bookmarks, deep history addresses */

	if (group == BOOKMARKS_GROUP)
	{
		relevance = 1 << 5;
	}
	else if (group == HISTORY_GROUP)
	{
		const char *address;
		int visits;
	
		visits = ephy_node_get_property_int
			(node, EPHY_NODE_PAGE_PROP_VISITS);
		address = ephy_node_get_property_string
			(node, EPHY_NODE_PAGE_PROP_LOCATION);

		visits = MIN (visits, (1 << 5) - 1);

		if (is_base_address (address))
		{
			relevance = visits << 10;
		}
		else
		{
			relevance = visits;
		}
	}
	
	g_value_set_int (value, relevance);
}

static void
init_url_col (GValue *value, EphyNode *node, int group)
{
        const char *url = NULL;

	if (group == BOOKMARKS_GROUP)
	{
	        url = ephy_node_get_property_string
		  (node, EPHY_NODE_BMK_PROP_LOCATION);
	}
	else if (group == HISTORY_GROUP)
	{
	        url = ephy_node_get_property_string
		  (node, EPHY_NODE_PAGE_PROP_LOCATION);
	}
	else
	{
	        url = "";
	}
	
	g_value_set_string (value, url);
}

static void
ephy_completion_model_get_value (GtkTreeModel *tree_model,
			         GtkTreeIter *iter,
			         int column,
			         GValue *value)
{
	int group;
	EphyCompletionModel *model = EPHY_COMPLETION_MODEL (tree_model);
	EphyNode *node;

	g_return_if_fail (EPHY_IS_COMPLETION_MODEL (tree_model));
	g_return_if_fail (iter != NULL);
	g_return_if_fail (iter->stamp == model->priv->stamp);

	node = iter->user_data;
	group = (iter->user_data2 == model->priv->history) ?
		HISTORY_GROUP : BOOKMARKS_GROUP;

	switch (column)
	{
		case EPHY_COMPLETION_EXTRA_COL:
			g_value_init (value, G_TYPE_STRING);
			/* We set an additional text for the item title only for history, since we assume that people know the url of their bookmarks */
			if (group == HISTORY_GROUP)
			{
				const char *text;
				text = ephy_node_get_property_string
					(node, EPHY_NODE_PAGE_PROP_TITLE);
				g_value_set_string (value, text);
			}
			break;
		case EPHY_COMPLETION_TEXT_COL:
			g_value_init (value, G_TYPE_STRING);
			init_text_col (value, node, group);
			break;
		case EPHY_COMPLETION_FAVICON_COL:
			g_value_init (value, GDK_TYPE_PIXBUF);
			init_favicon_col (value, node, group);
 			break;
		case EPHY_COMPLETION_ACTION_COL:
			g_value_init (value, G_TYPE_STRING);
			init_action_col (value, node, group);
			break;
		case EPHY_COMPLETION_KEYWORDS_COL:
			g_value_init (value, G_TYPE_STRING);
			init_keywords_col (value, node, group);
			break;
		case EPHY_COMPLETION_RELEVANCE_COL:
			g_value_init (value, G_TYPE_INT);
			init_relevance_col (value, node, group);
			break;
                case EPHY_COMPLETION_URL_COL:
                        g_value_init (value, G_TYPE_STRING);
                        init_url_col (value, node, group);
                        break;
	}
}

static GtkTreeModelFlags
ephy_completion_model_get_flags (GtkTreeModel *tree_model)
{
	return GTK_TREE_MODEL_ITERS_PERSIST | GTK_TREE_MODEL_LIST_ONLY;
}

static gboolean
ephy_completion_model_get_iter (GtkTreeModel *tree_model,
			        GtkTreeIter *iter,
			        GtkTreePath *path)
{
	EphyCompletionModel *model = EPHY_COMPLETION_MODEL (tree_model);
	EphyNode *root, *child;
	int i;

	g_return_val_if_fail (EPHY_IS_COMPLETION_MODEL (model), FALSE);
	g_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

	i = gtk_tree_path_get_indices (path)[0];

	root = get_index_root (model, &i);
	if (root == NULL) return FALSE;

	child = ephy_node_get_nth_child (root, i);
	g_return_val_if_fail (child != NULL, FALSE);

	node_iter_from_node (model, root, child, iter);

	return TRUE;
}

static GtkTreePath *
ephy_completion_model_get_path (GtkTreeModel *tree_model,
			        GtkTreeIter *iter)
{
	EphyCompletionModel *model = EPHY_COMPLETION_MODEL (tree_model);

	g_return_val_if_fail (iter != NULL, NULL);
	g_return_val_if_fail (iter->user_data != NULL, NULL);
	g_return_val_if_fail (iter->user_data2 != NULL, NULL);
	g_return_val_if_fail (iter->stamp == model->priv->stamp, NULL);

	return get_path_real (model, iter->user_data2, iter->user_data);
}

static gboolean
ephy_completion_model_iter_next (GtkTreeModel *tree_model,
			         GtkTreeIter *iter)
{
	EphyCompletionModel *model = EPHY_COMPLETION_MODEL (tree_model);
	EphyNode *node, *next, *root;

	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (iter->user_data != NULL, FALSE);
	g_return_val_if_fail (iter->user_data2 != NULL, FALSE);
	g_return_val_if_fail (iter->stamp == model->priv->stamp, FALSE);

	node = iter->user_data;
	root = iter->user_data2;

	next = ephy_node_get_next_child (root, node);

	if (next == NULL && root == model->priv->history)
	{
		root = model->priv->bookmarks;
		next = ephy_node_get_nth_child (model->priv->bookmarks, 0);
	}

	if (next == NULL) return FALSE;

	node_iter_from_node (model, root, next, iter);
	
	return TRUE;
}

static gboolean
ephy_completion_model_iter_children (GtkTreeModel *tree_model,
			             GtkTreeIter *iter,
			             GtkTreeIter *parent)
{
	EphyCompletionModel *model = EPHY_COMPLETION_MODEL (tree_model);
	EphyNode *root, *first_node;

	if (parent != NULL)
	{
		return FALSE;
	}

	root = model->priv->history;
	first_node = ephy_node_get_nth_child (root, 0);

	if (first_node == NULL)
	{
		root = model->priv->bookmarks;
		first_node = ephy_node_get_nth_child (root, 0);
	}

	if (first_node == NULL)
	{
		return FALSE;
	}

	node_iter_from_node (model, root, first_node, iter);

	return TRUE;
}

static gboolean
ephy_completion_model_iter_has_child (GtkTreeModel *tree_model,
			             GtkTreeIter *iter)
{
	return FALSE;
}

static int
ephy_completion_model_iter_n_children (GtkTreeModel *tree_model,
			               GtkTreeIter *iter)
{
	EphyCompletionModel *model = EPHY_COMPLETION_MODEL (tree_model);

	g_return_val_if_fail (EPHY_IS_COMPLETION_MODEL (tree_model), -1);

	if (iter == NULL)
	{
		return ephy_node_get_n_children (model->priv->history) +
		       ephy_node_get_n_children (model->priv->bookmarks);
	}

	g_return_val_if_fail (model->priv->stamp == iter->stamp, -1);

	return 0;
}

static gboolean
ephy_completion_model_iter_nth_child (GtkTreeModel *tree_model,
			              GtkTreeIter *iter,
			              GtkTreeIter *parent,
			              int n)
{
	EphyCompletionModel *model = EPHY_COMPLETION_MODEL (tree_model);
	EphyNode *node, *root;

	g_return_val_if_fail (EPHY_IS_COMPLETION_MODEL (tree_model), FALSE);

	if (parent != NULL)
	{
		return FALSE;
	}

	root = get_index_root (model, &n);
	node = ephy_node_get_nth_child (root, n);

	if (node == NULL) return FALSE;

	node_iter_from_node (model, root, node, iter);

	return TRUE;
}

static gboolean
ephy_completion_model_iter_parent (GtkTreeModel *tree_model,
			           GtkTreeIter *iter,
			           GtkTreeIter *child)
{
	return FALSE;
}

static void
ephy_completion_model_tree_model_init (GtkTreeModelIface *iface)
{
	iface->get_flags       = ephy_completion_model_get_flags;
	iface->get_iter        = ephy_completion_model_get_iter;
	iface->get_path        = ephy_completion_model_get_path;
	iface->iter_next       = ephy_completion_model_iter_next;
	iface->iter_children   = ephy_completion_model_iter_children;
	iface->iter_has_child  = ephy_completion_model_iter_has_child;
	iface->iter_n_children = ephy_completion_model_iter_n_children;
	iface->iter_nth_child  = ephy_completion_model_iter_nth_child;
	iface->iter_parent     = ephy_completion_model_iter_parent;
	iface->get_n_columns   = ephy_completion_model_get_n_columns;
	iface->get_column_type = ephy_completion_model_get_column_type;
	iface->get_value       = ephy_completion_model_get_value;
}
