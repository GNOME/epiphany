/*
 *  Copyright (C) 2002 Jorn Baayen
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "pdm-dialog.h"
#include "ephy-shell.h"
#include "ephy-embed-shell.h"
#include "ephy-gui.h"
#include "ephy-ellipsizing-label.h"

#include <gtk/gtktreeview.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtkcellrenderertext.h>

#include <bonobo/bonobo-i18n.h>

typedef struct PdmActionInfo PdmActionInfo;

typedef void (* PDM_add) (PdmActionInfo *info, gpointer data);
typedef void (* PDM_remove) (PdmActionInfo *info, GList *data);
typedef void (* PDM_free) (PdmActionInfo *info, GList *data);

static void pdm_dialog_class_init (PdmDialogClass *klass);
static void pdm_dialog_init (PdmDialog *dialog);
static void pdm_dialog_finalize (GObject *object);

static void pdm_cmd_delete_selection (PdmActionInfo *action);

/* Glade callbacks */
void
pdm_dialog_close_button_clicked_cb (GtkWidget *button,
			            PdmDialog *dialog);
void
pdm_dialog_cookies_properties_button_clicked_cb (GtkWidget *button,
						 PdmDialog *dialog);
void
pdm_dialog_cookies_treeview_selection_changed_cb (GtkTreeSelection *selection,
						  PdmDialog *dialog);
void
pdm_dialog_passwords_treeview_selection_changed_cb (GtkTreeSelection *selection,
						    PdmDialog *dialog);
void
pdm_dialog_response_cb (GtkDialog *dialog, gint response_id, gpointer data);

static GObjectClass *parent_class = NULL;

struct PdmActionInfo
{
	PDM_add add;
	PDM_remove remove;
	PDM_free free;
	GtkWidget *treeview;
	GList *list;
	int remove_id;
	int data_col;
	PdmDialog *dialog;
};

#define EPHY_PDM_DIALOG_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_PDM_DIALOG, PdmDialogPrivate))

struct PdmDialogPrivate
{
	GtkTreeModel *model;
	PdmActionInfo *cookies;
	PdmActionInfo *passwords;
};

enum
{
	PROP_WINDOW,
	PROP_NOTEBOOK,
	PROP_COOKIES_TREEVIEW,
	PROP_COOKIES_REMOVE,
	PROP_PASSWORDS_TREEVIEW,
	PROP_PASSWORDS_REMOVE,
	PROP_DIALOG,
	PROP_COOKIES_PROPERTIES
};

enum
{
	COL_COOKIES_HOST,
	COL_COOKIES_NAME,
	COL_COOKIES_DATA
};

enum
{
	COL_PASSWORDS_HOST,
	COL_PASSWORDS_USER,
	COL_PASSWORDS_DATA
};

static const
EphyDialogProperty properties [] =
{
	{ PROP_WINDOW, "pdm_dialog", NULL, PT_NORMAL, NULL },
	{ PROP_NOTEBOOK, "pdm_notebook", NULL, PT_NORMAL, NULL },

	{ PROP_COOKIES_TREEVIEW, "cookies_treeview", NULL, PT_NORMAL, NULL },
	{ PROP_COOKIES_REMOVE, "cookies_remove_button", NULL, PT_NORMAL, NULL },
	{ PROP_PASSWORDS_TREEVIEW, "passwords_treeview", NULL, PT_NORMAL, NULL },
	{ PROP_PASSWORDS_REMOVE, "passwords_remove_button", NULL, PT_NORMAL, NULL },
	{ PROP_DIALOG, "pdm_dialog", NULL, PT_NORMAL, NULL },
	{ PROP_COOKIES_PROPERTIES, "cookies_properties_button", NULL, PT_NORMAL, NULL },

	{ -1, NULL, NULL }
};

GType 
pdm_dialog_get_type (void)
{
        static GType pdm_dialog_type = 0;

        if (pdm_dialog_type == 0)
        {
                static const GTypeInfo our_info =
                {
                        sizeof (PdmDialogClass),
                        NULL, /* base_init */
                        NULL, /* base_finalize */
                        (GClassInitFunc) pdm_dialog_class_init,
                        NULL,
                        NULL, /* class_data */
                        sizeof (PdmDialog),
                        0, /* n_preallocs */
                        (GInstanceInitFunc) pdm_dialog_init
                };

		pdm_dialog_type = g_type_register_static (EPHY_TYPE_DIALOG,
							  "PdmDialog",
							  &our_info, 0);
        }

        return pdm_dialog_type;

}

static void
pdm_dialog_class_init (PdmDialogClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = pdm_dialog_finalize;

	g_type_class_add_private (object_class, sizeof(PdmDialogPrivate));
}

static void
pdm_dialog_show_help (PdmDialog *pd)
{
	GtkWidget *notebook, *window;
	gint id;

	/* FIXME: Once we actually have documentation we
	 * should point these at the correct links.
	 */
	gchar *help_preferences[] = {
		"pdm",
		"pdm"
	};

	window = ephy_dialog_get_control (EPHY_DIALOG (pd), PROP_WINDOW);
	g_return_if_fail (GTK_IS_WINDOW (window));

	notebook = ephy_dialog_get_control (EPHY_DIALOG (pd), PROP_NOTEBOOK);
	g_return_if_fail (notebook != NULL);

	id = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
	g_assert (id == 0 || id == 1);

	ephy_gui_help (GTK_WINDOW (window), "epiphany", help_preferences[id]);
}

static void
cookies_treeview_selection_changed_cb (GtkTreeSelection *selection,
                                       PdmDialog *dialog)
{
	GtkWidget *widget;
	EphyDialog *d = EPHY_DIALOG(dialog);
	gboolean has_selection;

	has_selection = gtk_tree_selection_count_selected_rows (selection) == 1;

	widget = ephy_dialog_get_control (d, PROP_COOKIES_PROPERTIES);
	gtk_widget_set_sensitive (widget, has_selection);
}

static void
action_treeview_selection_changed_cb (GtkTreeSelection *selection,
                                      PdmActionInfo *action)
{
	GtkWidget *widget;
	EphyDialog *d = EPHY_DIALOG(action->dialog);
	gboolean has_selection;

	has_selection = gtk_tree_selection_count_selected_rows (selection) > 0;

	widget = ephy_dialog_get_control (d, action->remove_id);
	gtk_widget_set_sensitive (widget, has_selection);
}

static GtkWidget *
setup_passwords_treeview (PdmDialog *dialog)
{

	GtkTreeView *treeview;
        GtkListStore *liststore;
        GtkCellRenderer *renderer;
        GtkTreeViewColumn *column;
	GtkTreeSelection *selection;

	treeview = GTK_TREE_VIEW(ephy_dialog_get_control
				 (EPHY_DIALOG(dialog),
				 PROP_PASSWORDS_TREEVIEW));

        /* set tree model */
        liststore = gtk_list_store_new (3,
                                        G_TYPE_STRING,
                                        G_TYPE_STRING,
                                        G_TYPE_POINTER);
        gtk_tree_view_set_model (treeview, GTK_TREE_MODEL(liststore));
        gtk_tree_view_set_headers_visible (treeview, TRUE);
	selection = gtk_tree_view_get_selection (treeview);
	gtk_tree_selection_set_mode (selection,
				     GTK_SELECTION_MULTIPLE);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (liststore),
					      COL_PASSWORDS_HOST,
					      GTK_SORT_ASCENDING);
	g_object_unref (liststore);

        renderer = gtk_cell_renderer_text_new ();

        gtk_tree_view_insert_column_with_attributes (treeview,
                                                     COL_PASSWORDS_HOST,
						     _("Host"),
                                                     renderer,
                                                     "text", COL_PASSWORDS_HOST,
                                                     NULL);
        column = gtk_tree_view_get_column (treeview, COL_PASSWORDS_HOST);
        gtk_tree_view_column_set_resizable (column, TRUE);
        gtk_tree_view_column_set_reorderable (column, TRUE);
        gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
        gtk_tree_view_column_set_sort_column_id (column, COL_PASSWORDS_HOST);

        gtk_tree_view_insert_column_with_attributes (treeview,
                                                     COL_PASSWORDS_USER,
						     _("User Name"),
                                                     renderer,
                                                     "text", COL_PASSWORDS_USER,
                                                     NULL);
        column = gtk_tree_view_get_column (treeview, COL_PASSWORDS_USER);
        gtk_tree_view_column_set_resizable (column, TRUE);
        gtk_tree_view_column_set_reorderable (column, TRUE);
        gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
        gtk_tree_view_column_set_sort_column_id (column, COL_PASSWORDS_USER);

	return GTK_WIDGET (treeview);
}

static GtkWidget *
setup_cookies_treeview (PdmDialog *dialog)
{
	GtkTreeView *treeview;
        GtkListStore *liststore;
        GtkCellRenderer *renderer;
        GtkTreeViewColumn *column;
	GtkTreeSelection *selection;

	treeview = GTK_TREE_VIEW (ephy_dialog_get_control
				  (EPHY_DIALOG(dialog),
				  PROP_COOKIES_TREEVIEW));

        /* set tree model */
        liststore = gtk_list_store_new (3,
                                        G_TYPE_STRING,
                                        G_TYPE_STRING,
                                        G_TYPE_POINTER);
        gtk_tree_view_set_model (treeview, GTK_TREE_MODEL(liststore));
        gtk_tree_view_set_headers_visible (treeview, TRUE);
	selection = gtk_tree_view_get_selection (treeview);
	gtk_tree_selection_set_mode (selection,
				     GTK_SELECTION_MULTIPLE);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (liststore),
					      COL_COOKIES_HOST,
					      GTK_SORT_ASCENDING);
	g_object_unref (liststore);

	g_signal_connect (selection, "changed",
			  G_CALLBACK(cookies_treeview_selection_changed_cb),
			  dialog);

	renderer = gtk_cell_renderer_text_new ();

	gtk_tree_view_insert_column_with_attributes (treeview,
                                                     COL_COOKIES_HOST,
						     _("Domain"),
                                                     renderer,
                                                     "text", COL_COOKIES_HOST,
                                                     NULL);
        column = gtk_tree_view_get_column (treeview, COL_COOKIES_HOST);
        gtk_tree_view_column_set_resizable (column, TRUE);
        gtk_tree_view_column_set_reorderable (column, TRUE);
        gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
        gtk_tree_view_column_set_sort_column_id (column, COL_COOKIES_HOST);

        gtk_tree_view_insert_column_with_attributes (treeview,
                                                     COL_COOKIES_NAME,
						     _("Name"),
                                                     renderer,
                                                     "text", COL_COOKIES_NAME,
                                                     NULL);
        column = gtk_tree_view_get_column (treeview, COL_COOKIES_NAME);
        gtk_tree_view_column_set_resizable (column, TRUE);
        gtk_tree_view_column_set_reorderable (column, TRUE);
        gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
        gtk_tree_view_column_set_sort_column_id (column, COL_COOKIES_NAME);

	return GTK_WIDGET(treeview);
}

static void
pdm_dialog_remove_button_clicked_cb (GtkWidget *button,
				     PdmActionInfo *action)
{
	pdm_cmd_delete_selection (action);
}

static void
pdm_cmd_delete_selection (PdmActionInfo *action)
{

	GList *llist, *rlist = NULL, *l, *r;
	GList *remove_list = NULL;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreePath *path;
	GtkTreeIter iter, iter2;
	GtkTreeRowReference *row_ref = NULL;

	selection = gtk_tree_view_get_selection
		(GTK_TREE_VIEW(action->treeview));
	llist = gtk_tree_selection_get_selected_rows (selection, &model);
	
	for (l = llist;l != NULL; l = l->next)
	{
		rlist = g_list_prepend (rlist, gtk_tree_row_reference_new
					(model, (GtkTreePath *)l->data));
	}

	/* Intelligent selection logic, no actual selection yet */
	
	path = gtk_tree_row_reference_get_path 
		((GtkTreeRowReference *) g_list_first (rlist)->data);
	
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_path_free (path);
	iter2 = iter;
	
	if (gtk_tree_model_iter_next (GTK_TREE_MODEL (model), &iter))
	{
		path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
		row_ref = gtk_tree_row_reference_new (model, path);
	}
	else
	{
		path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter2);
		if (gtk_tree_path_prev (path))
		{
			row_ref = gtk_tree_row_reference_new (model, path);
		}
	}
	gtk_tree_path_free (path);
	
	/* Removal */
	
	for (r = rlist; r != NULL; r = r->next)
	{
		gpointer data;
		GValue val = {0, };

		path = gtk_tree_row_reference_get_path
			((GtkTreeRowReference *)r->data);

		gtk_tree_model_get_iter
			(model, &iter, path);
		gtk_tree_model_get_value
			(model, &iter, action->data_col, &val);
		data = g_value_get_pointer (&val);
		g_value_unset (&val);

		gtk_list_store_remove (GTK_LIST_STORE(model),
				       &iter);

		action->list = g_list_remove (action->list, data);
		remove_list = g_list_append (remove_list, data);

		gtk_tree_row_reference_free ((GtkTreeRowReference *)r->data);
		gtk_tree_path_free (path);
	}

	if (remove_list)
	{
		action->remove (action, remove_list);
		action->free (action, remove_list);
	}

	g_list_foreach (llist, (GFunc)gtk_tree_path_free, NULL);
	g_list_free (llist);
	g_list_free (rlist);

	/* Selection */
	
	if (row_ref != NULL)
	{
		path = gtk_tree_row_reference_get_path (row_ref);

		if (path != NULL)
		{
			gtk_tree_view_set_cursor (GTK_TREE_VIEW (action->treeview), path, NULL, FALSE);
			gtk_tree_path_free (path);
		}

		gtk_tree_row_reference_free (row_ref);
	}
}

static gboolean
pdm_key_pressed_cb (GtkTreeView *treeview,
		    GdkEventKey *event,
		    PdmActionInfo *action)
{
	if (event->keyval == GDK_Delete || event->keyval == GDK_KP_Delete)
	{
		pdm_cmd_delete_selection (action);
		return TRUE;
	}

	return FALSE;
}

static void
setup_action (PdmActionInfo *action)
{
	GList *l;
	GtkWidget *widget;
	GtkTreeSelection *selection;

	for (l = action->list; l != NULL; l = l->next)
	{
		action->add (action, l->data);
	}

	widget = ephy_dialog_get_control (EPHY_DIALOG(action->dialog),
					  action->remove_id);
	g_signal_connect (widget, "clicked",
			  G_CALLBACK(pdm_dialog_remove_button_clicked_cb),
			  action);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(action->treeview));
	g_signal_connect (selection, "changed",
			  G_CALLBACK(action_treeview_selection_changed_cb),
			  action);

	g_signal_connect (G_OBJECT (action->treeview),
			  "key_press_event",
			  G_CALLBACK (pdm_key_pressed_cb),
			  action);

}

static void
pdm_dialog_cookie_add (PdmActionInfo *info,
		       gpointer cookie)
{
	GtkListStore *store;
	GtkTreeIter iter;
	CookieInfo *cinfo = (CookieInfo *)cookie;

	store = GTK_LIST_STORE(gtk_tree_view_get_model
			       (GTK_TREE_VIEW(info->treeview)));

	gtk_list_store_append (store, &iter);
        gtk_list_store_set (store,
                            &iter,
			    COL_COOKIES_HOST, cinfo->domain,
                            COL_COOKIES_NAME, cinfo->name,
			    COL_COOKIES_DATA, cinfo,
                            -1);
}

static void
pdm_dialog_password_add (PdmActionInfo *info,
			 gpointer password)
{
	GtkListStore *store;
	GtkTreeIter iter;
	PasswordInfo *pinfo = (PasswordInfo *)password;

	store = GTK_LIST_STORE(gtk_tree_view_get_model
			       (GTK_TREE_VIEW(info->treeview)));

	gtk_list_store_append (store, &iter);
        gtk_list_store_set (store,
                            &iter,
                            COL_PASSWORDS_HOST, pinfo->host,
                            COL_PASSWORDS_USER, pinfo->username,
			    COL_PASSWORDS_DATA, pinfo,
                            -1);
}

static void
pdm_dialog_cookie_remove (PdmActionInfo *info,
			  GList *data)
{
	EphyEmbedSingle *single;
	single = ephy_embed_shell_get_embed_single
		(EPHY_EMBED_SHELL (ephy_shell));
	ephy_embed_single_remove_cookies (single, data);
}

static void
pdm_dialog_password_remove (PdmActionInfo *info,
			    GList *data)
{
	EphyEmbedSingle *single;
	single = ephy_embed_shell_get_embed_single
		(EPHY_EMBED_SHELL (ephy_shell));

	ephy_embed_single_remove_passwords (single, data,
					    PASSWORD_PASSWORD);
}

static void
pdm_dialog_cookies_free (PdmActionInfo *info,
			 GList *data)
{
	GList *l;
	EphyEmbedSingle *single;
	single = ephy_embed_shell_get_embed_single
		(EPHY_EMBED_SHELL (ephy_shell));

	l = data ? data : info->list;
	ephy_embed_single_free_cookies (single, l);
}

static void
pdm_dialog_passwords_free (PdmActionInfo *info,
			   GList *data)
{
	GList *l;
	EphyEmbedSingle *single;
	single = ephy_embed_shell_get_embed_single
		(EPHY_EMBED_SHELL (ephy_shell));

	l = data ? data : info->list;
	ephy_embed_single_free_passwords (single, l);
}

/* Group all Properties and Remove buttons in the same size group to avoid the
 * little jerk you get otherwise when switching pages because one set of
 * buttons is wider than another. */
static void
group_button_allocations (EphyDialog *dialog)
{
       const gint props[] =
       {
               PROP_COOKIES_REMOVE,
               PROP_COOKIES_PROPERTIES,
               PROP_PASSWORDS_REMOVE
       };
       GtkSizeGroup *size_group;
       guint i;

       size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

       for (i = 0; i < G_N_ELEMENTS (props); ++i)
       {
               GtkWidget *w;
               w = ephy_dialog_get_control (dialog, props[i]);
               gtk_size_group_add_widget (size_group,  w);
       }
}

static void
pdm_dialog_init (PdmDialog *dialog)
{
	PdmActionInfo *cookies;
	PdmActionInfo *passwords;
	GtkWidget *cookies_tv;
	GtkWidget *passwords_tv;
	EphyEmbedSingle *single;

	single = ephy_embed_shell_get_embed_single
		(EPHY_EMBED_SHELL (ephy_shell));

	dialog->priv = EPHY_PDM_DIALOG_GET_PRIVATE (dialog);

	dialog->priv->cookies = NULL;
	dialog->priv->passwords = NULL;

	ephy_dialog_construct (EPHY_DIALOG(dialog),
                               properties,
                               "epiphany.glade",
                               "pdm_dialog");

	group_button_allocations (EPHY_DIALOG (dialog));

	cookies_tv = setup_cookies_treeview (dialog);
	passwords_tv = setup_passwords_treeview (dialog);

	cookies = g_new0 (PdmActionInfo, 1);
	cookies->list = ephy_embed_single_list_cookies (single);
	cookies->dialog = dialog;
	cookies->remove_id = PROP_COOKIES_REMOVE;
	cookies->add = pdm_dialog_cookie_add;
	cookies->remove = pdm_dialog_cookie_remove;
	cookies->free = pdm_dialog_cookies_free;
	cookies->treeview = cookies_tv;
	cookies->data_col = COL_COOKIES_DATA;
	setup_action (cookies);

	passwords = g_new0 (PdmActionInfo, 1);
	passwords->list = ephy_embed_single_list_passwords
				(single, PASSWORD_PASSWORD);
	passwords->dialog = dialog;
	passwords->remove_id = PROP_PASSWORDS_REMOVE;
	passwords->add = pdm_dialog_password_add;
	passwords->remove = pdm_dialog_password_remove;
	passwords->free = pdm_dialog_passwords_free;
	passwords->treeview = passwords_tv;
	passwords->data_col = COL_PASSWORDS_DATA;
	setup_action (passwords);

	dialog->priv->cookies = cookies;
	dialog->priv->passwords = passwords;
}

static void
pdm_dialog_finalize (GObject *object)
{
	PdmDialog *dialog = EPHY_PDM_DIALOG (object);

	pdm_dialog_passwords_free (dialog->priv->passwords, NULL);
	pdm_dialog_cookies_free (dialog->priv->cookies, NULL);

	g_free (dialog->priv->passwords);
	g_free (dialog->priv->cookies);

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

EphyDialog *
pdm_dialog_new (GtkWidget *window)
{
	PdmDialog *dialog;

	dialog = EPHY_PDM_DIALOG (g_object_new (EPHY_TYPE_PDM_DIALOG,
						"ParentWindow", window,
						NULL));

	return EPHY_DIALOG(dialog);
}

static void
show_cookies_properties (PdmDialog *dialog,
			 CookieInfo *info)
{
	GtkWidget *gdialog;
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *parent;
	GtkWidget *dialog_vbox;
	char *str;

	parent = ephy_dialog_get_control (EPHY_DIALOG(dialog),
					  PROP_DIALOG);

	gdialog = gtk_dialog_new_with_buttons
		 (_("Cookie Properties"),
		  GTK_WINDOW(parent),
		  GTK_DIALOG_MODAL,
		  GTK_STOCK_CLOSE, 0, NULL);
	gtk_dialog_set_has_separator (GTK_DIALOG(gdialog), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER(gdialog), 6);

	table = gtk_table_new (2, 4, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER(table), 5);
	gtk_table_set_row_spacings (GTK_TABLE(table), 6);
	gtk_table_set_col_spacings (GTK_TABLE(table), 12);
	gtk_widget_show (table);

	str = g_strconcat ("<b>", _("Value:"), "</b>", NULL);
	label = gtk_label_new (str);
	g_free (str);
	gtk_label_set_use_markup (GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment (GTK_MISC(label), 0, 0);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
			  GTK_FILL, GTK_FILL, 0, 0);

	label = ephy_ellipsizing_label_new (info->value);
	gtk_misc_set_alignment (GTK_MISC(label), 0, 0);
	gtk_widget_show (label);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 1, 2, 0, 1);

	str = g_strconcat ("<b>", _("Path:"), "</b>", NULL);
	label = gtk_label_new (str);
	g_free (str);
	gtk_label_set_use_markup (GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment (GTK_MISC(label), 0, 0);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
			  GTK_FILL, GTK_FILL, 0, 0);

	label = gtk_label_new (info->path);
	gtk_misc_set_alignment (GTK_MISC(label), 0, 0);
	gtk_widget_show (label);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 1, 2, 1, 2);

	str = g_strconcat ("<b>", _("Secure:"), "</b>", NULL);
	label = gtk_label_new (str);
	g_free (str);
	gtk_label_set_use_markup (GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment (GTK_MISC(label), 0, 0);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3,
			  GTK_FILL, GTK_FILL, 0, 0);

	label = gtk_label_new (info->secure);
	gtk_misc_set_alignment (GTK_MISC(label), 0, 0);
	gtk_widget_show (label);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 1, 2, 2, 3);

	str = g_strconcat ("<b>", _("Expire:"), "</b>", NULL);
	label = gtk_label_new (str);
	g_free (str);
	gtk_label_set_use_markup (GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment (GTK_MISC(label), 0, 0);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 3, 4,
			  GTK_FILL, GTK_FILL, 0, 0);

	label = gtk_label_new (info->expire);
	gtk_misc_set_alignment (GTK_MISC(label), 0, 0.5);
	gtk_widget_show (label);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 1, 2, 3, 4);

	dialog_vbox = GTK_DIALOG(gdialog)->vbox;
	gtk_box_pack_start (GTK_BOX(dialog_vbox),
                            table,
                            FALSE, FALSE, 0);

	gtk_dialog_run (GTK_DIALOG(gdialog));

	gtk_widget_destroy (gdialog);
}

void
pdm_dialog_cookies_properties_button_clicked_cb (GtkWidget *button,
						 PdmDialog *dialog)
{
	GtkTreeModel *model;
	GValue val = {0, };
	GtkTreeIter iter;
	GtkTreePath *path;
	CookieInfo *info;
	GList *l;
	GtkWidget *treeview = dialog->priv->cookies->treeview;
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
	l = gtk_tree_selection_get_selected_rows
		(selection, &model);

	path = (GtkTreePath *)l->data;
        gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get_value
		(model, &iter, COL_COOKIES_DATA, &val);
	info = (CookieInfo *)g_value_get_pointer (&val);
	g_value_unset (&val);

	show_cookies_properties (dialog, info);

	g_list_foreach (l, (GFunc)gtk_tree_path_free, NULL);
	g_list_free (l);
}

void
pdm_dialog_response_cb (GtkDialog *dialog, gint response_id, gpointer data)
{
	if (response_id == GTK_RESPONSE_CLOSE)
	{
		gtk_widget_destroy (GTK_WIDGET(dialog));
	}
	else if (response_id == GTK_RESPONSE_HELP)
	{
		g_return_if_fail (EPHY_IS_PDM_DIALOG (data));

		pdm_dialog_show_help (EPHY_PDM_DIALOG (data));
	}
}
