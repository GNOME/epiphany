/*
 *  Copyright (C) 2002  Ricardo Fernández Pascual
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

#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkvbox.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkframe.h>

#include "ephy-autocompletion-window.h"
#include "ephy-string.h"
#include "ephy-marshal.h"
#include "ephy-gui.h"
#include "ephy-debug.h"

/* This is copied from gtkscrollbarwindow.c */
#define DEFAULT_SCROLLBAR_SPACING  3

#define SCROLLBAR_SPACING(w)                                                            \
  (GTK_SCROLLED_WINDOW_GET_CLASS (w)->scrollbar_spacing >= 0 ?                          \
   GTK_SCROLLED_WINDOW_GET_CLASS (w)->scrollbar_spacing : DEFAULT_SCROLLBAR_SPACING)

#define MAX_VISIBLE_ROWS 9
#define MAX_COMPLETION_ALTERNATIVES 7

/**
 * Private data
 */
struct _EphyAutocompletionWindowPrivate {
	EphyAutocompletion *autocompletion;
	GtkWidget *parent;

	GtkWidget *window;
	GtkScrolledWindow *scrolled_window;
	GtkTreeView *tree_view;
	GtkTreeViewColumn *col1;
	GtkTreeView *action_tree_view;
	GtkTreeViewColumn *action_col1;
	int sel_index;

	char *selected;

	GtkListStore *list_store;
	GtkListStore *action_list_store;
	guint last_added_match;
	int view_nitems;

	gboolean shown;
};

/**
 * Private functions, only availble from this file
 */
static void	ephy_autocompletion_window_class_init		(EphyAutocompletionWindowClass *klass);
static void	ephy_autocompletion_window_init			(EphyAutocompletionWindow *aw);
static void	ephy_autocompletion_window_finalize_impl	(GObject *o);
static void	ephy_autocompletion_window_init_widgets		(EphyAutocompletionWindow *aw);
static void	ephy_autocompletion_window_selection_changed_cb (GtkTreeSelection *treeselection,
								 EphyAutocompletionWindow *aw);
static gboolean	ephy_autocompletion_window_button_press_event_cb (GtkWidget *widget,
								  GdkEventButton *event,
								  EphyAutocompletionWindow *aw);
static gboolean	ephy_autocompletion_window_key_press_cb		(GtkWidget *widget,
								 GdkEventKey *event,
								 EphyAutocompletionWindow *aw);
static void	ephy_autocompletion_window_event_after_cb	(GtkWidget *wid, GdkEvent *event,
								 EphyAutocompletionWindow *aw);
static void	ephy_autocompletion_window_fill_store_chunk	(EphyAutocompletionWindow *aw);


static gpointer g_object_class;

enum EphyAutocompletionWindowSignalsEnum {
	ACTIVATED,
	SELECTED,
	EPHY_AUTOCOMPLETION_WINDOW_HIDDEN,
	EPHY_AUTOCOMPLETION_WINDOW_LAST_SIGNAL
};
static gint EphyAutocompletionWindowSignals[EPHY_AUTOCOMPLETION_WINDOW_LAST_SIGNAL];

GType
ephy_autocompletion_window_get_type (void)
{
	static GType ephy_autocompletion_window_type = 0;

	if (ephy_autocompletion_window_type == 0)
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyAutocompletionWindowClass),
			NULL,
			NULL,
			(GClassInitFunc) ephy_autocompletion_window_class_init,
			NULL,
			NULL,
			sizeof (EphyAutocompletionWindow),
			0,
			(GInstanceInitFunc) ephy_autocompletion_window_init
		};

		ephy_autocompletion_window_type = g_type_register_static (G_TYPE_OBJECT,
							                  "EphyAutocompletionWindow",
							                  &our_info, 0);
	}

	return ephy_autocompletion_window_type;
}

static void
ephy_autocompletion_window_class_init (EphyAutocompletionWindowClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = ephy_autocompletion_window_finalize_impl;
	g_object_class = g_type_class_peek_parent (klass);

	EphyAutocompletionWindowSignals[ACTIVATED] = g_signal_new (
		"activated", G_OBJECT_CLASS_TYPE (klass),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST | G_SIGNAL_RUN_CLEANUP,
                G_STRUCT_OFFSET (EphyAutocompletionWindowClass, activated),
		NULL, NULL,
		ephy_marshal_VOID__STRING_INT,
		G_TYPE_NONE,
		2,
		G_TYPE_STRING,
		G_TYPE_INT);

	EphyAutocompletionWindowSignals[SELECTED] = g_signal_new (
		"selected", G_OBJECT_CLASS_TYPE (klass),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST | G_SIGNAL_RUN_CLEANUP,
                G_STRUCT_OFFSET (EphyAutocompletionWindowClass, selected),
		NULL, NULL,
		ephy_marshal_VOID__STRING_INT,
		G_TYPE_NONE,
		2,
		G_TYPE_STRING,
		G_TYPE_INT);

	EphyAutocompletionWindowSignals[EPHY_AUTOCOMPLETION_WINDOW_HIDDEN] = g_signal_new (
		"hidden", G_OBJECT_CLASS_TYPE (klass),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST | G_SIGNAL_RUN_CLEANUP,
                G_STRUCT_OFFSET (EphyAutocompletionWindowClass, hidden),
		NULL, NULL,
		ephy_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
ephy_autocompletion_window_init (EphyAutocompletionWindow *aw)
{
	EphyAutocompletionWindowPrivate *p = g_new0 (EphyAutocompletionWindowPrivate, 1);
	GtkTreeSelection *s;

	aw->priv = p;
	p->selected = NULL;

	ephy_autocompletion_window_init_widgets (aw);

	s = gtk_tree_view_get_selection (p->tree_view);
	/* I would like to use GTK_SELECTION_SINGLE, but it seems to require that one
	   item is selected always */
	gtk_tree_selection_set_mode (s, GTK_SELECTION_MULTIPLE);

	g_signal_connect (s, "changed", G_CALLBACK (ephy_autocompletion_window_selection_changed_cb), aw);

	s = gtk_tree_view_get_selection (p->action_tree_view);
	gtk_tree_selection_set_mode (s, GTK_SELECTION_MULTIPLE);

	g_signal_connect (s, "changed", G_CALLBACK (ephy_autocompletion_window_selection_changed_cb), aw);
}

static void
ephy_autocompletion_window_finalize_impl (GObject *o)
{
	EphyAutocompletionWindow *aw = EPHY_AUTOCOMPLETION_WINDOW (o);
	EphyAutocompletionWindowPrivate *p = aw->priv;

	if (p->list_store) g_object_unref (p->list_store);
	if (p->action_list_store) g_object_unref (p->action_list_store);
	if (p->parent) g_object_unref (p->parent);
	if (p->window) gtk_widget_destroy (p->window);

	if (p->autocompletion)
	{
		g_signal_handlers_disconnect_matched (p->autocompletion, G_SIGNAL_MATCH_DATA, 0, 0,
						      NULL, NULL, aw);
		g_object_unref (p->autocompletion);
	}

	g_free (p->selected);
	g_free (p);

	gdk_pointer_ungrab (GDK_CURRENT_TIME);
	gdk_keyboard_ungrab (GDK_CURRENT_TIME);

	G_OBJECT_CLASS (g_object_class)->finalize (o);
}

static void
ephy_autocompletion_window_init_widgets (EphyAutocompletionWindow *aw)
{
	EphyAutocompletionWindowPrivate *p = aw->priv;
	GtkWidget *sw;
	GtkCellRenderer *renderer;
	GtkWidget *frame;
	GtkWidget *vbox;
	GdkColor *bg_color;
	GtkStyle *style;
	GValue v = { 0 };

	p->window = gtk_window_new (GTK_WINDOW_POPUP);
	gtk_window_set_resizable (GTK_WINDOW (p->window), FALSE);

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame),
                                   GTK_SHADOW_OUT);
	gtk_container_add (GTK_CONTAINER (p->window), frame);
	gtk_widget_show (frame);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (frame), vbox);

	sw = gtk_scrolled_window_new (NULL, NULL);
        gtk_box_pack_start (GTK_BOX (vbox),
                            sw, TRUE, TRUE, 0);
	gtk_scrolled_window_set_shadow_type
		(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	p->scrolled_window = GTK_SCROLLED_WINDOW (sw);
	gtk_widget_show (sw);

	p->tree_view = GTK_TREE_VIEW (gtk_tree_view_new ());
	gtk_container_add (GTK_CONTAINER (sw), GTK_WIDGET (p->tree_view));
	gtk_widget_realize (GTK_WIDGET (p->tree_view));

	renderer = gtk_cell_renderer_text_new ();
	p->col1 = gtk_tree_view_column_new ();
	gtk_tree_view_column_pack_start (p->col1, renderer, TRUE);
	gtk_tree_view_column_set_attributes (p->col1, renderer,
                                             "text", 0,
					     NULL);
	gtk_tree_view_append_column (p->tree_view, p->col1);

	gtk_tree_view_set_headers_visible (p->tree_view, FALSE);
	gtk_widget_show (GTK_WIDGET(p->tree_view));

	p->action_tree_view = GTK_TREE_VIEW (gtk_tree_view_new ());
	gtk_box_pack_start (GTK_BOX (vbox),
                            GTK_WIDGET (p->action_tree_view),
			    FALSE, TRUE, 0);

	renderer = gtk_cell_renderer_text_new ();

	g_value_init (&v, GDK_TYPE_COLOR);
	g_object_get_property (G_OBJECT (renderer), "cell_background_gdk", &v);
	bg_color = g_value_peek_pointer (&v);
	style = gtk_widget_get_style (p->window);
	*bg_color = style->bg[GTK_STATE_NORMAL];
	g_object_set_property (G_OBJECT (renderer), "cell_background_gdk", &v);

	p->action_col1 = gtk_tree_view_column_new ();
	gtk_tree_view_column_pack_start (p->action_col1, renderer, TRUE);
	gtk_tree_view_column_set_attributes (p->action_col1, renderer,
                                             "text", 0,
					     NULL);
	gtk_tree_view_append_column (p->action_tree_view, p->action_col1);

	gtk_tree_view_set_headers_visible (p->action_tree_view, FALSE);
	gtk_widget_show (GTK_WIDGET(p->action_tree_view));
}

EphyAutocompletionWindow *
ephy_autocompletion_window_new (EphyAutocompletion *ac, GtkWidget *w)
{
	EphyAutocompletionWindow *ret = g_object_new (EPHY_TYPE_AUTOCOMPLETION_WINDOW, NULL);
	ephy_autocompletion_window_set_parent_widget (ret, w);
	ephy_autocompletion_window_set_autocompletion (ret, ac);
	return ret;
}

void
ephy_autocompletion_window_set_parent_widget (EphyAutocompletionWindow *aw, GtkWidget *w)
{
	if (aw->priv->parent) g_object_unref (aw->priv->parent);
	aw->priv->parent = g_object_ref (w);
}

void
ephy_autocompletion_window_set_autocompletion (EphyAutocompletionWindow *aw,
						 EphyAutocompletion *ac)
{
	EphyAutocompletionWindowPrivate *p = aw->priv;

	if (p->autocompletion)
	{
		g_signal_handlers_disconnect_matched (p->autocompletion, G_SIGNAL_MATCH_DATA, 0, 0,
						      NULL, NULL, aw);

		g_object_unref (p->autocompletion);

	}
	p->autocompletion = g_object_ref (ac);
}

static void
ephy_autocompletion_window_selection_changed_cb (GtkTreeSelection *treeselection,
						 EphyAutocompletionWindow *aw)
{
	GList *l;
	GtkTreeModel *model;

	if (aw->priv->selected) g_free (aw->priv->selected);

	l = gtk_tree_selection_get_selected_rows (treeselection, &model);
	if (l)
	{
		GtkTreePath *path;
		GtkTreeIter iter;
		path = (GtkTreePath *)l->data;

		gtk_tree_model_get_iter (model, &iter, path);
		gtk_tree_model_get (model, &iter, 1,
				    &aw->priv->selected, -1);

		g_list_foreach (l, (GFunc)gtk_tree_path_free, NULL);
		g_list_free (l);
	}
	else
	{
		aw->priv->selected = NULL;
	}
}

static void
ephy_autocompletion_window_get_dimensions (EphyAutocompletionWindow *aw,
					   int *x, int *y, int *width, int *height)
{
	GtkBin *popwin;
	GtkWidget *widget;
	GtkScrolledWindow *popup;
	gint real_height;
	GtkRequisition list_requisition;
	gboolean show_vscroll = FALSE;
	gint avail_height;
	gint min_height;
	gint alloc_width;
	gint work_height;
	gint old_height;
	gint old_width;
	int row_height;

	widget = GTK_WIDGET (aw->priv->parent);
	popup  = GTK_SCROLLED_WINDOW (aw->priv->scrolled_window);
	popwin = GTK_BIN (aw->priv->window);

	gdk_window_get_origin (widget->window, x, y);
	real_height = MIN (widget->requisition.height,
			   widget->allocation.height);
	*y += real_height;
	avail_height = gdk_screen_height () - *y;

	gtk_widget_size_request (GTK_WIDGET(aw->priv->tree_view),
				 &list_requisition);

	alloc_width = (widget->allocation.width -
		       2 * popwin->child->style->xthickness -
		       2 * GTK_CONTAINER (popwin->child)->border_width -
		       2 * GTK_CONTAINER (popup)->border_width -
		       2 * GTK_CONTAINER (GTK_BIN (popup)->child)->border_width -
		       2 * GTK_BIN (popup)->child->style->xthickness);

	work_height = (2 * popwin->child->style->ythickness +
		       2 * GTK_CONTAINER (popwin->child)->border_width +
		       2 * GTK_CONTAINER (popup)->border_width +
		       2 * GTK_CONTAINER (GTK_BIN (popup)->child)->border_width +
		       2 * GTK_BIN (popup)->child->style->ythickness);

	min_height = MIN (list_requisition.height,
                          popup->vscrollbar->requisition.height);

	row_height = list_requisition.height / MAX (aw->priv->view_nitems, 1);
	LOG ("Real list requisition %d, Items %d", list_requisition.height, aw->priv->view_nitems)
	list_requisition.height = MIN (row_height * MAX_VISIBLE_ROWS, list_requisition.height);
	LOG ("Row Height %d, Fake list requisition %d", row_height, list_requisition.height)

	do
	{
		old_width = alloc_width;
		old_height = work_height;

		if (!show_vscroll &&
		    work_height + list_requisition.height > avail_height)
		{
			if (work_height + min_height > avail_height &&
			    *y - real_height > avail_height)
			{
				*y -= (work_height + list_requisition.height +
				       real_height);
				break;
			}
			alloc_width -= (popup->vscrollbar->requisition.width +
					SCROLLBAR_SPACING (popup));
			show_vscroll = TRUE;
		}
	} while (old_width != alloc_width || old_height != work_height);

	*width = widget->allocation.width;

	if (*x < 0) *x = 0;

	*height = MIN (work_height + list_requisition.height,
		       avail_height);

	/* Action view */
	work_height = (2 * GTK_CONTAINER (popup)->border_width +
		       2 * GTK_WIDGET (popup)->style->ythickness);

	if (!GTK_WIDGET_VISIBLE (aw->priv->scrolled_window))
	{
		*height = work_height;
	}

	gtk_widget_size_request (GTK_WIDGET(aw->priv->action_tree_view),
				 &list_requisition);

	if (GTK_WIDGET_VISIBLE (aw->priv->action_tree_view))
	{
		*height += list_requisition.height;
	}
}

static void
ephy_autocompletion_window_fill_store_chunk (EphyAutocompletionWindow *aw)
{
	EphyAutocompletionWindowPrivate *p = aw->priv;
	const EphyAutocompletionMatch *matches;
	guint i;
	gboolean changed;
	guint nmatches;
	guint last;
	guint completion_nitems = 0, action_nitems = 0, substring_nitems = 0;

	START_PROFILER ("Fill store")

	nmatches = ephy_autocompletion_get_num_matches (p->autocompletion);
	matches = ephy_autocompletion_get_matches_sorted_by_score (p->autocompletion,
								   &changed);
	if (!changed) return;

	if (p->list_store) g_object_unref (p->list_store);
	p->list_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

	if (p->action_list_store) g_object_unref (p->action_list_store);
	p->action_list_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

	last = p->last_added_match = 0;

	for (i = 0; last < nmatches; i++, last++)
	{
		const EphyAutocompletionMatch *m = &matches[last];
		GtkTreeIter iter;
		GtkListStore *store;

		if (m->is_action || m->substring ||
		    completion_nitems <= MAX_COMPLETION_ALTERNATIVES)
		{
			if (m->is_action)
			{
				store = p->action_list_store;
				action_nitems ++;
			}
			else if (m->substring)
			{
				store = p->list_store;
				substring_nitems ++;
			}
			else
			{
				store = p->list_store;
				completion_nitems ++;
			}

			gtk_list_store_append (store, &iter);
			gtk_list_store_set (store, &iter,
					    0, m->title,
					    1, m->target,
					    -1);
		}
	}

	p->view_nitems = substring_nitems + completion_nitems;

	gtk_widget_show (GTK_WIDGET (p->scrolled_window));
	gtk_widget_show (GTK_WIDGET (p->action_tree_view));
	if (p->view_nitems == 0)
	{
		gtk_widget_hide (GTK_WIDGET (p->scrolled_window));
	}
	if (action_nitems == 0)
	{
		gtk_widget_hide (GTK_WIDGET (p->action_tree_view));
	}

	p->last_added_match = last;

	STOP_PROFILER ("Fill store")
}

void
ephy_autocompletion_window_show (EphyAutocompletionWindow *aw)
{
	EphyAutocompletionWindowPrivate *p = aw->priv;
	gint x, y, height, width;
	guint nmatches;

	g_return_if_fail (p->window);
	g_return_if_fail (p->autocompletion);

	nmatches = ephy_autocompletion_get_num_matches (p->autocompletion);
	if (nmatches <= 0)
	{
		ephy_autocompletion_window_hide (aw);
		return;
	}

	START_PROFILER ("Showing window")

	ephy_autocompletion_window_fill_store_chunk (aw);

	gtk_tree_view_set_model (p->tree_view, GTK_TREE_MODEL (p->list_store));
	gtk_tree_view_set_model (p->action_tree_view, GTK_TREE_MODEL (p->action_list_store));

	ephy_autocompletion_window_get_dimensions (aw, &x, &y, &width, &height);

	gtk_widget_set_size_request (GTK_WIDGET (p->window), width,
				     height);
	gtk_window_move (GTK_WINDOW (p->window), x, y);

	if (!p->shown)
	{
		gtk_widget_show (p->window);

		gdk_pointer_grab (p->parent->window, TRUE,
				  GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK |
				  GDK_BUTTON_RELEASE_MASK,
				  NULL, NULL, GDK_CURRENT_TIME);
		gdk_keyboard_grab (p->parent->window, TRUE, GDK_CURRENT_TIME);
		gtk_grab_add (p->window);

		g_signal_connect (p->window, "button-press-event",
				  G_CALLBACK (ephy_autocompletion_window_button_press_event_cb),
				  aw);
		g_signal_connect (p->window, "key-press-event",
				  G_CALLBACK (ephy_autocompletion_window_key_press_cb),
				  aw);
		g_signal_connect (p->tree_view, "event-after",
				  G_CALLBACK (ephy_autocompletion_window_event_after_cb),
				  aw);
		g_signal_connect (p->action_tree_view, "event-after",
				  G_CALLBACK (ephy_autocompletion_window_event_after_cb),
				  aw);

		p->shown = TRUE;
	}

	gtk_tree_view_scroll_to_point (GTK_TREE_VIEW (p->tree_view), 0, 0);

	gtk_widget_grab_focus (GTK_WIDGET (p->tree_view));

	STOP_PROFILER ("Showing window")
}

static gboolean
ephy_autocompletion_window_button_press_event_cb (GtkWidget *widget,
						  GdkEventButton *event,
						  EphyAutocompletionWindow *aw)
{
	GtkWidget *event_widget;

	event_widget = gtk_get_event_widget ((GdkEvent *) event);

	/* Check to see if button press happened inside the alternatives
	   window.  If not, destroy the window. */
	if (event_widget != aw->priv->window)
	{
		while (event_widget)
		{
			if (event_widget == aw->priv->window)
				return FALSE;
			event_widget = event_widget->parent;
		}
	}
	ephy_autocompletion_window_hide (aw);

	return TRUE;
}

static void
move_selection (EphyAutocompletionWindow *aw, int dir, gboolean *action)
{
	int new_index;
	int n_compl, n_actions, n_items;
	GtkTreeModel *compl_model, *actions_model;
	GtkTreeSelection *compl_sel, *actions_sel;

	compl_model = gtk_tree_view_get_model (aw->priv->tree_view);
	actions_model = gtk_tree_view_get_model (aw->priv->action_tree_view);
	compl_sel = gtk_tree_view_get_selection (aw->priv->tree_view);
	actions_sel = gtk_tree_view_get_selection (aw->priv->action_tree_view);

	n_compl = gtk_tree_model_iter_n_children (compl_model, NULL);
	n_actions = gtk_tree_model_iter_n_children (actions_model, NULL);
	n_items = n_compl + n_actions;

	/* Index 0  no selection.
	 * Index 1  in the completion.
	 * Index -1 in the actions. */
	new_index = aw->priv->sel_index + dir;

	/* On overflow stay on 0/max if you are no already there. Otherwise
	   go on the opposite limit */
	if (new_index < 0)
	{
		new_index = (aw->priv->sel_index != 0) ? 0 : n_items;
	}
	else if (new_index > n_items)
	{
		new_index = (aw->priv->sel_index != n_items) ? n_items : 0;
	}

	gtk_tree_selection_unselect_all (compl_sel);
	gtk_tree_selection_unselect_all (actions_sel);

	if (new_index == 0)
	{
	}
	else if (new_index > n_compl)
	{
		GtkTreeIter iter;
		GtkTreePath *path;

		gtk_tree_selection_unselect_all (compl_sel);
		gtk_tree_model_iter_nth_child (actions_model, &iter, NULL,
					       new_index - n_compl - 1);
		gtk_tree_selection_select_iter (actions_sel, &iter);

		path = gtk_tree_model_get_path (actions_model, &iter);
		gtk_tree_view_scroll_to_cell (aw->priv->action_tree_view,
					      path, NULL, FALSE, 0, 0);
		gtk_tree_path_free (path);

		*action = TRUE;
	}
	else if (new_index <= n_compl)
	{
		GtkTreeIter iter;
		GtkTreePath *path;

		gtk_tree_selection_unselect_all (actions_sel);
		gtk_tree_model_iter_nth_child (compl_model, &iter, NULL,
					       new_index - 1);
		gtk_tree_selection_select_iter (compl_sel, &iter);

		path = gtk_tree_model_get_path (compl_model, &iter);
		gtk_tree_view_scroll_to_cell (aw->priv->tree_view,
					      path, NULL, FALSE, 0, 0);
		gtk_tree_path_free (path);

		*action = FALSE;
	}
	else
	{
		g_assert_not_reached ();
	}

	aw->priv->sel_index = new_index;
}

static gboolean
ephy_autocompletion_window_key_press_hack (EphyAutocompletionWindow *aw,
					   guint keyval)
{
	gboolean action = FALSE;

	switch (keyval)
	{
	case GDK_Up:
		move_selection (aw, -1, &action);
		break;
	case GDK_Down:
		move_selection (aw, +1, &action);
		break;
	case GDK_Page_Down:
		move_selection (aw, +5, &action);
		break;
	case GDK_Page_Up:
		move_selection (aw, -5, &action);
		break;
	case GDK_Return:
	case GDK_space:
		if (aw->priv->selected)
		{
			g_signal_emit (aw, EphyAutocompletionWindowSignals
				       [ACTIVATED], 0, aw->priv->selected, action);
		}
		break;
	}

	switch (keyval)
	{
	case GDK_Up:
	case GDK_Down:
	case GDK_Page_Down:
	case GDK_Page_Up:
		if (aw->priv->selected)
		{
			g_signal_emit (aw, EphyAutocompletionWindowSignals
				       [SELECTED], 0, aw->priv->selected, action);
		}
		break;
	default:
		break;
	}

	return TRUE;
}

static gboolean
ephy_autocompletion_window_key_press_cb (GtkWidget *widget,
					 GdkEventKey *event,
					 EphyAutocompletionWindow *aw)
{
	EphyAutocompletionWindowPrivate *p = aw->priv;
	GdkEventKey tmp_event;

	if (event->keyval == GDK_Up || event->keyval == GDK_Down
	    || event->keyval == GDK_Page_Up ||  event->keyval == GDK_Page_Down
	    || ((event->keyval == GDK_space || event->keyval == GDK_Return)
		&& p->selected))
	{
		return ephy_autocompletion_window_key_press_hack
			(aw, event->keyval);
	}

	tmp_event = *event;
	gtk_widget_event (p->parent, (GdkEvent *)&tmp_event);

	return TRUE;
}

void
ephy_autocompletion_window_hide (EphyAutocompletionWindow *aw)
{
	if (aw->priv->window && aw->priv->shown)
	{
		gtk_widget_hide (aw->priv->window);
		gtk_grab_remove (aw->priv->window);
		gdk_pointer_ungrab (GDK_CURRENT_TIME);
		gdk_keyboard_ungrab (GDK_CURRENT_TIME);
		ephy_autocompletion_window_unselect (aw);
		g_signal_emit (aw, EphyAutocompletionWindowSignals[EPHY_AUTOCOMPLETION_WINDOW_HIDDEN], 0);
		aw->priv->sel_index = 0;
	}
	g_free (aw->priv->selected);
	aw->priv->selected = NULL;
	aw->priv->shown = FALSE;
}

void
ephy_autocompletion_window_unselect (EphyAutocompletionWindow *aw)
{
	EphyAutocompletionWindowPrivate *p = aw->priv;
	GtkTreeSelection *ts = gtk_tree_view_get_selection (p->tree_view);
	gtk_tree_selection_unselect_all (ts);
}

static void
ephy_autocompletion_window_event_after_cb (GtkWidget *wid, GdkEvent *event,
					   EphyAutocompletionWindow *aw)
{
	gboolean action;
	EphyAutocompletionWindowPrivate *p = aw->priv;

	action = (wid == GTK_WIDGET (p->action_tree_view));

	if (event->type == GDK_BUTTON_PRESS
	    && ((GdkEventButton *) event)->button == 1)
	{
		if (p->selected)
		{
			g_signal_emit (aw, EphyAutocompletionWindowSignals
				       [ACTIVATED], 0, p->selected, action);
		}
	}
}
