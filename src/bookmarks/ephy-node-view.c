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
 */

#include <gtk/gtktreeview.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtktreeviewcolumn.h>
#include <gtk/gtkcellrenderertext.h>
#include <libgnome/gnome-i18n.h>

#include "eggtreemodelfilter.h"
#include "ephy-tree-model-node.h"
#include "ephy-node-view.h"
#include "ephy-tree-model-sort.h"
#include "eggtreemultidnd.h"
#include "ephy-dnd.h"

static void ephy_node_view_class_init (EphyNodeViewClass *klass);
static void ephy_node_view_init (EphyNodeView *view);
static void ephy_node_view_finalize (GObject *object);
static void ephy_node_view_set_property (GObject *object,
				         guint prop_id,
				         const GValue *value,
				         GParamSpec *pspec);
static void ephy_node_view_get_property (GObject *object,
				         guint prop_id,
				         GValue *value,
				         GParamSpec *pspec);

struct EphyNodeViewPrivate
{
	EphyNode *root;

	EphyTreeModelNode *nodemodel;
	GtkTreeModel *filtermodel;
	GtkTreeModel *sortmodel;

	EphyNodeFilter *filter;

	GtkWidget *treeview;
};

enum
{
	NODE_ACTIVATED,
	NODE_SELECTED,
	SHOW_POPUP,
	LAST_SIGNAL
};

enum
{
	PROP_0,
	PROP_ROOT,
	PROP_FILTER
};

static GObjectClass *parent_class = NULL;

static guint ephy_node_view_signals[LAST_SIGNAL] = { 0 };

GType
ephy_node_view_get_type (void)
{
	static GType ephy_node_view_type = 0;

	if (ephy_node_view_type == 0)
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyNodeViewClass),
			NULL,
			NULL,
			(GClassInitFunc) ephy_node_view_class_init,
			NULL,
			NULL,
			sizeof (EphyNodeView),
			0,
			(GInstanceInitFunc) ephy_node_view_init
		};

		ephy_node_view_type = g_type_register_static (GTK_TYPE_SCROLLED_WINDOW,
							      "EphyNodeView",
							      &our_info, 0);
	}

	return ephy_node_view_type;
}

static void
ephy_node_view_class_init (EphyNodeViewClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = ephy_node_view_finalize;

	object_class->set_property = ephy_node_view_set_property;
	object_class->get_property = ephy_node_view_get_property;

	g_object_class_install_property (object_class,
					 PROP_ROOT,
					 g_param_spec_object ("root",
							      "Root node",
							      "Root node",
							      EPHY_TYPE_NODE,
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class,
					 PROP_FILTER,
					 g_param_spec_object ("filter",
							      "Filter object",
							      "Filter object",
							      EPHY_TYPE_NODE_FILTER,
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	ephy_node_view_signals[NODE_ACTIVATED] =
		g_signal_new ("node_activated",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyNodeViewClass, node_activated),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      EPHY_TYPE_NODE);
	ephy_node_view_signals[NODE_SELECTED] =
		g_signal_new ("node_selected",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyNodeViewClass, node_selected),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      EPHY_TYPE_NODE);
	ephy_node_view_signals[SHOW_POPUP] =
		g_signal_new ("show_popup",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyNodeViewClass, show_popup),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);
}

static void
ephy_node_view_finalize (GObject *object)
{
	EphyNodeView *view;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EPHY_IS_NODE_VIEW (object));

	view = EPHY_NODE_VIEW (object);

	g_return_if_fail (view->priv != NULL);

	g_object_unref (G_OBJECT (view->priv->sortmodel));
	g_object_unref (G_OBJECT (view->priv->filtermodel));
	g_object_unref (G_OBJECT (view->priv->nodemodel));

	g_free (view->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
filter_changed_cb (EphyNodeFilter *filter,
		   EphyNodeView *view)
{
	GtkWidget *window;

	g_return_if_fail (EPHY_IS_NODE_VIEW (view));

	window = gtk_widget_get_toplevel (GTK_WIDGET (view));

	if (window != NULL && window->window != NULL)
	{
		/* nice busy cursor */
		GdkCursor *cursor;

		cursor = gdk_cursor_new (GDK_WATCH);
		gdk_window_set_cursor (window->window, cursor);
		gdk_cursor_unref (cursor);

		gdk_flush ();

		gdk_window_set_cursor (window->window, NULL);

		/* no flush: this will cause the cursor to be reset
		 * only when the UI is free again */
	}
}

static void
ephy_node_view_selection_changed_cb (GtkTreeSelection *selection,
			             EphyNodeView *view)
{
	GList *list;
	EphyNode *node = NULL;

	list = ephy_node_view_get_selection (view);
	if (list)
	{
		node = EPHY_NODE (list->data);
	}
	g_list_free (list);

	g_signal_emit (G_OBJECT (view), ephy_node_view_signals[NODE_SELECTED], 0, node);
}

static void
ephy_node_view_row_activated_cb (GtkTreeView *treeview,
			         GtkTreePath *path,
			         GtkTreeViewColumn *column,
			         EphyNodeView *view)
{
	GtkTreeIter iter, iter2;
	EphyNode *node;

	gtk_tree_model_get_iter (view->priv->sortmodel, &iter, path);
	gtk_tree_model_sort_convert_iter_to_child_iter
		(GTK_TREE_MODEL_SORT (view->priv->sortmodel), &iter2, &iter);
	egg_tree_model_filter_convert_iter_to_child_iter
		(EGG_TREE_MODEL_FILTER (view->priv->filtermodel), &iter, &iter2);

	node = ephy_tree_model_node_node_from_iter (view->priv->nodemodel, &iter);

	g_signal_emit (G_OBJECT (view), ephy_node_view_signals[NODE_ACTIVATED], 0, node);
}

static gboolean
ephy_node_view_button_press_cb (GtkTreeView *treeview,
			        GdkEventButton *event,
			        EphyNodeView *view)
{
	if (event->button == 3)
	{
		g_signal_emit (G_OBJECT (view), ephy_node_view_signals[SHOW_POPUP], 0);
	}

	return FALSE;
}

static void
ephy_node_view_set_property (GObject *object,
			     guint prop_id,
			     const GValue *value,
			     GParamSpec *pspec)
{
	EphyNodeView *view = EPHY_NODE_VIEW (object);

	switch (prop_id)
	{
	case PROP_ROOT:
		view->priv->root = g_value_get_object (value);
		break;
	case PROP_FILTER:
		view->priv->filter = g_value_get_object (value);

		if (view->priv->filter != NULL)
		{
			g_signal_connect_object (G_OBJECT (view->priv->filter),
					         "changed",
					         G_CALLBACK (filter_changed_cb),
					         G_OBJECT (view),
						 0);
		}
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
ephy_node_view_get_property (GObject *object,
			     guint prop_id,
			     GValue *value,
			     GParamSpec *pspec)
{
	EphyNodeView *view = EPHY_NODE_VIEW (object);

	switch (prop_id)
	{
	case PROP_ROOT:
		g_value_set_object (value, view->priv->root);
		break;
	case PROP_FILTER:
		g_value_set_object (value, view->priv->filter);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
node_from_sort_iter_cb (EphyTreeModelSort *model,
		        GtkTreeIter *iter,
		        void **node,
		        EphyNodeView *view)
{
	GtkTreeIter filter_iter, node_iter;

	gtk_tree_model_sort_convert_iter_to_child_iter (GTK_TREE_MODEL_SORT (model),
							&filter_iter, iter);
	egg_tree_model_filter_convert_iter_to_child_iter (EGG_TREE_MODEL_FILTER (view->priv->filtermodel),
							  &node_iter, &filter_iter);
	*node = ephy_tree_model_node_node_from_iter
		(EPHY_TREE_MODEL_NODE (view->priv->nodemodel), &node_iter);
}

static void
ephy_node_view_construct (EphyNodeView *view)
{
	GtkTreeSelection *selection;


	view->priv->nodemodel = ephy_tree_model_node_new (view->priv->root,
							  view->priv->filter);
	view->priv->filtermodel = egg_tree_model_filter_new (GTK_TREE_MODEL (view->priv->nodemodel),
							     NULL);
	egg_tree_model_filter_set_visible_column (EGG_TREE_MODEL_FILTER (view->priv->filtermodel),
						  EPHY_TREE_MODEL_NODE_COL_VISIBLE);
	view->priv->sortmodel = ephy_tree_model_sort_new (view->priv->filtermodel);
	g_signal_connect_object (G_OBJECT (view->priv->sortmodel),
				 "node_from_iter",
				 G_CALLBACK (node_from_sort_iter_cb),
				 view,
				 0);
	view->priv->treeview = gtk_tree_view_new_with_model
		(GTK_TREE_MODEL (view->priv->sortmodel));
	gtk_widget_show (view->priv->treeview);
	g_signal_connect_object (G_OBJECT (view->priv->treeview),
			         "button_press_event",
			         G_CALLBACK (ephy_node_view_button_press_cb),
			         view,
				 0);
	g_signal_connect_object (G_OBJECT (view->priv->treeview),
			         "row_activated",
			         G_CALLBACK (ephy_node_view_row_activated_cb),
			         view,
				 0);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view->priv->treeview));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
	g_signal_connect_object (G_OBJECT (selection),
			         "changed",
			         G_CALLBACK (ephy_node_view_selection_changed_cb),
			         view,
				 0);

	gtk_container_add (GTK_CONTAINER (view), view->priv->treeview);
}

EphyNodeView *
ephy_node_view_new (EphyNode *root,
		    EphyNodeFilter *filter)
{
	EphyNodeView *view;

	view = EPHY_NODE_VIEW (g_object_new (EPHY_TYPE_NODE_VIEW,
					     "filter", filter,
					     "hadjustment", NULL,
					     "vadjustment", NULL,
					     "hscrollbar_policy", GTK_POLICY_AUTOMATIC,
					     "vscrollbar_policy", GTK_POLICY_AUTOMATIC,
					     "shadow_type", GTK_SHADOW_IN,
					     "root", root,
					     NULL));

	ephy_node_view_construct (view);

	g_return_val_if_fail (view->priv != NULL, NULL);

	return view;
}

void
ephy_node_view_add_column (EphyNodeView *view,
			   const char  *title,
			   EphyTreeModelNodeColumn column,
			   gboolean sortable)
{
	GtkTreeViewColumn *gcolumn;
	GtkCellRenderer *renderer;

	gcolumn = (GtkTreeViewColumn *) gtk_tree_view_column_new ();
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (gcolumn, renderer, TRUE);
	gtk_tree_view_column_set_attributes (gcolumn, renderer,
					     "text", column,
					     NULL);
	gtk_tree_view_column_set_sizing (gcolumn,
					 GTK_TREE_VIEW_COLUMN_AUTOSIZE);
	gtk_tree_view_column_set_title (gcolumn, title);
	gtk_tree_view_append_column (GTK_TREE_VIEW (view->priv->treeview),
				     gcolumn);
	if (sortable)
	{
		gtk_tree_view_column_set_sort_column_id (gcolumn, column);
	}
}

static void
ephy_node_view_init (EphyNodeView *view)
{
	view->priv = g_new0 (EphyNodeViewPrivate, 1);
}

static void
get_selection (GtkTreeModel *model,
	       GtkTreePath *path,
	       GtkTreeIter *iter,
	       void **data)
{
	GtkTreeModelSort *sortmodel = GTK_TREE_MODEL_SORT (model);
	EggTreeModelFilter *filtermodel = EGG_TREE_MODEL_FILTER (sortmodel->child_model);
	EphyTreeModelNode *nodemodel = EPHY_TREE_MODEL_NODE (filtermodel->child_model);
	GList **list = (GList **) data;
	GtkTreeIter *iter2 = gtk_tree_iter_copy (iter);
	GtkTreeIter iter3;
	GtkTreeIter iter4;
	EphyNode *node;

	gtk_tree_model_sort_convert_iter_to_child_iter (GTK_TREE_MODEL_SORT (model),
							&iter3, iter2);
	egg_tree_model_filter_convert_iter_to_child_iter (filtermodel, &iter4, &iter3);

	node = ephy_tree_model_node_node_from_iter (nodemodel, &iter4);

	gtk_tree_iter_free (iter2);

	*list = g_list_prepend (*list, node);
}

GList *
ephy_node_view_get_selection (EphyNodeView *view)
{
	GList *list = NULL;
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection
		(GTK_TREE_VIEW (view->priv->treeview));

	gtk_tree_selection_selected_foreach (selection,
					     (GtkTreeSelectionForeachFunc) get_selection,
					     (void **) &list);

	return list;
}

void
ephy_node_view_remove (EphyNodeView *view)
{
	GList *list;

	list = ephy_node_view_get_selection (view);

	for (; list != NULL; list = list->next)
	{
		ephy_node_unref (EPHY_NODE (list->data));
	}

	g_list_free (list);
}

void
ephy_node_view_set_browse_mode (EphyNodeView *view)
{
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view->priv->treeview));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
}

void
ephy_node_view_select_node (EphyNodeView *view,
			    EphyNode *node)
{
	GtkTreeIter iter, iter2;
	GValue val = { 0, };
	gboolean visible;
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view->priv->treeview));

	g_return_if_fail (node != NULL);

	ephy_tree_model_node_iter_from_node (EPHY_TREE_MODEL_NODE (view->priv->nodemodel),
					     node, &iter);
	gtk_tree_model_get_value (GTK_TREE_MODEL (view->priv->nodemodel), &iter,
				  EPHY_TREE_MODEL_NODE_COL_VISIBLE, &val);
	visible = g_value_get_boolean (&val);
	g_value_unset (&val);
	if (visible == FALSE) return;

	egg_tree_model_filter_convert_child_iter_to_iter (EGG_TREE_MODEL_FILTER (view->priv->filtermodel),
							  &iter2, &iter);
	gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (view->priv->sortmodel),
							&iter, &iter2);

	gtk_tree_selection_select_iter (selection, &iter);
}

void
ephy_node_view_enable_drag_source (EphyNodeView *view)
{
	g_return_if_fail (view != NULL);

	egg_tree_multi_drag_add_drag_support (GTK_TREE_VIEW (view->priv->treeview));
	ephy_dnd_enable_model_drag_source (GTK_WIDGET (view->priv->treeview));
}
