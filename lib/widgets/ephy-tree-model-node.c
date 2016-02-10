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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <time.h>
#include <string.h>

#include "ephy-tree-model-node.h"
#include "ephy-node.h"
#include "ephy-debug.h"

/**
 * SECTION:ephy-tree-model-node
 * @short_description: a #GtkTreeModel of #EphyNode elements
 *
 * #EphyTreeModelNode implements a #GtkTreeModel that holds #EphyNode elements.
 * It can be used with #EphyNodeView and #EphyTreeModelFilter.
 */

static void ephy_tree_model_node_finalize (GObject *object);
static void ephy_tree_model_node_tree_model_interface_init (GtkTreeModelIface *iface);

struct _EphyTreeModelNode
{
	GObject parent_instance;

	EphyNode *root;

	GPtrArray *columns;
	int columns_num;

	int stamp;
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
	LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

G_DEFINE_TYPE_WITH_CODE (EphyTreeModelNode, ephy_tree_model_node, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL,
                                                ephy_tree_model_node_tree_model_interface_init))

static void
root_child_removed_cb (EphyNode *node,
		       EphyNode *child,
		       guint old_index,
		       EphyTreeModelNode *model)
{
	GtkTreePath *path;

	path = gtk_tree_path_new ();
	gtk_tree_path_append_index (path, old_index);
	gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);
	gtk_tree_path_free (path);
}

static inline GtkTreePath *
get_path_real (EphyTreeModelNode *model,
	       EphyNode *node)
{
	GtkTreePath *retval;

	retval = gtk_tree_path_new ();
	gtk_tree_path_append_index (retval, ephy_node_get_child_index (model->root, node));

	return retval;
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

	LOG ("Updating row");

	gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
	gtk_tree_path_free (path);
}

static void
root_child_changed_cb (EphyNode *node,
		       EphyNode *child,
		       guint property_id,
		       EphyTreeModelNode *model)
{
	ephy_tree_model_node_update_node (model, child, -1);
}

static void
root_children_reordered_cb (EphyNode *node,
			    int *new_order,
			    EphyTreeModelNode *model)
{
	GtkTreePath *path;

	path = gtk_tree_path_new ();
	gtk_tree_model_rows_reordered (GTK_TREE_MODEL (model), path, NULL, new_order);
	gtk_tree_path_free (path);
}

static void
root_destroy_cb (EphyNode *node,
		 EphyTreeModelNode *model)
{
	model->root = NULL;

	/* no need to do other stuff since we should have had a bunch of child_removed
	 * signals already */
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
		model->root = g_value_get_pointer (value);

		ephy_node_signal_connect_object (model->root,
				                 EPHY_NODE_CHILD_ADDED,
				                 (EphyNodeCallback) root_child_added_cb,
				                 G_OBJECT (model));
		ephy_node_signal_connect_object (model->root,
				                 EPHY_NODE_CHILD_REMOVED,
				                 (EphyNodeCallback) root_child_removed_cb,
				                 G_OBJECT (model));
		ephy_node_signal_connect_object (model->root,
				                 EPHY_NODE_CHILD_CHANGED,
				                 (EphyNodeCallback) root_child_changed_cb,
				                 G_OBJECT (model));
		ephy_node_signal_connect_object (model->root,
				                 EPHY_NODE_CHILDREN_REORDERED,
				                 (EphyNodeCallback) root_children_reordered_cb,
				                 G_OBJECT (model));
		ephy_node_signal_connect_object (model->root,
				                 EPHY_NODE_DESTROY,
				                 (EphyNodeCallback) root_destroy_cb,
				                 G_OBJECT (model));

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
		g_value_set_pointer (value, model->root);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
ephy_tree_model_node_class_init (EphyTreeModelNodeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = ephy_tree_model_node_finalize;

	object_class->set_property = ephy_tree_model_node_set_property;
	object_class->get_property = ephy_tree_model_node_get_property;

	/**
	* EphyTreeModelNode:root:
	*
	* The root #EphyNode of the model.
	*/
	obj_properties[PROP_ROOT] =
		g_param_spec_pointer ("root",
		                      "Root node",
		                      "Root node",
		                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY);

	g_object_class_install_properties (object_class, LAST_PROP, obj_properties);
}

static void
ephy_tree_model_node_init (EphyTreeModelNode *model)
{
	model->stamp = g_random_int ();
	model->columns = g_ptr_array_new ();
	model->columns_num = 0;
}

static void
ephy_tree_model_node_finalize (GObject *object)
{
	EphyTreeModelNode *model = EPHY_TREE_MODEL_NODE (object);

	g_ptr_array_foreach (model->columns, (GFunc) g_free, NULL);
	g_ptr_array_free (model->columns, TRUE);

	G_OBJECT_CLASS (ephy_tree_model_node_parent_class)->finalize (object);
}

/**
 * ephy_tree_model_node_new:
 * @root: root #EphyNode of the model
 *
 * Create a new #EphyTreeModelNode object and set @root as its root node.
 *
 * Returns: a new #EphyTreeModelNode
 **/
EphyTreeModelNode *
ephy_tree_model_node_new (EphyNode *root)
{
	EphyTreeModelNode *model;

	model = EPHY_TREE_MODEL_NODE (g_object_new (EPHY_TYPE_TREE_MODEL_NODE,
						    "root", root,
						    NULL));

	return model;
}

/**
 * ephy_tree_model_node_add_column_full:
 * @model: an #EphyTreeModelNode
 * @value_type: type held by the new column
 * @prop_id: column in @model to get the value for this column
 * @func: data function to be used to modify the value of the new column
 * @user_data: optional user data for @func
 *
 * Add a new column to @model obtaining its value from @prop_id in @model,
 * modified by @func.
 *
 * Returns: the id of the new column
 **/
int
ephy_tree_model_node_add_column_full (EphyTreeModelNode *model,
				      GType value_type,
				      int prop_id,
				      EphyTreeModelNodeValueFunc func,
				      gpointer user_data)
{
	EphyTreeModelNodeColData *col;
	int col_id;

	col = g_new0 (EphyTreeModelNodeColData, 1);
	col->prop_id = prop_id;
	col->type = value_type;
	col->func = func;
	col->user_data = user_data;

	g_ptr_array_add (model->columns, col);
	col_id = model->columns_num;
	model->columns_num++;

	return col_id;
}


/**
 * ephy_tree_model_node_add_prop_column:
 * @model: an #EphyTreeModelNode
 * @value_type: type held by the new column
 * @prop_id: column in @model to get the value for this column
 *
 * Add a new column to @model obtaining its value from @prop_id in @model.
 *
 * Returns: the id of the new column
 **/
int
ephy_tree_model_node_add_prop_column (EphyTreeModelNode *model,
				      GType value_type,
				      int prop_id)
{
	return ephy_tree_model_node_add_column_full (model, value_type, prop_id, NULL, NULL);
}

/**
 * ephy_tree_model_node_add_func_column:
 * @model: an #EphyTreeModelNode
 * @value_type: type held by the new column
 * @func: data function to be used to provide the value of the new column
 * @user_data: optional user data for @func
 *
 * Adds a new column to @model with its value determined by @func.
 *
 * Returns: the id of the new column
 **/
int
ephy_tree_model_node_add_func_column (EphyTreeModelNode *model,
				      GType value_type,
				      EphyTreeModelNodeValueFunc func,
				      gpointer user_data)
{
	return ephy_tree_model_node_add_column_full (model, value_type, -1, func, user_data);
}

static int
ephy_tree_model_node_get_n_columns (GtkTreeModel *tree_model)
{
	EphyTreeModelNode *model = EPHY_TREE_MODEL_NODE (tree_model);

	return model->columns_num;
}

static GType
ephy_tree_model_node_get_column_type (GtkTreeModel *tree_model,
			              int index)
{
	EphyTreeModelNodeColData *col;
	EphyTreeModelNode *model = EPHY_TREE_MODEL_NODE (tree_model);

	col = g_ptr_array_index (model->columns, index);

	return col->type;
}

static void
ephy_tree_model_node_get_value (GtkTreeModel *tree_model,
			        GtkTreeIter *iter,
			        int column,
			        GValue *value)
{
	EphyTreeModelNodeColData *col;
	EphyTreeModelNode *model = EPHY_TREE_MODEL_NODE (tree_model);
	EphyNode *node;

	g_return_if_fail (EPHY_IS_TREE_MODEL_NODE (tree_model));
	g_return_if_fail (iter != NULL);
	g_return_if_fail (iter->stamp == model->stamp);

	if (model->root == NULL)
		return;

	node = iter->user_data;

	col = g_ptr_array_index (model->columns, column);

	g_return_if_fail (col != NULL);

	if (col->prop_id >= 0)
	{
		if (!ephy_node_get_property (node, col->prop_id, value))
		{
			/* make sure to return a valid string anyway */
			g_value_init (value, col->type);
			if (col->type == G_TYPE_STRING)
			{
				g_value_set_string (value, "");
			}
		}
	}

	if (col->func)
	{
		col->func (node, value, col->user_data);
	}
}

static GtkTreeModelFlags
ephy_tree_model_node_get_flags (GtkTreeModel *tree_model)
{
	return GTK_TREE_MODEL_ITERS_PERSIST | GTK_TREE_MODEL_LIST_ONLY;
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

	if (model->root == NULL)
		return FALSE;

	i = gtk_tree_path_get_indices (path)[0];

	iter->stamp = model->stamp;
	iter->user_data = ephy_node_get_nth_child (model->root, i);

	if (iter->user_data == NULL)
	{
		iter->stamp = 0;
		return FALSE;
	}

	return TRUE;
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

	if (model->root == NULL)
		return NULL;

	node = iter->user_data;

	if (node == model->root)
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
	g_return_val_if_fail (iter->stamp == model->stamp, FALSE);

	if (model->root == NULL)
		return FALSE;

	node = iter->user_data;

	if (node == model->root)
		return FALSE;

	iter->user_data = ephy_node_get_next_child (model->root, node);

	return (iter->user_data != NULL);
}

static gboolean
ephy_tree_model_node_iter_children (GtkTreeModel *tree_model,
			            GtkTreeIter *iter,
			            GtkTreeIter *parent)
{
	EphyTreeModelNode *model = EPHY_TREE_MODEL_NODE (tree_model);

	if (model->root == NULL)
		return FALSE;

	if (parent != NULL)
		return FALSE;

	iter->stamp = model->stamp;
	iter->user_data = model->root;

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

	if (model->root == NULL)
		return 0;

	if (iter == NULL)
		return ephy_node_get_n_children (model->root);

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

	if (model->root == NULL)
		return FALSE;

	if (parent != NULL)
		return FALSE;

	node = ephy_node_get_nth_child (model->root, n);

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

/**
 * ephy_tree_model_node_node_from_iter:
 * @model: an #EphyTreeModelNode
 * @iter: iter from where to get the node
 *
 * Gets the #EphyNode corresponding to @iter from @model.
 *
 * Returns: the #EphyNode corresponding to @iter
 **/
EphyNode *
ephy_tree_model_node_node_from_iter (EphyTreeModelNode *model,
				     GtkTreeIter *iter)
{
	return iter->user_data;
}

/**
 * ephy_tree_model_node_iter_from_node:
 * @model: an #EphyTreeModelNode
 * @node: the #EphyNode from which we want the iter to be obtained
 * @iter: location to return the #GtkTreeIter
 *
 * Gets the corresponding #GtkTreeIter for @node from @model.
 **/
void
ephy_tree_model_node_iter_from_node (EphyTreeModelNode *model,
				     EphyNode *node,
				     GtkTreeIter *iter)
{
	iter->stamp = model->stamp;
	iter->user_data = node;
}

static void
ephy_tree_model_node_tree_model_interface_init (GtkTreeModelIface *iface)
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
