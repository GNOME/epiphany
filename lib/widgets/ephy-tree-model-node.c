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
#include <gtk/gtktreeview.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <time.h>
#include <string.h>

#include "ephy-node-filter.h"
#include "ephy-tree-model-node.h"
#include "ephy-stock-icons.h"
#include "ephy-node.h"
#include "ephy-debug.h"

static void ephy_tree_model_node_class_init (EphyTreeModelNodeClass *klass);
static void ephy_tree_model_node_init (EphyTreeModelNode *model);
static void ephy_tree_model_node_finalize (GObject *object);
static void ephy_tree_model_node_set_property (GObject *object,
					       guint prop_id,
					       const GValue *value,
					       GParamSpec *pspec);
static void ephy_tree_model_node_get_property (GObject *object,
					       guint prop_id,
					       GValue *value,
					       GParamSpec *pspec);
static guint ephy_tree_model_node_get_flags (GtkTreeModel *tree_model);
static gboolean ephy_tree_model_node_get_iter (GtkTreeModel *tree_model,
					       GtkTreeIter *iter,
					       GtkTreePath *path);
static GtkTreePath *ephy_tree_model_node_get_path (GtkTreeModel *tree_model,
						   GtkTreeIter *iter);
static gboolean	ephy_tree_model_node_iter_next (GtkTreeModel *tree_model,
					        GtkTreeIter *iter);
static gboolean	ephy_tree_model_node_iter_children (GtkTreeModel *tree_model,
						    GtkTreeIter *iter,
						    GtkTreeIter *parent);
static gboolean	ephy_tree_model_node_iter_has_child (GtkTreeModel *tree_model,
						     GtkTreeIter *iter);
static int ephy_tree_model_node_iter_n_children (GtkTreeModel *tree_model,
					         GtkTreeIter *iter);
static gboolean	ephy_tree_model_node_iter_nth_child (GtkTreeModel *tree_model,
						     GtkTreeIter *iter,
						     GtkTreeIter *parent,
					             int n);
static gboolean	ephy_tree_model_node_iter_parent (GtkTreeModel *tree_model,
					          GtkTreeIter *iter,
					          GtkTreeIter *child);
static void ephy_tree_model_node_tree_model_init (GtkTreeModelIface *iface);
static void root_child_removed_cb (EphyNode *node,
				   EphyNode *child,
				   EphyTreeModelNode *model);
static void root_child_added_cb (EphyNode *node,
				 EphyNode *child,
				 EphyTreeModelNode *model);
static void root_child_changed_cb (EphyNode *node,
				   EphyNode *child,
		                   EphyTreeModelNode *model);
static void root_children_reordered_cb (EphyNode *node,
				     int *new_order,
				     EphyTreeModelNode *model);
static inline void ephy_tree_model_node_update_node (EphyTreeModelNode *model,
				                     EphyNode *node,
					             int idx);
static void root_destroyed_cb (EphyNode *node,
		               EphyTreeModelNode *model);
static inline GtkTreePath *get_path_real (EphyTreeModelNode *model,
	                                  EphyNode *node);

struct EphyTreeModelNodePrivate
{
	EphyNode *root;

	EphyNodeFilter *filter;

	GPtrArray *columns;
	int columns_num;
};

typedef struct
{
	GType type;
	int prop_id;
	EphyTreeModelNodeValueFunc func;
	gpointer user_data;
} EphyTreeModelNodeColData;

enum
{
	PROP_0,
	PROP_ROOT,
	PROP_FILTER
};

static GObjectClass *parent_class = NULL;

GType
ephy_tree_model_node_get_type (void)
{
	static GType ephy_tree_model_node_type = 0;

	if (ephy_tree_model_node_type == 0)
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyTreeModelNodeClass),
			NULL,
			NULL,
			(GClassInitFunc) ephy_tree_model_node_class_init,
			NULL,
			NULL,
			sizeof (EphyTreeModelNode),
			0,
			(GInstanceInitFunc) ephy_tree_model_node_init
		};

		static const GInterfaceInfo tree_model_info =
		{
			(GInterfaceInitFunc) ephy_tree_model_node_tree_model_init,
			NULL,
			NULL
		};

		ephy_tree_model_node_type = g_type_register_static (G_TYPE_OBJECT,
								  "EphyTreeModelNode",
								  &our_info, 0);

		g_type_add_interface_static (ephy_tree_model_node_type,
					     GTK_TYPE_TREE_MODEL,
					     &tree_model_info);
	}

	return ephy_tree_model_node_type;
}

static void
ephy_tree_model_node_class_init (EphyTreeModelNodeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = ephy_tree_model_node_finalize;

	object_class->set_property = ephy_tree_model_node_set_property;
	object_class->get_property = ephy_tree_model_node_get_property;

	g_object_class_install_property (object_class,
					 PROP_ROOT,
					 g_param_spec_pointer ("root",
							      "Root node",
							      "Root node",
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class,
					 PROP_FILTER,
					 g_param_spec_object ("filter",
							      "Filter object",
							      "Filter object",
							      EPHY_TYPE_NODE_FILTER,
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
ephy_tree_model_node_init (EphyTreeModelNode *model)
{
	GtkWidget *dummy;

	do
	{
		model->stamp = g_random_int ();
	}
	while (model->stamp == 0);

	model->priv = g_new0 (EphyTreeModelNodePrivate, 1);

	dummy = gtk_tree_view_new ();

	gtk_widget_destroy (dummy);

	model->priv->columns = g_ptr_array_new ();
	model->priv->columns_num = EPHY_TREE_MODEL_NODE_BUILTIN_COLUMNS;
}

static void
ephy_tree_model_node_finalize (GObject *object)
{
	EphyTreeModelNode *model;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EPHY_IS_TREE_MODEL_NODE (object));

	model = EPHY_TREE_MODEL_NODE (object);

	g_return_if_fail (model->priv != NULL);

	g_ptr_array_free (model->priv->columns, TRUE);

	g_free (model->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
filter_changed_cb (EphyNodeFilter *filter,
		   EphyTreeModelNode *model)
{
	GPtrArray *kids;
	int i;

	kids = ephy_node_get_children (model->priv->root);

	for (i = 0; i < kids->len; i++)
	{
		ephy_tree_model_node_update_node (model,
						g_ptr_array_index (kids, i),
						i);
	}

	ephy_node_thaw (model->priv->root);
}

static void
ephy_tree_model_node_set_property (GObject *object,
			           guint prop_id,
			           const GValue *value,
			           GParamSpec *pspec)
{
	EphyTreeModelNode *model = EPHY_TREE_MODEL_NODE (object);

	switch (prop_id)
	{
	case PROP_ROOT:
		model->priv->root = g_value_get_pointer (value);

		ephy_node_signal_connect_object (model->priv->root,
				                 EPHY_NODE_CHILD_ADDED,
				                 (EphyNodeCallback) root_child_added_cb,
				                 G_OBJECT (model));
		ephy_node_signal_connect_object (model->priv->root,
				                 EPHY_NODE_CHILD_REMOVED,
				                 (EphyNodeCallback) root_child_removed_cb,
				                 G_OBJECT (model));
		ephy_node_signal_connect_object (model->priv->root,
				                 EPHY_NODE_CHILD_CHANGED,
				                 (EphyNodeCallback) root_child_changed_cb,
				                 G_OBJECT (model));
		ephy_node_signal_connect_object (model->priv->root,
				                 EPHY_NODE_CHILDREN_REORDERED,
				                 (EphyNodeCallback) root_children_reordered_cb,
				                 G_OBJECT (model));
		ephy_node_signal_connect_object (model->priv->root,
				                 EPHY_NODE_DESTROYED,
				                 (EphyNodeCallback) root_destroyed_cb,
				                 G_OBJECT (model));

		break;
	case PROP_FILTER:
		model->priv->filter = g_value_get_object (value);

		if (model->priv->filter != NULL)
		{
			g_signal_connect_object (G_OBJECT (model->priv->filter),
					         "changed",
					         G_CALLBACK (filter_changed_cb),
					         G_OBJECT (model),
						 0);
		}
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
ephy_tree_model_node_get_property (GObject *object,
			           guint prop_id,
				   GValue *value,
			           GParamSpec *pspec)
{
	EphyTreeModelNode *model = EPHY_TREE_MODEL_NODE (object);

	switch (prop_id)
	{
	case PROP_ROOT:
		g_value_set_pointer (value, model->priv->root);
		break;
	case PROP_FILTER:
		g_value_set_object (value, model->priv->filter);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

EphyTreeModelNode *
ephy_tree_model_node_new (EphyNode *root,
			  EphyNodeFilter *filter)
{
	EphyTreeModelNode *model;

	model = EPHY_TREE_MODEL_NODE (g_object_new (EPHY_TYPE_TREE_MODEL_NODE,
						    "filter", filter,
						    "root", root,
						    NULL));

	g_return_val_if_fail (model->priv != NULL, NULL);

	return model;
}

int
ephy_tree_model_node_add_prop_column (EphyTreeModelNode *model,
				      GType value_type,
				      int prop_id)
{
	EphyTreeModelNodeColData *col;

	col = g_new0 (EphyTreeModelNodeColData, 1);
	col->prop_id = prop_id;
	col->type = value_type;
	col->func = NULL;
	col->user_data = NULL;

	g_ptr_array_add (model->priv->columns, col);
	model->priv->columns_num++;

	return model->priv->columns_num;
}

int
ephy_tree_model_node_add_func_column (EphyTreeModelNode *model,
				      GType value_type,
				      EphyTreeModelNodeValueFunc func,
				      gpointer user_data)
{
	EphyTreeModelNodeColData *col;

	col = g_new0 (EphyTreeModelNodeColData, 1);
	col->prop_id = -1;
	col->type = value_type;
	col->func = func;
	col->user_data = user_data;

	g_ptr_array_add (model->priv->columns, col);
	model->priv->columns_num++;

	return model->priv->columns_num;
}

static int
ephy_tree_model_node_get_n_columns (GtkTreeModel *tree_model)
{
	EphyTreeModelNode *model = EPHY_TREE_MODEL_NODE (tree_model);

	return model->priv->columns_num;
}

static GType
ephy_tree_model_node_get_column_type (GtkTreeModel *tree_model,
			              int index)
{
	int list_index;
	EphyTreeModelNodeColData *col;
	EphyTreeModelNode *model = EPHY_TREE_MODEL_NODE (tree_model);

	if (index == EPHY_TREE_MODEL_NODE_COL_VISIBLE)
		return G_TYPE_BOOLEAN;

	list_index = index - EPHY_TREE_MODEL_NODE_BUILTIN_COLUMNS - 1;
	col = g_ptr_array_index (model->priv->columns, list_index);

	return col->type;
}

static void
ephy_tree_model_node_get_value (GtkTreeModel *tree_model,
			        GtkTreeIter *iter,
			        int column,
			        GValue *value)
{
	int list_index;
	EphyTreeModelNodeColData *col;
	EphyTreeModelNode *model = EPHY_TREE_MODEL_NODE (tree_model);
	EphyNode *node;

	g_return_if_fail (EPHY_IS_TREE_MODEL_NODE (tree_model));
	g_return_if_fail (iter != NULL);
	g_return_if_fail (iter->stamp == model->stamp);

	if (model->priv->root == NULL)
		return;

	node = iter->user_data;

	if (column == EPHY_TREE_MODEL_NODE_COL_VISIBLE)
	{
		g_value_init (value, G_TYPE_BOOLEAN);

		if (model->priv->filter != NULL)
		{
			g_value_set_boolean (value,
					     ephy_node_filter_evaluate (model->priv->filter, node));
		}
		else
		{
			g_value_set_boolean (value, TRUE);
		}
	}
	else
	{
		list_index = column - EPHY_TREE_MODEL_NODE_BUILTIN_COLUMNS - 1;
		col = g_ptr_array_index (model->priv->columns, list_index);

		g_return_if_fail (col != NULL);

		if (col->prop_id >= 0)
		{
			ephy_node_get_property (node,
					        col->prop_id,
					        value);
		}
		else
		{
			col->func (node, value, col->user_data);
		}
	}
}

static void
ephy_tree_model_node_tree_model_init (GtkTreeModelIface *iface)
{
	iface->get_flags       = ephy_tree_model_node_get_flags;
	iface->get_iter        = ephy_tree_model_node_get_iter;
	iface->get_path        = ephy_tree_model_node_get_path;
	iface->iter_next       = ephy_tree_model_node_iter_next;
	iface->iter_children   = ephy_tree_model_node_iter_children;
	iface->iter_has_child  = ephy_tree_model_node_iter_has_child;
	iface->iter_n_children = ephy_tree_model_node_iter_n_children;
	iface->iter_nth_child  = ephy_tree_model_node_iter_nth_child;
	iface->iter_parent     = ephy_tree_model_node_iter_parent;
	iface->get_n_columns   = ephy_tree_model_node_get_n_columns;
	iface->get_column_type = ephy_tree_model_node_get_column_type;
	iface->get_value       = ephy_tree_model_node_get_value;
}

static guint
ephy_tree_model_node_get_flags (GtkTreeModel *tree_model)
{
	return 0;
}

static gboolean
ephy_tree_model_node_get_iter (GtkTreeModel *tree_model,
			       GtkTreeIter *iter,
			       GtkTreePath *path)
{
	EphyTreeModelNode *model = EPHY_TREE_MODEL_NODE (tree_model);
	int i;

	g_return_val_if_fail (EPHY_IS_TREE_MODEL_NODE (model), FALSE);
	g_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

	if (model->priv->root == NULL)
		return FALSE;

	i = gtk_tree_path_get_indices (path)[0];

	iter->stamp = model->stamp;
	iter->user_data = ephy_node_get_nth_child (model->priv->root, i);

	if (iter->user_data == NULL)
	{
		iter->stamp = 0;
		return FALSE;
	}

	return TRUE;
}

static inline GtkTreePath *
get_path_real (EphyTreeModelNode *model,
	       EphyNode *node)
{
	GtkTreePath *retval;

	retval = gtk_tree_path_new ();
	gtk_tree_path_append_index (retval, ephy_node_get_child_index (model->priv->root, node));

	return retval;
}

static GtkTreePath *
ephy_tree_model_node_get_path (GtkTreeModel *tree_model,
			       GtkTreeIter *iter)
{
	EphyTreeModelNode *model = EPHY_TREE_MODEL_NODE (tree_model);
	EphyNode *node;

	g_return_val_if_fail (EPHY_IS_TREE_MODEL_NODE (tree_model), NULL);
	g_return_val_if_fail (iter != NULL, NULL);
	g_return_val_if_fail (iter->user_data != NULL, NULL);
	g_return_val_if_fail (iter->stamp == model->stamp, NULL);

	if (model->priv->root == NULL)
		return NULL;

	node = iter->user_data;

	if (node == model->priv->root)
		return gtk_tree_path_new ();

	return get_path_real (model, node);
}

static gboolean
ephy_tree_model_node_iter_next (GtkTreeModel *tree_model,
			        GtkTreeIter *iter)
{
	EphyTreeModelNode *model = EPHY_TREE_MODEL_NODE (tree_model);
	EphyNode *node;

	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (iter->user_data != NULL, FALSE);
	g_return_val_if_fail (iter->stamp == EPHY_TREE_MODEL_NODE (tree_model)->stamp, FALSE);

	if (model->priv->root == NULL)
		return FALSE;

	node = iter->user_data;

	if (node == model->priv->root)
		return FALSE;

	iter->user_data = ephy_node_get_next_child (model->priv->root, node);

	return (iter->user_data != NULL);
}

static gboolean
ephy_tree_model_node_iter_children (GtkTreeModel *tree_model,
			            GtkTreeIter *iter,
			            GtkTreeIter *parent)
{
	EphyTreeModelNode *model = EPHY_TREE_MODEL_NODE (tree_model);

	if (model->priv->root == NULL)
		return FALSE;

	if (parent != NULL)
		return FALSE;

	iter->stamp = model->stamp;
	iter->user_data = model->priv->root;

	return TRUE;
}

static gboolean
ephy_tree_model_node_iter_has_child (GtkTreeModel *tree_model,
			             GtkTreeIter *iter)
{
	return FALSE;
}

static int
ephy_tree_model_node_iter_n_children (GtkTreeModel *tree_model,
			              GtkTreeIter *iter)
{
	EphyTreeModelNode *model = EPHY_TREE_MODEL_NODE (tree_model);

	g_return_val_if_fail (EPHY_IS_TREE_MODEL_NODE (tree_model), -1);

	if (model->priv->root == NULL)
		return 0;

	if (iter == NULL)
		return ephy_node_get_n_children (model->priv->root);

	g_return_val_if_fail (model->stamp == iter->stamp, -1);

	return 0;
}

static gboolean
ephy_tree_model_node_iter_nth_child (GtkTreeModel *tree_model,
			             GtkTreeIter *iter,
			             GtkTreeIter *parent,
			             int n)
{
	EphyTreeModelNode *model = EPHY_TREE_MODEL_NODE (tree_model);
	EphyNode *node;

	g_return_val_if_fail (EPHY_IS_TREE_MODEL_NODE (tree_model), FALSE);

	if (model->priv->root == NULL)
		return FALSE;

	if (parent != NULL)
		return FALSE;

	node = ephy_node_get_nth_child (model->priv->root, n);

	if (node != NULL)
	{
		iter->stamp = model->stamp;
		iter->user_data = node;
		return TRUE;
	}
	else
		return FALSE;
}

static gboolean
ephy_tree_model_node_iter_parent (GtkTreeModel *tree_model,
			          GtkTreeIter *iter,
			          GtkTreeIter *child)
{
	return FALSE;
}

EphyNode *
ephy_tree_model_node_node_from_iter (EphyTreeModelNode *model,
				     GtkTreeIter *iter)
{
	return iter->user_data;
}

void
ephy_tree_model_node_iter_from_node (EphyTreeModelNode *model,
				     EphyNode *node,
				     GtkTreeIter *iter)
{
	iter->stamp = model->stamp;
	iter->user_data = node;
}

static void
root_child_removed_cb (EphyNode *node,
		       EphyNode *child,
		       EphyTreeModelNode *model)
{
	GtkTreePath *path;

	path = get_path_real (model, child);
	gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);
	gtk_tree_path_free (path);
}

static void
root_child_added_cb (EphyNode *node,
		     EphyNode *child,
		     EphyTreeModelNode *model)
{
	GtkTreePath *path;
	GtkTreeIter iter;

	ephy_tree_model_node_iter_from_node (model, child, &iter);

	path = get_path_real (model, child);
	gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), path, &iter);
	gtk_tree_path_free (path);
}

static inline void
ephy_tree_model_node_update_node (EphyTreeModelNode *model,
				  EphyNode *node,
				  int idx)
{
	GtkTreePath *path;
	GtkTreeIter iter;

	ephy_tree_model_node_iter_from_node (model, node, &iter);

	if (idx >= 0)
	{
		path = gtk_tree_path_new ();
		gtk_tree_path_append_index (path, idx);
	}
	else
	{
		path = get_path_real (model, node);
	}

	LOG ("Updating row")

	gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
	gtk_tree_path_free (path);
}

static void
root_child_changed_cb (EphyNode *node,
		       EphyNode *child,
		       EphyTreeModelNode *model)
{
	ephy_tree_model_node_update_node (model, child, -1);
}

static void
root_children_reordered_cb (EphyNode *node,
			    int *new_order,
			    EphyTreeModelNode *model)
{
	gtk_tree_model_rows_reordered (GTK_TREE_MODEL (model), NULL, NULL, new_order);
}

static void
root_destroyed_cb (EphyNode *node,
		   EphyTreeModelNode *model)
{
	model->priv->root = NULL;

	/* no need to do other stuff since we should have had a bunch of child_removed
	 * signals already */
}

