/*  Rhythmbox.
 *  Copyright (C) 2002 Olivier Martin <omartin@ifrance.com>
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

#include <gtk/gtkmarshal.h>
#include <gtk/gtktreednd.h>
#include <string.h>

#include "ephy-node.h"
#include "ephy-tree-model-sort.h"
#include "eggtreemultidnd.h"
#include "ephy-dnd.h"
#include "ephy-marshal.h"
#include "ephy-debug.h"

static void ephy_tree_model_sort_class_init (EphyTreeModelSortClass *klass);
static void ephy_tree_model_sort_init (EphyTreeModelSort *ma);
static void ephy_tree_model_sort_finalize (GObject *object);
static void ephy_tree_model_sort_multi_drag_source_init (EggTreeMultiDragSourceIface *iface);
static gboolean ephy_tree_model_sort_multi_row_draggable (EggTreeMultiDragSource *drag_source,
							  GList *path_list);
static gboolean ephy_tree_model_sort_multi_drag_data_get (EggTreeMultiDragSource *drag_source,
							  GList *path_list,
							  GtkSelectionData *selection_data);
static gboolean ephy_tree_model_sort_multi_drag_data_delete (EggTreeMultiDragSource *drag_source,
							     GList *path_list);

struct EphyTreeModelSortPrivate
{
	char *str_list;
	guint drag_property_id;
};

enum
{
	NODE_FROM_ITER,
	LAST_SIGNAL
};

static GObjectClass *parent_class = NULL;

static guint ephy_tree_model_sort_signals[LAST_SIGNAL] = { 0 };

GType
ephy_tree_model_sort_get_type (void)
{
	static GType ephy_tree_model_sort_type = 0;

	if (ephy_tree_model_sort_type == 0)
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyTreeModelSortClass),
			NULL, /* base init */
			NULL, /* base finalize */
			(GClassInitFunc) ephy_tree_model_sort_class_init,
			NULL, /* class finalize */
			NULL, /* class data */
			sizeof (EphyTreeModelSort),
			0, /* n_preallocs */
			(GInstanceInitFunc) ephy_tree_model_sort_init
		};
		static const GInterfaceInfo multi_drag_source_info =
		{
			(GInterfaceInitFunc) ephy_tree_model_sort_multi_drag_source_init,
			NULL,
			NULL
		};

		ephy_tree_model_sort_type = g_type_register_static (GTK_TYPE_TREE_MODEL_SORT,
								  "EphyTreeModelSort",
								  &our_info, 0);

		g_type_add_interface_static (ephy_tree_model_sort_type,
					     EGG_TYPE_TREE_MULTI_DRAG_SOURCE,
					     &multi_drag_source_info);
	}

	return ephy_tree_model_sort_type;
}

static void
ephy_tree_model_sort_class_init (EphyTreeModelSortClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = ephy_tree_model_sort_finalize;

	ephy_tree_model_sort_signals[NODE_FROM_ITER] =
		g_signal_new ("node_from_iter",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyTreeModelSortClass, node_from_iter),
			      NULL, NULL,
			      ephy_marshal_VOID__POINTER_POINTER,
			      G_TYPE_NONE,
			      2,
			      G_TYPE_POINTER,
			      G_TYPE_POINTER);
}

static void
ephy_tree_model_sort_init (EphyTreeModelSort *ma)
{
	ma->priv = g_new0 (EphyTreeModelSortPrivate, 1);

	ma->priv->drag_property_id = -1;
}

static void
ephy_tree_model_sort_finalize (GObject *object)
{
	EphyTreeModelSort *model;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EPHY_IS_TREE_MODEL_SORT (object));

	model = EPHY_TREE_MODEL_SORT (object);

	g_free (model->priv->str_list);
	g_free (model->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GtkTreeModel*
ephy_tree_model_sort_new (GtkTreeModel *child_model)
{
	GtkTreeModel *model;

	g_return_val_if_fail (child_model != NULL, NULL);

	model = GTK_TREE_MODEL (g_object_new (EPHY_TYPE_TREE_MODEL_SORT,
					      "model", child_model,
					      NULL));

	return model;
}

static void
ephy_tree_model_sort_multi_drag_source_init (EggTreeMultiDragSourceIface *iface)
{
	iface->row_draggable    = ephy_tree_model_sort_multi_row_draggable;
	iface->drag_data_get    = ephy_tree_model_sort_multi_drag_data_get;
	iface->drag_data_delete = ephy_tree_model_sort_multi_drag_data_delete;
}

static gboolean
ephy_tree_model_sort_multi_row_draggable (EggTreeMultiDragSource *drag_source, GList *path_list)
{
	GList *l;

	for (l = path_list; l != NULL; l = g_list_next (l))
	{
		GtkTreeIter iter;
		GtkTreePath *path;
		EphyNode *node = NULL;

		path = gtk_tree_row_reference_get_path (l->data);
		gtk_tree_model_get_iter (GTK_TREE_MODEL (drag_source), &iter, path);
		g_signal_emit (G_OBJECT (drag_source),
			       ephy_tree_model_sort_signals[NODE_FROM_ITER],
			       0, &iter, &node);

		if (node == NULL)
		{
			return FALSE;
		}
	}

	return TRUE;
}

void
ephy_tree_model_sort_set_drag_property (EphyTreeModelSort *ms,
					guint id)
{
	ms->priv->drag_property_id = id;
}

static gboolean
ephy_tree_model_sort_multi_drag_data_delete (EggTreeMultiDragSource *drag_source,
					     GList *path_list)
{
	return TRUE;
}

static void
each_url_get_data_binder (EphyDragEachSelectedItemDataGet iteratee,
			  gpointer iterator_context, gpointer data)
{
	gpointer *context = (gpointer *) iterator_context;
	GList *path_list = (GList *) (context[0]);
	GList *i;
	GtkTreeModel *model = GTK_TREE_MODEL (context[1]);

	for (i = path_list; i != NULL; i = i->next)
	{
		GtkTreeIter iter;
		GtkTreePath *path = gtk_tree_row_reference_get_path (i->data);
		EphyNode *node = NULL;
		const char *value;

		gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &iter, path);
		g_signal_emit (G_OBJECT (model),
			       ephy_tree_model_sort_signals[NODE_FROM_ITER],
			       0, &iter, &node);

		if (node == NULL)
			return;

		value = ephy_node_get_property_string
			(node, EPHY_TREE_MODEL_SORT (model)->priv->drag_property_id);

		LOG ("Data get %s", value)

		iteratee (value, -1, -1, -1, -1, data);
	}
}

static void
each_node_get_data_binder (EphyDragEachSelectedItemDataGet iteratee,
			   gpointer iterator_context, gpointer data)
{
	gpointer *context = (gpointer *) iterator_context;
	GList *path_list = (GList *) (context[0]);
	GList *i;
	GtkTreeModel *model = GTK_TREE_MODEL (context[1]);

	for (i = path_list; i != NULL; i = i->next)
	{
		GtkTreeIter iter;
		GtkTreePath *path = gtk_tree_row_reference_get_path (i->data);
		EphyNode *node = NULL;
		EphyNodeDb *db;
		char *value;

		gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &iter, path);
		g_signal_emit (G_OBJECT (model),
			       ephy_tree_model_sort_signals[NODE_FROM_ITER],
			       0, &iter, &node);
		if (node == NULL)
			return;

		db = ephy_node_get_db (node);
		value = g_strdup_printf ("%s;%ld",
					 ephy_node_db_get_name (db),
					 ephy_node_get_id (node));
		iteratee (value, -1, -1, -1, -1, data);
		g_free (value);
	}
}

static gboolean
ephy_tree_model_sort_multi_drag_data_get (EggTreeMultiDragSource *drag_source,
					  GList *path_list,
					  GtkSelectionData *selection_data)
{
	GdkAtom target;

	target = selection_data->target;

	if (target == gdk_atom_intern (EPHY_DND_BOOKMARK_TYPE, FALSE) ||
	    target == gdk_atom_intern (EPHY_DND_TOPIC_TYPE, FALSE))
	{
		gpointer icontext[2];

		icontext[0] = path_list;
		icontext[1] = drag_source;

		ephy_dnd_drag_data_get (NULL, NULL, selection_data,
			0, &icontext, each_node_get_data_binder);

	}
	else
	{
		gpointer icontext[2];

		icontext[0] = path_list;
		icontext[1] = drag_source;

		ephy_dnd_drag_data_get (NULL, NULL, selection_data,
			0, &icontext, each_url_get_data_binder);
	}

	return TRUE;
}
