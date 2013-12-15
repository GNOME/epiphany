/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 2013 Red Hat, Inc.
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

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

#define SECRET_API_SUBJECT_TO_CHANGE
#include <libsecret/secret.h>

#include "ephy-form-auth-data.h"
#include "ephy-string.h"
#include "passwords-dialog.h"

typedef enum
{
	COL_PASSWORDS_HOST,
	COL_PASSWORDS_USER,
	COL_PASSWORDS_PASSWORD,
	COL_PASSWORDS_INVISIBLE,
	COL_PASSWORDS_DATA,
} PasswordsDialogColumn;

#define URI_KEY           "uri"
#define FORM_USERNAME_KEY "form_username"
#define FORM_PASSWORD_KEY "form_password"
#define USERNAME_KEY      "username"

struct PasswordsDialogPrivate
{
	GtkWidget *passwords_treeview;
	GtkWidget *liststore;
	GtkWidget *treemodelfilter;
	GtkWidget *treemodelsort;
	GtkWidget *remove_button;
	GtkWidget *show_passwords_button;
	GtkWidget *clear_button;
	GtkWidget *password_column;
	GtkWidget *password_renderer;
	GtkWidget *treeview_popup_menu;
	GtkWidget *copy_password_menuitem;
	GtkWidget *copy_username_menuitem;

	SecretService *ss;
	GCancellable *ss_cancellable;
	gboolean filled;

	char *search_text;
};

G_DEFINE_TYPE_WITH_PRIVATE (PasswordsDialog, passwords_dialog, GTK_TYPE_DIALOG)

static void populate_model    (PasswordsDialog       *dialog);

static void
reload_model (PasswordsDialog *dialog)
{
	gtk_list_store_clear (GTK_LIST_STORE (dialog->priv->liststore));
	dialog->priv->filled = FALSE;
	populate_model (dialog);
}

static void
passwords_dialog_dispose (GObject *object)
{
	PasswordsDialogPrivate *priv;

	priv = EPHY_PASSWORDS_DIALOG (object)->priv;

	if (priv->ss_cancellable != NULL) {
		g_cancellable_cancel (priv->ss_cancellable);
		g_clear_object (&priv->ss_cancellable);
	}

	g_clear_object (&priv->ss);
	g_free (priv->search_text);
	priv->search_text = NULL;

	G_OBJECT_CLASS (passwords_dialog_parent_class)->dispose (object);
}

static void
secret_remove_ready_cb (GObject *source,
			GAsyncResult *res,
			PasswordsDialog *dialog)
{
	secret_item_delete_finish (SECRET_ITEM (source), res, NULL);
}

static void
secret_remove (PasswordsDialog *dialog,
	       SecretItem *item)
{
	secret_item_delete (item, NULL, (GAsyncReadyCallback)secret_remove_ready_cb, dialog);
}

static void
delete_selection (PasswordsDialog *dialog)
{
	GList *llist, *rlist = NULL, *l, *r;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreePath *path;
	GtkTreeIter iter, iter2;
	GtkTreeRowReference *row_ref = NULL;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->priv->passwords_treeview));
	llist = gtk_tree_selection_get_selected_rows (selection, &model);

	if (llist == NULL)
	{
		/* nothing to delete, return early */
		return;
	}

	for (l = llist; l != NULL; l = l->next)
	{
		rlist = g_list_prepend (rlist, gtk_tree_row_reference_new (model, (GtkTreePath *)l->data));
	}

	/* Intelligent selection logic, no actual selection yet */

	path = gtk_tree_row_reference_get_path ((GtkTreeRowReference *) g_list_first (rlist)->data);

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
		GValue val = { 0, };
		SecretItem *item;
		GtkTreeIter filter_iter;
		GtkTreeIter child_iter;

		path = gtk_tree_row_reference_get_path ((GtkTreeRowReference *)r->data);
		gtk_tree_model_get_iter (model, &iter, path);
		gtk_tree_model_get_value (model, &iter, COL_PASSWORDS_DATA, &val);
		item = g_value_get_object (&val);
		secret_remove (dialog, item);
		g_value_unset (&val);

		gtk_tree_model_sort_convert_iter_to_child_iter (GTK_TREE_MODEL_SORT (dialog->priv->treemodelsort),
								&filter_iter,
								&iter);

		gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (dialog->priv->treemodelfilter),
								&child_iter,
								&filter_iter);

		gtk_list_store_remove (GTK_LIST_STORE (dialog->priv->liststore), &child_iter);

		gtk_tree_row_reference_free ((GtkTreeRowReference *)r->data);
		gtk_tree_path_free (path);
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
			gtk_tree_view_set_cursor (GTK_TREE_VIEW (dialog->priv->passwords_treeview), path, NULL, FALSE);
			gtk_tree_path_free (path);
		}

		gtk_tree_row_reference_free (row_ref);
	}
}

static gboolean
on_passwords_treeview_key_press_event (GtkWidget     *widget,
				     GdkEventKey   *event,
				     PasswordsDialog *dialog)
{
	if (event->keyval == GDK_KEY_Delete || event->keyval == GDK_KEY_KP_Delete)
	{
		delete_selection (dialog);

		return TRUE;
	}

	return FALSE;
}

static void
on_remove_button_clicked (GtkButton *button,
			  PasswordsDialog *dialog)
{
	delete_selection (dialog);

	/* Restore the focus to the button */
	gtk_widget_grab_focus (GTK_WIDGET (button));
}

static void
on_show_passwords_button_toggled (GtkToggleButton *button,
				  PasswordsDialog *dialog)
{
	gboolean active;

	active = gtk_toggle_button_get_active (button);

	gtk_tree_view_column_set_attributes (GTK_TREE_VIEW_COLUMN (dialog->priv->password_column),
					     GTK_CELL_RENDERER (dialog->priv->password_renderer),
					     "text", (active ? COL_PASSWORDS_PASSWORD : COL_PASSWORDS_INVISIBLE),
					     NULL);
	gtk_widget_queue_draw (dialog->priv->passwords_treeview);
}

static void
on_treeview_selection_changed (GtkTreeSelection *selection,
			       PasswordsDialog    *dialog)
{
	gboolean has_selection;

	has_selection = gtk_tree_selection_count_selected_rows (selection) > 0;

	gtk_widget_set_sensitive (dialog->priv->remove_button, has_selection);
}

static void
on_search_entry_changed (GtkSearchEntry *entry,
			 PasswordsDialog *dialog)
{
	const char *text;

	text = gtk_entry_get_text (GTK_ENTRY (entry));
	g_free (dialog->priv->search_text);
	dialog->priv->search_text = g_strdup (text);
	gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (dialog->priv->treemodelfilter));
}

static char *
get_selected_item (PasswordsDialog *dialog,
		   PasswordsDialogColumn column)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GList *selected;
	GtkTreeIter iter;
	char *value;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->priv->passwords_treeview));
	selected = gtk_tree_selection_get_selected_rows (selection, &model);
	gtk_tree_model_get_iter (model, &iter, selected->data);
	gtk_tree_model_get (model, &iter,
			    column, &value,
			    -1);
	g_list_free_full (selected, (GDestroyNotify) gtk_tree_path_free);

	return value;
}

static void
on_copy_password_menuitem_activate (GtkMenuItem *menuitem,
				    PasswordsDialog *dialog)
{
	char *password;

	password = get_selected_item (dialog, COL_PASSWORDS_PASSWORD);
	if (password != NULL) {
		gtk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (menuitem),
								  GDK_SELECTION_CLIPBOARD),
					password, -1);
	}
	g_free (password);
}

static void
on_copy_username_menuitem_activate (GtkMenuItem *menuitem,
				    PasswordsDialog *dialog)
{
	char *username;

	username = get_selected_item (dialog, COL_PASSWORDS_USER);
	if (username != NULL) {
		gtk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (menuitem),
								  GDK_SELECTION_CLIPBOARD),
					username, -1);
	}
	g_free (username);
}

static gboolean
on_passwords_treeview_button_press_event (GtkWidget       *widget,
					  GdkEventButton  *event,
					  PasswordsDialog *dialog)
{
	if (event->button == 3) {
		GtkTreeSelection *selection;
		int n;

		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->priv->passwords_treeview));
		n = gtk_tree_selection_count_selected_rows (selection);
		if (n == 0)
			return FALSE;

		gtk_widget_set_sensitive (dialog->priv->copy_password_menuitem, (n == 1));
		gtk_widget_set_sensitive (dialog->priv->copy_username_menuitem, (n == 1));

		gtk_menu_popup (GTK_MENU (dialog->priv->treeview_popup_menu),
				NULL, NULL, NULL, NULL,
				event->button, event->time);
		return TRUE;
	}

	return FALSE;
}

static void
passwords_dialog_class_init (PasswordsDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->dispose = passwords_dialog_dispose;

	gtk_widget_class_set_template_from_resource (widget_class,
	                                             "/org/gnome/epiphany/passwords-dialog.ui");

	gtk_widget_class_bind_template_child_private (widget_class, PasswordsDialog, liststore);
	gtk_widget_class_bind_template_child_private (widget_class, PasswordsDialog, treemodelfilter);
	gtk_widget_class_bind_template_child_private (widget_class, PasswordsDialog, treemodelsort);
	gtk_widget_class_bind_template_child_private (widget_class, PasswordsDialog, passwords_treeview);
	gtk_widget_class_bind_template_child_private (widget_class, PasswordsDialog, clear_button);
	gtk_widget_class_bind_template_child_private (widget_class, PasswordsDialog, remove_button);
	gtk_widget_class_bind_template_child_private (widget_class, PasswordsDialog, show_passwords_button);
	gtk_widget_class_bind_template_child_private (widget_class, PasswordsDialog, password_column);
	gtk_widget_class_bind_template_child_private (widget_class, PasswordsDialog, password_renderer);
	gtk_widget_class_bind_template_child_private (widget_class, PasswordsDialog, treeview_popup_menu);
	gtk_widget_class_bind_template_child_private (widget_class, PasswordsDialog, copy_password_menuitem);
	gtk_widget_class_bind_template_child_private (widget_class, PasswordsDialog, copy_username_menuitem);

	gtk_widget_class_bind_template_callback (widget_class, on_passwords_treeview_key_press_event);
	gtk_widget_class_bind_template_callback (widget_class, on_passwords_treeview_button_press_event);
	gtk_widget_class_bind_template_callback (widget_class, on_treeview_selection_changed);
	gtk_widget_class_bind_template_callback (widget_class, on_remove_button_clicked);
	gtk_widget_class_bind_template_callback (widget_class, on_show_passwords_button_toggled);
	gtk_widget_class_bind_template_callback (widget_class, on_search_entry_changed);
	gtk_widget_class_bind_template_callback (widget_class, on_copy_password_menuitem_activate);
	gtk_widget_class_bind_template_callback (widget_class, on_copy_username_menuitem_activate);
}

static void
delete_all_passwords_ready_cb (GObject *source_object,
			       GAsyncResult *res,
			       PasswordsDialog *dialog)
{
	secret_service_clear_finish (dialog->priv->ss, res, NULL);
	reload_model (dialog);
}

static void
delete_all_passwords (PasswordsDialog *dialog)
{
	GHashTable *attributes;

	attributes = secret_attributes_build (EPHY_FORM_PASSWORD_SCHEMA, NULL);
	secret_service_clear (dialog->priv->ss,
			      EPHY_FORM_PASSWORD_SCHEMA,
			      attributes,
			      dialog->priv->ss_cancellable,
			      (GAsyncReadyCallback)delete_all_passwords_ready_cb,
			      dialog);
	g_hash_table_unref (attributes);
}

static void
passwords_dialog_response_cb (GtkDialog *widget,
			    int response,
			    PasswordsDialog *dialog)
{
	if (response == GTK_RESPONSE_REJECT) {
		delete_all_passwords (dialog);
		return;
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
secrets_search_ready_cb (GObject *source_object,
			 GAsyncResult *res,
			 PasswordsDialog *dialog)
{
	GList *matches;
	GList *l;

	matches = secret_service_search_finish (dialog->priv->ss, res, NULL);

	for (l = matches; l != NULL; l = l->next) {
		SecretItem *item = l->data;
		SecretValue *value = NULL;
		GHashTable *attributes = NULL;
		const char *username = NULL;
		const char *password = NULL;
		char *host = NULL;
		GtkTreeIter iter;

		attributes = secret_item_get_attributes (item);
		username = g_hash_table_lookup (attributes, USERNAME_KEY);
		host = ephy_string_get_host_name (g_hash_table_lookup (attributes, URI_KEY));
		value = secret_item_get_secret (item);
		password = secret_value_get (value, NULL);

		gtk_list_store_insert_with_values (GTK_LIST_STORE (dialog->priv->liststore),
						   &iter,
						   -1,
						   COL_PASSWORDS_HOST, host,
						   COL_PASSWORDS_USER, username,
						   COL_PASSWORDS_PASSWORD, password,
						   COL_PASSWORDS_INVISIBLE, "●●●●●●●●",
						   COL_PASSWORDS_DATA, item,
						   -1);

		g_free (host);
		g_hash_table_unref (attributes);
	}

	g_list_free_full (matches, g_object_unref);
}

static void
populate_model (PasswordsDialog *dialog)
{
	GHashTable *attributes;

	g_assert (dialog->priv->filled == FALSE);

	attributes = secret_attributes_build (EPHY_FORM_PASSWORD_SCHEMA, NULL);

	secret_service_search (dialog->priv->ss,
			       EPHY_FORM_PASSWORD_SCHEMA,
			       attributes,
			       SECRET_SEARCH_ALL | SECRET_SEARCH_UNLOCK | SECRET_SEARCH_LOAD_SECRETS,
			       dialog->priv->ss_cancellable,
			       (GAsyncReadyCallback)secrets_search_ready_cb,
			       dialog);

	g_hash_table_unref (attributes);
}

static void
secrets_ready_cb (GObject *source_object,
		  GAsyncResult *res,
		  PasswordsDialog *dialog)
{
	dialog->priv->ss = secret_service_get_finish (res, NULL);
	populate_model (dialog);
}

static gboolean
row_visible_func (GtkTreeModel *model,
		  GtkTreeIter  *iter,
		  PasswordsDialog *dialog)
{
	char *username;
	char *host;
	gboolean visible = FALSE;

	if (dialog->priv->search_text == NULL)
		return TRUE;

	gtk_tree_model_get (model, iter,
			    COL_PASSWORDS_HOST, &host,
			    COL_PASSWORDS_USER, &username,
			    -1);

	if (host != NULL && g_strrstr (host, dialog->priv->search_text) != NULL)
		visible = TRUE;
	else if (username != NULL && g_strrstr (username, dialog->priv->search_text) != NULL)
		visible = TRUE;

	g_free (host);
	g_free (username);

	return visible;
}

static void
passwords_dialog_init (PasswordsDialog *dialog)
{
	dialog->priv = passwords_dialog_get_instance_private (dialog);
	gtk_widget_init_template (GTK_WIDGET (dialog));

	gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (dialog->priv->treemodelfilter),
						(GtkTreeModelFilterVisibleFunc)row_visible_func,
						dialog,
						NULL);

	dialog->priv->ss_cancellable = g_cancellable_new ();
	secret_service_get (SECRET_SERVICE_OPEN_SESSION | SECRET_SERVICE_LOAD_COLLECTIONS,
			    dialog->priv->ss_cancellable,
			    (GAsyncReadyCallback)secrets_ready_cb,
			    dialog);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (passwords_dialog_response_cb), dialog);
}
