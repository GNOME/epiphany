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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <libgnome/gnome-i18n.h>

#include "ephy-gobject-misc.h"
#include "ephy-marshal.h"
#include "ephy-toolbar-editor.h"
#include "ephy-toolbar-tree-model.h"
#include "ephy-glade.h"

#define NOT_IMPLEMENTED g_warning ("not implemented: " G_STRLOC);
//#define DEBUG_MSG(x) g_print x
#define DEBUG_MSG(x)

/**
 * Private data
 */
struct _EphyTbEditorPrivate
{
	EphyToolbar *tb;
	EphyToolbar *available;

	gchar *tb_undo_string;
	gchar *available_undo_string;

	gboolean in_toolbar_changed;

	GtkWidget *window;
	GtkWidget *available_view;
	GtkWidget *current_view;
	GtkWidget *close_button;
	GtkWidget *undo_button;
	GtkWidget *revert_button;
	GtkWidget *up_button;
	GtkWidget *down_button;
	GtkWidget *left_button;
	GtkWidget *right_button;
};

/**
 * Private functions, only available from this file
 */
static void		ephy_tb_editor_class_init		(EphyTbEditorClass *klass);
static void		ephy_tb_editor_init			(EphyTbEditor *tbe);
static void		ephy_tb_editor_finalize_impl		(GObject *o);
static void		ephy_tb_editor_init_widgets		(EphyTbEditor *tbe);
static void		ephy_tb_editor_set_treeview_toolbar	(EphyTbEditor *tbe,
								 GtkTreeView *tv, EphyToolbar *tb);
static void		ephy_tb_editor_setup_treeview		(EphyTbEditor *tbe, GtkTreeView *tv);
static EphyTbItem *	ephy_tb_editor_get_selected		(EphyTbEditor *tbe, GtkTreeView *tv);
static gint		ephy_tb_editor_get_selected_index	(EphyTbEditor *tbe, GtkTreeView *tv);
static void		ephy_tb_editor_select_index		(EphyTbEditor *tbe, GtkTreeView *tv,
								 gint index);
static void		ephy_tb_editor_remove_used_items	(EphyTbEditor *tbe);

static void		ephy_tb_editor_undo_clicked_cb		(GtkWidget *b, EphyTbEditor *tbe);
static void		ephy_tb_editor_close_clicked_cb		(GtkWidget *b, EphyTbEditor *tbe);
static void		ephy_tb_editor_up_clicked_cb		(GtkWidget *b, EphyTbEditor *tbe);
static void		ephy_tb_editor_down_clicked_cb		(GtkWidget *b, EphyTbEditor *tbe);
static void		ephy_tb_editor_left_clicked_cb		(GtkWidget *b, EphyTbEditor *tbe);
static void		ephy_tb_editor_right_clicked_cb		(GtkWidget *b, EphyTbEditor *tbe);
static void		ephy_tb_editor_toolbar_changed_cb	(EphyToolbar *tb, EphyTbEditor *tbe);
static gboolean		ephy_tb_editor_treeview_button_press_event_cb (GtkWidget *widget,
								      GdkEventButton *event,
								      EphyTbEditor *tbe);


static gpointer g_object_class;

/* treeview dnd */
enum
{
	TARGET_GTK_TREE_MODEL_ROW
};
static GtkTargetEntry tree_view_row_targets[] = {
	{ "GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_APP, TARGET_GTK_TREE_MODEL_ROW }
};

/**
 * TbEditor object
 */

MAKE_GET_TYPE (ephy_tb_editor, "EphyTbEditor", EphyTbEditor, ephy_tb_editor_class_init,
	       ephy_tb_editor_init, G_TYPE_OBJECT);

static void
ephy_tb_editor_class_init (EphyTbEditorClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = ephy_tb_editor_finalize_impl;

	g_object_class = g_type_class_peek_parent (klass);
}

static void
ephy_tb_editor_init (EphyTbEditor *tb)
{
	EphyTbEditorPrivate *p = g_new0 (EphyTbEditorPrivate, 1);
	tb->priv = p;

	ephy_tb_editor_init_widgets (tb);
}

static void
update_arrows_sensitivity (EphyTbEditor *tbe)
{
	GtkTreeSelection *selection;
	gboolean current_sel;
	gboolean avail_sel;
	gboolean first = FALSE;
	gboolean last = FALSE;
	GtkTreeModel *tm;
	GtkTreeIter iter;
	GtkTreePath *path;

	selection = gtk_tree_view_get_selection
		(GTK_TREE_VIEW (tbe->priv->current_view));
	current_sel = gtk_tree_selection_get_selected (selection, &tm, &iter);
	if (current_sel)
	{
		path = gtk_tree_model_get_path (tm, &iter);
		first = !gtk_tree_path_prev (path);
		last = !gtk_tree_model_iter_next (tm, &iter);
	}

	selection = gtk_tree_view_get_selection
		(GTK_TREE_VIEW (tbe->priv->available_view));
	avail_sel = gtk_tree_selection_get_selected (selection, &tm, &iter);

	gtk_widget_set_sensitive (tbe->priv->right_button,
				  avail_sel);
	gtk_widget_set_sensitive (tbe->priv->left_button,
				  current_sel);
	gtk_widget_set_sensitive (tbe->priv->up_button,
				  current_sel && !first);
	gtk_widget_set_sensitive (tbe->priv->down_button,
				  current_sel && !last);
}

static void
ephy_tb_editor_treeview_selection_changed_cb (GtkTreeSelection *selection,
					      EphyTbEditor *tbe)
{
	update_arrows_sensitivity (tbe);
}

static void
ephy_tb_editor_init_widgets (EphyTbEditor *tbe)
{
	EphyTbEditorPrivate *p = tbe->priv;

	GladeXML *gxml = ephy_glade_widget_new ("toolbar-editor.glade", "toolbar-editor-dialog",
					       NULL, tbe);
	p->window = glade_xml_get_widget (gxml, "toolbar-editor-dialog");
	p->available_view = glade_xml_get_widget (gxml, "toolbar-editor-available-view");
	p->current_view = glade_xml_get_widget (gxml, "toolbar-editor-current-view");
	p->close_button = glade_xml_get_widget (gxml, "toolbar-editor-close-button");
	p->undo_button = glade_xml_get_widget (gxml, "toolbar-editor-undo-button");
	p->revert_button = glade_xml_get_widget (gxml, "toolbar-editor-revert-button");
	p->up_button = glade_xml_get_widget (gxml, "toolbar-editor-up-button");
	p->down_button = glade_xml_get_widget (gxml, "toolbar-editor-down-button");
	p->left_button = glade_xml_get_widget (gxml, "toolbar-editor-left-button");
	p->right_button = glade_xml_get_widget (gxml, "toolbar-editor-right-button");
	g_object_unref (gxml);

	g_signal_connect_swapped (p->window, "delete_event", G_CALLBACK (g_object_unref), tbe);
	g_signal_connect (p->undo_button, "clicked", G_CALLBACK (ephy_tb_editor_undo_clicked_cb), tbe);
	g_signal_connect (p->close_button, "clicked", G_CALLBACK (ephy_tb_editor_close_clicked_cb), tbe);
	g_signal_connect (p->up_button, "clicked", G_CALLBACK (ephy_tb_editor_up_clicked_cb), tbe);
	g_signal_connect (p->down_button, "clicked", G_CALLBACK (ephy_tb_editor_down_clicked_cb), tbe);
	g_signal_connect (p->left_button, "clicked", G_CALLBACK (ephy_tb_editor_left_clicked_cb), tbe);
	g_signal_connect (p->right_button, "clicked", G_CALLBACK (ephy_tb_editor_right_clicked_cb), tbe);

	ephy_tb_editor_setup_treeview (tbe, GTK_TREE_VIEW (p->current_view));
	ephy_tb_editor_setup_treeview (tbe, GTK_TREE_VIEW (p->available_view));
}

static void
ephy_tb_editor_undo_clicked_cb (GtkWidget *b, EphyTbEditor *tbe)
{
	EphyTbEditorPrivate *p = tbe->priv;
	if (p->available_undo_string && p->available)
	{
		ephy_toolbar_parse (p->available, p->available_undo_string);
	}

	if (p->tb_undo_string && p->tb)
	{
		ephy_toolbar_parse (p->tb, p->tb_undo_string);
	}
}

static void
ephy_tb_editor_close_clicked_cb (GtkWidget *b, EphyTbEditor *tbe)
{
	gtk_widget_hide (tbe->priv->window);
	g_object_unref (tbe);
}

static void
ephy_tb_editor_up_clicked_cb (GtkWidget *b, EphyTbEditor *tbe)
{
	EphyTbEditorPrivate *p = tbe->priv;
	EphyTbItem *item = ephy_tb_editor_get_selected (tbe, GTK_TREE_VIEW (p->current_view));
	gint index = ephy_tb_editor_get_selected_index (tbe, GTK_TREE_VIEW (p->current_view));
	if (item && index > 0)
	{
		g_object_ref (item);
		ephy_toolbar_remove_item (p->tb, item);
		ephy_toolbar_add_item (p->tb, item, index - 1);
		ephy_tb_editor_select_index (tbe, GTK_TREE_VIEW (p->current_view), index - 1);
		g_object_unref (item);
	}
}

static void
ephy_tb_editor_down_clicked_cb (GtkWidget *b, EphyTbEditor *tbe)
{
	EphyTbEditorPrivate *p = tbe->priv;
	EphyTbItem *item = ephy_tb_editor_get_selected (tbe, GTK_TREE_VIEW (p->current_view));
	gint index = ephy_tb_editor_get_selected_index (tbe, GTK_TREE_VIEW (p->current_view));
	if (item)
	{
		g_object_ref (item);
		ephy_toolbar_remove_item (p->tb, item);
		ephy_toolbar_add_item (p->tb, item, index + 1);
		ephy_tb_editor_select_index (tbe, GTK_TREE_VIEW (p->current_view), index + 1);
		g_object_unref (item);
	}
}

static void
ephy_tb_editor_left_clicked_cb (GtkWidget *b, EphyTbEditor *tbe)
{
	EphyTbEditorPrivate *p = tbe->priv;
	EphyTbItem *item = ephy_tb_editor_get_selected (tbe, GTK_TREE_VIEW (p->current_view));
	/* probably is better not allowing reordering the available_view */
	gint index = ephy_tb_editor_get_selected_index (tbe, GTK_TREE_VIEW (p->available_view));
	if (item)
	{
		g_object_ref (item);
		ephy_toolbar_remove_item (p->tb, item);
		if (ephy_tb_item_is_unique (item))
		{
			ephy_toolbar_add_item (p->available, item, index);
		}
		g_object_unref (item);
	}
}

static void
ephy_tb_editor_right_clicked_cb (GtkWidget *b, EphyTbEditor *tbe)
{
	EphyTbEditorPrivate *p = tbe->priv;
	EphyTbItem *item = ephy_tb_editor_get_selected (tbe, GTK_TREE_VIEW (p->available_view));
	gint index = ephy_tb_editor_get_selected_index (tbe, GTK_TREE_VIEW (p->current_view));
	if (item)
	{
		if (ephy_tb_item_is_unique (item))
		{
			g_object_ref (item);
			ephy_toolbar_remove_item (p->available, item);
		}
		else
		{
			item = ephy_tb_item_clone (item);
		}
		ephy_toolbar_add_item (p->tb, item, index);
		ephy_tb_editor_select_index (tbe, GTK_TREE_VIEW (p->current_view), index);
		g_object_unref (item);
	}
}

static EphyTbItem *
ephy_tb_editor_get_selected (EphyTbEditor *tbe, GtkTreeView *tv)
{
	GtkTreeSelection *sel = gtk_tree_view_get_selection (tv);
	GtkTreeModel *tm;
	GtkTreeIter iter;
	if (gtk_tree_selection_get_selected (sel, &tm, &iter))
	{
		EphyTbItem *ret;
		g_return_val_if_fail (EPHY_IS_TB_TREE_MODEL (tm), NULL);
		ret = ephy_tb_tree_model_item_from_iter (EPHY_TB_TREE_MODEL (tm), &iter);
		return ret;
	}
	else
	{
		return NULL;
	}
}

static gint
ephy_tb_editor_get_selected_index (EphyTbEditor *tbe, GtkTreeView *tv)
{
	GtkTreeSelection *sel = gtk_tree_view_get_selection (tv);
	GtkTreeModel *tm;
	GtkTreeIter iter;
	if (gtk_tree_selection_get_selected (sel, &tm, &iter))
	{
		GtkTreePath *p = gtk_tree_model_get_path (tm, &iter);
		if (p)
		{
			gint ret = gtk_tree_path_get_depth (p) > 0 ? gtk_tree_path_get_indices (p)[0] : -1;
			gtk_tree_path_free (p);
			return ret;
		}
		else
		{
			return -1;
		}
	}
	else
	{
		return -1;
	}
}

static void
ephy_tb_editor_select_index (EphyTbEditor *tbe, GtkTreeView *tv, gint index)
{
	GtkTreeSelection *sel = gtk_tree_view_get_selection (tv);
	GtkTreePath *p = gtk_tree_path_new ();
	GtkTreeModel *tm = gtk_tree_view_get_model (tv);
	gint max = gtk_tree_model_iter_n_children (tm, NULL);

	if (index < 0 || index >= max)
	{
		index = max - 1;
	}

	gtk_tree_path_append_index (p, index);
	gtk_tree_selection_select_path (sel, p);
	gtk_tree_path_free (p);
}

static void
ephy_tb_editor_finalize_impl (GObject *o)
{
	EphyTbEditor *tbe = EPHY_TB_EDITOR (o);
	EphyTbEditorPrivate *p = tbe->priv;

	if (p->tb)
	{
		g_signal_handlers_disconnect_matched (p->tb, G_SIGNAL_MATCH_DATA, 0, 0,
						      NULL, NULL, tbe);

		g_object_unref (p->tb);
	}
	if (p->available)
	{
		g_signal_handlers_disconnect_matched (p->available, G_SIGNAL_MATCH_DATA, 0, 0,
						      NULL, NULL, tbe);
		g_object_unref (p->available);
	}

	if (p->window)
	{
		gtk_widget_destroy (p->window);
	}

	g_free (p->tb_undo_string);
	g_free (p->available_undo_string);

	g_free (p);

	DEBUG_MSG (("EphyTbEditor finalized\n"));

	G_OBJECT_CLASS (g_object_class)->finalize (o);
}

EphyTbEditor *
ephy_tb_editor_new (void)
{
	EphyTbEditor *ret = g_object_new (EPHY_TYPE_TB_EDITOR, NULL);
	return ret;
}

void
ephy_tb_editor_set_toolbar (EphyTbEditor *tbe, EphyToolbar *tb)
{
	EphyTbEditorPrivate *p = tbe->priv;

	if (p->tb)
	{
		g_signal_handlers_disconnect_matched (p->tb, G_SIGNAL_MATCH_DATA, 0, 0,
						      NULL, NULL, tbe);
		g_object_unref (p->tb);
	}
	p->tb = g_object_ref (tb);

	g_free (p->tb_undo_string);
	p->tb_undo_string = ephy_toolbar_to_string (p->tb);

	if (p->available)
	{
		ephy_tb_editor_remove_used_items (tbe);
	}

	g_signal_connect (p->tb, "changed", G_CALLBACK (ephy_tb_editor_toolbar_changed_cb), tbe);

	ephy_tb_editor_set_treeview_toolbar (tbe, GTK_TREE_VIEW (p->current_view), p->tb);

	gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (p->current_view),
						GDK_BUTTON1_MASK,
						tree_view_row_targets,
						G_N_ELEMENTS (tree_view_row_targets),
						GDK_ACTION_MOVE);
	gtk_tree_view_enable_model_drag_dest (GTK_TREE_VIEW (p->current_view),
					      tree_view_row_targets,
					      G_N_ELEMENTS (tree_view_row_targets),
					      GDK_ACTION_COPY);
}

void
ephy_tb_editor_set_available (EphyTbEditor *tbe, EphyToolbar *tb)
{
	EphyTbEditorPrivate *p = tbe->priv;

	if (p->available)
	{
		g_signal_handlers_disconnect_matched (p->available, G_SIGNAL_MATCH_DATA, 0, 0,
						      NULL, NULL, tbe);
		g_object_unref (p->available);
	}
	p->available = g_object_ref (tb);

	g_free (p->available_undo_string);
	p->available_undo_string = ephy_toolbar_to_string (p->available);

	ephy_toolbar_set_fixed_order (p->available, TRUE);

	if (p->tb)
	{
		ephy_tb_editor_remove_used_items (tbe);
	}

	ephy_tb_editor_set_treeview_toolbar (tbe, GTK_TREE_VIEW (p->available_view), p->available);

	gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (p->available_view),
						GDK_BUTTON1_MASK,
						tree_view_row_targets,
						G_N_ELEMENTS (tree_view_row_targets),
						GDK_ACTION_COPY);
	gtk_tree_view_enable_model_drag_dest (GTK_TREE_VIEW (p->available_view),
					      tree_view_row_targets,
					      G_N_ELEMENTS (tree_view_row_targets),
					      GDK_ACTION_MOVE);
}

void
ephy_tb_editor_set_parent (EphyTbEditor *tbe, GtkWidget *parent)
{
        gtk_window_set_transient_for (GTK_WINDOW (tbe->priv->window),
                                      GTK_WINDOW (parent));
}

void
ephy_tb_editor_show (EphyTbEditor *tbe)
{
	gtk_window_present (GTK_WINDOW (tbe->priv->window));
}

static void
ephy_tb_editor_set_treeview_toolbar (EphyTbEditor *tbe, GtkTreeView *tv, EphyToolbar *tb)
{
	EphyTbTreeModel *tm = ephy_tb_tree_model_new ();
	ephy_tb_tree_model_set_toolbar (tm, tb);
	gtk_tree_view_set_model (tv, GTK_TREE_MODEL (tm));
	g_object_unref (tm);
}

static void
ephy_tb_editor_setup_treeview (EphyTbEditor *tbe, GtkTreeView *tv)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection (tv);
	column = gtk_tree_view_column_new ();
	renderer = gtk_cell_renderer_pixbuf_new ();

	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_set_attributes (column, renderer,
                                             "pixbuf", EPHY_TB_TREE_MODEL_COL_ICON,
					     NULL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_attributes (column, renderer,
                                             "text", EPHY_TB_TREE_MODEL_COL_NAME,
					     NULL);
	gtk_tree_view_column_set_title (column,  "Name");
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tv), column);

	g_signal_connect (tv, "button-press-event",
			  G_CALLBACK (ephy_tb_editor_treeview_button_press_event_cb), tbe);
	g_signal_connect (selection, "changed",
			  G_CALLBACK (ephy_tb_editor_treeview_selection_changed_cb), tbe);
}

EphyToolbar *
ephy_tb_editor_get_toolbar (EphyTbEditor *tbe)
{
	EphyTbEditorPrivate *p;

	g_return_val_if_fail (EPHY_IS_TB_EDITOR (tbe), NULL);

	p = tbe->priv;

	return p->tb;
}

EphyToolbar *
ephy_tb_editor_get_available (EphyTbEditor *tbe)
{
	EphyTbEditorPrivate *p;

	g_return_val_if_fail (EPHY_IS_TB_EDITOR (tbe), NULL);

	p = tbe->priv;

	return p->available;
}


static void
ephy_tb_editor_remove_used_items (EphyTbEditor *tbe)
{
	EphyTbEditorPrivate *p = tbe->priv;
	const GSList *current_items;
	const GSList *li;

	g_return_if_fail (EPHY_IS_TOOLBAR (p->tb));
	g_return_if_fail (EPHY_IS_TOOLBAR (p->available));

	current_items = ephy_toolbar_get_item_list (p->tb);
	for (li = current_items; li; li = li->next)
	{
		EphyTbItem *i = li->data;
		if (ephy_tb_item_is_unique (i))
		{
			EphyTbItem *j = ephy_toolbar_get_item_by_id (p->available, i->id);
			if (j)
			{
				ephy_toolbar_remove_item (p->available, j);
			}
		}
	}
}

static void
ephy_tb_editor_toolbar_changed_cb (EphyToolbar *tb, EphyTbEditor *tbe)
{
	EphyTbEditorPrivate *p = tbe->priv;

	if (p->in_toolbar_changed)
	{
		return;
	}

	if (p->tb && p->available)
	{
		p->in_toolbar_changed = TRUE;
		ephy_tb_editor_remove_used_items (tbe);
		p->in_toolbar_changed = FALSE;
	}
}

static gboolean
ephy_tb_editor_treeview_button_press_event_cb (GtkWidget *widget,
					       GdkEventButton *event,
					       EphyTbEditor *tbe)
{
	EphyTbEditorPrivate *p = tbe->priv;

	if (event->window != gtk_tree_view_get_bin_window (GTK_TREE_VIEW (widget)))
	{
		return FALSE;
	}

	if (event->type == GDK_2BUTTON_PRESS)
	{
		if (widget == p->current_view)
		{
			ephy_tb_editor_left_clicked_cb (NULL, tbe);
		}
		else if (widget == p->available_view)
		{
			ephy_tb_editor_right_clicked_cb (NULL, tbe);
		}
		else
		{
			g_assert_not_reached ();
		}
		return TRUE;
	}

	return FALSE;
}

GtkButton *
ephy_tb_editor_get_revert_button (EphyTbEditor *tbe)
{
	EphyTbEditorPrivate *p;
	g_return_val_if_fail (EPHY_IS_TB_EDITOR (tbe), NULL);
	p = tbe->priv;
	g_return_val_if_fail (GTK_IS_BUTTON (p->revert_button), NULL);

	return GTK_BUTTON (p->revert_button);

}

