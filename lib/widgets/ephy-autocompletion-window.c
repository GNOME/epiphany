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

#include <libgnome/gnome-i18n.h>
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
#include "ephy-gobject-misc.h"
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
	GtkTreeView *active_tree_view;
	gboolean only_actions;

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
	EPHY_AUTOCOMPLETION_WINDOW_HIDDEN,
	EPHY_AUTOCOMPLETION_WINDOW_LAST_SIGNAL
};
static gint EphyAutocompletionWindowSignals[EPHY_AUTOCOMPLETION_WINDOW_LAST_SIGNAL];

/**
 * AutocompletionWindow object
 */

MAKE_GET_TYPE (ephy_autocompletion_window, "EphyAutocompletionWindow", EphyAutocompletionWindow,
	       ephy_autocompletion_window_class_init,
	       ephy_autocompletion_window_init, G_TYPE_OBJECT);

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
	guint32 base, dark;
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
	base = ephy_gui_gdk_color_to_rgb (bg_color);
	dark = ephy_gui_rgb_shift_color (base, 0.15);
	*bg_color = ephy_gui_gdk_rgb_to_color (dark);
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

	p->only_actions = (!GTK_WIDGET_VISIBLE (p->scrolled_window) ||
	                   GTK_WIDGET_HAS_FOCUS (p->action_tree_view));
	if (p->only_actions)
	{
		p->active_tree_view = p->action_tree_view;
	}
	else
	{
		p->active_tree_view = p->tree_view;
	}

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
		gdk_keyboard_grab (p->parent->window, TRUE, GDK_CURRENT_TIME);\
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

static GtkTreeView *
hack_tree_view_move_selection (GtkTreeView *tv, GtkTreeView *alternate, int dir)
{
	GtkTreeSelection *ts = gtk_tree_view_get_selection (tv);
	GtkTreeModel *model;
	GList *selected = NULL;
	gboolean prev_result = TRUE;
	selected = gtk_tree_selection_get_selected_rows (ts, &model);

	gtk_tree_selection_unselect_all (ts);

	if (!selected)
	{
		GtkTreePath *p = gtk_tree_path_new_first ();
		gtk_tree_selection_select_path (ts, p);
		gtk_tree_view_scroll_to_cell (tv, p, NULL, FALSE, 0, 0);
		gtk_tree_path_free (p);
	}
	else
	{
		GtkTreePath *p = selected->data;
		int i;
		if (dir > 0)
		{
			for (i = 0; i < dir; ++i)
			{
				gtk_tree_path_next (p);
			}
		}
		else
		{
			for (i = 0; i > dir; --i)
			{
				prev_result = gtk_tree_path_prev (p);
			}
		}

		if (prev_result)
		{
			gtk_tree_selection_select_path (ts, p);
			gtk_tree_view_scroll_to_cell (tv, p, NULL, FALSE, 0, 0);
		}
	}

	g_list_foreach (selected, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (selected);

	if (!prev_result)
	{
		GtkTreeModel *model;
		int c;
		GtkTreeIter iter;
		GtkTreePath *p;
		GtkTreeSelection *selection;

		model = gtk_tree_view_get_model (alternate);
		c = gtk_tree_model_iter_n_children (model, NULL);
		if (c > 0)
		{
			gtk_tree_model_iter_nth_child (model, &iter, NULL, c - 1);
			p = gtk_tree_model_get_path (model, &iter);
			selection = gtk_tree_view_get_selection (alternate);
			gtk_tree_selection_select_path (selection, p);
			gtk_tree_view_scroll_to_cell (alternate, p, NULL, FALSE, 0, 0);
			gtk_tree_path_free (p);
		}
		return alternate;
	}
	else if (gtk_tree_selection_count_selected_rows (ts) == 0)
	{
		hack_tree_view_move_selection (alternate, tv, dir);
		return alternate;
	}

	return tv;
}

static gboolean
ephy_autocompletion_window_key_press_hack (EphyAutocompletionWindow *aw,
					   guint keyval)
{
	GtkTreeView *tree_view, *alt;
	EphyAutocompletionWindowPrivate *p = aw->priv;
	gboolean action;

	action = (p->active_tree_view == p->action_tree_view);
	tree_view = action ? p->action_tree_view : p->tree_view;
	alt = action ? p->tree_view : p->action_tree_view;
	alt = p->only_actions ? p->action_tree_view : alt;

	switch (keyval)
	{
	case GDK_Up:
		p->active_tree_view = hack_tree_view_move_selection
			(tree_view, alt, -1);
		break;
	case GDK_Down:
		p->active_tree_view = hack_tree_view_move_selection
			(tree_view, alt, +1);
		break;
	case GDK_Page_Down:
		p->active_tree_view = hack_tree_view_move_selection
			(tree_view, alt, +5);
		break;
	case GDK_Page_Up:
		p->active_tree_view = hack_tree_view_move_selection
			(tree_view, alt, -5);
		break;
	case GDK_Return:
	case GDK_space:
		if (aw->priv->selected)
		{
			g_signal_emit (aw, EphyAutocompletionWindowSignals
				       [ACTIVATED], 0, aw->priv->selected, action);
		}
		break;
	default:
		g_warning ("Unexpected keyval");
		break;
	}
	return TRUE;
}

static gboolean
ephy_autocompletion_window_key_press_cb (GtkWidget *widget,
					 GdkEventKey *event,
					 EphyAutocompletionWindow *aw)
{
	GdkEventKey tmp_event;
	EphyAutocompletionWindowPrivate *p = aw->priv;
	GtkWidget *dest_widget;

	/* allow keyboard navigation in the alternatives clist */
	if (event->keyval == GDK_Up || event->keyval == GDK_Down
	    || event->keyval == GDK_Page_Up ||  event->keyval == GDK_Page_Down
	    || ((event->keyval == GDK_space || event->keyval == GDK_Return)
		&& p->selected))
	{
		return ephy_autocompletion_window_key_press_hack
			(aw, event->keyval);
	}
	else
	{
		dest_widget = p->parent;
	}

	if (dest_widget != widget)
	{
		tmp_event = *event;
		gtk_widget_event (dest_widget, (GdkEvent *)&tmp_event);

		return TRUE;
	}
	else
	{
		return FALSE;
	}

}

void
ephy_autocompletion_window_hide (EphyAutocompletionWindow *aw)
{
	if (aw->priv->window)
	{
		gtk_widget_hide (aw->priv->window);
		gtk_grab_remove (aw->priv->window);
		gdk_pointer_ungrab (GDK_CURRENT_TIME);
		gdk_keyboard_ungrab (GDK_CURRENT_TIME);
		ephy_autocompletion_window_unselect (aw);
		g_signal_emit (aw, EphyAutocompletionWindowSignals[EPHY_AUTOCOMPLETION_WINDOW_HIDDEN], 0);
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
