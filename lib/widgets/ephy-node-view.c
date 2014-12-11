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
 */

#include "config.h"

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "ephy-node-view.h"
#include "ephy-tree-model-sort.h"
#include "eggtreemultidnd.h"
#include "ephy-dnd.h"
#include "ephy-gui.h"
#include <string.h>

/**
 * SECTION:ephy-node-view
 * @short_description: A #GtkTreeView displaying a collection of #EphyNode elements.
 *
 * #EphyNodeView implements a #GtkTreeView displaying a given set of #EphyNode
 * elements. It implements drag and dropping.
 */

static void ephy_node_view_class_init (EphyNodeViewClass *klass);
static void ephy_node_view_init (EphyNodeView *view);
static void ephy_node_view_finalize (GObject *object);

#define EPHY_NODE_VIEW_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_NODE_VIEW, EphyNodeViewPrivate))

struct _EphyNodeViewPrivate
{
	EphyNode *root;

	EphyTreeModelNode *nodemodel;
	GtkTreeModel *filtermodel;
	GtkTreeModel *sortmodel;
	GtkCellRenderer *editable_renderer;
	GtkTreeViewColumn *editable_column;
	int editable_node_column;
	int toggle_column;

	EphyNodeFilter *filter;

	GtkTargetList *drag_targets;

	int sort_column;
	GtkSortType sort_type;
	guint priority_prop_id;
	int priority_column;

	EphyNode *edited_node;
	gboolean remove_if_cancelled;
	int editable_property;

	gboolean drag_started;
	int drag_button;
	int drag_x;
	int drag_y;
	GtkTargetList *source_target_list;

	gboolean drop_occurred;
	gboolean have_drag_data;
	GtkSelectionData *drag_data;
	guint scroll_id;

	guint changing_selection : 1;
};

enum
{
	NODE_TOGGLED,
	NODE_ACTIVATED,
	NODE_SELECTED,
	NODE_DROPPED,
	NODE_MIDDLE_CLICKED,
	LAST_SIGNAL
};

enum
{
	PROP_0,
	PROP_ROOT,
	PROP_FILTER
};

#define AUTO_SCROLL_MARGIN 20

static GObjectClass *parent_class = NULL;

static guint ephy_node_view_signals[LAST_SIGNAL] = { 0 };

GType
ephy_node_view_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		const GTypeInfo our_info =
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

		type = g_type_register_static (GTK_TYPE_TREE_VIEW,
					       "EphyNodeView",
					       &our_info, 0);
	}

	return type;
}

static void
ephy_node_view_finalize (GObject *object)
{
	EphyNodeView *view = EPHY_NODE_VIEW (object);

	g_object_unref (G_OBJECT (view->priv->sortmodel));
	g_object_unref (G_OBJECT (view->priv->filtermodel));
	g_object_unref (G_OBJECT (view->priv->nodemodel));

	if (view->priv->source_target_list)
	{
		gtk_target_list_unref (view->priv->source_target_list);
	}

	if (view->priv->drag_targets)
	{
		gtk_target_list_unref (view->priv->drag_targets);
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static EphyNode *
get_node_from_path (EphyNodeView *view, GtkTreePath *path)
{
	EphyNode *node;
	GtkTreeIter iter, iter2, iter3;

	if (path == NULL) return NULL;

	gtk_tree_model_get_iter (view->priv->sortmodel, &iter, path);
	gtk_tree_model_sort_convert_iter_to_child_iter
		(GTK_TREE_MODEL_SORT (view->priv->sortmodel), &iter2, &iter);

	if (iter2.stamp == 0) {
		return NULL;
	}
	gtk_tree_model_filter_convert_iter_to_child_iter
		(GTK_TREE_MODEL_FILTER (view->priv->filtermodel), &iter3, &iter2);

	node = ephy_tree_model_node_node_from_iter (view->priv->nodemodel, &iter3);

	return node;
}

static void
gtk_tree_view_vertical_autoscroll (GtkTreeView *tree_view)
{
	GdkRectangle visible_rect;
	GtkAdjustment *vadjustment;
	GdkWindow *window;
	int y;
	int offset;
	float value;
	
	window = gtk_tree_view_get_bin_window (tree_view);
	vadjustment = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (tree_view));

	gdk_window_get_device_position (window,
					gdk_device_manager_get_client_pointer (
						gdk_display_get_device_manager (
							gtk_widget_get_display (GTK_WIDGET (tree_view)))),
					NULL, &y, NULL);

	y += gtk_adjustment_get_value (vadjustment);

	gtk_tree_view_get_visible_rect (tree_view, &visible_rect);
	
	offset = y - (visible_rect.y + 2 * AUTO_SCROLL_MARGIN);
	if (offset > 0)
	{
		offset = y - (visible_rect.y + visible_rect.height - 2 * AUTO_SCROLL_MARGIN);
		if (offset < 0)
		{
			return;
		}
	}

	value = CLAMP (gtk_adjustment_get_value (vadjustment) + offset, 0.0,
		       gtk_adjustment_get_upper (vadjustment) - gtk_adjustment_get_page_size (vadjustment));
	gtk_adjustment_set_value (vadjustment, value);
}

static int
scroll_timeout (gpointer data)
{
	GtkTreeView *tree_view = GTK_TREE_VIEW (data);
	
	gtk_tree_view_vertical_autoscroll (tree_view);

	return TRUE;
}

static void
remove_scroll_timeout (EphyNodeView *view)
{
	if (view->priv->scroll_id)
	{
		g_source_remove (view->priv->scroll_id);
		view->priv->scroll_id = 0;
	}
}

static void
set_drag_dest_row (EphyNodeView *view,
		   GtkTreePath *path)
{
	if (path)
	{
		gtk_tree_view_set_drag_dest_row
			(GTK_TREE_VIEW (view),
			 path,
			 GTK_TREE_VIEW_DROP_INTO_OR_BEFORE);
	}
	else
	{
		gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW (view), 
						 NULL, 
						 0);
	}
}

static void
clear_drag_dest_row (EphyNodeView *view)
{
	gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW (view), NULL, 0);
}

static void
get_drag_data (EphyNodeView *view,
	       GdkDragContext *context, 
	       guint32 time)
{
	GdkAtom target;
	
	target = gtk_drag_dest_find_target (GTK_WIDGET (view), 
					    context, 
					    NULL);

	gtk_drag_get_data (GTK_WIDGET (view),
			   context, target, time);
}

static void
free_drag_data (EphyNodeView *view)
{
	view->priv->have_drag_data = FALSE;

	if (view->priv->drag_data)
	{
		gtk_selection_data_free (view->priv->drag_data);
		view->priv->drag_data = NULL;
	}
}

static gboolean
drag_motion_cb (GtkWidget *widget,
		GdkDragContext *context,
		int x,
		int y,
		guint32 time,
		EphyNodeView *view)
{
	EphyNode *node;
	GdkAtom target;
	GtkTreePath *path;
	GtkTreeViewDropPosition pos;
	guint action = 0;
	int priority;

	gtk_tree_view_get_dest_row_at_pos (GTK_TREE_VIEW (widget),
					   x, y, &path, &pos);
	
	if (!view->priv->have_drag_data)
	{
		get_drag_data (view, context, time);
	}

	target = gtk_drag_dest_find_target (widget, context, NULL);
	node = get_node_from_path (view, path);

	if (target != GDK_NONE && node != NULL)
	{
		priority = ephy_node_get_property_int
				(node, view->priv->priority_prop_id);

		if (priority != EPHY_NODE_VIEW_ALL_PRIORITY &&
		    priority != EPHY_NODE_VIEW_SPECIAL_PRIORITY &&
		    ephy_node_get_is_drag_source (node))
		{
			action = gdk_drag_context_get_suggested_action (context);
		}
	}
	
	if (action)
	{
		set_drag_dest_row (view, path);
	}
	else
	{
		clear_drag_dest_row (view);
	}
	
	if (path)
	{
		gtk_tree_path_free (path);
	}
	
	if (view->priv->scroll_id == 0)
	{
		view->priv->scroll_id = 
			g_timeout_add (150, 
				       scroll_timeout, 
				       GTK_TREE_VIEW (view));
		g_source_set_name_by_id (view->priv->scroll_id, "[epiphany] scroll_timeout");
	}

	gdk_drag_status (context, action, time);

	return TRUE;
}

static void
drag_leave_cb (GtkWidget *widget,
	       GdkDragContext *context,
	       guint32 time,
	       EphyNodeView *view)
{
	clear_drag_dest_row (view);

	free_drag_data (view);

	remove_scroll_timeout (view);
}

static void
drag_data_received_cb (GtkWidget *widget,
		       GdkDragContext *context,
		       int x,
		       int y,
		       GtkSelectionData *selection_data,
		       guint info,
		       guint32 time,
		       EphyNodeView *view)
{
	GtkTreeViewDropPosition pos;

	/* x and y here are valid only on drop ! */

	if ((gtk_selection_data_get_length (selection_data) <= 0) ||
	    (gtk_selection_data_get_data (selection_data) == NULL))
	{
		return;
	}	

	/* appease GtkTreeView by preventing its drag_data_receive
	* from being called */
	g_signal_stop_emission_by_name (view, "drag_data_received");

	if (!view->priv->have_drag_data)
	{
		view->priv->have_drag_data = TRUE;
		view->priv->drag_data = 
			gtk_selection_data_copy (selection_data);
	}

	if (view->priv->drop_occurred)
	{
		EphyNode *node;
		char **uris;
		gboolean success = FALSE;
		GtkTreePath *path;

		if (gtk_tree_view_get_dest_row_at_pos
			(GTK_TREE_VIEW (widget), x, y, &path, &pos) == FALSE)
		{
			return;
		}

		node = get_node_from_path (view, path);
		if (node == NULL) return;

		uris = gtk_selection_data_get_uris (selection_data);

		if (uris != NULL && ephy_node_get_is_drag_dest (node))
		{
			/* FIXME fill success */
			g_signal_emit (G_OBJECT (view),
				       ephy_node_view_signals[NODE_DROPPED], 0,
				       node, uris);
			g_strfreev (uris);

		}

		view->priv->drop_occurred = FALSE;
		free_drag_data (view);
		gtk_drag_finish (context, success, FALSE, time);

		if (path)
		{
			gtk_tree_path_free (path);
		}
	}
}

static gboolean
drag_drop_cb (GtkWidget *widget,
	      GdkDragContext *context,
	      int x, 
	      int y,
	      guint32 time,
	      EphyNodeView *view)
{
	view->priv->drop_occurred = TRUE;

	get_drag_data (view, context, time);
	remove_scroll_timeout (view);
	clear_drag_dest_row (view);
	
	return TRUE;
}

/**
 * ephy_node_view_enable_drag_dest:
 * @view: the #EphyNodeView
 * @types: types allowed as #GtkTargetEntry
 * @n_types: count of @types
 *
 * Set the types of drag destination allowed by @view.
 *
 **/
void
ephy_node_view_enable_drag_dest (EphyNodeView *view,
				 const GtkTargetEntry *types,
				 int n_types)
{
	GtkWidget *treeview;

	g_return_if_fail (view != NULL);

	treeview = GTK_WIDGET (view);

	gtk_drag_dest_set (GTK_WIDGET (treeview),
			   0, types, n_types,
			   GDK_ACTION_COPY);
	view->priv->drag_targets = gtk_target_list_new (types, n_types);

	g_signal_connect (treeview, "drag_data_received",
			  G_CALLBACK (drag_data_received_cb), view);
	g_signal_connect (treeview, "drag_drop",
			  G_CALLBACK (drag_drop_cb), view);
	g_signal_connect (treeview, "drag_motion",
			  G_CALLBACK (drag_motion_cb), view);
	g_signal_connect (treeview, "drag_leave",
			  G_CALLBACK (drag_leave_cb), view);
}

static void
filter_changed_cb (EphyNodeFilter *filter,
		   EphyNodeView *view)
{
	GtkWidget *window;
	GdkWindow *gdk_window;

	g_return_if_fail (EPHY_IS_NODE_VIEW (view));

	window = gtk_widget_get_toplevel (GTK_WIDGET (view));
	gdk_window = gtk_widget_get_window (window);

	if (window != NULL && gdk_window != NULL)
	{
		/* nice busy cursor */
		GdkCursor *cursor;

		cursor = gdk_cursor_new (GDK_WATCH);
		gdk_window_set_cursor (gdk_window, cursor);
		g_object_unref (cursor);

		gdk_flush ();

		gdk_window_set_cursor (gdk_window, NULL);

		/* no flush: this will cause the cursor to be reset
		 * only when the UI is free again */
	}

	gtk_tree_model_filter_refilter
			(GTK_TREE_MODEL_FILTER (view->priv->filtermodel));
}

static void
ephy_node_view_selection_changed_cb (GtkTreeSelection *selection,
				     EphyNodeView *view)
{
	EphyNodeViewPrivate *priv = view->priv;
	GList *list;
	EphyNode *node = NULL;

	/* Work around bug #346662 */
	if (priv->changing_selection) return;

	list = ephy_node_view_get_selection (view);
	if (list)
	{
		node = list->data;
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
	gtk_tree_model_filter_convert_iter_to_child_iter
		(GTK_TREE_MODEL_FILTER (view->priv->filtermodel), &iter, &iter2);

	node = ephy_tree_model_node_node_from_iter (view->priv->nodemodel, &iter);

	g_signal_emit (G_OBJECT (view), ephy_node_view_signals[NODE_ACTIVATED], 0, node);
}

static void
path_toggled (GtkTreeModel *dummy_model, GtkTreePath *path,
	      GtkTreeIter *dummy, gpointer data)
{
	EphyNodeView *view = EPHY_NODE_VIEW (data);
	gboolean checked;
	EphyNode *node;
	GtkTreeIter iter, iter2;
	GValue value = {0, };

	gtk_tree_model_get_iter (view->priv->sortmodel, &iter, path);
	gtk_tree_model_sort_convert_iter_to_child_iter
		(GTK_TREE_MODEL_SORT (view->priv->sortmodel), &iter2, &iter);
	gtk_tree_model_filter_convert_iter_to_child_iter
		(GTK_TREE_MODEL_FILTER (view->priv->filtermodel), &iter, &iter2);

	node = ephy_tree_model_node_node_from_iter (view->priv->nodemodel, &iter);
	gtk_tree_model_get_value (GTK_TREE_MODEL (view->priv->nodemodel), &iter,
				  view->priv->toggle_column, &value);
	checked = !g_value_get_boolean (&value);

	g_signal_emit (G_OBJECT (view), ephy_node_view_signals[NODE_TOGGLED], 0,
		       node, checked);
}

static EphyNode *
process_middle_click (GtkTreePath *path,
		      EphyNodeView *view)
{
	EphyNode *node;
	GtkTreeIter iter, iter2;
	
	gtk_tree_model_get_iter (view->priv->sortmodel, &iter, path);
	gtk_tree_model_sort_convert_iter_to_child_iter
		(GTK_TREE_MODEL_SORT (view->priv->sortmodel), &iter2, &iter);
	gtk_tree_model_filter_convert_iter_to_child_iter
		(GTK_TREE_MODEL_FILTER (view->priv->filtermodel), &iter, &iter2);

	node = ephy_tree_model_node_node_from_iter (view->priv->nodemodel, &iter);
	
	return node;
}

static gboolean
ephy_node_view_key_press_cb (GtkTreeView *treeview,
			     GdkEventKey *event,
			     EphyNodeView *view)
{
	gboolean handled = FALSE;

	if (event->keyval == GDK_KEY_space ||
	    event->keyval == GDK_KEY_Return ||
	    event->keyval == GDK_KEY_KP_Enter ||
	    event->keyval == GDK_KEY_ISO_Enter)
	{
		if (view->priv->toggle_column >= 0)
		{
			GtkTreeSelection *selection;

			selection = gtk_tree_view_get_selection (treeview);
			gtk_tree_selection_selected_foreach
					(selection, path_toggled, view);
			handled = TRUE;
		}
	}

	return handled;
}

static void
selection_foreach (GtkTreeModel *model,
		   GtkTreePath *path,
		   GtkTreeIter *iter,
		   gpointer data)
{
	GList **list;

	list = (GList**)data;

	*list = g_list_prepend (*list,
				gtk_tree_row_reference_new (model, path));
}

static GList *
get_selection_refs (GtkTreeView *tree_view)
{
	GtkTreeSelection *selection;
	GList *ref_list = NULL;

	selection = gtk_tree_view_get_selection (tree_view);
	gtk_tree_selection_selected_foreach (selection,
					     selection_foreach,
					     &ref_list);
	ref_list = g_list_reverse (ref_list);
	return ref_list;
}

static void
ref_list_free (GList *ref_list)
{
	g_list_foreach (ref_list, (GFunc) gtk_tree_row_reference_free, NULL);
	g_list_free (ref_list);
}

static void
stop_drag_check (EphyNodeView *view)
{
	view->priv->drag_button = 0;
}

static gboolean
button_event_modifies_selection (GdkEventButton *event)
{
	return (event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) != 0;
}

static void
did_not_drag (EphyNodeView *view,
	      GdkEventButton *event)
{
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;
	GtkTreePath *path;

	tree_view = GTK_TREE_VIEW (view);
	selection = gtk_tree_view_get_selection (tree_view);

	if (gtk_tree_view_get_path_at_pos (tree_view, event->x, event->y,
					   &path, NULL, NULL, NULL))
	{
		if((event->button == 1 || event->button == 2) &&
		   gtk_tree_selection_path_is_selected (selection, path) &&
		   !button_event_modifies_selection (event))
		{
			if (gtk_tree_selection_get_mode (selection) == GTK_SELECTION_MULTIPLE)
			{
				gtk_tree_selection_unselect_all (selection);
			}

			gtk_tree_selection_select_path (selection, path);
		}

		gtk_tree_path_free (path);
	}
}

typedef struct
{
	EphyNodeView *view;
	gboolean result;
}
ForeachData;

static void
check_node_is_drag_source (GtkTreeModel *model,
			   GtkTreePath *path,
			   GtkTreeIter *iter,
			   ForeachData *data)
{
	EphyNode *node;

	node = get_node_from_path (data->view, path);
	data->result = data->result &&
		       node != NULL &&
		       ephy_node_get_is_drag_source (node);
}

static gboolean
can_drag_selection (EphyNodeView *view)
{
	GtkTreeView *tree_view = GTK_TREE_VIEW (view);
	GtkTreeSelection *selection;
	ForeachData data = { view, TRUE };

	selection = gtk_tree_view_get_selection (tree_view);
	gtk_tree_selection_selected_foreach (selection,
					     (GtkTreeSelectionForeachFunc) check_node_is_drag_source,
					     &data);

	return data.result;
}

static void
drag_data_get_cb (GtkWidget *widget,
		  GdkDragContext *context,
		  GtkSelectionData *selection_data,
		  guint info,
		  guint time)
{
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GList *ref_list;

	tree_view = GTK_TREE_VIEW (widget);

	model = gtk_tree_view_get_model (tree_view);
	g_return_if_fail (model != NULL);

	ref_list = g_object_get_data (G_OBJECT (context), "drag-info");

	if (ref_list == NULL)
	{
		return;
	}

	if (EGG_IS_TREE_MULTI_DRAG_SOURCE (model))
	{
		egg_tree_multi_drag_source_drag_data_get (EGG_TREE_MULTI_DRAG_SOURCE (model),
							  ref_list,
							  selection_data);
	}
}

static gboolean
button_release_cb (GtkWidget *widget,
		   GdkEventButton *event,
		   EphyNodeView *view)
{
	if (event->button == view->priv->drag_button)
	{
		stop_drag_check (view);
		if (!view->priv->drag_started)
		{
			did_not_drag (view, event);
			return TRUE;
		}
		view->priv->drag_started = FALSE;
	}
	return FALSE;
}

static gboolean
motion_notify_cb (GtkWidget *widget,
		  GdkEventMotion *event,
		  EphyNodeView *view)
{
	GdkDragContext *context;
	GList *ref_list;

	if (event->window != gtk_tree_view_get_bin_window (GTK_TREE_VIEW (widget)))
	{
		return FALSE;
	}
	if (view->priv->drag_button != 0)
	{
		if (gtk_drag_check_threshold (widget, view->priv->drag_x,
					      view->priv->drag_y, event->x,
					      event->y)
		    && can_drag_selection (view))
		{
			context = gtk_drag_begin
				(widget, view->priv->source_target_list,
				 GDK_ACTION_ASK | GDK_ACTION_COPY | GDK_ACTION_LINK,
				 view->priv->drag_button,
				 (GdkEvent*)event);

			stop_drag_check (view);
			view->priv->drag_started = TRUE;

			ref_list = get_selection_refs (GTK_TREE_VIEW (widget));
			g_object_set_data_full (G_OBJECT (context),
						"drag-info",
						ref_list,
						(GDestroyNotify)ref_list_free);

			gtk_drag_set_icon_default (context);
		}
	}

	return TRUE;
}

static gboolean
ephy_node_view_button_press_cb (GtkWidget *treeview,
				GdkEventButton *event,
				EphyNodeView *view)
{
	GtkTreePath *path = NULL;
	GtkTreeSelection *selection;
	gboolean call_parent = TRUE, path_is_selected;
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
	
	if (event->window != gtk_tree_view_get_bin_window (GTK_TREE_VIEW (treeview)))
	{
		return GTK_WIDGET_CLASS (parent_class)->button_press_event (treeview, event);
	}

	if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (treeview),
					   event->x,
					   event->y,
					   &path,
					   NULL, NULL, NULL))
	{
		path_is_selected = gtk_tree_selection_path_is_selected (selection, path);

		if (!gtk_widget_is_focus (GTK_WIDGET (treeview)))
		{
			gtk_widget_grab_focus (GTK_WIDGET (treeview));
		}

		if (event->button == 3 && path_is_selected)
		{
			call_parent = FALSE;
		}

		if(!button_event_modifies_selection (event) &&
		   event->button == 1 && path_is_selected &&
		   gtk_tree_selection_count_selected_rows (selection) > 1)
		{
			call_parent = FALSE;
		}

		if (call_parent)
		{
			GTK_WIDGET_CLASS (parent_class)->button_press_event (treeview, event);
		}

		if (event->button == 3)
		{
			gboolean retval;

			g_signal_emit_by_name (view, "popup_menu", &retval);
		}
		else if (event->button == 2)
		{
			EphyNode *clicked_node;
			
			clicked_node = process_middle_click (path, view);
			g_signal_emit (G_OBJECT (view),
				       ephy_node_view_signals[NODE_MIDDLE_CLICKED], 0, clicked_node);
		}
		else if (event->button == 1)
		{
			if (view->priv->toggle_column >= 0)
			{
				path_toggled (NULL, path, NULL, view);
			}
			else
			{
				view->priv->drag_started = FALSE;
				view->priv->drag_button = event->button;
				view->priv->drag_x = event->x;
				view->priv->drag_y = event->y;
			}
		}

		gtk_tree_path_free (path);
	}
	else
	{
		gtk_tree_selection_unselect_all (selection);
	}

	return TRUE;
}

static void
ephy_node_view_set_filter (EphyNodeView *view, EphyNodeFilter *filter)
{
	gboolean refilter = FALSE;

	if (view->priv->filter)
	{
		g_object_unref (view->priv->filter);
		refilter = TRUE;
	}

	if (filter)
	{
		view->priv->filter = g_object_ref (filter);
		g_signal_connect_object (G_OBJECT (view->priv->filter),
					 "changed", G_CALLBACK (filter_changed_cb),
					 G_OBJECT (view), 0);
	}

	if (refilter)
	{
		gtk_tree_model_filter_refilter
				(GTK_TREE_MODEL_FILTER (view->priv->filtermodel));
	}
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
		view->priv->root = g_value_get_pointer (value);
		break;
	case PROP_FILTER:
		ephy_node_view_set_filter (view, g_value_get_object (value));
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
		g_value_set_pointer (value, view->priv->root);
		break;
	case PROP_FILTER:
		g_value_set_object (value, view->priv->filter);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * ephy_node_view_new:
 * @root: the root #EphyNode for the view
 * @filter: a filter model for the view
 *
 * Creates a new #EphyNodeView using @filter as the model and @root as the root
 * node.
 *
 * Returns: a new #EphyNodeView as a #GtkWidget
 **/
GtkWidget *
ephy_node_view_new (EphyNode *root,
		    EphyNodeFilter *filter)
{
	EphyNodeView *view;

	view = EPHY_NODE_VIEW (g_object_new (EPHY_TYPE_NODE_VIEW,
					     "filter", filter,
					     "root", root,
					     NULL));

	g_return_val_if_fail (view->priv != NULL, NULL);

	return GTK_WIDGET (view);
}

static void
cell_renderer_edited (GtkCellRendererText *cell,
		      const char *path_str,
		      const char *new_text,
		      EphyNodeView *view)
{
	GtkTreePath *path;
	GtkTreeIter iter, iter2;
	EphyNode *node;

	view->priv->edited_node = NULL;

	g_object_set (G_OBJECT (view->priv->editable_renderer),
		      "editable", FALSE,
		      NULL);

	path = gtk_tree_path_new_from_string (path_str);
	gtk_tree_model_get_iter (view->priv->sortmodel, &iter, path);
	gtk_tree_model_sort_convert_iter_to_child_iter
		(GTK_TREE_MODEL_SORT (view->priv->sortmodel), &iter2, &iter);
	gtk_tree_model_filter_convert_iter_to_child_iter
		(GTK_TREE_MODEL_FILTER (view->priv->filtermodel), &iter, &iter2);
	node = ephy_tree_model_node_node_from_iter (view->priv->nodemodel, &iter);

	ephy_node_set_property_string (node, view->priv->editable_property,
				       new_text);

	gtk_tree_path_free (path);

	view->priv->remove_if_cancelled = FALSE;
}

static void
renderer_editing_canceled_cb (GtkCellRendererText *cell,
			      EphyNodeView *view)
{
	if (view->priv->remove_if_cancelled)
	{
		ephy_node_unref (view->priv->edited_node);
		view->priv->remove_if_cancelled = FALSE;
	}
}

static inline int
compare_string_values (const GValue *a_value, const GValue *b_value)
{
	const char *str1, *str2;
	int retval;

	str1 = g_value_get_string (a_value);
	str2 = g_value_get_string (b_value);

	if (str1 == NULL)
	{
		retval = -1;
	}
	else if (str2 == NULL)
	{
		retval = 1;
	}
	else
	{
		char *str_a;
		char *str_b;

		str_a = g_utf8_casefold (str1, -1);
		str_b = g_utf8_casefold (str2, -1);
		retval = g_utf8_collate (str_a, str_b);
		g_free (str_a);
		g_free (str_b);
	}

	return retval;
}

static int
ephy_node_view_sort_func (GtkTreeModel *model,
			  GtkTreeIter *a,
			  GtkTreeIter *b,
			  EphyNodeView *view)
{
	GValue a_value = {0, };
	GValue b_value = {0, };
	int p_column, column, retval = 0;
	GtkSortType sort_type;

	g_return_val_if_fail (model != NULL, 0);
	g_return_val_if_fail (view != NULL, 0);

	p_column = view->priv->priority_column;
	column = view->priv->sort_column;
	sort_type = view->priv->sort_type;

	if (p_column >= 0)
	{
		gtk_tree_model_get_value (model, a, p_column, &a_value);
		gtk_tree_model_get_value (model, b, p_column, &b_value);

		if (g_value_get_int (&a_value) < g_value_get_int (&b_value))
		{
			retval = -1;
		}
		else if (g_value_get_int (&a_value) == g_value_get_int (&b_value))
		{
			retval = 0;
		}
		else
		{
			retval = 1;
		}

		g_value_unset (&a_value);
		g_value_unset (&b_value);
	}


	if (retval == 0)
	{
		GType type;

		type = gtk_tree_model_get_column_type (model, column);

		gtk_tree_model_get_value (model, a, column, &a_value);
		gtk_tree_model_get_value (model, b, column, &b_value);

		switch (G_TYPE_FUNDAMENTAL (type))
		{
		case G_TYPE_STRING:
			retval = compare_string_values (&a_value, &b_value);
			break;
		case G_TYPE_INT:
			if (g_value_get_int (&a_value) < g_value_get_int (&b_value))
			{
				retval = -1;
			}
			else if (g_value_get_int (&a_value) == g_value_get_int (&b_value))
			{
				retval = 0;
			}
			else
			{
				retval = 1;
			}
				break;
		case G_TYPE_BOOLEAN:
			if (g_value_get_boolean (&a_value) < g_value_get_boolean (&b_value))
			{
				retval = -1;
			}
			else if (g_value_get_boolean (&a_value) == g_value_get_boolean (&b_value))
			{
				retval = 0;
			}
			else
			{
				retval = 1;
			}
			break;
		default:
			g_warning ("Attempting to sort on invalid type %s\n", g_type_name (type));
			break;
		}

		g_value_unset (&a_value);
		g_value_unset (&b_value);
	}

	if (sort_type == GTK_SORT_DESCENDING)
	{
		if (retval > 0)
		{
			retval = -1;
		}
		else if (retval < 0)
		{
			retval = 1;
		}
	}

	return retval;
}

static void
provide_priority (EphyNode *node, GValue *value, EphyNodeView *view)
{
	int priority;

	g_value_init (value, G_TYPE_INT);
	priority = ephy_node_get_property_int (node, view->priv->priority_prop_id);
	if (priority == EPHY_NODE_VIEW_ALL_PRIORITY ||
	    priority == EPHY_NODE_VIEW_SPECIAL_PRIORITY)
		g_value_set_int (value, priority);
	else
		g_value_set_int (value, EPHY_NODE_VIEW_NORMAL_PRIORITY);
}

static void
provide_text_weight (EphyNode *node, GValue *value, EphyNodeView *view)
{
	int priority;

	g_value_init (value, G_TYPE_INT);
	priority = ephy_node_get_property_int
		(node, view->priv->priority_prop_id);
	if (priority == EPHY_NODE_VIEW_ALL_PRIORITY ||
	    priority == EPHY_NODE_VIEW_SPECIAL_PRIORITY)
	{
		g_value_set_int (value, PANGO_WEIGHT_BOLD);
	}
	else
	{
		g_value_set_int (value, PANGO_WEIGHT_NORMAL);
	}
}

/**
 * ephy_node_view_add_data_column:
 * @view: an #EphyNodeView
 * @value_type: type to be held by the column
 * @prop_id: property corresponding to the model behind @view
 * @func: a data function for the column or %NULL
 * @data: optional data for @func
 *
 * Adds a new column to @view, taking its value from a column in the model defined
 * by @prop_id, or from @func.
 *
 * Returns: the id of the new column
 **/
int
ephy_node_view_add_data_column (EphyNodeView *view,
				GType value_type,
				guint prop_id,
				EphyTreeModelNodeValueFunc func,
				gpointer data)
{
	int column;

	if (func)
	{
		column = ephy_tree_model_node_add_func_column
			(view->priv->nodemodel, value_type, func, data);
	}
	else
	{
		column = ephy_tree_model_node_add_prop_column
			(view->priv->nodemodel, value_type, prop_id);
	}

	return column;
}

/**
 * ephy_node_view_add_column_full:
 * @view: an #EphyNodeView
 * @title: title for the column
 * @value_type: type to be held by the column
 * @prop_id: numeric id corresponding to the column in the model to be shown
 * @flags: flags for the new column
 * @func: optional function to modify the view of properties in the column
 * @user_data: optional data passed to @func
 * @icon_func: a function providing the icon for the column
 * @ret: location to store the created column
 *
 * Adds a new column, corresponding to a @prop_id of the model, to the @view.
 *
 * Returns: the id of the new column
 **/
int
ephy_node_view_add_column_full (EphyNodeView *view,
				const char  *title,
				GType value_type,
				guint prop_id,
				EphyNodeViewFlags flags,
				EphyTreeModelNodeValueFunc func,
				gpointer user_data,
				EphyTreeModelNodeValueFunc icon_func,
				GtkTreeViewColumn **ret)
{
	GtkTreeViewColumn *gcolumn;
	GtkCellRenderer *renderer;
	int column;
	int icon_column;

	column = ephy_tree_model_node_add_column_full
		(view->priv->nodemodel, value_type, prop_id, func, user_data);

	gcolumn = (GtkTreeViewColumn *) gtk_tree_view_column_new ();

	if (icon_func)
	{
		icon_column = ephy_tree_model_node_add_func_column
			 (view->priv->nodemodel, GDK_TYPE_PIXBUF, icon_func, view);

		renderer = gtk_cell_renderer_pixbuf_new ();
		gtk_tree_view_column_pack_start (gcolumn, renderer, FALSE);
		gtk_tree_view_column_set_attributes (gcolumn, renderer,
						     "pixbuf", icon_column,
						     NULL);
	}

	renderer = gtk_cell_renderer_text_new ();

	if (flags & EPHY_NODE_VIEW_EDITABLE)
	{
		view->priv->editable_renderer = renderer;
		view->priv->editable_column = gcolumn;
		view->priv->editable_node_column = column;
		view->priv->editable_property = prop_id;

		g_signal_connect (renderer, "edited",
				  G_CALLBACK (cell_renderer_edited), view);
		g_signal_connect (renderer, "editing-canceled",
				  G_CALLBACK (renderer_editing_canceled_cb), view);
	}

	gtk_tree_view_column_pack_start (gcolumn, renderer, TRUE);
	gtk_tree_view_column_set_attributes (gcolumn, renderer,
					     "text", column,
					     NULL);

	gtk_tree_view_column_set_title (gcolumn, title);
	gtk_tree_view_append_column (GTK_TREE_VIEW (view),
				     gcolumn);

	if (flags & EPHY_NODE_VIEW_SHOW_PRIORITY)
	{
		int wcol;

		wcol = ephy_tree_model_node_add_func_column
			(view->priv->nodemodel, G_TYPE_INT,
			 (EphyTreeModelNodeValueFunc) provide_text_weight,
			 view);
		gtk_tree_view_column_add_attribute (gcolumn, renderer,
						    "weight", wcol);
	}

	if (flags & EPHY_NODE_VIEW_SORTABLE)
	{
		/* Now we have created a new column, re-create the
		 * sort model, but ensure that the set_sort function
		 * hasn't been called, see bug #320686 */
		g_assert (view->priv->sort_column == -1);
		g_object_unref (view->priv->sortmodel);
		view->priv->sortmodel = ephy_tree_model_sort_new (view->priv->filtermodel);
		gtk_tree_view_set_model (GTK_TREE_VIEW (view), GTK_TREE_MODEL (view->priv->sortmodel));

		gtk_tree_view_column_set_sort_column_id (gcolumn, column);
	}

	if (flags & EPHY_NODE_VIEW_SEARCHABLE)
	{
		gtk_tree_view_set_search_column (GTK_TREE_VIEW (view), column);
		gtk_tree_view_set_enable_search (GTK_TREE_VIEW (view), TRUE);
	}
	
	if (flags & EPHY_NODE_VIEW_ELLIPSIZED)
	{
		g_object_set (renderer, "ellipsize-set", TRUE,
			      "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	}

	if (ret != NULL)
		*ret = gcolumn;

	return column;
}

/**
 * ephy_node_view_add_column:
 * @view: an #EphyNodeView
 * @title: title for the column
 * @value_type: type to be held by the column
 * @prop_id: numeric id corresponding to the column in the model to be shown
 * @flags: flags for the new column
 * @icon_func: a function providing the icon for the column
 * @ret: location to store the created column
 *
 * Adds a new column, corresponding to a @prop_id of the model, to the @view.
 *
 * Returns: the id of the new column
 **/
int
ephy_node_view_add_column (EphyNodeView *view,
			   const char  *title,
			   GType value_type,
			   guint prop_id,
			   EphyNodeViewFlags flags,
			   EphyTreeModelNodeValueFunc icon_func,
			   GtkTreeViewColumn **ret)
{
	return ephy_node_view_add_column_full (view, title, value_type, prop_id,
					       flags, NULL, NULL, icon_func, ret);
}

/**
 * ephy_node_view_set_priority:
 * @view: an #EphyNodeView
 * @priority_prop_id: one of #EphyNodeViewPriority
 *
 * Adds a priority column to the @view with @priority_prop_id as the value.
 **/
void
ephy_node_view_set_priority (EphyNodeView *view, EphyNodeViewPriority priority_prop_id)
{
	int priority_column;

	priority_column = ephy_tree_model_node_add_func_column
				(view->priv->nodemodel, G_TYPE_INT,
				 (EphyTreeModelNodeValueFunc) provide_priority,
				 view);

	view->priv->priority_column = priority_column;
	view->priv->priority_prop_id = priority_prop_id;
}

/**
 * ephy_node_view_set_sort:
 * @view: an #EphyNodeView
 * @value_type: type of the value held at @prop_id by the model
 * @prop_id: column id in the model
 * @sort_type: the sort mode
 *
 * Adds a sort column to the @view corresponding to @prop_id in the model.
 **/
void
ephy_node_view_set_sort (EphyNodeView *view, GType value_type, guint prop_id,
			 GtkSortType sort_type)
{
	GtkTreeSortable *sortable = GTK_TREE_SORTABLE (view->priv->sortmodel);
	int column;

	column = ephy_tree_model_node_add_prop_column
		(view->priv->nodemodel, value_type, prop_id);
	view->priv->sort_column = column;
	view->priv->sort_type = sort_type;

	gtk_tree_sortable_set_default_sort_func
			(sortable, (GtkTreeIterCompareFunc)ephy_node_view_sort_func,
			 view, NULL);
	gtk_tree_sortable_set_sort_column_id
			(sortable, GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
			 sort_type);
}

static void
ephy_node_view_init (EphyNodeView *view)
{
	view->priv = EPHY_NODE_VIEW_GET_PRIVATE (view);

	view->priv->toggle_column = -1;
	view->priv->priority_column = -1;
	view->priv->priority_prop_id = 0;
	view->priv->sort_column = -1;
	view->priv->sort_type = GTK_SORT_ASCENDING;

	gtk_tree_view_set_enable_search (GTK_TREE_VIEW (view), FALSE);
}

static void
get_selection (GtkTreeModel *model,
	       GtkTreePath *path,
	       GtkTreeIter *iter,
	       gpointer *data)
{
	GList **list = data[0];
	EphyNodeView *view = EPHY_NODE_VIEW (data[1]);
	EphyNode *node;

	node = get_node_from_path (view, path);

	*list = g_list_prepend (*list, node);
}

/**
 * ephy_node_view_get_selection:
 * @view: an #EphyNodeView
 *
 * Returns the selected elements of @view as a #GList of #EphyNode elements.
 *
 * Returns: a #GList of #EphyNode elements
 **/
GList *
ephy_node_view_get_selection (EphyNodeView *view)
{
	GList *list = NULL;
	GtkTreeSelection *selection;
	gpointer data[2];

	selection = gtk_tree_view_get_selection	(GTK_TREE_VIEW (view));

	data[0] = &list;
	data[1] = view;
	gtk_tree_selection_selected_foreach
			(selection,
			 (GtkTreeSelectionForeachFunc) get_selection,
			 (gpointer) data);

	return list;
}

/**
 * ephy_node_view_remove:
 * @view: an #EphyNodeView
 *
 * Remove the currently selected nodes from @view.
 **/
void
ephy_node_view_remove (EphyNodeView *view)
{
	EphyNodeViewPrivate *priv = view->priv;
	GList *list, *l;
	EphyNode *node;
	GtkTreeIter iter, iter2, iter3;
	GtkTreePath *path;
	GtkTreeRowReference *row_ref = NULL;
	GtkTreeSelection *selection;

	/* Before removing we try to get a reference to the next node in the view. If that is
	 * not available we try with the previous one, and if that is absent too,
	 * we will not select anything (which equals to select the topic "All")
	 */

	list = ephy_node_view_get_selection (view);
	if (list == NULL) return;

	node = g_list_first (list)->data;
	ephy_tree_model_node_iter_from_node (EPHY_TREE_MODEL_NODE (view->priv->nodemodel),
					     node, &iter3);
	gtk_tree_model_filter_convert_child_iter_to_iter (GTK_TREE_MODEL_FILTER (view->priv->filtermodel),
							  &iter2, &iter3);
	gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (view->priv->sortmodel),
							&iter, &iter2);
	iter2 = iter;

	if (gtk_tree_model_iter_next (GTK_TREE_MODEL (view->priv->sortmodel), &iter))
	{
		path = gtk_tree_model_get_path (GTK_TREE_MODEL (view->priv->sortmodel), &iter);
		row_ref = gtk_tree_row_reference_new (GTK_TREE_MODEL (view->priv->sortmodel), path);
	}
	else
	{
		path = gtk_tree_model_get_path (GTK_TREE_MODEL (view->priv->sortmodel), &iter2);
		if (gtk_tree_path_prev (path))
		{
			row_ref = gtk_tree_row_reference_new (GTK_TREE_MODEL (view->priv->sortmodel), path);
		}
	}
	gtk_tree_path_free (path);

	/* Work around bug #346662 */
	priv->changing_selection = TRUE;
	for (l = list; l != NULL; l = l->next)
	{
		ephy_node_unref (l->data);
	}
	priv->changing_selection = FALSE;

	g_list_free (list);

	/* Fake a selection changed signal */
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
	g_signal_emit_by_name (selection, "changed");

	/* Select the "next" node */

	if (row_ref != NULL)
	{
		path = gtk_tree_row_reference_get_path (row_ref);

		if (path != NULL)
		{
			gtk_tree_view_set_cursor (GTK_TREE_VIEW (view), path, NULL, FALSE);
			gtk_tree_path_free (path);
		}	

		gtk_tree_row_reference_free (row_ref);
	}
}

/**
 * ephy_node_view_select_node:
 * @view: an #EphyNodeView
 * @node: the #EphyNode in the @view to be selected
 *
 * Puts the selection of @view on @node.
 **/
void
ephy_node_view_select_node (EphyNodeView *view,
			    EphyNode *node)
{
	GtkTreeIter iter, iter2;

	g_return_if_fail (node != NULL);

	ephy_tree_model_node_iter_from_node (EPHY_TREE_MODEL_NODE (view->priv->nodemodel),
					     node, &iter);
	gtk_tree_model_filter_convert_child_iter_to_iter (GTK_TREE_MODEL_FILTER (view->priv->filtermodel),
							  &iter2, &iter);
	gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (view->priv->sortmodel),
							&iter, &iter2);

	gtk_tree_selection_select_iter (gtk_tree_view_get_selection (GTK_TREE_VIEW (view)),
					&iter);
}

/**
 * ephy_node_view_enable_drag_source:
 * @view: an #EphyNodeView
 * @types: a #GtkTargetEntry for the @view
 * @n_types: length of @types
 * @base_drag_column_id: id of the column for ephy_tree_model_sort_set_base_drag_column_id
 * @extra_drag_column_id: id of the column for ephy_tree_model_sort_set_extra_drag_column_id
 *
 * Sets @view as a drag source.
 **/
void
ephy_node_view_enable_drag_source (EphyNodeView *view,
				   const GtkTargetEntry *types,
				   int n_types,
				   int base_drag_column_id,
				   int extra_drag_column_id)
{
	g_return_if_fail (view != NULL);

	view->priv->source_target_list =
		gtk_target_list_new (types, n_types);

	ephy_tree_model_sort_set_base_drag_column_id  (EPHY_TREE_MODEL_SORT (view->priv->sortmodel),
						       base_drag_column_id);
	ephy_tree_model_sort_set_extra_drag_column_id (EPHY_TREE_MODEL_SORT (view->priv->sortmodel),
						       extra_drag_column_id);

	g_signal_connect_object (G_OBJECT (view),
				 "button_release_event",
				 G_CALLBACK (button_release_cb),
				 view,
				 0);
	g_signal_connect_object (G_OBJECT (view),
				 "motion_notify_event",
				 G_CALLBACK (motion_notify_cb),
				 view,
				 0);
	g_signal_connect_object (G_OBJECT (view),
				 "drag_data_get",
				 G_CALLBACK (drag_data_get_cb),
				 view,
				 0);
}

/**
 * ephy_node_view_edit:
 * @view: an #EphyNodeView
 * @remove_if_cancelled: whether the edited node should be removed if editing is cancelled
 *
 * Edits the currently selected node in @view, the @remove_if_cancelled parameter
 * controls if the node should be removed if editing is cancelled.
 **/
void
ephy_node_view_edit (EphyNodeView *view, gboolean remove_if_cancelled)
{
	GtkTreePath *path;
	GtkTreeSelection *selection;
	GList *rows;
	GtkTreeModel *model;

	g_return_if_fail (view->priv->editable_renderer != NULL);

	selection = gtk_tree_view_get_selection
		(GTK_TREE_VIEW (view));
	rows = gtk_tree_selection_get_selected_rows (selection, &model);
	if (rows == NULL) return;

	path = rows->data;

	g_object_set (G_OBJECT (view->priv->editable_renderer),
		      "editable", TRUE,
		      NULL);

	gtk_tree_view_set_cursor (GTK_TREE_VIEW (view), path,
				  view->priv->editable_column,
				  TRUE);

	view->priv->edited_node = get_node_from_path (view, path);
	view->priv->remove_if_cancelled = remove_if_cancelled;

	g_list_foreach (rows, (GFunc)gtk_tree_path_free, NULL);
	g_list_free (rows);
}

/**
 * ephy_node_view_is_target:
 * @view: an #EphyNodeView
 *
 * Tells if @view is currently focused.
 *
 * Returns: %TRUE if @view is focused
 **/
gboolean
ephy_node_view_is_target (EphyNodeView *view)
{
	return gtk_widget_is_focus (GTK_WIDGET (view));
}

static gboolean
filter_visible_func (GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	EphyNode *node;
	EphyNodeView *view = EPHY_NODE_VIEW (data);

	if (view->priv->filter)
	{
		node = ephy_tree_model_node_node_from_iter (view->priv->nodemodel, iter);

		return ephy_node_filter_evaluate (view->priv->filter, node);
	}

	return TRUE;
}

static GObject *
ephy_node_view_constructor (GType type, guint n_construct_properties,
			    GObjectConstructParam *construct_params)

{
	GObject *object;
	EphyNodeView *view;
	EphyNodeViewPrivate *priv;
	GtkTreeSelection *selection;

	object = parent_class->constructor (type, n_construct_properties,
					    construct_params);
	view = EPHY_NODE_VIEW (object);
	priv = EPHY_NODE_VIEW_GET_PRIVATE (object);

	priv->nodemodel = ephy_tree_model_node_new (priv->root);
	priv->filtermodel = gtk_tree_model_filter_new (GTK_TREE_MODEL (priv->nodemodel),
						       NULL);
	gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (priv->filtermodel),
						filter_visible_func, view, NULL);
	priv->sortmodel = ephy_tree_model_sort_new (priv->filtermodel);
	gtk_tree_view_set_model (GTK_TREE_VIEW (object), GTK_TREE_MODEL (priv->sortmodel));
	g_signal_connect_object (object, "button_press_event",
				 G_CALLBACK (ephy_node_view_button_press_cb),
				 view, 0);
	g_signal_connect (object, "key_press_event",
			  G_CALLBACK (ephy_node_view_key_press_cb),
			  view);
	g_signal_connect_object (object, "row_activated",
				 G_CALLBACK (ephy_node_view_row_activated_cb),
				 view, 0);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
	g_signal_connect_object (G_OBJECT (selection), "changed",
				 G_CALLBACK (ephy_node_view_selection_changed_cb),
				 view, 0);

	return object;
}

/**
 * ephy_node_view_add_toggle:
 * @view: an #EphyNodeView widget
 * @value_func: an #EphyTreeModelNodeValueFunc function to set column values
 * @data: user_data to be passed to @value_func
 *
 * Append a new toggle column to @view with its value set by @value_func which can
 * receive the optional @data parameter.
 **/
void
ephy_node_view_add_toggle (EphyNodeView *view, EphyTreeModelNodeValueFunc value_func,
			   gpointer data)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *col;
	int column;

	column = ephy_tree_model_node_add_func_column
			(view->priv->nodemodel, G_TYPE_BOOLEAN, value_func, data);
	view->priv->toggle_column = column;

	renderer = gtk_cell_renderer_toggle_new ();
	col = gtk_tree_view_column_new_with_attributes
		("", renderer, "active", column, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (view), col);
}

/**
 * ephy_node_view_popup:
 * @view: an #EphyNodeView widget
 * @menu: a #GtkMenu to be shown
 *
 * Triggers the popup of @menu in @view.
 **/
void
ephy_node_view_popup (EphyNodeView *view, GtkWidget *menu)
{
	GdkEvent *event;

	event = gtk_get_current_event ();
	if (event)
	{
		if (event->type == GDK_KEY_PRESS)
		{
			GdkEventKey *key = (GdkEventKey *) event;

			gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
					ephy_gui_menu_position_tree_selection,
					view, 0, key->time);
			gtk_menu_shell_select_first (GTK_MENU_SHELL (menu), FALSE);
		}
		else if (event->type == GDK_BUTTON_PRESS)
		{
			GdkEventButton *button = (GdkEventButton *) event;

			gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL,
					NULL, button->button, button->time);
		}

		gdk_event_free (event);
	}
}

gboolean
ephy_node_view_get_iter_for_node (EphyNodeView *view,
				  GtkTreeIter *iter,
				  EphyNode *node)
{
	GtkTreeIter node_iter, filter_iter;

	ephy_tree_model_node_iter_from_node (EPHY_TREE_MODEL_NODE (view->priv->nodemodel), node, &node_iter);
	if (!gtk_tree_model_filter_convert_child_iter_to_iter (GTK_TREE_MODEL_FILTER (view->priv->filtermodel),
							       &filter_iter, &node_iter))
		return FALSE;

	if (!gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (view->priv->sortmodel),
							     iter, &filter_iter))
		return FALSE;

	return TRUE;
}

static void
ephy_node_view_class_init (EphyNodeViewClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->constructor = ephy_node_view_constructor;
	object_class->finalize = ephy_node_view_finalize;

	object_class->set_property = ephy_node_view_set_property;
	object_class->get_property = ephy_node_view_get_property;

	/**
	* EphyNodeView:root:
	*
	* A #gpointer to the root node of the #EphyNode elements of the view.
	*/
	g_object_class_install_property (object_class,
					 PROP_ROOT,
					 g_param_spec_pointer ("root",
							       "Root node",
							       "Root node",
							       G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_CONSTRUCT_ONLY));

	/**
	* EphyNodeView:filter:
	*
	* An #EphyNodeFilter object to use in the view.
	*/
	g_object_class_install_property (object_class,
					 PROP_FILTER,
					 g_param_spec_object ("filter",
							      "Filter object",
							      "Filter object",
							      EPHY_TYPE_NODE_FILTER,
							      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	/**
	* EphyNodeView::node-toggled:
	* @view: the object on which the signal is emitted
	* @node: the target #EphyNode
	* @checked: the new value of the toggle column
	*
	* Emitted when a row value is toggled, only emitted for toggle columns.
	*/
	ephy_node_view_signals[NODE_TOGGLED] =
		g_signal_new ("node_toggled",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyNodeViewClass, node_toggled),
			      NULL, NULL,
			      g_cclosure_marshal_generic,
			      G_TYPE_NONE,
			      2,
			      G_TYPE_POINTER,
			      G_TYPE_BOOLEAN);
	/**
	* EphyNodeView::node-activated:
	* @view: the object on which the signal is emitted
	* @node: the activated #EphyNode
	*
	* Emitted when a row is activated.
	*/
	ephy_node_view_signals[NODE_ACTIVATED] =
		g_signal_new ("node_activated",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyNodeViewClass, node_activated),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_POINTER);
	/**
	* EphyNodeView::node-selected:
	* @view: the object on which the signal is emitted
	* @node: the selected #EphyNode
	*
	* Emitted when a row is selected.
	*/
	ephy_node_view_signals[NODE_SELECTED] =
		g_signal_new ("node_selected",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyNodeViewClass, node_selected),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_POINTER);
	/**
	* EphyNodeView::node-dropped:
	* @view: the object on which the signal is emitted
	* @node: the dropped #EphyNode
	* @uris: URIs from the dragged data
	*
	* Emitted when an #EphyNode is dropped into the #EphyNodeView.
	*/
	ephy_node_view_signals[NODE_DROPPED] =
		g_signal_new ("node_dropped",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyNodeViewClass, node_dropped),
			      NULL, NULL,
			      g_cclosure_marshal_generic,
			      G_TYPE_NONE,
			      2,
			      G_TYPE_POINTER,
			      G_TYPE_POINTER);

	/**
	* EphyNodeView::node-middle-clicked:
	* @view: the object on which the signal is emitted
	* @node: the clicked #EphyNode
	*
	* Emitted when the user middle clicks on a row of the #EphyNodeView.
	*/
	ephy_node_view_signals[NODE_MIDDLE_CLICKED] =
		g_signal_new ("node_middle_clicked",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyNodeViewClass, node_middle_clicked),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_POINTER);

	g_type_class_add_private (object_class, sizeof (EphyNodeViewPrivate));
}
