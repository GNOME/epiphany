/* 
 *  Copyright (C) 2002  Ricardo Fernándezs Pascual <ric@users.sourceforge.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtktreednd.h>
#include <glib-object.h>
#include <string.h>

#include "ephy-toolbar-tree-model.h"

#define NOT_IMPLEMENTED g_warning ("not implemented: " G_STRLOC);
//#define DEBUG_MSG(x) g_print x
#define DEBUG_MSG(x)

#define VALID_ITER(iter, tb_tree_model)	(iter!= NULL && iter->user_data != NULL \
					&& tb_tree_model->stamp == iter->stamp)

/**
 * Private data
 */
struct _EphyTbTreeModelPrivate
{
	EphyToolbar *tb;
	GSList *curr_items;
};

/**
 * Private functions
 */
static void		ephy_tb_tree_model_init			(EphyTbTreeModel *tb_tree_model);
static void		ephy_tb_tree_model_class_init		(EphyTbTreeModelClass *tb_tree_model_class);
static void		ephy_tb_tree_model_tb_tree_model_init	(GtkTreeModelIface *iface);
static void		ephy_tb_tree_model_drag_source_init	(GtkTreeDragSourceIface *iface);
static void		ephy_tb_tree_model_drag_dest_init	(GtkTreeDragDestIface *iface);
static void		ephy_tb_tree_model_finalize_impl		(GObject *object);
static guint		ephy_tb_tree_model_get_flags_impl	(GtkTreeModel *tb_tree_model);
static gint		ephy_tb_tree_model_get_n_columns_impl	(GtkTreeModel *tb_tree_model);
static GType		ephy_tb_tree_model_get_column_type_impl	(GtkTreeModel *tb_tree_model,
								 gint index);
static gboolean		ephy_tb_tree_model_get_iter_impl	(GtkTreeModel *tb_tree_model,
								 GtkTreeIter *iter,
								 GtkTreePath *path);
static GtkTreePath *	ephy_tb_tree_model_get_path_impl	(GtkTreeModel *tb_tree_model,
								 GtkTreeIter *iter);
static void		ephy_tb_tree_model_get_value_impl	(GtkTreeModel *tb_tree_model,
								 GtkTreeIter *iter,
								 gint column,
								 GValue *value);
static gboolean		ephy_tb_tree_model_iter_next_impl	(GtkTreeModel *tb_tree_model,
								 GtkTreeIter *iter);
static gboolean		ephy_tb_tree_model_iter_children_impl	(GtkTreeModel *tb_tree_model,
								 GtkTreeIter *iter,
								 GtkTreeIter *parent);
static gboolean		ephy_tb_tree_model_iter_has_child_impl	(GtkTreeModel *tb_tree_model,
								 GtkTreeIter *iter);
static gint		ephy_tb_tree_model_iter_n_children_impl	(GtkTreeModel *tb_tree_model,
								 GtkTreeIter *iter);
static gboolean		ephy_tb_tree_model_iter_nth_child_impl	(GtkTreeModel *tb_tree_model,
								 GtkTreeIter *iter,
								 GtkTreeIter *parent,
								 gint n);
static gboolean		ephy_tb_tree_model_iter_parent_impl	(GtkTreeModel *tb_tree_model,
								 GtkTreeIter *iter,
								 GtkTreeIter *child);

/* DND interfaces */
static gboolean		ephy_tb_tree_model_drag_data_delete_impl(GtkTreeDragSource *drag_source,
								 GtkTreePath *path);
static gboolean		ephy_tb_tree_model_drag_data_get_impl	(GtkTreeDragSource *drag_source,
								 GtkTreePath *path,
								 GtkSelectionData *selection_data);
static gboolean		ephy_tb_tree_model_drag_data_received_impl (GtkTreeDragDest *drag_dest,
								    GtkTreePath *dest,
								    GtkSelectionData *selection_data);
static gboolean		ephy_tb_tree_model_row_drop_possible_impl (GtkTreeDragDest *drag_dest,
								   GtkTreePath *dest_path,
								   GtkSelectionData *selection_data);

/* helper functions */
static void		ephy_tb_tree_model_toolbar_changed_cb	(EphyToolbar *tb, EphyTbTreeModel *tm);
static void		ephy_tb_tree_model_update		(EphyTbTreeModel *tm);


static GObjectClass *parent_class = NULL;

GtkType
ephy_tb_tree_model_get_type (void)
{
	static GType tb_tree_model_type = 0;

	if (!tb_tree_model_type)
	{
		static const GTypeInfo tb_tree_model_info =
			{
				sizeof (EphyTbTreeModelClass),
				NULL,		/* base_init */
				NULL,		/* base_finalize */
				(GClassInitFunc) ephy_tb_tree_model_class_init,
				NULL,		/* class_finalize */
				NULL,		/* class_data */
				sizeof (EphyTbTreeModel),
				0, /* n_preallocs */
				(GInstanceInitFunc) ephy_tb_tree_model_init
			};

		static const GInterfaceInfo tb_gtk_tree_model_info =
			{
				(GInterfaceInitFunc) ephy_tb_tree_model_tb_tree_model_init,
				NULL,
				NULL
			};

		static const GInterfaceInfo drag_source_info =
			{
				(GInterfaceInitFunc) ephy_tb_tree_model_drag_source_init,
				NULL,
				NULL
			};

		static const GInterfaceInfo drag_dest_info =
			{
				(GInterfaceInitFunc) ephy_tb_tree_model_drag_dest_init,
				NULL,
				NULL
			};

		tb_tree_model_type = g_type_register_static (G_TYPE_OBJECT, "EphyTbTreeModel",
							     &tb_tree_model_info, 0);

		g_type_add_interface_static (tb_tree_model_type,
					     GTK_TYPE_TREE_MODEL,
					     &tb_gtk_tree_model_info);
		g_type_add_interface_static (tb_tree_model_type,
					     GTK_TYPE_TREE_DRAG_SOURCE,
					     &drag_source_info);
		g_type_add_interface_static (tb_tree_model_type,
					     GTK_TYPE_TREE_DRAG_DEST,
					     &drag_dest_info);
	}

	return tb_tree_model_type;
}

static void
ephy_tb_tree_model_class_init (EphyTbTreeModelClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	object_class = (GObjectClass *) class;

	object_class->finalize = ephy_tb_tree_model_finalize_impl;
}

static void
ephy_tb_tree_model_tb_tree_model_init (GtkTreeModelIface *iface)
{
	iface->get_flags = ephy_tb_tree_model_get_flags_impl;
	iface->get_n_columns = ephy_tb_tree_model_get_n_columns_impl;
	iface->get_column_type = ephy_tb_tree_model_get_column_type_impl;
	iface->get_iter = ephy_tb_tree_model_get_iter_impl;
	iface->get_path = ephy_tb_tree_model_get_path_impl;
	iface->get_value = ephy_tb_tree_model_get_value_impl;
	iface->iter_next = ephy_tb_tree_model_iter_next_impl;
	iface->iter_children = ephy_tb_tree_model_iter_children_impl;
	iface->iter_has_child = ephy_tb_tree_model_iter_has_child_impl;
	iface->iter_n_children = ephy_tb_tree_model_iter_n_children_impl;
	iface->iter_nth_child = ephy_tb_tree_model_iter_nth_child_impl;
	iface->iter_parent = ephy_tb_tree_model_iter_parent_impl;
}

static void
ephy_tb_tree_model_drag_source_init (GtkTreeDragSourceIface *iface)
{
	iface->drag_data_delete = ephy_tb_tree_model_drag_data_delete_impl;
	iface->drag_data_get = ephy_tb_tree_model_drag_data_get_impl;
}

static void
ephy_tb_tree_model_drag_dest_init (GtkTreeDragDestIface *iface)
{
	iface->drag_data_received = ephy_tb_tree_model_drag_data_received_impl;
	iface->row_drop_possible = ephy_tb_tree_model_row_drop_possible_impl;
}

static void
ephy_tb_tree_model_init (EphyTbTreeModel *tb_tree_model)
{
	EphyTbTreeModelPrivate *p = g_new0 (EphyTbTreeModelPrivate, 1);
	tb_tree_model->priv = p;

	do
	{
		tb_tree_model->stamp = g_random_int ();
	}
	while (tb_tree_model->stamp == 0);
}

EphyTbTreeModel *
ephy_tb_tree_model_new (void)
{
	EphyTbTreeModel *ret = EPHY_TB_TREE_MODEL (g_object_new (EPHY_TYPE_TB_TREE_MODEL, NULL));
	return ret;
}


void
ephy_tb_tree_model_set_toolbar (EphyTbTreeModel *tm, EphyToolbar *tb)
{
	EphyTbTreeModelPrivate *p;

	g_return_if_fail (EPHY_IS_TB_TREE_MODEL (tm));
	g_return_if_fail (EPHY_IS_TOOLBAR (tb));

	p = tm->priv;

	if (p->tb)
	{
		g_signal_handlers_disconnect_matched (p->tb, G_SIGNAL_MATCH_DATA, 0, 0,
						      NULL, NULL, tm);
		g_object_unref (p->tb);
	}

	p->tb = g_object_ref (tb);
	g_signal_connect (p->tb, "changed", G_CALLBACK (ephy_tb_tree_model_toolbar_changed_cb), tm);

	ephy_tb_tree_model_update (tm);
}

static void
ephy_tb_tree_model_finalize_impl (GObject *object)
{
	EphyTbTreeModel *tm = EPHY_TB_TREE_MODEL (object);
	EphyTbTreeModelPrivate *p = tm->priv;

	DEBUG_MSG (("Finalizing a EphyTbTreeModel\n"));

	if (p->tb)
	{
		g_signal_handlers_disconnect_matched (p->tb, G_SIGNAL_MATCH_DATA, 0, 0,
						      NULL, NULL, tm);
		g_object_unref (p->tb);
	}

	g_slist_foreach (p->curr_items, (GFunc) g_object_unref, NULL);
	g_slist_free (p->curr_items);
	g_free (p);

	(* parent_class->finalize) (object);
}

/* fulfill the GtkTreeModel requirements */

static guint
ephy_tb_tree_model_get_flags_impl (GtkTreeModel *tb_tree_model)
{
	return 0;
}

static gint
ephy_tb_tree_model_get_n_columns_impl (GtkTreeModel *tb_tree_model)
{
	return EPHY_TB_TREE_MODEL_NUM_COLUMS;
}

static GType
ephy_tb_tree_model_get_column_type_impl (GtkTreeModel *tb_tree_model,
					gint index)
{
	g_return_val_if_fail (EPHY_IS_TB_TREE_MODEL (tb_tree_model), G_TYPE_INVALID);
	g_return_val_if_fail ((index < EPHY_TB_TREE_MODEL_NUM_COLUMS) && (index >= 0), G_TYPE_INVALID);

	switch (index)
	{
	case EPHY_TB_TREE_MODEL_COL_ICON:
		return GDK_TYPE_PIXBUF;
		break;
	case EPHY_TB_TREE_MODEL_COL_NAME:
		return G_TYPE_STRING;
		break;
	default:
		g_assert_not_reached ();
		return G_TYPE_INVALID;
		break;
	}
}

static gboolean
ephy_tb_tree_model_get_iter_impl (GtkTreeModel *tree_model,
				 GtkTreeIter *iter,
				 GtkTreePath *path)
{
	EphyTbTreeModel *tb_tree_model = (EphyTbTreeModel *) tree_model;
	EphyTbTreeModelPrivate *p;
	GSList *li;
	gint i;

	g_return_val_if_fail (EPHY_IS_TB_TREE_MODEL (tb_tree_model), FALSE);
	g_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

	p = tb_tree_model->priv;
	i = gtk_tree_path_get_indices (path)[0];
	li = g_slist_nth (p->curr_items, i);

	if (!li)
	{
		return FALSE;
	}

	iter->stamp = tb_tree_model->stamp;
	iter->user_data = li;

	return TRUE;
}

static GtkTreePath *
ephy_tb_tree_model_get_path_impl (GtkTreeModel *tree_model,
				 GtkTreeIter *iter)
{
	EphyTbTreeModel *tb_tree_model = (EphyTbTreeModel *) tree_model;
	EphyTbTreeModelPrivate *p;
	gint i;

	g_return_val_if_fail (EPHY_IS_TB_TREE_MODEL (tb_tree_model), NULL);
	g_return_val_if_fail (iter != NULL, NULL);
	g_return_val_if_fail (iter->user_data != NULL, NULL);
	g_return_val_if_fail (iter->stamp == tb_tree_model->stamp, NULL);

	p = tb_tree_model->priv;

	i = g_slist_position (p->curr_items, iter->user_data);
	if (i < 0)
	{
		return NULL;
	}
	else
	{
		GtkTreePath *retval;
		retval = gtk_tree_path_new ();
		gtk_tree_path_append_index (retval, i);
		return retval;
	}
}


static void
ephy_tb_tree_model_get_value_impl (GtkTreeModel *tb_tree_model,
				  GtkTreeIter *iter,
				  gint column,
				  GValue *value)
{
	EphyTbItem *it;
	GdkPixbuf *pb;
	gchar *s;

	g_return_if_fail (EPHY_IS_TB_TREE_MODEL (tb_tree_model));
	g_return_if_fail (iter != NULL);
	g_return_if_fail (iter->stamp == EPHY_TB_TREE_MODEL (tb_tree_model)->stamp);
	g_return_if_fail (EPHY_IS_TB_ITEM (((GSList *) iter->user_data)->data));
	g_return_if_fail (column < EPHY_TB_TREE_MODEL_NUM_COLUMS);

	it = ((GSList *) iter->user_data)->data;

	switch (column) {
	case EPHY_TB_TREE_MODEL_COL_ICON:
		g_value_init (value, GDK_TYPE_PIXBUF);
		pb = ephy_tb_item_get_icon (it);
		g_value_set_object (value, pb);
		break;
	case EPHY_TB_TREE_MODEL_COL_NAME:
		g_value_init (value, G_TYPE_STRING);
		s = ephy_tb_item_get_name_human (it);
		g_value_set_string_take_ownership (value, s);
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}

static gboolean
ephy_tb_tree_model_iter_next_impl (GtkTreeModel *tree_model,
				  GtkTreeIter *iter)
{
	g_return_val_if_fail (EPHY_IS_TB_TREE_MODEL (tree_model), FALSE);
	g_return_val_if_fail (EPHY_TB_TREE_MODEL (tree_model)->stamp == iter->stamp, FALSE);

	iter->user_data = ((GSList *) (iter->user_data))->next;
	return (iter->user_data != NULL);
}

static gboolean
ephy_tb_tree_model_iter_children_impl (GtkTreeModel *tb_tree_model,
				      GtkTreeIter *iter,
				      GtkTreeIter *parent)
{
	if (parent)
	{
		/* this is a list, nodes have no children */
		return FALSE;
	}
	else
	{
		/* but if parent == NULL we return the list itself as children of the
		 * "root"
		 */
		EphyTbTreeModel *tm = EPHY_TB_TREE_MODEL (tb_tree_model);
		EphyTbTreeModelPrivate *p = tm->priv;

		iter->stamp = tm->stamp;
		iter->user_data = p->curr_items;
		return (p->curr_items != NULL);
	}
}

static gboolean
ephy_tb_tree_model_iter_has_child_impl (GtkTreeModel *tb_tree_model,
				       GtkTreeIter *iter)
{
	return FALSE;
}

static gint
ephy_tb_tree_model_iter_n_children_impl (GtkTreeModel *tb_tree_model,
					GtkTreeIter *iter)
{
	EphyTbTreeModel *tm = (EphyTbTreeModel *) tb_tree_model;
	EphyTbTreeModelPrivate *p;
	g_return_val_if_fail (EPHY_IS_TB_TREE_MODEL (tm), -1);

	p = tm->priv;

	if (iter == NULL)
	{
		return g_slist_length (p->curr_items);
	}

	g_return_val_if_fail (tm->stamp == iter->stamp, -1);
	return 0;
}

static gboolean
ephy_tb_tree_model_iter_nth_child_impl (GtkTreeModel *tb_tree_model,
				       GtkTreeIter *iter,
				       GtkTreeIter *parent,
				       gint n)
{
	EphyTbTreeModel *tm = (EphyTbTreeModel *) tb_tree_model;
	EphyTbTreeModelPrivate *p;
	g_return_val_if_fail (EPHY_IS_TB_TREE_MODEL (tm), FALSE);

	p = tm->priv;

	if (parent)
	{
		return FALSE;
	}
	else
	{
		GSList *li = g_slist_nth (p->curr_items, n);

		if (li)
		{
			iter->stamp = tm->stamp;
			iter->user_data = li;
			return TRUE;
		}
		else
		{
			return FALSE;
		}
	}
}

static gboolean
ephy_tb_tree_model_iter_parent_impl (GtkTreeModel *tb_tree_model,
				    GtkTreeIter *iter,
				    GtkTreeIter *child)
{
	return FALSE;
}



/* DND */


static gboolean
ephy_tb_tree_model_drag_data_delete_impl (GtkTreeDragSource *drag_source,
					 GtkTreePath *path)
{
	GtkTreeIter iter;
	EphyTbTreeModel *tm;

	g_return_val_if_fail (EPHY_IS_TB_TREE_MODEL (drag_source), FALSE);

	tm = EPHY_TB_TREE_MODEL (drag_source);

	DEBUG_MSG (("in ephy_tb_tree_model_drag_data_delete_impl\n"));

	if (ephy_tb_tree_model_get_iter_impl (GTK_TREE_MODEL (tm), &iter, path))
	{
		EphyTbTreeModelPrivate *p = tm->priv;
		EphyTbItem *it = ephy_tb_tree_model_item_from_iter (tm, &iter);
		EphyTbItem *delete_hack;
		if ((delete_hack = g_object_get_data (G_OBJECT (tm),
						      "gul-toolbar-tree-model-dnd-delete-hack")) != NULL)
		{
			g_return_val_if_fail (EPHY_IS_TB_ITEM (delete_hack), FALSE);
			g_object_ref (delete_hack);

			g_object_set_data (G_OBJECT (tm),
					   "gul-toolbar-tree-model-dnd-delete-hack", NULL);

			if (!strcmp (delete_hack->id, it->id))
			{
				g_object_unref (delete_hack);
				return FALSE;
			}
			g_object_unref (delete_hack);
		}

		ephy_toolbar_remove_item (p->tb, it);
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

static gboolean
ephy_tb_tree_model_drag_data_get_impl (GtkTreeDragSource *drag_source,
				      GtkTreePath *path,
				      GtkSelectionData *selection_data)
{
	g_return_val_if_fail (EPHY_IS_TB_TREE_MODEL (drag_source), FALSE);

	/* Note that we don't need to handle the GTK_TB_TREE_MODEL_ROW
	 * target, because the default handler does it for us, but
	 * we do anyway for the convenience of someone maybe overriding the
	 * default handler.
	 */

	if (gtk_tree_set_row_drag_data (selection_data,
					GTK_TREE_MODEL (drag_source),
					path))
	{
		return TRUE;
	}
	else
	{
		/* to string ? */
	}

	return FALSE;
}


static gboolean
ephy_tb_tree_model_drag_data_received_impl (GtkTreeDragDest *drag_dest,
					   GtkTreePath *dest,
					   GtkSelectionData *selection_data)
{
	EphyTbTreeModel *tbm;
	GtkTreeModel *src_model = NULL;
	GtkTreePath *src_path = NULL;

	g_return_val_if_fail (EPHY_IS_TB_TREE_MODEL (drag_dest), FALSE);
	g_return_val_if_fail (gtk_tree_path_get_depth (dest) == 1, FALSE);

	tbm = EPHY_TB_TREE_MODEL (drag_dest);

	DEBUG_MSG (("in ephy_tb_tree_model_drag_data_received_impl\n"));

	if (gtk_tree_get_row_drag_data (selection_data,
					&src_model,
					&src_path)
	    && EPHY_IS_TB_TREE_MODEL (src_model))
	{
		/* copy the item */

		GtkTreeIter src_iter;
		EphyTbItem *it;
		int idx = gtk_tree_path_get_indices (dest)[0];

		if (!gtk_tree_model_get_iter (src_model,
					      &src_iter,
					      src_path))
		{
			gtk_tree_path_free (src_path);
			return FALSE;
		}
		gtk_tree_path_free (src_path);

		if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (drag_dest),
							"gtk-tree-model-drop-append")))
		{
			++idx;
		}

		it = ephy_tb_item_clone (EPHY_TB_ITEM (((GSList *)src_iter.user_data)->data));
		ephy_toolbar_add_item (tbm->priv->tb, it, idx);

		/* hack */
		if (src_model == GTK_TREE_MODEL (drag_dest)
		    && ephy_toolbar_get_check_unique (EPHY_TB_TREE_MODEL (src_model)->priv->tb)
		    && ephy_tb_item_is_unique (it))
		{
			g_object_set_data_full (G_OBJECT (src_model),
						"gul-toolbar-tree-model-dnd-delete-hack", it,
						g_object_unref);
		}
		else
		{
			g_object_unref (it);
		}

		g_object_set_data (G_OBJECT (drag_dest), "gtk-tree-model-drop-append", NULL);
		return TRUE;
	}

	return FALSE;
}

static gboolean
ephy_tb_tree_model_row_drop_possible_impl (GtkTreeDragDest *drag_dest,
					  GtkTreePath *dest_path,
					  GtkSelectionData *selection_data)
{
	GtkTreeModel *src_model = NULL;
	GtkTreePath *src_path = NULL;
	gboolean retval = FALSE;
	EphyTbTreeModel *tm;
	EphyTbTreeModelPrivate *p;

	g_return_val_if_fail (EPHY_IS_TB_TREE_MODEL (drag_dest), FALSE);
	tm = EPHY_TB_TREE_MODEL (drag_dest);
	p = tm->priv;

	if (gtk_tree_path_get_depth (dest_path) != 1)
	{
		return FALSE;
	}
	if (!gtk_tree_get_row_drag_data (selection_data,
					 &src_model,
					 &src_path))
	{
		return FALSE;
	}

	/* can drop before any existing node, or before one past any existing. */

	retval = (gtk_tree_path_get_indices (dest_path)[0] <= (gint) g_slist_length (p->curr_items));

	gtk_tree_path_free (src_path);

	return retval;
}


EphyTbItem *
ephy_tb_tree_model_item_from_iter (EphyTbTreeModel *tm, GtkTreeIter *iter)
{
	return iter ? EPHY_TB_ITEM (((GSList *) iter->user_data)->data) : NULL;
}

static void
ephy_tb_tree_model_toolbar_changed_cb (EphyToolbar *tb, EphyTbTreeModel *tm)
{
	ephy_tb_tree_model_update (tm);
}

static void
ephy_tb_tree_model_update (EphyTbTreeModel *tm)
{
	EphyTbTreeModelPrivate *p;
	GSList *new_items;
	GSList *old_items;
	GSList *li;
	GSList *lj;
	int i;

	g_return_if_fail (EPHY_IS_TB_TREE_MODEL (tm));
	p = tm->priv;
	g_return_if_fail (EPHY_IS_TOOLBAR (p->tb));

	old_items = p->curr_items;
	new_items = g_slist_copy ((GSList *) ephy_toolbar_get_item_list (p->tb));
	g_slist_foreach (new_items, (GFunc) g_object_ref, NULL);
	p->curr_items = new_items;

	li = new_items;
	lj = old_items;
	i = 0;

	while (li && lj)
	{
		if (li->data == lj->data)
		{
			li = li->next;
			lj = lj->next;
			++i;
		}
		else if (lj->next && lj->next->data == li->data)
		{
			GtkTreePath *p = gtk_tree_path_new ();
			gtk_tree_path_append_index (p, i);
			gtk_tree_model_row_deleted (GTK_TREE_MODEL (tm), p);
			gtk_tree_path_free (p);
			lj = lj->next;
		}
		else if (li->next && li->next->data == lj->data)
		{
			GtkTreePath *p = gtk_tree_path_new ();
			GtkTreeIter iter;
			iter.stamp = tm->stamp;
			iter.user_data = li;
			gtk_tree_path_append_index (p, i);
			gtk_tree_model_row_inserted (GTK_TREE_MODEL (tm), p, &iter);
			gtk_tree_path_free (p);
			li = li->next;
			++i;
		}
		else
		{
			GtkTreePath *p = gtk_tree_path_new ();
			GtkTreeIter iter;
			iter.stamp = tm->stamp;
			iter.user_data = li;
			gtk_tree_path_append_index (p, i);
			gtk_tree_model_row_deleted (GTK_TREE_MODEL (tm), p);
			gtk_tree_model_row_inserted (GTK_TREE_MODEL (tm), p, &iter);
			gtk_tree_path_free (p);
			lj = lj->next;
			li = li->next;
			++i;
		}
	}

	while (li)
	{
		GtkTreePath *p = gtk_tree_path_new ();
		GtkTreeIter iter;
		iter.stamp = tm->stamp;
		iter.user_data = li;
		gtk_tree_path_append_index (p, i);
		gtk_tree_model_row_inserted (GTK_TREE_MODEL (tm), p, &iter);
		gtk_tree_path_free (p);
		li = li->next;
		++i;
	}

	while (lj)
	{
		GtkTreePath *p = gtk_tree_path_new ();
		gtk_tree_path_append_index (p, i);
		gtk_tree_model_row_deleted (GTK_TREE_MODEL (tm), p);
		gtk_tree_path_free (p);
		lj = lj->next;
	}

	g_slist_foreach (old_items, (GFunc) g_object_unref, NULL);
	g_slist_free (old_items);
}

