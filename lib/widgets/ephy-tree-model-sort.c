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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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

#define EPHY_TREE_MODEL_SORT_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_TREE_MODEL_SORT, EphyTreeModelSortPrivate))

struct EphyTreeModelSortPrivate
{
	char *str_list;
	int drag_column_id;
};

static GObjectClass *parent_class = NULL;

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

	g_type_class_add_private (object_class, sizeof (EphyTreeModelSortPrivate));
}

static void
ephy_tree_model_sort_init (EphyTreeModelSort *ma)
{
	ma->priv = EPHY_TREE_MODEL_SORT_GET_PRIVATE (ma);

	ma->priv->drag_column_id = -1;
}

static void
ephy_tree_model_sort_finalize (GObject *object)
{
	EphyTreeModelSort *model = EPHY_TREE_MODEL_SORT (object);

	g_free (model->priv->str_list);

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
	return (EPHY_TREE_MODEL_SORT (drag_source)->priv->drag_column_id > 0);
}

void
ephy_tree_model_sort_set_column_id (EphyTreeModelSort *ms,
				    int id)
{
	ms->priv->drag_column_id = id;
}

static gboolean
ephy_tree_model_sort_multi_drag_data_delete (EggTreeMultiDragSource *drag_source,
					     GList *path_list)
{
	return TRUE;
}

static void
each_property_get_data_binder (EphyDragEachSelectedItemDataGet iteratee,
			       gpointer iterator_context, gpointer data)
{
	gpointer *context = (gpointer *) iterator_context;
	GList *path_list = (GList *) (context[0]);
	GList *i;
	EphyTreeModelSort *model = EPHY_TREE_MODEL_SORT (context[1]);
	GValue value = {0, };

	for (i = path_list; i != NULL; i = i->next)
	{
		GtkTreeIter iter;
		GtkTreePath *path = NULL;
		const char *svalue;

		path = gtk_tree_row_reference_get_path (i->data);
		gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &iter, path);
		gtk_tree_model_get_value (GTK_TREE_MODEL (model), &iter,
					  model->priv->drag_column_id,
					  &value);
		svalue = g_value_get_string (&value);

		g_return_if_fail (svalue != NULL);

		LOG ("Data get %s", svalue)

		iteratee (svalue, NULL, data);

		gtk_tree_path_free (path);
		g_value_unset (&value);
	}
}

static gboolean
ephy_tree_model_sort_multi_drag_data_get (EggTreeMultiDragSource *drag_source,
					  GList *path_list,
					  GtkSelectionData *selection_data)
{
	gpointer icontext[2];

	icontext[0] = path_list;
	icontext[1] = drag_source;

	ephy_dnd_drag_data_get (NULL, NULL, selection_data,
				0, &icontext, each_property_get_data_binder);

	return TRUE;
}
