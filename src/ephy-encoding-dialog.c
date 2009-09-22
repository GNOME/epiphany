/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 2000, 2001, 2002, 2003 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
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
#include "ephy-encodings.h"
#include "ephy-embed.h"
#include "ephy-embed-container.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-utils.h"
#include "ephy-file-helpers.h"
#include "ephy-shell.h"
#include "ephy-node.h"
#include "ephy-node-view.h"
#include "ephy-debug.h"
#include "ephy-gui.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>
#include <webkit/webkit.h>

enum
{
	WINDOW_PROP,
	SCROLLED_WINDOW_PROP,
	AUTOMATIC_PROP,
	MANUAL_PROP
};

static const
EphyDialogProperty properties [] =
{
	{ "encoding_dialog",	NULL, PT_NORMAL, 0 },
	{ "scrolled_window",	NULL, PT_NORMAL, 0 },
	{ "automatic_button",	NULL, PT_NORMAL, 0 },
	{ "manual_button",	NULL, PT_NORMAL, 0 },

	{ NULL }
};

#define EPHY_ENCODING_DIALOG_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_ENCODING_DIALOG, EphyEncodingDialogPrivate))

struct _EphyEncodingDialogPrivate
{
	EphyEncodings *encodings;
	EphyWindow *window;
	EphyEmbed *embed;
	GtkWidget *enc_view;
	EphyNodeFilter *filter;
	EphyNode *selected_node;
	gboolean update_tag;
};

static void	ephy_encoding_dialog_class_init		(EphyEncodingDialogClass *klass);
static void	ephy_encoding_dialog_init		(EphyEncodingDialog *ge);

G_DEFINE_TYPE (EphyEncodingDialog, ephy_encoding_dialog, EPHY_TYPE_EMBED_DIALOG)

static void
sync_encoding_against_embed (EphyEncodingDialog *dialog)
{
	EphyEmbed *embed;
	EphyNode *node;
        GtkTreeSelection *selection;
        GtkTreeModel *model;
        GList *rows;
	GtkWidget *button;
	const char *encoding;
	gboolean is_automatic = FALSE;
	WebKitWebView *view;

	dialog->priv->update_tag = TRUE;

	embed = ephy_embed_dialog_get_embed (EPHY_EMBED_DIALOG (dialog));
	g_return_if_fail (EPHY_IS_EMBED (embed));

	view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);
	encoding = webkit_web_view_get_custom_encoding (view);
	if (encoding == NULL)
	{
		encoding = webkit_web_view_get_encoding (view);
		if (encoding == NULL) return;
		is_automatic = TRUE;
	}

	node = ephy_encodings_get_node (dialog->priv->encodings, encoding, TRUE);
	g_assert (EPHY_IS_NODE (node));

	/* select the current encoding in the list view */
	ephy_node_view_select_node (EPHY_NODE_VIEW (dialog->priv->enc_view),
				    node);

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

	button = ephy_dialog_get_control (EPHY_DIALOG (dialog), properties[AUTOMATIC_PROP].id);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), is_automatic);

	dialog->priv->update_tag = FALSE;
}


static void
embed_net_stop_cb (EphyWebView *view,
		   GParamSpec *pspec,
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

	g_signal_connect (G_OBJECT (EPHY_GET_EPHY_WEB_VIEW_FROM_EMBED (embed)), "notify::load-status",
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

	button = ephy_dialog_get_control (EPHY_DIALOG (dialog), properties[AUTOMATIC_PROP].id);
	is_automatic = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

	view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);

	if (is_automatic)
	{
		webkit_web_view_set_custom_encoding (view, NULL);
	}
	else if (dialog->priv->selected_node != NULL)
	{
		const char *code;

		code = ephy_node_get_property_string (dialog->priv->selected_node,
						      EPHY_NODE_ENCODING_PROP_ENCODING);

		webkit_web_view_set_custom_encoding (view, code);

		ephy_encodings_add_recent (dialog->priv->encodings, code);
	}
}

static void
ephy_encoding_dialog_response_cb (GtkWidget *widget,
				  int response,
				  EphyEncodingDialog *dialog)
{
	if (response == GTK_RESPONSE_HELP)
	{
		ephy_gui_help (GTK_WINDOW (widget), "epiphany", "text-encoding");
		return;
	}

	g_object_unref (dialog);
}

static void
view_node_selected_cb (EphyNodeView *view,
		       EphyNode *node,
		       EphyEncodingDialog *dialog)
{
	GtkWidget *button;

	dialog->priv->selected_node = node;

	if (dialog->priv->update_tag) return;

	button = ephy_dialog_get_control (EPHY_DIALOG (dialog), properties[MANUAL_PROP].id);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);

	activate_choice (dialog);
}

static void
view_node_activated_cb (GtkWidget *view,
			EphyNode *node,
			EphyEncodingDialog *dialog)
{
	GtkWidget *button;

	dialog->priv->selected_node = node;

	if (dialog->priv->update_tag) return;

	button = ephy_dialog_get_control (EPHY_DIALOG (dialog), properties[MANUAL_PROP].id);
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
	EphyNode *node;

	dialog->priv = EPHY_ENCODING_DIALOG_GET_PRIVATE (dialog);

	dialog->priv->encodings =
		EPHY_ENCODINGS (ephy_embed_shell_get_encodings
				(EPHY_EMBED_SHELL (ephy_shell)));

	ephy_dialog_construct (EPHY_DIALOG (dialog),
			       properties,
			       ephy_file ("epiphany.ui"),
			       "encoding_dialog",
			       NULL);

	window = ephy_dialog_get_control (EPHY_DIALOG (dialog), properties[WINDOW_PROP].id);
	g_signal_connect (window, "response",
			  G_CALLBACK (ephy_encoding_dialog_response_cb), dialog);

	dialog->priv->filter = ephy_node_filter_new ();

	node = ephy_encodings_get_all (dialog->priv->encodings);
	treeview = ephy_node_view_new (node, dialog->priv->filter);

	ephy_node_view_add_column (EPHY_NODE_VIEW (treeview), _("Encodings"),
				   G_TYPE_STRING,
				   EPHY_NODE_ENCODING_PROP_TITLE_ELIDED,
				   EPHY_NODE_VIEW_SEARCHABLE,
				   NULL, NULL);

	ephy_node_view_set_sort (EPHY_NODE_VIEW (treeview), G_TYPE_STRING,
				 EPHY_NODE_ENCODING_PROP_TITLE_ELIDED,
				 GTK_SORT_ASCENDING);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW(treeview), FALSE);

	g_signal_connect (G_OBJECT (treeview),
			  "node_selected",
			  G_CALLBACK (view_node_selected_cb),
			  dialog);
	g_signal_connect (G_OBJECT (treeview),
			  "node_activated",
			  G_CALLBACK (view_node_activated_cb),
			  dialog);

	gtk_widget_show (treeview);

	scroller = ephy_dialog_get_control
			(EPHY_DIALOG (dialog), properties[SCROLLED_WINDOW_PROP].id);
	gtk_container_add (GTK_CONTAINER (scroller), treeview);

	child = gtk_bin_get_child (GTK_BIN (button));

	button = ephy_dialog_get_control (EPHY_DIALOG (dialog), properties[AUTOMATIC_PROP].id);
	gtk_label_set_use_markup (GTK_LABEL (child), TRUE);
	g_signal_connect (button, "toggled",
			  G_CALLBACK (automatic_toggled_cb), dialog);

	button = ephy_dialog_get_control (EPHY_DIALOG (dialog), properties[MANUAL_PROP].id);
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
		g_signal_handlers_disconnect_by_func (EPHY_GET_EPHY_WEB_VIEW_FROM_EMBED (dialog->priv->embed),
						      G_CALLBACK (embed_net_stop_cb),
						      dialog);
	}

	g_object_unref (dialog->priv->filter);

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
