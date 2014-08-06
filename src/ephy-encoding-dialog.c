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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
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

#define EPHY_ENCODING_DIALOG_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_ENCODING_DIALOG, EphyEncodingDialogPrivate))

struct _EphyEncodingDialogPrivate
{
	EphyEncodings *encodings;
	EphyWindow *window;
	EphyEmbed *embed;
	GtkWidget *enc_view;
	gboolean update_tag;
	char *selected_encoding;
};

enum {
	COL_TITLE_ELIDED,
	COL_ENCODING,
	NUM_COLS
};
	
G_DEFINE_TYPE (EphyEncodingDialog, ephy_encoding_dialog, EPHY_TYPE_EMBED_DIALOG)

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
	EphyEmbed *embed;
        GtkTreeSelection *selection;
        GtkTreeModel *model;
        GList *rows;
	GtkWidget *button;
	const char *encoding;
	gboolean is_automatic = FALSE;
	WebKitWebView *view;
	EphyEncoding *node;

	dialog->priv->update_tag = TRUE;

	embed = ephy_embed_dialog_get_embed (EPHY_EMBED_DIALOG (dialog));
	g_return_if_fail (EPHY_IS_EMBED (embed));

	view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);

	encoding = webkit_web_view_get_custom_charset (view);
	if (encoding == NULL) goto out;

	node = ephy_encodings_get_encoding (dialog->priv->encodings, encoding, TRUE);
	g_assert (EPHY_IS_ENCODING (node));

	/* Select the current encoding in the list view. */
	select_encoding_row (GTK_TREE_VIEW (dialog->priv->enc_view), node);

	/* scroll the view so the active encoding is visible */
        selection = gtk_tree_view_get_selection
                (GTK_TREE_VIEW (dialog->priv->enc_view));
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->priv->enc_view));
        rows = gtk_tree_selection_get_selected_rows (selection, &model);
        if (rows != NULL)
	{
		gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (dialog->priv->enc_view),
					      (GtkTreePath *) rows->data,
					      NULL, /* column */
					      TRUE,
					      0.5,
					      0.0);
		g_list_foreach (rows, (GFunc)gtk_tree_path_free, NULL);
		g_list_free (rows);
	}

	button = ephy_dialog_get_control (EPHY_DIALOG (dialog),
					  "automatic_button");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), is_automatic);
out:
	dialog->priv->update_tag = FALSE;
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
sync_embed_cb (EphyEncodingDialog *dialog, GParamSpec *pspec, gpointer dummy)
{
	EphyEmbed *embed;
	embed = ephy_embed_dialog_get_embed (EPHY_EMBED_DIALOG (dialog));

	if (dialog->priv->embed != NULL)
	{
		g_signal_handlers_disconnect_by_func (dialog->priv->embed,
						      G_CALLBACK (embed_net_stop_cb),
						      dialog);
	}

	g_signal_connect (G_OBJECT (ephy_embed_get_web_view (embed)), "load-changed",
			  G_CALLBACK (embed_net_stop_cb), dialog);

	dialog->priv->embed = embed;

	sync_encoding_against_embed (dialog);
}

static void
sync_active_tab (EphyWindow *window, GParamSpec *pspec, EphyEncodingDialog *dialog)
{
	EphyEmbed *embed;

	embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (dialog->priv->window));

	g_object_set (G_OBJECT (dialog), "embed", embed, NULL);
}

static void
sync_parent_window_cb (EphyEncodingDialog *dialog, GParamSpec *pspec, gpointer dummy)
{
	EphyWindow *window;
	GValue value = { 0, };

	g_return_if_fail (dialog->priv->window == NULL);

	g_value_init (&value, GTK_TYPE_WIDGET);
	g_object_get_property (G_OBJECT (dialog), "parent-window", &value);
	window = EPHY_WINDOW (g_value_get_object (&value));
	g_value_unset (&value);

	g_return_if_fail (EPHY_IS_WINDOW (window));

	dialog->priv->window = window;

	sync_active_tab (window, NULL, dialog);
	g_signal_connect (G_OBJECT (window), "notify::active-child",
			  G_CALLBACK (sync_active_tab), dialog);
}

static void
activate_choice (EphyEncodingDialog *dialog)
{
	EphyEmbed *embed;
	GtkWidget *button;
	gboolean is_automatic;
	WebKitWebView *view;

	embed = ephy_embed_dialog_get_embed (EPHY_EMBED_DIALOG (dialog));
	g_return_if_fail (EPHY_IS_EMBED (embed));

	button = ephy_dialog_get_control (EPHY_DIALOG (dialog),
					  "automatic_button");
	is_automatic = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

	view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);

	if (is_automatic)
	{
		webkit_web_view_set_custom_charset (view, NULL);
	}
	else if (dialog->priv->selected_encoding != NULL)
	{
		const char *code;

		code = dialog->priv->selected_encoding;

		webkit_web_view_set_custom_charset (view, code);

		ephy_encodings_add_recent (dialog->priv->encodings, code);
	}
}

static void
ephy_encoding_dialog_response_cb (GtkWidget *widget,
				  int response,
				  EphyEncodingDialog *dialog)
{
	g_object_unref (dialog);
}

static void
view_row_selected_cb (GtkTreeSelection *selection,
		      EphyEncodingDialog *dialog)
{
	GtkWidget *button;
	GtkTreeModel *model;
	GtkTreeIter iter;
	EphyEncodingDialogPrivate *priv = dialog->priv;

	if (gtk_tree_selection_get_selected (selection, &model, &iter))
	{
		char *encoding;

		gtk_tree_model_get (model, &iter,
				    COL_ENCODING, &encoding,
				    -1);

		g_free (priv->selected_encoding);
		priv->selected_encoding = encoding;
	}

	if (dialog->priv->update_tag) 
		return;

	button = ephy_dialog_get_control (EPHY_DIALOG (dialog), "manual_button");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);

	activate_choice (dialog);
}

static void
view_row_activated_cb (GtkTreeView *treeview,
		       GtkTreePath *path,
		       GtkTreeViewColumn *column,
		       EphyEncodingDialog *dialog)
{
	GtkWidget *button;
	GtkTreeIter iter;
	char *encoding;
	GtkTreeModel *model;
	EphyEncodingDialogPrivate *priv = dialog->priv;

	model = gtk_tree_view_get_model (treeview);

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter,
			    COL_ENCODING, &encoding,
			    -1);

	g_free (priv->selected_encoding);
	priv->selected_encoding = encoding;

	if (dialog->priv->update_tag) 
		return;

	button = ephy_dialog_get_control (EPHY_DIALOG (dialog), "manual_button");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);

	activate_choice (dialog);

	g_object_unref (dialog);
}

static void
automatic_toggled_cb (GtkToggleButton *button, EphyEncodingDialog *dialog)
{
	if (gtk_toggle_button_get_active (button)
	    && dialog->priv->update_tag == FALSE)
	{
		activate_choice (dialog);
	}
}

static void
ephy_encoding_dialog_init (EphyEncodingDialog *dialog)
{
	GtkWidget *treeview, *scroller, *button, *window, *child;
	GtkTreeSelection *selection;
	GList *encodings, *p;
	GtkListStore *store;
	GtkTreeIter iter;
	GtkCellRenderer *renderer;

	dialog->priv = EPHY_ENCODING_DIALOG_GET_PRIVATE (dialog);

	dialog->priv->encodings =
		EPHY_ENCODINGS (ephy_embed_shell_get_encodings
				(EPHY_EMBED_SHELL (ephy_shell_get_default ())));

	ephy_dialog_construct (EPHY_DIALOG (dialog),
			       "/org/gnome/epiphany/encoding-dialog.ui",
			       "encoding_dialog",
			       NULL);

	window = ephy_dialog_get_control (EPHY_DIALOG (dialog),
					  "encoding_dialog");
	g_signal_connect (window, "response",
			  G_CALLBACK (ephy_encoding_dialog_response_cb), dialog);

	encodings = ephy_encodings_get_all (dialog->priv->encodings);
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

	g_signal_connect (selection, "changed",
			  G_CALLBACK (view_row_selected_cb),
			  dialog);
	g_signal_connect (treeview, "row-activated",
			  G_CALLBACK (view_row_activated_cb),
			  dialog);

	gtk_widget_show (treeview);

	scroller = ephy_dialog_get_control (EPHY_DIALOG (dialog),
					    "scrolled_window");
	gtk_container_add (GTK_CONTAINER (scroller), treeview);

	button = ephy_dialog_get_control (EPHY_DIALOG (dialog),
					  "automatic_button");
	child = gtk_bin_get_child (GTK_BIN (button));
	gtk_label_set_use_markup (GTK_LABEL (child), TRUE);
	g_signal_connect (button, "toggled",
			  G_CALLBACK (automatic_toggled_cb), dialog);

	button = ephy_dialog_get_control (EPHY_DIALOG (dialog), "manual_button");
	child = gtk_bin_get_child (GTK_BIN (button));
	gtk_label_set_use_markup (GTK_LABEL (child), TRUE);

	dialog->priv->enc_view = treeview;

	g_signal_connect (G_OBJECT (dialog), "notify::parent-window",
			  G_CALLBACK (sync_parent_window_cb), NULL);
	g_signal_connect (G_OBJECT (dialog), "notify::embed",
			  G_CALLBACK (sync_embed_cb), NULL);
}

static void
ephy_encoding_dialog_finalize (GObject *object)
{
	EphyEncodingDialog *dialog = EPHY_ENCODING_DIALOG (object);

	if (dialog->priv->window != NULL)
	{
		g_signal_handlers_disconnect_by_func (dialog->priv->window,
						      G_CALLBACK (sync_active_tab),
						      dialog);
	}

	if (dialog->priv->embed)
	{
		g_signal_handlers_disconnect_by_func (ephy_embed_get_web_view (dialog->priv->embed),
						      G_CALLBACK (embed_net_stop_cb),
						      dialog);
	}

	g_free (dialog->priv->selected_encoding);

	G_OBJECT_CLASS (ephy_encoding_dialog_parent_class)->finalize (object);
}

static void
ephy_encoding_dialog_class_init (EphyEncodingDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = ephy_encoding_dialog_finalize;

	g_type_class_add_private (object_class, sizeof(EphyEncodingDialogPrivate));
}
		
EphyEncodingDialog *
ephy_encoding_dialog_new (EphyWindow *parent)
{
	return g_object_new (EPHY_TYPE_ENCODING_DIALOG,
			     "parent-window", parent,
			     "default-width", 350,
		             "default-height", 420,
			     NULL);
}
