/*
 *  Copyright (C) 2000, 2001, 2002, 2003 Marco Pesenti Gritti
 *  Copyright (C) 2003 Christian Persch
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
 *
 *  $Id$
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ephy-encoding-dialog.h"
#include "ephy-encodings.h"
#include "ephy-embed.h"
#include "ephy-embed-shell.h"
#include "ephy-shell.h"
#include "ephy-node.h"
#include "ephy-node-view.h"
#include "ephy-debug.h"

#include <gtk/gtklabel.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkdialog.h>
#include <glib/gi18n.h>
#include <string.h>

enum
{
	SCROLLED_WINDOW_PROP,
	AUTOMATIC_PROP,
	MANUAL_PROP
};

static const
EphyDialogProperty properties [] =
{
	{ SCROLLED_WINDOW_PROP,	"scrolled_window",	NULL, PT_NORMAL, NULL },
	{ AUTOMATIC_PROP,	"automatic_button",	NULL, PT_NORMAL, NULL },
	{ AUTOMATIC_PROP,	"manual_button",	NULL, PT_NORMAL, NULL },

	{ -1, NULL, NULL }
};

#define EPHY_ENCODING_DIALOG_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_ENCODING_DIALOG, EphyEncodingDialogPrivate))

struct EphyEncodingDialogPrivate
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
void		ephy_encoding_dialog_response_cb	(GtkWidget *widget,
							 gint response,
							 EphyEncodingDialog *dialog);

static GObjectClass *parent_class = NULL;

GType
ephy_encoding_dialog_get_type (void)
{
	static GType ephy_type_encoding_dialog = 0;

	if (ephy_type_encoding_dialog == 0)
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyEncodingDialogClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) ephy_encoding_dialog_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (EphyEncodingDialog),
			0, /* n_preallocs */
			(GInstanceInitFunc) ephy_encoding_dialog_init
		};

		ephy_type_encoding_dialog = g_type_register_static (EPHY_TYPE_EMBED_DIALOG,
								    "EphyEncodingDialog",
								    &our_info, 0);
	}

	return ephy_type_encoding_dialog;
}

static gboolean
encoding_is_automatic (EphyEncodingInfo *info)
{
	g_return_val_if_fail (info != NULL, FALSE);
	
	return (info->encoding_source < EMBED_ENCODING_PARENT_FORCED);
}

static void
sync_embed_cb (EphyEncodingDialog *dialog, GParamSpec *pspec, gpointer dummy)
{
	EphyEmbed *embed;
	EphyEncodingInfo *info;
	EphyNode *node;
        GtkTreeSelection *selection;
        GtkTreeModel *model;
        GList *rows;
	GtkWidget *button;
	gboolean is_automatic;

	dialog->priv->update_tag = TRUE;

	embed = ephy_embed_dialog_get_embed (EPHY_EMBED_DIALOG (dialog));
	g_return_if_fail (EPHY_IS_EMBED (embed));

	info = ephy_embed_get_encoding_info (embed);
	if (info == NULL) return;

	node = ephy_encodings_get_node (dialog->priv->encodings, info->encoding);
	g_return_if_fail (EPHY_IS_NODE (node));

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

	is_automatic = encoding_is_automatic (info);

	button = ephy_dialog_get_control (EPHY_DIALOG (dialog), AUTOMATIC_PROP);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), is_automatic);

	ephy_encoding_info_free (info);

	dialog->priv->update_tag = FALSE;
}

static void
sync_active_tab (EphyWindow *window, GParamSpec *pspec, EphyEncodingDialog *dialog)
{
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (dialog->priv->window);

	g_object_set (G_OBJECT (dialog), "embed", embed, NULL);
}

static void
sync_parent_window_cb (EphyEncodingDialog *dialog, GParamSpec *pspec, gpointer dummy)
{
	EphyWindow *window;
	GValue value = { 0, };

	g_return_if_fail (dialog->priv->window == NULL);

	g_value_init (&value, GTK_TYPE_WIDGET);
	g_object_get_property (G_OBJECT (dialog), "ParentWindow", &value);
	window = EPHY_WINDOW (g_value_get_object (&value));
	g_value_unset (&value);

	g_return_if_fail (EPHY_IS_WINDOW (window));

	dialog->priv->window = window;

	sync_active_tab (window, NULL, dialog);
	g_signal_connect (G_OBJECT (window), "notify::active-tab",
			  G_CALLBACK (sync_active_tab), dialog);
}

static void
activate_choice (EphyEncodingDialog *dialog)
{
	EphyEmbed *embed;
	EphyEncodingInfo *info;
	GtkWidget *button;
	gboolean is_automatic;

	embed = ephy_embed_dialog_get_embed (EPHY_EMBED_DIALOG (dialog));
	g_return_if_fail (EPHY_IS_EMBED (embed));

	info = ephy_embed_get_encoding_info (embed);
	if (info == NULL) return;

	button = ephy_dialog_get_control (EPHY_DIALOG (dialog), AUTOMATIC_PROP);
	is_automatic = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

	if (is_automatic)
	{
		/* only unset if it was forced before */
		if ((info->forced_encoding != NULL && info->forced_encoding[0] != '\0')
		    || info->encoding_source >= EMBED_ENCODING_PARENT_FORCED)
		{
			ephy_embed_set_encoding (embed, "");
		}
	}
	else if (dialog->priv->selected_node != NULL)
	{
		const char *code;

		code = ephy_node_get_property_string (dialog->priv->selected_node,
						      EPHY_NODE_ENCODING_PROP_ENCODING);

		/* only force it if it's different from active */
		if (info->encoding && strcmp (info->encoding, code) != 0)
		{
			ephy_embed_set_encoding (embed, code);

			ephy_encodings_add_recent (dialog->priv->encodings, code);
		}
	}

	ephy_encoding_info_free (info);
}

void
ephy_encoding_dialog_response_cb (GtkWidget *widget,
				  gint response,
				  EphyEncodingDialog *dialog)
{
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

	button = ephy_dialog_get_control (EPHY_DIALOG (dialog), MANUAL_PROP);
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

	button = ephy_dialog_get_control (EPHY_DIALOG (dialog), MANUAL_PROP);
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
	GtkWidget *treeview, *scroller, *button;
	GtkTreeSelection *selection;
	EphyNode *node;

	dialog->priv = EPHY_ENCODING_DIALOG_GET_PRIVATE (dialog);

	dialog->priv->encodings =
		EPHY_ENCODINGS (ephy_embed_shell_get_encodings
				(EPHY_EMBED_SHELL (ephy_shell)));

	dialog->priv->update_tag = FALSE;
	dialog->priv->selected_node = NULL;

	ephy_dialog_construct (EPHY_DIALOG (dialog),
			       properties,
			       "epiphany.glade",
			       "encoding_dialog");

	dialog->priv->filter = ephy_node_filter_new ();

	node = ephy_encodings_get_all (dialog->priv->encodings);
	treeview = ephy_node_view_new (node, dialog->priv->filter);

	ephy_node_view_add_column (EPHY_NODE_VIEW (treeview), _("Encodings"),
				   G_TYPE_STRING,
				   EPHY_NODE_ENCODING_PROP_TITLE_ELIDED,
				   -1,
				   EPHY_NODE_VIEW_AUTO_SORT |
				   EPHY_NODE_VIEW_SEARCHABLE,
				   NULL);

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
			(EPHY_DIALOG (dialog), SCROLLED_WINDOW_PROP);
	gtk_container_add (GTK_CONTAINER (scroller), treeview);

	button = ephy_dialog_get_control (EPHY_DIALOG (dialog), AUTOMATIC_PROP);
	gtk_label_set_use_markup (GTK_LABEL (GTK_BIN (button)->child), TRUE);
	g_signal_connect (button, "toggled",
			  G_CALLBACK (automatic_toggled_cb), dialog);

	button = ephy_dialog_get_control (EPHY_DIALOG (dialog), MANUAL_PROP);
	gtk_label_set_use_markup (GTK_LABEL (GTK_BIN (button)->child), TRUE);

	dialog->priv->enc_view = treeview;

	g_signal_connect (G_OBJECT (dialog), "notify::ParentWindow",
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

	g_object_unref (dialog->priv->filter);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
ephy_encoding_dialog_class_init (EphyEncodingDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = ephy_encoding_dialog_finalize;

	g_type_class_add_private (object_class, sizeof(EphyEncodingDialogPrivate));
}
		
EphyEncodingDialog *
ephy_encoding_dialog_new (EphyWindow *parent)
{
	return g_object_new (EPHY_TYPE_ENCODING_DIALOG,
			     "ParentWindow", parent,
			     NULL);
}
