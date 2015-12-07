/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 2000, 2001, 2002, 2003 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
 *  Copyright © 2012 Igalia S.L.
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "ephy-encoding-dialog.h"

#include "ephy-debug.h"
#include "ephy-embed-container.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-utils.h"
#include "ephy-embed.h"
#include "ephy-encodings.h"
#include "ephy-file-helpers.h"
#include "ephy-gui.h"
#include "ephy-shell.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

struct _EphyEncodingDialog
{
	GtkDialog parent_instance;

	EphyEncodings *encodings;
	EphyWindow *window;
	EphyEmbed *embed;
	GtkWidget *enc_view;
	gboolean update_tag;
	char *selected_encoding;

	/* from the UI file */
	GtkRadioButton    *automatic_button;
	GtkRadioButton    *manual_button;
	GtkScrolledWindow *scrolled_window;
};

enum {
	COL_TITLE_ELIDED,
	COL_ENCODING,
	NUM_COLS
};

enum
{
	PROP_0,
	PROP_PARENT_WINDOW
};

G_DEFINE_TYPE (EphyEncodingDialog, ephy_encoding_dialog, GTK_TYPE_DIALOG)

static void
select_encoding_row (GtkTreeView *view, EphyEncoding *encoding)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	gboolean valid_iter = FALSE;
	const char *target_encoding;

	model = gtk_tree_view_get_model (view);
	valid_iter = gtk_tree_model_get_iter_first (model, &iter);

	target_encoding = ephy_encoding_get_encoding (encoding);

	while (valid_iter)
	{
		char *encoding_string = NULL;

		gtk_tree_model_get (model, &iter,
				    COL_ENCODING, &encoding_string, -1);

		if (g_str_equal (encoding_string,
				 target_encoding))
		{
			GtkTreeSelection *selection;

			selection = gtk_tree_view_get_selection (view);
			gtk_tree_selection_select_iter (selection, &iter);
			g_free (encoding_string);

			return;
		}

		g_free (encoding_string);
		valid_iter = gtk_tree_model_iter_next (model, &iter);
	}
}

static void
sync_encoding_against_embed (EphyEncodingDialog *dialog)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GList *rows;
	const char *encoding;
	gboolean is_automatic = FALSE;
	WebKitWebView *view;
	EphyEncoding *node;

	dialog->update_tag = TRUE;

	g_return_if_fail (EPHY_IS_EMBED (dialog->embed));
	view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (dialog->embed);

	encoding = webkit_web_view_get_custom_charset (view);
	if (encoding == NULL) goto out;

	node = ephy_encodings_get_encoding (dialog->encodings, encoding, TRUE);
	g_assert (EPHY_IS_ENCODING (node));

	/* Select the current encoding in the list view. */
	select_encoding_row (GTK_TREE_VIEW (dialog->enc_view), node);

	/* scroll the view so the active encoding is visible */
        selection = gtk_tree_view_get_selection
                (GTK_TREE_VIEW (dialog->enc_view));
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->enc_view));
        rows = gtk_tree_selection_get_selected_rows (selection, &model);
        if (rows != NULL)
	{
		gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (dialog->enc_view),
					      (GtkTreePath *) rows->data,
					      NULL, /* column */
					      TRUE,
					      0.5,
					      0.0);
		g_list_foreach (rows, (GFunc)gtk_tree_path_free, NULL);
		g_list_free (rows);
	}

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->automatic_button), is_automatic);
out:
	dialog->update_tag = FALSE;
}

static void
embed_net_stop_cb (EphyWebView *view,
		   WebKitLoadEvent load_event,
		   EphyEncodingDialog *dialog)
{
	if (ephy_web_view_is_loading (view) == FALSE)
		sync_encoding_against_embed (dialog);
}

static void
ephy_encoding_dialog_detach_embed (EphyEncodingDialog *dialog)
{
	EphyEmbed **embedptr;

	g_signal_handlers_disconnect_by_func (ephy_embed_get_web_view (dialog->embed),
	                                      G_CALLBACK (embed_net_stop_cb),
	                                      dialog);

	embedptr = &dialog->embed;
	g_object_remove_weak_pointer (G_OBJECT (dialog->embed),
	                              (gpointer *) embedptr);
	dialog->embed = NULL;
}

static void
ephy_encoding_dialog_attach_embed (EphyEncodingDialog *dialog)
{
	EphyEmbed *embed;
	EphyEmbed **embedptr;

	embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (dialog->window));
	g_return_if_fail (EPHY_IS_EMBED (embed));

	g_signal_connect (G_OBJECT (ephy_embed_get_web_view (embed)), "load-changed",
			  G_CALLBACK (embed_net_stop_cb), dialog);

	dialog->embed = embed;

	embedptr = &dialog->embed;
	g_object_add_weak_pointer (G_OBJECT (dialog->embed),
	                           (gpointer *) embedptr);

	sync_encoding_against_embed (dialog);
}

static void
ephy_encoding_dialog_sync_embed (EphyWindow *window, GParamSpec *pspec, EphyEncodingDialog *dialog)
{
	ephy_encoding_dialog_detach_embed (dialog);
	ephy_encoding_dialog_attach_embed (dialog);
}

static void
activate_choice (EphyEncodingDialog *dialog)
{
	gboolean is_automatic;
	WebKitWebView *view;

	g_return_if_fail (EPHY_IS_EMBED (dialog->embed));

	is_automatic = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->automatic_button));

	view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (dialog->embed);

	if (is_automatic)
	{
		webkit_web_view_set_custom_charset (view, NULL);
	}
	else if (dialog->selected_encoding != NULL)
	{
		const char *code;

		code = dialog->selected_encoding;

		webkit_web_view_set_custom_charset (view, code);

		ephy_encodings_add_recent (dialog->encodings, code);
	}
}

static void
ephy_encoding_dialog_response_cb (GtkWidget *widget,
				  int response,
				  EphyEncodingDialog *dialog)
{
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
view_row_activated_cb (GtkTreeView *treeview,
		       GtkTreePath *path,
		       GtkTreeViewColumn *column,
		       EphyEncodingDialog *dialog)
{
	GtkTreeIter iter;
	char *encoding;
	GtkTreeModel *model;

	model = gtk_tree_view_get_model (treeview);

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter,
			    COL_ENCODING, &encoding,
			    -1);

	g_free (dialog->selected_encoding);
	dialog->selected_encoding = encoding;

	if (dialog->update_tag)
		return;

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->manual_button), TRUE);

	activate_choice (dialog);
}

static void
automatic_toggled_cb (GtkToggleButton *button, EphyEncodingDialog *dialog)
{
	if (gtk_toggle_button_get_active (button)
	    && dialog->update_tag == FALSE)
	{
		activate_choice (dialog);
	}
}

static void
ephy_encoding_dialog_init (EphyEncodingDialog *dialog)
{
	GtkWidget *treeview;
	GtkTreeSelection *selection;
	GList *encodings, *p;
	GtkListStore *store;
	GtkTreeIter iter;
	GtkCellRenderer *renderer;

	gtk_widget_init_template (GTK_WIDGET (dialog));

	dialog->encodings =
		EPHY_ENCODINGS (ephy_embed_shell_get_encodings
				(EPHY_EMBED_SHELL (ephy_shell_get_default ())));

	encodings = ephy_encodings_get_all (dialog->encodings);
	store = gtk_list_store_new (NUM_COLS, G_TYPE_STRING, G_TYPE_STRING);
	for (p = encodings; p; p = p->next) 
	{
		EphyEncoding *encoding = EPHY_ENCODING (p->data);

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    COL_TITLE_ELIDED, 
				    ephy_encoding_get_title_elided (encoding),
				    -1);
		gtk_list_store_set (store, &iter,
				    COL_ENCODING, 
				    ephy_encoding_get_encoding (encoding),
				    -1);
	}
	g_list_free (encodings);

	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store), COL_TITLE_ELIDED,
					      GTK_SORT_ASCENDING);

	treeview = gtk_tree_view_new ();
	renderer = gtk_cell_renderer_text_new ();

	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview),
						     -1,
						     _("Encodings"),
						     renderer,
						     "text", COL_TITLE_ELIDED,
						     NULL);
	gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), GTK_TREE_MODEL (store));

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW(treeview), FALSE);
	gtk_tree_view_set_activate_on_single_click (GTK_TREE_VIEW (treeview), TRUE);

	g_signal_connect (treeview, "row-activated",
			  G_CALLBACK (view_row_activated_cb),
			  dialog);

	gtk_widget_show (treeview);

	gtk_container_add (GTK_CONTAINER (dialog->scrolled_window), treeview);

	dialog->enc_view = treeview;
}

static void
ephy_encoding_dialog_dispose (GObject *object)
{
	EphyEncodingDialog *dialog = EPHY_ENCODING_DIALOG (object);

	g_signal_handlers_disconnect_by_func (dialog->window,
	                                      G_CALLBACK (ephy_encoding_dialog_sync_embed),
	                                      dialog);

	if (dialog->embed != NULL)
		ephy_encoding_dialog_detach_embed (dialog);

	G_OBJECT_CLASS (ephy_encoding_dialog_parent_class)->dispose (object);
}

static void
ephy_encoding_dialog_finalize (GObject *object)
{
	EphyEncodingDialog *dialog = EPHY_ENCODING_DIALOG (object);

	g_free (dialog->selected_encoding);

	G_OBJECT_CLASS (ephy_encoding_dialog_parent_class)->finalize (object);
}

static void
ephy_encoding_dialog_set_parent_window (EphyEncodingDialog *dialog,
                                        EphyWindow *window)
{
	g_return_if_fail (EPHY_IS_WINDOW (window));

	g_signal_connect (G_OBJECT (window), "notify::active-child",
	                  G_CALLBACK (ephy_encoding_dialog_sync_embed), dialog);

	dialog->window = window;

	ephy_encoding_dialog_attach_embed (dialog);
}

static void
ephy_encoding_dialog_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
	switch (prop_id)
	{
	case PROP_PARENT_WINDOW:
		ephy_encoding_dialog_set_parent_window (EPHY_ENCODING_DIALOG (object),
		                                        g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
ephy_encoding_dialog_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
	switch (prop_id)
	{
	case PROP_PARENT_WINDOW:
		g_value_set_object (value, EPHY_ENCODING_DIALOG (object)->window);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
ephy_encoding_dialog_class_init (EphyEncodingDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	/* class creation */
	object_class->finalize = ephy_encoding_dialog_finalize;
	object_class->set_property = ephy_encoding_dialog_set_property;
	object_class->get_property = ephy_encoding_dialog_get_property;
	object_class->dispose = ephy_encoding_dialog_dispose;

	g_object_class_install_property (object_class,
	                                 PROP_PARENT_WINDOW,
	                                 g_param_spec_object ("parent-window",
	                                                      "Parent window",
	                                                      "Parent window",
	                                                      EPHY_TYPE_WINDOW,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	/* load from UI file */
	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/epiphany/encoding-dialog.ui");

	gtk_widget_class_bind_template_child (widget_class, EphyEncodingDialog, automatic_button);
	gtk_widget_class_bind_template_child (widget_class, EphyEncodingDialog, manual_button);
	gtk_widget_class_bind_template_child (widget_class, EphyEncodingDialog, scrolled_window);

	gtk_widget_class_bind_template_callback (widget_class, automatic_toggled_cb);
	gtk_widget_class_bind_template_callback (widget_class, ephy_encoding_dialog_response_cb);
}

EphyEncodingDialog *
ephy_encoding_dialog_new (EphyWindow *parent)
{
	return g_object_new (EPHY_TYPE_ENCODING_DIALOG,
			     "use-header-bar" , TRUE,
			     "parent-window", parent,
			     NULL);
}
