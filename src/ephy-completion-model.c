/* 
 *  Copyright (C) 2002 Jorn Baayen <jorn@nl.linux.org>
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

#include <config.h>

#include "ephy-completion-model.h"
#include "ephy-node.h"
#include "ephy-shell.h"

static void ephy_completion_model_class_init (EphyCompletionModelClass *klass);
static void ephy_completion_model_init (EphyCompletionModel *model);
static void ephy_completion_model_tree_model_init (GtkTreeModelIface *iface);

#define EPHY_COMPLETION_MODEL_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_COMPLETION_MODEL, EphyCompletionModelPrivate))

struct EphyCompletionModelPrivate
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
	static GType ephy_completion_model_type = 0;

	if (ephy_completion_model_type == 0)
	{
		static const GTypeInfo our_info =
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

		static const GInterfaceInfo tree_model_info =
		{
			(GInterfaceInitFunc) ephy_completion_model_tree_model_init,
			NULL,
			NULL
		};

		ephy_completion_model_type = g_type_register_static (G_TYPE_OBJECT,
								     "EphyCompletionModel",
								     &our_info, 0);

		g_type_add_interface_static (ephy_completion_model_type,
					     GTK_TYPE_TREE_MODEL,
					     &tree_model_info);
	}

	return ephy_completion_model_type;
}

static void
ephy_completion_model_class_init (EphyCompletionModelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	g_type_class_add_private (object_class, sizeof (EphyCompletionModelPrivate));
}

static void
root_child_removed_cb (EphyNode *node,
		       EphyNode *child,
		       guint old_index,
		       EphyCompletionModel *tree_model)
{
	EphyCompletionModel *model = EPHY_COMPLETION_MODEL (tree_model);
	GtkTreePath *path;
	int real_index;

	real_index = old_index;

	if (node == model->priv->bookmarks)
	{
		real_index += ephy_node_get_n_children (model->priv->history);
	}

	path = gtk_tree_path_new ();
	gtk_tree_path_append_index (path, real_index);
	gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);
	gtk_tree_path_free (path);
}

static void
node_iter_from_node (EphyCompletionModel *model,
		     EphyNode *node,
		     GtkTreeIter *iter)
{
	iter->stamp = model->priv->stamp;
	iter->user_data = node;
}

static inline GtkTreePath *
get_path_real (EphyCompletionModel *model,
	       EphyNode *node)
{
	GtkTreePath *retval;
	int index;

	retval = gtk_tree_path_new ();

	index = ephy_node_get_child_index (model->priv->bookmarks, node);
	if (index < 0)
	{
		index = ephy_node_get_child_index (model->priv->history, node);
	}

	g_return_val_if_fail (index >= 0, NULL);

	gtk_tree_path_append_index (retval, index);

	return retval;
}

static void
root_child_added_cb (EphyNode *node,
		     EphyNode *child,
		     EphyCompletionModel *model)
{
	GtkTreePath *path;
	GtkTreeIter iter;

	node_iter_from_node (model, child, &iter);

	path = get_path_real (model, child);
	gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), path, &iter);
	gtk_tree_path_free (path);
}

static void
root_child_changed_cb (EphyNode *node,
		       EphyNode *child,
		       EphyCompletionModel *model)
{
	GtkTreePath *path;
	GtkTreeIter iter;

	node_iter_from_node (model, node, &iter);

	path = get_path_real (model, child);
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

	history = ephy_embed_shell_get_global_history
		(EPHY_EMBED_SHELL (ephy_shell));
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
			type =  G_TYPE_STRING;
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
				(node, EPHY_NODE_PAGE_PROP_TITLE);
			break;
		case HISTORY_GROUP:
			text = ephy_node_get_property_string
				(node, EPHY_NODE_BMK_PROP_LOCATION);
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
init_relevance_col (GValue *value, EphyNode *node, int group)
{
	int relevance, visits;

	switch (group)
	{
		case HISTORY_GROUP:
			visits = ephy_node_get_property_int
				(node, EPHY_NODE_PAGE_PROP_VISITS);
			relevance = visits;
			break;
		case BOOKMARKS_GROUP:
			relevance = 2000;
			break;
		default:
			relevance = 0;
	}
	
	g_value_set_int (value, relevance);
}

static EphyNode *
get_node_root (EphyCompletionModel *model, EphyNode *node)
{
	if (ephy_node_has_child (model->priv->bookmarks, node))
	{
		return model->priv->bookmarks;
	}
	else
	{
		return model->priv->history;
	}
}

static int
get_node_group (EphyCompletionModel *model, EphyNode *node)
{
	if (ephy_node_has_child (model->priv->bookmarks, node))
	{
		return BOOKMARKS_GROUP;
	}
	else
	{
		return HISTORY_GROUP;
	}
}

static EphyNode *
get_index_root (EphyCompletionModel *model, int *index)
{
	int children;

	children = ephy_node_get_n_children (model->priv->history);

	if (*index >= children)
	{
		*index = *index - children;
		return model->priv->bookmarks;
	}
	else
	{
		return model->priv->history;
	}
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
	group = get_node_group (model, node);

	switch (column)
	{
		case EPHY_COMPLETION_TEXT_COL:
			g_value_init (value, G_TYPE_STRING);
			init_text_col (value, node, group);
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
	}
}

static guint
ephy_completion_model_get_flags (GtkTreeModel *tree_model)
{
	return 0;
}

static gboolean
ephy_completion_model_get_iter (GtkTreeModel *tree_model,
			        GtkTreeIter *iter,
			        GtkTreePath *path)
{
	EphyCompletionModel *model = EPHY_COMPLETION_MODEL (tree_model);
	EphyNode *root;
	int i;

	g_return_val_if_fail (EPHY_IS_COMPLETION_MODEL (model), FALSE);
	g_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

	i = gtk_tree_path_get_indices (path)[0];

	iter->stamp = model->priv->stamp;

	root = get_index_root (model, &i);
	iter->user_data = ephy_node_get_nth_child (root, i);

	if (iter->user_data == NULL)
	{
		iter->stamp = 0;
		return FALSE;
	}

	return TRUE;
}

static GtkTreePath *
ephy_completion_model_get_path (GtkTreeModel *tree_model,
			       GtkTreeIter *iter)
{
	EphyCompletionModel *model = EPHY_COMPLETION_MODEL (tree_model);
	EphyNode *node;

	g_return_val_if_fail (EPHY_IS_COMPLETION_MODEL (tree_model), NULL);
	g_return_val_if_fail (iter != NULL, NULL);
	g_return_val_if_fail (iter->user_data != NULL, NULL);
	g_return_val_if_fail (iter->stamp == model->priv->stamp, NULL);

	node = iter->user_data;

	if (node == model->priv->history)
		return gtk_tree_path_new ();

	return get_path_real (model, node);
}

static gboolean
ephy_completion_model_iter_next (GtkTreeModel *tree_model,
			        GtkTreeIter *iter)
{
	EphyCompletionModel *model = EPHY_COMPLETION_MODEL (tree_model);
	EphyNode *node, *next, *root;

	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (iter->user_data != NULL, FALSE);
	g_return_val_if_fail (iter->stamp == model->priv->stamp, FALSE);

	node = iter->user_data;

	if (node == model->priv->history)
		return FALSE;

	root = get_node_root (model, node);
	next = ephy_node_get_next_child (root, node);
	if (next == NULL && root == model->priv->history)
	{
		next = ephy_node_get_nth_child (model->priv->bookmarks, 0);
	}

	iter->user_data = next;

	return (iter->user_data != NULL);
}

static gboolean
ephy_completion_model_iter_children (GtkTreeModel *tree_model,
			            GtkTreeIter *iter,
			            GtkTreeIter *parent)
{
	EphyCompletionModel *model = EPHY_COMPLETION_MODEL (tree_model);

	if (parent != NULL)
		return FALSE;

	iter->stamp = model->priv->stamp;
	iter->user_data = model->priv->history;

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
		return FALSE;

	root = get_index_root (model, &n);
	node = ephy_node_get_nth_child (root, n);

	if (node != NULL)
	{
		iter->stamp = model->priv->stamp;
		iter->user_data = node;
		return TRUE;
	}
	else
		return FALSE;
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
