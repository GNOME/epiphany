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
#include <libgnome/gnome-i18n.h>
#include <time.h>
#include <string.h>

#include "ephy-node-filter.h"
#include "ephy-history-model.h"
#include "ephy-history.h"
#include "ephy-tree-model-node.h"
#include "ephy-stock-icons.h"
#include "ephy-node.h"

static void ephy_history_model_class_init (EphyHistoryModelClass *klass);
static void ephy_history_model_init (EphyHistoryModel *model);
static void ephy_history_model_finalize (GObject *object);
static void ephy_history_model_set_property (GObject *object,
					     guint prop_id,
					     const GValue *value,
					     GParamSpec *pspec);
static void ephy_history_model_get_property (GObject *object,
					     guint prop_id,
					     GValue *value,
					     GParamSpec *pspec);
static guint ephy_history_model_get_flags (GtkTreeModel *tree_model);
static int ephy_history_model_get_n_columns (GtkTreeModel *tree_model);
static GType ephy_history_model_get_column_type (GtkTreeModel *tree_model,
						 int index);
static gboolean ephy_history_model_get_iter (GtkTreeModel *tree_model,
					     GtkTreeIter *iter,
					     GtkTreePath *path);
static GtkTreePath *ephy_history_model_get_path (GtkTreeModel *tree_model,
						 GtkTreeIter *iter);
static void ephy_history_model_get_value (GtkTreeModel *tree_model,
					  GtkTreeIter *iter,
					  int column,
					  GValue *value);
static gboolean	ephy_history_model_iter_next (GtkTreeModel *tree_model,
					      GtkTreeIter *iter);
static gboolean	ephy_history_model_iter_children (GtkTreeModel *tree_model,
						  GtkTreeIter *iter,
						  GtkTreeIter *parent);
static gboolean	ephy_history_model_iter_has_child (GtkTreeModel *tree_model,
						   GtkTreeIter *iter);
static int ephy_history_model_iter_n_children (GtkTreeModel *tree_model,
					       GtkTreeIter *iter);
static gboolean	ephy_history_model_iter_nth_child (GtkTreeModel *tree_model,
						   GtkTreeIter *iter,
						   GtkTreeIter *parent,
					           int n);
static gboolean	ephy_history_model_iter_parent (GtkTreeModel *tree_model,
					        GtkTreeIter *iter,
					        GtkTreeIter *child);
static void ephy_history_model_tree_model_init (GtkTreeModelIface *iface);
static void root_child_removed_cb (EphyNode *node,
				   EphyNode *child,
				   EphyHistoryModel *model);
static void root_child_added_cb (EphyNode *node,
				 EphyNode *child,
				 EphyHistoryModel *model);
static void root_child_changed_cb (EphyNode *node,
				   EphyNode *child,
		                   EphyHistoryModel *model);
static inline void ephy_history_model_update_node (EphyHistoryModel *model,
				                   EphyNode *node,
					           int idx);

struct EphyHistoryModelPrivate
{
	EphyNode *root;
	EphyNode *pages;

	EphyNodeFilter *filter;
};

enum
{
	PROP_0,
	PROP_ROOT,
	PROP_PAGES,
	PROP_FILTER
};

static GObjectClass *parent_class = NULL;

GType
ephy_history_model_get_type (void)
{
	static GType ephy_history_model_type = 0;

	if (ephy_history_model_type == 0)
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyHistoryModelClass),
			NULL,
			NULL,
			(GClassInitFunc) ephy_history_model_class_init,
			NULL,
			NULL,
			sizeof (EphyHistoryModel),
			0,
			(GInstanceInitFunc) ephy_history_model_init
		};

		static const GInterfaceInfo tree_model_info =
		{
			(GInterfaceInitFunc) ephy_history_model_tree_model_init,
			NULL,
			NULL
		};

		ephy_history_model_type = g_type_register_static (G_TYPE_OBJECT,
								  "EphyHistoryModel",
								  &our_info, 0);

		g_type_add_interface_static (ephy_history_model_type,
					     GTK_TYPE_TREE_MODEL,
					     &tree_model_info);
	}

	return ephy_history_model_type;
}

static void
ephy_history_model_class_init (EphyHistoryModelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = ephy_history_model_finalize;

	object_class->set_property = ephy_history_model_set_property;
	object_class->get_property = ephy_history_model_get_property;

	g_object_class_install_property (object_class,
					 PROP_ROOT,
					 g_param_spec_object ("root",
							      "Root node",
							      "Root node",
							      EPHY_TYPE_NODE,
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class,
					 PROP_PAGES,
					 g_param_spec_object ("pages",
							      "Pages node",
							      "Pages node",
							      EPHY_TYPE_NODE,
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
ephy_history_model_init (EphyHistoryModel *model)
{
	GtkWidget *dummy;

	do
	{
		model->stamp = g_random_int ();
	}
	while (model->stamp == 0);

	model->priv = g_new0 (EphyHistoryModelPrivate, 1);

	dummy = gtk_tree_view_new ();

	gtk_widget_destroy (dummy);
}

static void
ephy_history_model_finalize (GObject *object)
{
	EphyHistoryModel *model;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EPHY_IS_HISTORY_MODEL (object));

	model = EPHY_HISTORY_MODEL (object);

	g_return_if_fail (model->priv != NULL);

	g_free (model->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
filter_changed_cb (EphyNodeFilter *filter,
		   EphyHistoryModel *model)
{
	GPtrArray *kids;
	int i;

	kids = ephy_node_get_children (model->priv->root);

	for (i = 0; i < kids->len; i++)
	{
		ephy_history_model_update_node (model,
						g_ptr_array_index (kids, i),
						i);
	}

	ephy_node_thaw (model->priv->root);
}

static void
ephy_history_model_set_property (GObject *object,
			         guint prop_id,
			         const GValue *value,
			         GParamSpec *pspec)
{
	EphyHistoryModel *model = EPHY_HISTORY_MODEL (object);

	switch (prop_id)
	{
	case PROP_ROOT:
		model->priv->root = g_value_get_object (value);
		g_signal_connect_object (G_OBJECT (model->priv->root),
				         "child_added",
				         G_CALLBACK (root_child_added_cb),
				         G_OBJECT (model),
					 0);
		g_signal_connect_object (G_OBJECT (model->priv->root),
				         "child_removed",
				         G_CALLBACK (root_child_removed_cb),
				         G_OBJECT (model),
					 0);
		g_signal_connect_object (G_OBJECT (model->priv->root),
				         "child_changed",
				         G_CALLBACK (root_child_changed_cb),
				         G_OBJECT (model),
					 0);
		break;
	case PROP_PAGES:
		model->priv->pages = g_value_get_object (value);
		g_signal_connect_object (G_OBJECT (model->priv->pages),
				         "child_added",
				         G_CALLBACK (root_child_added_cb),
				         G_OBJECT (model),
					 0);
		g_signal_connect_object (G_OBJECT (model->priv->pages),
				         "child_removed",
				         G_CALLBACK (root_child_removed_cb),
				         G_OBJECT (model),
					 0);
		g_signal_connect_object (G_OBJECT (model->priv->pages),
				         "child_changed",
				         G_CALLBACK (root_child_changed_cb),
				         G_OBJECT (model),
					 0);
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
ephy_history_model_get_property (GObject *object,
			         guint prop_id,
				 GValue *value,
			         GParamSpec *pspec)
{
	EphyHistoryModel *model = EPHY_HISTORY_MODEL (object);

	switch (prop_id)
	{
	case PROP_ROOT:
		g_value_set_object (value, model->priv->root);
		break;
	case PROP_PAGES:
		g_value_set_object (value, model->priv->root);
		break;
	case PROP_FILTER:
		g_value_set_object (value, model->priv->filter);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

EphyHistoryModel *
ephy_history_model_new (EphyNode *root,
			EphyNode *pages,
			EphyNodeFilter *filter)
{
	EphyHistoryModel *model;

	model = EPHY_HISTORY_MODEL (g_object_new (EPHY_TYPE_HISTORY_MODEL,
						  "filter", filter,
						  "root", root,
						  "pages", pages,
						  NULL));

	g_return_val_if_fail (model->priv != NULL, NULL);

	return model;
}

static void
ephy_history_model_tree_model_init (GtkTreeModelIface *iface)
{
	iface->get_flags       = ephy_history_model_get_flags;
	iface->get_n_columns   = ephy_history_model_get_n_columns;
	iface->get_column_type = ephy_history_model_get_column_type;
	iface->get_iter        = ephy_history_model_get_iter;
	iface->get_path        = ephy_history_model_get_path;
	iface->get_value       = ephy_history_model_get_value;
	iface->iter_next       = ephy_history_model_iter_next;
	iface->iter_children   = ephy_history_model_iter_children;
	iface->iter_has_child  = ephy_history_model_iter_has_child;
	iface->iter_n_children = ephy_history_model_iter_n_children;
	iface->iter_nth_child  = ephy_history_model_iter_nth_child;
	iface->iter_parent     = ephy_history_model_iter_parent;
}

static guint
ephy_history_model_get_flags (GtkTreeModel *tree_model)
{
	return 0;
}

static int
ephy_history_model_get_n_columns (GtkTreeModel *tree_model)
{
	return EPHY_HISTORY_MODEL_NUM_COLUMNS;
}

static GType
ephy_history_model_get_column_type (GtkTreeModel *tree_model,
			            int index)
{
	g_return_val_if_fail (EPHY_IS_HISTORY_MODEL (tree_model), G_TYPE_INVALID);
	g_return_val_if_fail ((index < EPHY_HISTORY_MODEL_NUM_COLUMNS) && (index >= 0), G_TYPE_INVALID);

	switch (index)
	{
		case EPHY_HISTORY_MODEL_COL_TITLE:
		case EPHY_HISTORY_MODEL_COL_LOCATION:
		case EPHY_HISTORY_MODEL_COL_VISITS:
		case EPHY_HISTORY_MODEL_COL_LAST_VISIT:
		case EPHY_HISTORY_MODEL_COL_FIRST_VISIT:
			return G_TYPE_STRING;
		case EPHY_HISTORY_MODEL_COL_VISIBLE:
			return G_TYPE_BOOLEAN;
	default:
		g_assert_not_reached ();
		return G_TYPE_INVALID;
	}
}

static gboolean
ephy_history_model_get_iter (GtkTreeModel *tree_model,
			     GtkTreeIter *iter,
			     GtkTreePath *path)
{
	EphyHistoryModel *model = EPHY_HISTORY_MODEL (tree_model);
	gint *indices;
	gint depth;
	EphyNode *host;

	g_return_val_if_fail (EPHY_IS_HISTORY_MODEL (model), FALSE);

	indices = gtk_tree_path_get_indices (path);
	depth = gtk_tree_path_get_depth (path);

	g_return_val_if_fail (depth > 0, FALSE);

	iter->stamp = model->stamp;
	host = ephy_node_get_nth_child (model->priv->root, indices [0]);

	if (depth == 2 && host != NULL)
	{
		iter->user_data = ephy_node_get_nth_child (host, indices [1]);
	}
	else
	{
		iter->user_data = host;
	}

	if (iter->user_data == NULL)
	{
		iter->stamp = 0;
		return FALSE;
	}

	return TRUE;
}

static EphyNode *
ensure_iter (EphyHistoryModel *model, GtkTreeIter *parent)
{
	EphyNode *node;

	if (parent)
	{
		node = EPHY_NODE (parent->user_data);
	}
	else
	{
		node = model->priv->root;
	}

	return node;
}

static EphyNode *
get_parent_node (EphyHistoryModel *model, EphyNode *node)
{
	int host_id;

	host_id = ephy_node_get_property_int (node, EPHY_NODE_PAGE_PROP_HOST_ID);

	if (host_id < 0)
	{
		return model->priv->root;
	}
	else
	{
		EphyNode *host;
		host = ephy_node_get_from_id (host_id);
		return host;
	}
}

static inline GtkTreePath *
get_one_level_path_real (EphyHistoryModel *model,
	                 EphyNode *node)
{
	GtkTreePath *retval;
	EphyNode *my_parent;

	retval = gtk_tree_path_new ();

	my_parent = get_parent_node (model, node);
	g_return_val_if_fail (my_parent != NULL, NULL);

	gtk_tree_path_append_index (retval, ephy_node_get_child_index (my_parent, node));

	return retval;
}

static inline GtkTreePath *
get_path_real (EphyHistoryModel *model,
	       EphyNode *page)
{
	GtkTreePath *retval;
	EphyNode *host;

	host = get_parent_node (model, page);
	if (host != NULL) return NULL;

	retval = gtk_tree_path_new ();

	gtk_tree_path_append_index (retval, ephy_node_get_child_index (model->priv->root, host));
	gtk_tree_path_append_index (retval, ephy_node_get_child_index (host, page));

	return retval;
}

static GtkTreePath *
ephy_history_model_get_path (GtkTreeModel *tree_model,
			     GtkTreeIter *iter)
{
	EphyHistoryModel *model = EPHY_HISTORY_MODEL (tree_model);
	EphyNode *node;

	g_return_val_if_fail (EPHY_IS_HISTORY_MODEL (tree_model), NULL);
	g_return_val_if_fail (iter != NULL, NULL);
	g_return_val_if_fail (iter->user_data != NULL, NULL);
	g_return_val_if_fail (iter->stamp == model->stamp, NULL);

	node = EPHY_NODE (iter->user_data);

	if (node == model->priv->root)
		return gtk_tree_path_new ();

	return get_one_level_path_real (model, node);
}

static void
get_property_as_date (EphyNode *node,
		      int id,
		      GValue *value)
{
	GTime time;
	char s[50];
	GDate *date;

	time = ephy_node_get_property_int (node, id);
	date = g_date_new ();
	g_date_set_time (date, time);
	g_date_strftime (s, 50, "%c", date);

	g_value_init (value, G_TYPE_STRING);
	g_value_set_string (value, s);

	g_date_free (date);
}

static void
ephy_history_model_get_value (GtkTreeModel *tree_model,
			      GtkTreeIter *iter,
			      int column,
			      GValue *value)
{
	EphyHistoryModel *model = EPHY_HISTORY_MODEL (tree_model);
	EphyNode *node;

	g_return_if_fail (EPHY_IS_HISTORY_MODEL (tree_model));
	g_return_if_fail (iter != NULL);
	g_return_if_fail (iter->stamp == model->stamp);
	g_return_if_fail (EPHY_IS_NODE (iter->user_data));
	g_return_if_fail (column < EPHY_HISTORY_MODEL_NUM_COLUMNS);

	node = EPHY_NODE (iter->user_data);

	if (ephy_node_get_property_int (node, EPHY_NODE_PAGE_PROP_HOST_ID) < 0 &&
	    (column == EPHY_HISTORY_MODEL_COL_LOCATION ||
	     column == EPHY_HISTORY_MODEL_COL_FIRST_VISIT ||
	     column == EPHY_HISTORY_MODEL_COL_LAST_VISIT ||
	     column == EPHY_HISTORY_MODEL_COL_VISITS))
	{
		g_value_init (value, G_TYPE_STRING);
		g_value_set_string (value, "");
		return;
	}

	switch (column)
	{
	case EPHY_HISTORY_MODEL_COL_TITLE:
		ephy_node_get_property (node,
					EPHY_NODE_PAGE_PROP_TITLE,
				        value);
		break;
	case EPHY_HISTORY_MODEL_COL_LOCATION:
		ephy_node_get_property (node,
				        EPHY_NODE_PAGE_PROP_LOCATION,
				        value);
		break;
	case EPHY_HISTORY_MODEL_COL_VISITS:
		ephy_node_get_property (node,
				        EPHY_NODE_PAGE_PROP_VISITS,
				        value);
		break;
	case EPHY_HISTORY_MODEL_COL_FIRST_VISIT:
		get_property_as_date (node,
				      EPHY_NODE_PAGE_PROP_FIRST_VISIT,
				      value);
		break;
	case EPHY_HISTORY_MODEL_COL_LAST_VISIT:
		get_property_as_date (node,
				      EPHY_NODE_PAGE_PROP_LAST_VISIT,
				      value);
		break;
	case EPHY_HISTORY_MODEL_COL_VISIBLE:
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
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}

static gboolean
ephy_history_model_iter_next (GtkTreeModel *tree_model,
			      GtkTreeIter *iter)
{
	EphyHistoryModel *model = EPHY_HISTORY_MODEL (tree_model);
	EphyNode *node;

	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (iter->user_data != NULL, FALSE);
	g_return_val_if_fail (iter->stamp == EPHY_HISTORY_MODEL (tree_model)->stamp, FALSE);

	node = EPHY_NODE (iter->user_data);

	iter->user_data = ephy_node_get_next_child
		(get_parent_node (model, node), node);

	return (iter->user_data != NULL);
}

static gboolean
ephy_history_model_iter_children (GtkTreeModel *tree_model,
			          GtkTreeIter *iter,
			          GtkTreeIter *parent)
{
	EphyHistoryModel *model = EPHY_HISTORY_MODEL (tree_model);
	EphyNode *node;

	node = ensure_iter (model, parent);

	iter->user_data = ephy_node_get_nth_child (node, 0);
	iter->stamp = model->stamp;

	return (iter->user_data != NULL);
}

static gboolean
ephy_history_model_iter_has_child (GtkTreeModel *tree_model,
			           GtkTreeIter *iter)
{
	EphyHistoryModel *model = EPHY_HISTORY_MODEL (tree_model);
	EphyNode *node;
	int host_id;

	node = EPHY_NODE (iter->user_data);
	host_id = ephy_node_get_property_int (node, EPHY_NODE_PAGE_PROP_HOST_ID);
	if (host_id < 0)
	{
		return ephy_node_has_child (model->priv->root, node);
	}
	else
	{
		return FALSE;
	}
}

static int
ephy_history_model_iter_n_children (GtkTreeModel *tree_model,
			            GtkTreeIter *iter)
{
	EphyHistoryModel *model = EPHY_HISTORY_MODEL (tree_model);
	EphyNode *node;

	g_return_val_if_fail (iter == NULL || iter->user_data != NULL, FALSE);

	node = ensure_iter (model, iter);

	return ephy_node_get_n_children (node);
}

static gboolean
ephy_history_model_iter_nth_child (GtkTreeModel *tree_model,
			           GtkTreeIter *iter,
			           GtkTreeIter *parent,
			           int n)
{
	EphyHistoryModel *model = EPHY_HISTORY_MODEL (tree_model);
	EphyNode *node;

	node = ensure_iter (model, parent);

	iter->user_data = ephy_node_get_nth_child (node, n);
	iter->stamp = model->stamp;

	return (iter->user_data != NULL);
}

static gboolean
ephy_history_model_iter_parent (GtkTreeModel *tree_model,
			        GtkTreeIter *iter,
			        GtkTreeIter *child)
{
	EphyHistoryModel *model = EPHY_HISTORY_MODEL (tree_model);
	EphyNode *parent, *node;

	node = EPHY_NODE (iter->user_data);
	parent = get_parent_node (model, node);

	if (parent != model->priv->root)
	{
		iter->user_data = parent;
		iter->stamp = model->stamp;
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

EphyNode *
ephy_history_model_node_from_iter (EphyHistoryModel *model,
				   GtkTreeIter *iter)
{
	return EPHY_NODE (iter->user_data);
}

void
ephy_history_model_iter_from_node (EphyHistoryModel *model,
				   EphyNode *node,
				   GtkTreeIter *iter)
{
	iter->stamp = model->stamp;
	iter->user_data = node;
}

static inline void
ephy_history_model_update_node (EphyHistoryModel *model,
				EphyNode *node,
				int idx)
{
	GtkTreePath *path;
	GtkTreeIter iter;

	ephy_history_model_iter_from_node (model, node, &iter);

	if (idx >= 0)
	{
		path = gtk_tree_path_new ();
		gtk_tree_path_append_index (path, idx);
	}
	else
	{
		path = get_one_level_path_real (model, node);
	}

	gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
	gtk_tree_path_free (path);
}

static void
root_child_removed_cb (EphyNode *node,
		       EphyNode *child,
		       EphyHistoryModel *model)
{
	GtkTreePath *path;

	if (node == model->priv->root)
	{
		path = get_one_level_path_real (model, child);
	}
	else
	{
		path = get_path_real (model, child);
	}

	if (path)
	{
		gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);
		gtk_tree_path_free (path);
	}
}

static void
root_child_added_cb (EphyNode *node,
		     EphyNode *child,
		     EphyHistoryModel *model)
{
	GtkTreePath *path;
	GtkTreeIter iter;

	ephy_history_model_iter_from_node (model, child, &iter);

	if (node == model->priv->root)
	{
		path = get_one_level_path_real (model, child);
	}
	else
	{
		path = get_path_real (model, child);
	}

	gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), path, &iter);
	gtk_tree_path_free (path);
}

static void
root_child_changed_cb (EphyNode *node,
		       EphyNode *child,
		       EphyHistoryModel *model)
{
	ephy_history_model_update_node (model, child, -1);
}

GType
ephy_history_model_column_get_type (void)
{
	static GType etype = 0;

	if (etype == 0)
	{
		static const GEnumValue values[] =
		{
			{ EPHY_HISTORY_MODEL_COL_TITLE,    "EPHY_HISTORY_MODEL_COL_TITLE", "title" },
			{ EPHY_HISTORY_MODEL_COL_LOCATION, "EPHY_HISTORY_MODEL_COL_LOCATION", "location" },
			{ EPHY_HISTORY_MODEL_COL_VISITS,   "EPHY_HISTORY_MODEL_COL_VISITS", "visits" },
			{ EPHY_HISTORY_MODEL_COL_FIRST_VISIT,  "EPHY_HISTORY_MODEL_COL_FIRST_VISIT", "last_visit" },
			{ EPHY_HISTORY_MODEL_COL_LAST_VISIT,  "EPHY_HISTORY_MODEL_COL_LAST_VISIT", "last_visit" },
			{ EPHY_HISTORY_MODEL_COL_VISIBLE,  "EPHY_HISTORY_MODEL_COL_FIRST_VISIT", "first_visit" },

			{ 0, 0, 0 }
		};

		etype = g_enum_register_static ("EphyHistoryModelColumn", values);
	}

	return etype;
}

