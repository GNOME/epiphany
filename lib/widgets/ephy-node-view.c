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

#include <gtk/gtktreeselection.h>
#include <gtk/gtktreeviewcolumn.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkcellrendererpixbuf.h>
#include <gtk/gtktreemodelfilter.h>
#include <gtk/gtkwindow.h>
#include <gdk/gdkkeysyms.h>
#include <libgnomevfs/gnome-vfs-uri.h>

#include "ephy-node-view.h"
#include "ephy-tree-model-sort.h"
#include "eggtreemultidnd.h"
#include "ephy-dnd.h"
#include "ephy-marshal.h"
#include "string.h"

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
	GtkCellRenderer *editable_renderer;
	GtkTreeViewColumn *editable_column;
	EphyTreeModelNodeColumn editable_node_column;

	EphyNodeFilter *filter;

	GtkTargetList *drag_targets;

	int default_sort_column_id;
	int priority_prop_id;

	gboolean editing;
	int editable_property;

	int searchable_data_column;

	gboolean drag_started;
	int drag_button;
	int drag_x;
        int drag_y;
	GtkTargetList *source_target_list;

	gboolean drop_occurred;                                                                                          
	gboolean have_drag_data;
	GtkSelectionData *drag_data;
	guint scroll_id;
};

enum
{
	NODE_ACTIVATED,
	NODE_SELECTED,
	NODE_DROPPED,
	SHOW_POPUP,
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

		ephy_node_view_type = g_type_register_static (GTK_TYPE_TREE_VIEW,
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
	ephy_node_view_signals[NODE_DROPPED] =
		g_signal_new ("node_dropped",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyNodeViewClass, node_dropped),
			      NULL, NULL,
			      ephy_marshal_VOID__POINTER_POINTER,
			      G_TYPE_NONE,
			      2,
			      G_TYPE_POINTER,
			      G_TYPE_POINTER);
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

	if (view->priv->source_target_list)
	{
		gtk_target_list_unref (view->priv->source_target_list);
	}

	g_free (view->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static EphyNode *
get_node_from_path (EphyNodeView *view, GtkTreePath *path)
{
	EphyNode *node;
	GtkTreeIter iter, iter2;

	if (path == NULL) return NULL;

	gtk_tree_model_get_iter (view->priv->sortmodel, &iter, path);
	gtk_tree_model_sort_convert_iter_to_child_iter
		(GTK_TREE_MODEL_SORT (view->priv->sortmodel), &iter2, &iter);
	gtk_tree_model_filter_convert_iter_to_child_iter
		(GTK_TREE_MODEL_FILTER (view->priv->filtermodel), &iter, &iter2);
	node = ephy_tree_model_node_node_from_iter (view->priv->nodemodel, &iter);

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
	vadjustment = gtk_tree_view_get_vadjustment (tree_view);
	
	gdk_window_get_pointer (window, NULL, &y, NULL);
	
	y += vadjustment->value;

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

	value = CLAMP (vadjustment->value + offset, 0.0,
		       vadjustment->upper - vadjustment->page_size);
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

	node = get_node_from_path (view, path);

	if (node)
	{
		priority = ephy_node_get_property_int
				(node, view->priv->priority_prop_id);

		if (priority != EPHY_NODE_VIEW_ALL_PRIORITY &&
		    priority != EPHY_NODE_VIEW_SPECIAL_PRIORITY)
		{
			action = context->suggested_action;
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
	GtkTreePath *path;
	GtkTreeViewDropPosition pos;
	gboolean on_row;

	if (selection_data->length <= 0 || selection_data->data == NULL)
	{
		return;
	}
	
	on_row = gtk_tree_view_get_dest_row_at_pos (GTK_TREE_VIEW (widget),
					            x, y, &path, &pos);

	if (!view->priv->have_drag_data)
	{
		view->priv->have_drag_data = TRUE;
		view->priv->drag_data = 
			gtk_selection_data_copy (selection_data);
	}

	if (view->priv->drop_occurred)
	{
		EphyNode *node;
		GList *uris;
		gboolean success = FALSE;

		g_return_if_fail (on_row && path != NULL);

		node = get_node_from_path (view, path);

		uris = gnome_vfs_uri_list_parse (selection_data->data);
	
		if (uris != NULL)
		{
			/* FIXME fill success */
			g_signal_emit (G_OBJECT (view),
				       ephy_node_view_signals[NODE_DROPPED], 0,
				       node, uris);
			gnome_vfs_uri_list_free (uris);
		}

		view->priv->drop_occurred = FALSE;
		free_drag_data (view);
		gtk_drag_finish (context, success, FALSE, time);
	}

	if (path)
	{
		gtk_tree_path_free (path);
	}

	/* appease GtkTreeView by preventing its drag_data_receive
	 * from being called */
	g_signal_stop_emission_by_name (GTK_TREE_VIEW (view),
					"drag_data_received");
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

void
ephy_node_view_enable_drag_dest (EphyNodeView *view,
				 GtkTargetEntry *types,
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


static gboolean
ephy_node_view_select_node_by_key (EphyNodeView *view, GdkEventKey *event)
{
	GtkTreeIter iter, last_iter;
	GtkTreePath *path;
	GValue value = {0, };
	gchar *string;
	gchar *event_string;
	gboolean found = FALSE;
	gchar outbuf[6];
	gint length;

	length = g_unichar_to_utf8 (gdk_keyval_to_unicode (event->keyval), outbuf);
	event_string = g_utf8_casefold (outbuf, length);

	if (!gtk_tree_model_get_iter_first (view->priv->sortmodel, &iter))
	{
		g_free (event_string);
		return FALSE;
	}

	do
	{
		last_iter = iter;
		gtk_tree_model_get_value (view->priv->sortmodel, &iter,
					  view->priv->searchable_data_column,
					  &value);

		string = g_utf8_casefold (g_value_get_string (&value), -1);
		g_utf8_strncpy (string, string, 1);
		found = (g_utf8_collate (string, event_string) >= 0);

		g_free (string);
		g_value_unset (&value);
	}
	while (!found && gtk_tree_model_iter_next (view->priv->sortmodel, &iter));

	if (!found)
	{
		iter = last_iter;
	}

	path = gtk_tree_model_get_path (view->priv->sortmodel, &iter);
	gtk_tree_view_set_cursor (GTK_TREE_VIEW (view), path, NULL, FALSE);
	gtk_tree_path_free (path);
	g_free (event_string);

	return TRUE;
}

static gboolean
ephy_node_view_key_press_cb (GtkTreeView *treeview,
			     GdkEventKey *event,
			     EphyNodeView *view)
{
	if ((event->state & GDK_SHIFT_MASK) &&
	    (event->keyval == GDK_F10))
	{
		g_signal_emit (G_OBJECT (view), ephy_node_view_signals[SHOW_POPUP], 0);

		return TRUE;
	}
	else if (view->priv->searchable_data_column != -1 &&
		 gdk_keyval_to_unicode (event->keyval))
	{
		return ephy_node_view_select_node_by_key (view, event);
	}

	return FALSE;
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
	GList *ref_list;
                                                                                                                              
	ref_list = NULL;
                                                                                                                              
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
                        gtk_tree_selection_unselect_all (selection);
                        gtk_tree_selection_select_path (selection, path);
                }
                                                                                                                              
                gtk_tree_path_free (path);
        }                                                                                                              
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
					      event->y))
		{
			context = gtk_drag_begin
				(widget, view->priv->source_target_list,
                                 GDK_ACTION_COPY,
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
			g_signal_emit (G_OBJECT (view), ephy_node_view_signals[SHOW_POPUP], 0);
		}
		else if (event->button == 1)
		{
			view->priv->drag_started = FALSE;
			view->priv->drag_button = event->button;
	                view->priv->drag_x = event->x;
	                view->priv->drag_y = event->y;
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

static void
ephy_node_view_construct (EphyNodeView *view)
{
	GtkTreeSelection *selection;

	view->priv->nodemodel = ephy_tree_model_node_new (view->priv->root,
							  view->priv->filter);
	view->priv->filtermodel = gtk_tree_model_filter_new (GTK_TREE_MODEL (view->priv->nodemodel),
							     NULL);
	gtk_tree_model_filter_set_visible_column (GTK_TREE_MODEL_FILTER (view->priv->filtermodel),
						  EPHY_TREE_MODEL_NODE_COL_VISIBLE);
	view->priv->sortmodel = ephy_tree_model_sort_new (view->priv->filtermodel);
	gtk_tree_view_set_model (GTK_TREE_VIEW (view), GTK_TREE_MODEL (view->priv->sortmodel));
	g_signal_connect_object (G_OBJECT (view),
			         "button_press_event",
			         G_CALLBACK (ephy_node_view_button_press_cb),
			         view,
				 0);
	g_signal_connect_after (G_OBJECT (view),
			         "key_press_event",
			         G_CALLBACK (ephy_node_view_key_press_cb),
			         view);
	g_signal_connect_object (G_OBJECT (view),
			         "row_activated",
			         G_CALLBACK (ephy_node_view_row_activated_cb),
			         view,
				 0);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
	g_signal_connect_object (G_OBJECT (selection),
			         "changed",
			         G_CALLBACK (ephy_node_view_selection_changed_cb),
			         view,
				 0);
}

GtkWidget *
ephy_node_view_new (EphyNode *root,
		    EphyNodeFilter *filter)
{
	EphyNodeView *view;

	view = EPHY_NODE_VIEW (g_object_new (EPHY_TYPE_NODE_VIEW,
					     "filter", filter,
					     "root", root,
					     NULL));

	ephy_node_view_construct (view);

	g_return_val_if_fail (view->priv != NULL, NULL);

	return GTK_WIDGET (view);
}

static void
cell_renderer_edited (GtkCellRendererText *cell,
                      const char *path_str,
                      const char *new_text,
                      EphyNodeView *view)
{
	GValue value = { 0, };
	GtkTreePath *path;
	GtkTreeIter iter, iter2;
	EphyNode *node;

	view->priv->editing = FALSE;

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

	g_value_init (&value, G_TYPE_STRING);
	g_value_set_string (&value, new_text);
	ephy_node_set_property (node,
			        view->priv->editable_property,
			        &value);
	g_value_unset (&value);

	gtk_tree_path_free (path);
}

static int
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
			  gpointer user_data)
{
	GList *order;
	GList *l;
	int retval = 0;

	g_return_val_if_fail (model != NULL, 0);
	g_return_val_if_fail (user_data != NULL, 0);

	order = (GList *) user_data;

	for (l = order; l != NULL && retval == 0; l = g_list_next (l))
	{
		EphyTreeModelNodeColumn column = GPOINTER_TO_INT (l->data);
		GType type = gtk_tree_model_get_column_type (model, column);
		GValue a_value = {0, };
		GValue b_value = {0, };

		gtk_tree_model_get_value (model, a, column, &a_value);
		gtk_tree_model_get_value (model, b, column, &b_value);

		switch (G_TYPE_FUNDAMENTAL (type))
		{
		case G_TYPE_STRING:
			retval = compare_string_values (&a_value, &b_value);
			break;
		case G_TYPE_INT:
			if (g_value_get_int (&a_value) < g_value_get_int (&b_value))
				retval = -1;
			else if (g_value_get_int (&a_value) == g_value_get_int (&b_value))
				retval = 0;
			else
				retval = 1;
			break;
		case G_TYPE_BOOLEAN:
			if (g_value_get_boolean (&a_value) < g_value_get_boolean (&b_value))
				retval = -1;
			else if (g_value_get_boolean (&a_value) == g_value_get_boolean (&b_value))
				retval = 0;
			else
				retval = 1;
			break;
		default:
			g_warning ("Attempting to sort on invalid type %s\n", g_type_name (type));
			break;
		}

		g_value_unset (&a_value);
		g_value_unset (&b_value);
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

int
ephy_node_view_add_data_column (EphyNodeView *view,
			        GType value_type,
				int prop_id,
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

GtkTreeViewColumn *
ephy_node_view_add_column (EphyNodeView *view,
			   const char  *title,
			   GType value_type,
			   int prop_id,
			   int priority_prop_id,
			   EphyNodeViewFlags flags,
			   EphyTreeModelNodeValueFunc icon_func)

{
	GtkTreeViewColumn *gcolumn;
	GtkCellRenderer *renderer;
	int column;
	int icon_column;

	g_return_val_if_fail (!(flags & EPHY_NODE_VIEW_EDITABLE) ||
			      view->priv->editable_renderer == NULL, NULL);

	column = ephy_tree_model_node_add_prop_column
		(view->priv->nodemodel, value_type, prop_id);

	gcolumn = (GtkTreeViewColumn *) gtk_tree_view_column_new ();

	if (icon_func)
	{
		icon_column = ephy_tree_model_node_add_func_column
	                 (view->priv->nodemodel, GDK_TYPE_PIXBUF, icon_func, NULL);

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
		g_signal_connect (renderer, "edited", G_CALLBACK (cell_renderer_edited), view);
	}

	if ((flags & EPHY_NODE_VIEW_SEARCHABLE) &&
	    (view->priv->searchable_data_column == -1))
	{
		view->priv->searchable_data_column = column;
	}

	gtk_tree_view_column_pack_start (gcolumn, renderer, TRUE);
	gtk_tree_view_column_set_attributes (gcolumn, renderer,
					     "text", column,
					     NULL);

	if (priority_prop_id >= 0)
	{
		int wcol;

		view->priv->priority_prop_id = priority_prop_id;

		wcol = ephy_tree_model_node_add_func_column
			(view->priv->nodemodel, G_TYPE_INT,
			 (EphyTreeModelNodeValueFunc) provide_text_weight,
			 view);
		gtk_tree_view_column_add_attribute (gcolumn, renderer,
						    "weight", wcol);
	}

	gtk_tree_view_column_set_title (gcolumn, title);
	gtk_tree_view_append_column (GTK_TREE_VIEW (view),
				     gcolumn);

	if (flags & EPHY_NODE_VIEW_USER_SORT)
	{
		GList *order = NULL;

		order = g_list_append (order, GINT_TO_POINTER (column));
		gtk_tree_sortable_set_sort_func
			(GTK_TREE_SORTABLE (view->priv->sortmodel),
			 column, ephy_node_view_sort_func,
			 order, (GtkDestroyNotify)g_list_free);
		gtk_tree_view_column_set_sort_column_id (gcolumn, column);
	}
	else if (flags & EPHY_NODE_VIEW_AUTO_SORT)
	{
		int scol;
		GList *order = NULL;

		if (priority_prop_id >= 0)
		{
			scol = ephy_tree_model_node_add_func_column
				(view->priv->nodemodel, G_TYPE_INT,
				 (EphyTreeModelNodeValueFunc) provide_priority,
				 view);
			order = g_list_append (order, GINT_TO_POINTER (scol));
		}

		order = g_list_append (order, GINT_TO_POINTER (column));
		gtk_tree_sortable_set_default_sort_func
			(GTK_TREE_SORTABLE (view->priv->sortmodel),
			 ephy_node_view_sort_func,
			 order, (GtkDestroyNotify)g_list_free);
		gtk_tree_sortable_set_sort_column_id
			(GTK_TREE_SORTABLE (view->priv->sortmodel),
			 GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
			 GTK_SORT_ASCENDING);
	}

	return gcolumn;
}

static void
ephy_node_view_init (EphyNodeView *view)
{
	view->priv = g_new0 (EphyNodeViewPrivate, 1);
	view->priv->editable_renderer = NULL;
	view->priv->editing = TRUE;
	view->priv->searchable_data_column = -1;
	view->priv->source_target_list = NULL;

	gtk_tree_view_set_enable_search (GTK_TREE_VIEW (view), FALSE);
}

static void
get_selection (GtkTreeModel *model,
	       GtkTreePath *path,
	       GtkTreeIter *iter,
	       gpointer *data)
{
	GList **list = data[0];
	EphyNodeView *view = EPHY_NODE_VIEW (data);
	EphyNode *node;

	node = get_node_from_path (view, path);

	*list = g_list_prepend (*list, node);
}

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

void
ephy_node_view_remove (EphyNodeView *view)
{
	GList *list, *l;
	EphyNode *node;
	GtkTreeIter iter, iter2;
	GtkTreePath *path;
	GtkTreeRowReference *row_ref = NULL;

	/* Before removing we try to get a reference to the next node in the view. If that is
	 * not available we try with the previous one, and if that is absent too,
	 * we will not select anything (which equals to select the topic "All")
	 */

	list = ephy_node_view_get_selection (view);
	if (list == NULL) return;

	node = g_list_first (list)->data;
	ephy_tree_model_node_iter_from_node (EPHY_TREE_MODEL_NODE (view->priv->nodemodel),
					     node, &iter);
	gtk_tree_model_filter_convert_child_iter_to_iter (GTK_TREE_MODEL_FILTER (view->priv->filtermodel),
							  &iter2, &iter);
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

	for (l = list; l != NULL; l = l->next)
	{
		ephy_node_unref (l->data);
	}

	g_list_free (list);

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

void
ephy_node_view_select_node (EphyNodeView *view,
			    EphyNode *node)
{
	GtkTreeIter iter, iter2;
	GValue val = { 0, };
	gboolean visible;

	g_return_if_fail (node != NULL);

	ephy_tree_model_node_iter_from_node (EPHY_TREE_MODEL_NODE (view->priv->nodemodel),
					     node, &iter);
	gtk_tree_model_get_value (GTK_TREE_MODEL (view->priv->nodemodel), &iter,
				  EPHY_TREE_MODEL_NODE_COL_VISIBLE, &val);
	visible = g_value_get_boolean (&val);
	g_value_unset (&val);
	if (visible == FALSE) return;

	gtk_tree_model_filter_convert_child_iter_to_iter (GTK_TREE_MODEL_FILTER (view->priv->filtermodel),
							  &iter2, &iter);
	gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (view->priv->sortmodel),
							&iter, &iter2);

	gtk_tree_selection_select_iter (gtk_tree_view_get_selection (GTK_TREE_VIEW (view)),
					&iter);
}

void
ephy_node_view_enable_drag_source (EphyNodeView *view,
				   GtkTargetEntry *types,
				   int n_types,
				   int column)
{
	GtkWidget *treeview;

	g_return_if_fail (view != NULL);

	treeview = GTK_WIDGET (view);

	view->priv->source_target_list =
		gtk_target_list_new (types, n_types);

	ephy_tree_model_sort_set_column_id (EPHY_TREE_MODEL_SORT (view->priv->sortmodel),
					    column);

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

void
ephy_node_view_edit (EphyNodeView *view)
{
	GtkTreeSelection *selection;
	GList *rows;
	GtkTreeModel *model;

	g_return_if_fail (view->priv->editable_renderer != NULL);

	selection = gtk_tree_view_get_selection
		(GTK_TREE_VIEW (view));
	rows = gtk_tree_selection_get_selected_rows (selection, &model);
	if (rows == NULL) return;

	g_object_set (G_OBJECT (view->priv->editable_renderer),
                      "editable", TRUE,
                      NULL);

	gtk_tree_view_set_cursor (GTK_TREE_VIEW (view),
                                  (GtkTreePath *)rows->data,
                                  view->priv->editable_column,
                                  TRUE);

	view->priv->editing = TRUE;

	g_list_foreach (rows, (GFunc)gtk_tree_path_free, NULL);
        g_list_free (rows);
}

gboolean
ephy_node_view_is_target (EphyNodeView *view)
{
	return gtk_widget_is_focus (GTK_WIDGET (view));
}

gboolean
ephy_node_view_has_selection (EphyNodeView *view, gboolean *multiple)
{
	GtkTreeSelection *selection;
	int rows;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
	rows = gtk_tree_selection_count_selected_rows (selection);

	if (multiple)
	{
		*multiple = rows > 1;
	}

	return rows > 0;
}
