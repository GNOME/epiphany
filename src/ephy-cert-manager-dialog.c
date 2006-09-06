/*
 *  Copyright (C) 2003 Robert Marcano
 *  Copyright (C) 2005 Crispin Flowerday
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
 * $Id$
 */

#include "config.h"

#include "ephy-cert-manager-dialog.h"
#include "ephy-shell.h"
#include "ephy-embed-shell.h"
#include "ephy-gui.h"
#include "ephy-x509-cert.h"
#include "ephy-certificate-manager.h"
#include "ephy-embed-single.h"
#include "ephy-file-helpers.h"
#include "ephy-file-chooser.h"

#include <gtk/gtkdialog.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkliststore.h>

#include <glib/gi18n.h>
#include <glib.h>

/* Glade callbacks */
void
certs_manager_dialog_response_cb (GtkDialog *dialog,
				  gint response_id,
				  CertsManagerDialog *cm_dialog);
void
certs_manager_dialog_remove_button_clicked_cb (GtkButton *button,
					       CertsManagerDialog *dialog);
static void
tree_view_selection_changed_cb (GtkTreeSelection *selection,
				CertsManagerDialog *dialog);

static GObjectClass *parent_class = NULL;

#define EPHY_CERTIFICATE_MANAGER_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_CERTS_MANAGER_DIALOG, CertsManagerDialogPrivate))

struct _CertsManagerDialogPrivate
{
	EphyCertificateManager *certs_manager;
};

enum
{
	RESPONSE_IMPORT = 1
};

enum
{
	PROP_WINDOW,
	PERSONAL_TREE_VIEW,
	PERSONAL_REMOVE_BUTTON,
	PERSONAL_VIEW_BUTTON,
	PERSONAL_EXPORT_BUTTON,
	SERVER_TREE_VIEW,
	SERVER_REMOVE_BUTTON,
	SERVER_VIEW_BUTTON,
	SERVER_PROPS_BUTTON,
	CA_TREE_VIEW,
	CA_REMOVE_BUTTON,
	CA_VIEW_BUTTON,
	CA_PROPS_BUTTON
};

static const
EphyDialogProperty properties [] =
{
	{ "certs_manager_dialog", NULL, PT_NORMAL, 0 },
	{ "personal_treeview", NULL, PT_NORMAL, 0 },
	{ "personal_remove_button", NULL, PT_NORMAL, 0 },
	{ "personal_view_button", NULL, PT_NORMAL, 0 },
	{ "personal_export_button", NULL, PT_NORMAL, 0 },
	{ "server_treeview", NULL, PT_NORMAL, 0 },
	{ "server_remove_button", NULL, PT_NORMAL, 0 },
	{ "server_view_button", NULL, PT_NORMAL, 0 },
	{ "server_props_button", NULL, PT_NORMAL, 0 },
	{ "ca_treeview", NULL, PT_NORMAL, 0 },
	{ "ca_remove_button", NULL, PT_NORMAL, 0 },
	{ "ca_view_button", NULL, PT_NORMAL, 0 },
	{ "ca_props_button", NULL, PT_NORMAL, 0 },

	{ NULL }
};

enum
{
	COL_NAME,
	COL_CERT,
	N_COLUMNS
};

static void
init_tree_view (CertsManagerDialog *dialog, GtkTreeView *tree_view)
{
	GtkCellRenderer *renderer;
	GtkTreeSelection *selection;
        GtkTreeViewColumn * column;

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_set_headers_visible (tree_view, TRUE);
	gtk_tree_view_insert_column_with_attributes (tree_view,
						     -1,
						     _("Name"),
						     renderer,
						     "text", COL_NAME,
						     NULL);
	column = gtk_tree_view_get_column (tree_view,  COL_NAME);
	gtk_tree_view_column_set_sort_column_id (column, COL_NAME);
	selection = gtk_tree_view_get_selection (tree_view);
	g_signal_connect (selection,
			  "changed",
			  G_CALLBACK (tree_view_selection_changed_cb),
			  dialog);
	
	
}


static void
append_cert (EphyX509Cert* cert, GtkListStore *list_store)
{
	GtkTreeIter iter;
	const char *title;

	title = ephy_x509_cert_get_title (cert);
	gtk_list_store_append (list_store, &iter);
	gtk_list_store_set (list_store, &iter,
			    COL_NAME, title,
			    COL_CERT, cert,
			    -1);
}

static void
fill_tree_view_from_list (GtkTreeView *tree_view,
			  GList *list)
{
	GtkListStore *list_store;

	list_store = gtk_list_store_new (N_COLUMNS,
					 G_TYPE_STRING,
					 G_TYPE_OBJECT);
	g_list_foreach (list, (GFunc)append_cert, list_store);
	gtk_tree_view_set_model(tree_view, GTK_TREE_MODEL (list_store));
}

static void
set_buttons_sensitive (CertsManagerDialog *dialog,
		       gint button1,
		       gint button2,
		       gint button3,
		       gboolean value)
{
	GtkWidget *widget;

	widget = ephy_dialog_get_control (EPHY_DIALOG (dialog), properties[button1].id);
	gtk_widget_set_sensitive (widget, value);

	widget = ephy_dialog_get_control (EPHY_DIALOG (dialog), properties[button2].id);
	gtk_widget_set_sensitive (widget, value);

	widget = ephy_dialog_get_control (EPHY_DIALOG (dialog), properties[button3].id);
	gtk_widget_set_sensitive (widget, value);
}


static void
chooser_response_cb (EphyFileChooser *chooser,
		     int response,
		     CertsManagerDialog *dialog)
{
	CertsManagerDialogPrivate *priv = dialog->priv;
	if (response == GTK_RESPONSE_ACCEPT)
	{
		char *file = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));

		ephy_certificate_manager_import (priv->certs_manager, file);

		g_free (file);
	}

	gtk_widget_destroy (GTK_WIDGET (chooser));
}

static void
import (GtkDialog *dialog, CertsManagerDialog *cm_dialog)
{
	EphyFileChooser *chooser;

	chooser = ephy_file_chooser_new (_("Import Certificate"),
					 GTK_WIDGET (dialog),
					 GTK_FILE_CHOOSER_ACTION_OPEN,
					 NULL,
					 EPHY_FILE_FILTER_ALL);

	g_signal_connect (chooser, "response", G_CALLBACK (chooser_response_cb), cm_dialog);

	gtk_widget_show (GTK_WIDGET (chooser));
}

void
certs_manager_dialog_response_cb (GtkDialog *widget,
				  int response,
				  CertsManagerDialog *dialog)
{
	if (response == GTK_RESPONSE_HELP)
	{
		/* TODO open help */
		return;
	}
	else if (response == RESPONSE_IMPORT)
	{
		import (widget, dialog);
		return;
	}
		
	g_object_unref (dialog);
}

void
certs_manager_dialog_remove_button_clicked_cb (GtkButton *button,
					       CertsManagerDialog *dialog)
{
	CertsManagerDialogPrivate *priv = dialog->priv;
	GtkTreeView *tree_view = NULL;
	GtkTreeModel *model;
	GtkTreeSelection  *tree_selection;
	EphyDialog *ephy_dialog;
	GtkButton *personal_button;
	GtkButton *server_button;
	GtkButton *ca_button;
	GList *selected_list, *remove_list = NULL, *l, *r;
	GtkTreeIter iter;
	GtkTreePath *path;

	ephy_dialog = EPHY_DIALOG (dialog);
	personal_button = GTK_BUTTON (ephy_dialog_get_control
				      (ephy_dialog,
				       properties[PERSONAL_REMOVE_BUTTON].id));
	server_button = GTK_BUTTON (ephy_dialog_get_control
				    (ephy_dialog,
				     properties[SERVER_REMOVE_BUTTON].id));
	ca_button = GTK_BUTTON (ephy_dialog_get_control
				(ephy_dialog,
				 properties[CA_REMOVE_BUTTON].id));
	if (button == personal_button)
		tree_view = GTK_TREE_VIEW (ephy_dialog_get_control
					  (ephy_dialog,
					   properties[PERSONAL_TREE_VIEW].id));
	else if (button == server_button)
		tree_view = GTK_TREE_VIEW (ephy_dialog_get_control
					  (ephy_dialog,
					   properties[SERVER_TREE_VIEW].id));
	else if (button == ca_button)
		tree_view = GTK_TREE_VIEW (ephy_dialog_get_control
					  (ephy_dialog,
					   properties[CA_TREE_VIEW].id));
	g_assert (tree_view != NULL);
	tree_selection = gtk_tree_view_get_selection (tree_view);
	selected_list = gtk_tree_selection_get_selected_rows (tree_selection, &model);

	for (l = selected_list; l != NULL; l = l->next)
	{
		remove_list = g_list_prepend (remove_list,
					      gtk_tree_row_reference_new (model,
									  (GtkTreePath *)l->data));
	}
	for (r = remove_list; r != NULL; r = r->next)
	{
		EphyX509Cert *cert;
		GValue val = {0, };
		path = gtk_tree_row_reference_get_path ((GtkTreeRowReference *)r->data);

		gtk_tree_model_get_iter (model, &iter, path);
		gtk_tree_model_get_value (model, &iter, COL_CERT, &val);
		cert = EPHY_X509_CERT (g_value_dup_object (&val));
		g_value_unset (&val);

		// TODO check return value and notify if an error ocurred
		ephy_certificate_manager_remove_certificate (priv->certs_manager,
							     cert);
		g_object_unref (cert);

		gtk_list_store_remove (GTK_LIST_STORE (model),
				       &iter);

		gtk_tree_row_reference_free ((GtkTreeRowReference *)r->data);
		gtk_tree_path_free (path);
	} 
	g_list_free (selected_list);
	g_list_free (remove_list);
}

static void
tree_view_selection_changed_cb (GtkTreeSelection *selection,
				CertsManagerDialog *dialog)
{
	gint count;
	EphyDialog *ephy_dialog;
	GtkTreeView *tree_view;
	GtkTreeView *personal_tree_view;
	GtkTreeView *server_tree_view;
	GtkTreeView *ca_tree_view;
	gboolean has_selection;

	ephy_dialog = EPHY_DIALOG (dialog);
	tree_view = gtk_tree_selection_get_tree_view (selection);
	personal_tree_view = GTK_TREE_VIEW (ephy_dialog_get_control
					    (ephy_dialog,
					     properties[PERSONAL_TREE_VIEW].id));
	server_tree_view = GTK_TREE_VIEW (ephy_dialog_get_control
					  (ephy_dialog,
					   properties[SERVER_TREE_VIEW].id));
	ca_tree_view = GTK_TREE_VIEW (ephy_dialog_get_control
				      (ephy_dialog,
				       properties[CA_TREE_VIEW].id));
	count = gtk_tree_selection_count_selected_rows (selection);
	has_selection = count == 0 ? FALSE : TRUE;
	if (tree_view == personal_tree_view)
		set_buttons_sensitive (dialog,
				       PERSONAL_REMOVE_BUTTON,
				       PERSONAL_VIEW_BUTTON,
				       PERSONAL_EXPORT_BUTTON,
				       has_selection);
	else if (tree_view == server_tree_view)
		set_buttons_sensitive (dialog,
				       SERVER_REMOVE_BUTTON,
				       SERVER_VIEW_BUTTON,
				       SERVER_PROPS_BUTTON,
				       has_selection);
	else if (tree_view == ca_tree_view)
		set_buttons_sensitive (dialog,
				       CA_REMOVE_BUTTON,
				       CA_VIEW_BUTTON,
				       CA_PROPS_BUTTON,
				       has_selection);
}


static void
certs_manager_dialog_init (CertsManagerDialog *dialog)
{
	CertsManagerDialogPrivate *priv;
	EphyEmbedShell *shell;
	GtkWidget *window;
	GtkTreeView *personal_treeview;
	GtkTreeView *server_treeview;
	GtkTreeView *ca_treeview;
	GList *personalCerts;
	GList *serverCerts;
	GList *caCerts;
	GtkWidget *button;

	priv = dialog->priv = EPHY_CERTIFICATE_MANAGER_GET_PRIVATE (dialog);

	shell = ephy_embed_shell_get_default ();
	g_object_ref (shell);

	priv->certs_manager = EPHY_CERTIFICATE_MANAGER (ephy_embed_shell_get_embed_single (shell));

	ephy_dialog_construct (EPHY_DIALOG (dialog),
			       properties,
			       ephy_file ("certs-manager.glade"),
			       "certs_manager_dialog",
                               NULL);

	window = ephy_dialog_get_control (EPHY_DIALOG (dialog), properties[PROP_WINDOW].id);
	g_signal_connect (window, "response",
			  G_CALLBACK (certs_manager_dialog_response_cb), dialog);

	button = ephy_dialog_get_control (EPHY_DIALOG (dialog), properties[PERSONAL_REMOVE_BUTTON].id);
	g_signal_connect (button, "clicked",
			  G_CALLBACK (certs_manager_dialog_remove_button_clicked_cb), dialog);
	button = ephy_dialog_get_control (EPHY_DIALOG (dialog), properties[CA_REMOVE_BUTTON].id);
	g_signal_connect (button, "clicked",
			  G_CALLBACK (certs_manager_dialog_remove_button_clicked_cb), dialog);
	button = ephy_dialog_get_control (EPHY_DIALOG (dialog), properties[SERVER_REMOVE_BUTTON].id);
	g_signal_connect (button, "clicked",
			  G_CALLBACK (certs_manager_dialog_remove_button_clicked_cb), dialog);

	/* disabling buttons until a certificate is selected */
	set_buttons_sensitive (dialog,
			       PERSONAL_REMOVE_BUTTON,
			       PERSONAL_VIEW_BUTTON,
			       PERSONAL_EXPORT_BUTTON,
			       FALSE);
	set_buttons_sensitive (dialog,
			       SERVER_REMOVE_BUTTON,
			       SERVER_VIEW_BUTTON,
			       SERVER_PROPS_BUTTON,
			       FALSE);
	set_buttons_sensitive (dialog,
			       CA_REMOVE_BUTTON,
			       CA_VIEW_BUTTON,
			       CA_PROPS_BUTTON,
			       FALSE);

	/* filling personal treeview */
	personalCerts = ephy_certificate_manager_get_certificates (priv->certs_manager,
                                                                   PERSONAL_CERTIFICATE);

	personal_treeview = GTK_TREE_VIEW (ephy_dialog_get_control
					   (EPHY_DIALOG(dialog),
					    properties[PERSONAL_TREE_VIEW].id));
	init_tree_view (dialog, personal_treeview);
	fill_tree_view_from_list (personal_treeview, personalCerts);
	/* filling server treeview */
	serverCerts = ephy_certificate_manager_get_certificates (priv->certs_manager,
                                                                 SERVER_CERTIFICATE);
	server_treeview = GTK_TREE_VIEW (ephy_dialog_get_control
					 (EPHY_DIALOG(dialog),
					  properties[SERVER_TREE_VIEW].id));
	init_tree_view (dialog, server_treeview);
	fill_tree_view_from_list (server_treeview, serverCerts);
	/* filling ca treeview */
	caCerts = ephy_certificate_manager_get_certificates (priv->certs_manager,
                                                             CA_CERTIFICATE);
	ca_treeview = GTK_TREE_VIEW (ephy_dialog_get_control
				     (EPHY_DIALOG(dialog),
				      properties[CA_TREE_VIEW].id));
	init_tree_view (dialog, ca_treeview);
	fill_tree_view_from_list (ca_treeview, caCerts);

	g_list_foreach (personalCerts, (GFunc)g_object_unref, NULL);
	g_list_free (personalCerts);

	g_list_foreach (serverCerts, (GFunc)g_object_unref, NULL);
	g_list_free (serverCerts);

	g_list_foreach (caCerts, (GFunc)g_object_unref, NULL);
	g_list_free (caCerts);
}

static void
certs_manager_dialog_finalize (GObject *object)
{
	//CertsManagerDialog *dialog = EPHY_CERTS_MANAGER_DIALOG (object);
	//CertsManagerDialogPrivate *priv = dialog->priv;
	EphyEmbedShell *shell;

	/* TODO free certs in the treeviews */

	G_OBJECT_CLASS (parent_class)->finalize (object);

	shell = ephy_embed_shell_get_default ();
	g_object_unref (shell);
}

static void
certs_manager_dialog_class_init (CertsManagerDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = certs_manager_dialog_finalize;

	g_type_class_add_private (object_class, sizeof (CertsManagerDialogPrivate));
}

/* public functions */

GType 
certs_manager_dialog_get_type (void)
{
	static GType certs_manager_dialog_type = 0;

	if (certs_manager_dialog_type == 0)
	{
		const GTypeInfo our_info =
		{
			sizeof (CertsManagerDialogClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) certs_manager_dialog_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (CertsManagerDialog),
			0, /* n_preallocs */
			(GInstanceInitFunc) certs_manager_dialog_init
		};

		certs_manager_dialog_type = g_type_register_static (EPHY_TYPE_DIALOG,
								    "CertsManagerDialog",
								    &our_info, 0);
	}

	return certs_manager_dialog_type;

}

EphyDialog *
certs_manager_dialog_new (void)
{
	return g_object_new (EPHY_TYPE_CERTS_MANAGER_DIALOG, NULL);
}
