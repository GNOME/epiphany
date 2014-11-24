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

#include <libsoup/soup.h>
#include <webkit2/webkit2.h>

#include "ephy-string.h"
#include "ephy-shell.h"

#include "cookies-dialog.h"

enum
{
	COL_COOKIES_HOST,
	COL_COOKIES_HOST_KEY,
	COL_COOKIES_DATA,
};

struct CookiesDialogPrivate
{
	GtkWidget *cookies_treeview;
	GtkWidget *liststore;
	GtkWidget *remove_toolbutton;
	GtkWidget *clear_button;

	WebKitCookieManager *cookie_manager;
	gboolean filled;
};

G_DEFINE_TYPE_WITH_PRIVATE (CookiesDialog, cookies_dialog, GTK_TYPE_DIALOG)

static void populate_model    (CookiesDialog       *dialog);
static void cookie_changed_cb (WebKitCookieManager *cookie_manager,
			       CookiesDialog       *dialog);

static void
reload_model (CookiesDialog *dialog)
{
	g_signal_handlers_disconnect_by_func (dialog->priv->cookie_manager, cookie_changed_cb, dialog);
	gtk_list_store_clear (GTK_LIST_STORE (dialog->priv->liststore));
	dialog->priv->filled = FALSE;
	populate_model (dialog);
}

static void
cookie_changed_cb (WebKitCookieManager *cookie_manager,
		   CookiesDialog       *dialog)
{
	reload_model (dialog);
}

static void
cookies_dialog_dispose (GObject *object)
{
	CookiesDialogPrivate *priv;

	priv = EPHY_COOKIES_DIALOG (object)->priv;

	g_signal_handlers_disconnect_by_func (priv->cookie_manager, cookie_changed_cb, object);

	G_OBJECT_CLASS (cookies_dialog_parent_class)->dispose (object);
}

static void
cookie_remove (CookiesDialog *dialog,
	       gpointer data)
{
	const char *domain = (const char *) data;

	webkit_cookie_manager_delete_cookies_for_domain (dialog->priv->cookie_manager,
							 domain);
}

static void
delete_selection (CookiesDialog *dialog)
{
	GList *llist, *rlist = NULL, *l, *r;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreePath *path;
	GtkTreeIter iter, iter2;
	GtkTreeRowReference *row_ref = NULL;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->priv->cookies_treeview));
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

		path = gtk_tree_row_reference_get_path ((GtkTreeRowReference *)r->data);
		gtk_tree_model_get_iter (model, &iter, path);
		gtk_tree_model_get_value (model, &iter, COL_COOKIES_HOST, &val);
		cookie_remove (dialog, (gpointer)g_value_get_string (&val));
		g_value_unset (&val);

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
			gtk_tree_view_set_cursor (GTK_TREE_VIEW (dialog->priv->cookies_treeview), path, NULL, FALSE);
			gtk_tree_path_free (path);
		}

		gtk_tree_row_reference_free (row_ref);
	}
}

static gboolean
on_cookies_treeview_key_press_event (GtkWidget     *widget,
				     GdkEventKey   *event,
				     CookiesDialog *dialog)
{
	if (event->keyval == GDK_KEY_Delete || event->keyval == GDK_KEY_KP_Delete)
	{
		delete_selection (dialog);

		return TRUE;
	}

	return FALSE;
}

static void
on_remove_toolbutton_clicked (GtkToolButton *toolbutton,
			      CookiesDialog *dialog)
{
	delete_selection (dialog);

	/* Restore the focus to the button */
	gtk_widget_grab_focus (GTK_WIDGET (toolbutton));
}

static void
on_treeview_selection_changed (GtkTreeSelection *selection,
			       CookiesDialog    *dialog)
{
	gboolean has_selection;

	has_selection = gtk_tree_selection_count_selected_rows (selection) > 0;

	gtk_widget_set_sensitive (dialog->priv->remove_toolbutton, has_selection);
}

static void
cookies_dialog_class_init (CookiesDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->dispose = cookies_dialog_dispose;

	gtk_widget_class_set_template_from_resource (widget_class,
	                                             "/org/gnome/epiphany/cookies-dialog.ui");

	gtk_widget_class_bind_template_child_private (widget_class, CookiesDialog, liststore);
	gtk_widget_class_bind_template_child_private (widget_class, CookiesDialog, cookies_treeview);
	gtk_widget_class_bind_template_child_private (widget_class, CookiesDialog, clear_button);
	gtk_widget_class_bind_template_child_private (widget_class, CookiesDialog, remove_toolbutton);

	gtk_widget_class_bind_template_callback (widget_class, on_cookies_treeview_key_press_event);
	gtk_widget_class_bind_template_callback (widget_class, on_treeview_selection_changed);
	gtk_widget_class_bind_template_callback (widget_class, on_remove_toolbutton_clicked);
}

static void
cookies_dialog_response_cb (GtkDialog *widget,
			    int response,
			    CookiesDialog *dialog)
{
	if (response == GTK_RESPONSE_REJECT) {
		webkit_cookie_manager_delete_all_cookies (dialog->priv->cookie_manager);
		reload_model (dialog);
		return;
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static gboolean
cookie_search_equal (GtkTreeModel *model,
		     int column,
		     const gchar *key,
		     GtkTreeIter *iter,
		     gpointer search_data)
{
	GValue value = { 0, };
	gboolean retval;

	/* Note that this is function has to return FALSE for a *match* ! */

	gtk_tree_model_get_value (model, iter, column, &value);
	retval = strstr (g_value_get_string (&value), key) == NULL;
	g_value_unset (&value);

	return retval;
}

static void
cookie_add (CookiesDialog *dialog,
	    gpointer data)
{
	char *domain = (char *) data;
	GtkListStore *store;
	GtkTreeIter iter;
	int column[3] = { COL_COOKIES_HOST, COL_COOKIES_HOST_KEY, COL_COOKIES_DATA };
	GValue value[3] = { { 0, }, { 0, }, { 0, } };

	store = GTK_LIST_STORE (dialog->priv->liststore);

	/* NOTE: We use this strange method to insert the row, because
	 * we want to use g_value_take_string but all the row data needs to
	 * be inserted in one call as it's needed when the new row is sorted
	 * into the model.
	 */

	g_value_init (&value[0], G_TYPE_STRING);
	g_value_init (&value[1], G_TYPE_STRING);
	g_value_init (&value[2], SOUP_TYPE_COOKIE);

	g_value_set_static_string (&value[0], domain);
	g_value_take_string (&value[1], ephy_string_collate_key_for_domain (domain, -1));

	gtk_list_store_insert_with_valuesv (store, &iter, -1,
					    column, value,
					    G_N_ELEMENTS (value));

	g_value_unset (&value[0]);
	g_value_unset (&value[1]);
	g_value_unset (&value[2]);
}

static int
compare_cookie_host_keys (GtkTreeModel *model,
			  GtkTreeIter  *a,
			  GtkTreeIter  *b,
			  gpointer user_data)
{
	GValue a_value = {0, };
	GValue b_value = {0, };
	int retval;

	gtk_tree_model_get_value (model, a, COL_COOKIES_HOST_KEY, &a_value);
	gtk_tree_model_get_value (model, b, COL_COOKIES_HOST_KEY, &b_value);

	retval = strcmp (g_value_get_string (&a_value),
			 g_value_get_string (&b_value));

	g_value_unset (&a_value);
	g_value_unset (&b_value);

	return retval;
}

static void
get_domains_with_cookies_cb (WebKitCookieManager *cookie_manager,
			     GAsyncResult *result,
			     CookiesDialog *dialog)
{
	gchar **domains;
	guint i;

	domains = webkit_cookie_manager_get_domains_with_cookies_finish (cookie_manager, result, NULL);
	if (!domains)
		return;

	for (i = 0; domains[i]; i++)
		cookie_add (dialog, domains[i]);

	/* The array items have been consumed, so we need only to free the array. */
	g_free (domains);

	/* Now turn on sorting */
	gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (dialog->priv->liststore),
					 COL_COOKIES_HOST_KEY,
					 (GtkTreeIterCompareFunc) compare_cookie_host_keys,
					 NULL, NULL);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (dialog->priv->liststore),
					      COL_COOKIES_HOST_KEY,
					      GTK_SORT_ASCENDING);

	g_signal_connect (cookie_manager,
			  "changed",
			  G_CALLBACK (cookie_changed_cb),
			  dialog);

	dialog->priv->filled = TRUE;
}

static void
populate_model (CookiesDialog *dialog)
{
	g_assert (dialog->priv->filled == FALSE);

	webkit_cookie_manager_get_domains_with_cookies (dialog->priv->cookie_manager,
							NULL,
							(GAsyncReadyCallback) get_domains_with_cookies_cb,
							dialog);
}

static void
setup_page (CookiesDialog *dialog)
{
	CookiesDialogPrivate *priv = dialog->priv;

	gtk_tree_view_set_search_equal_func (GTK_TREE_VIEW (priv->cookies_treeview),
					     (GtkTreeViewSearchEqualFunc) cookie_search_equal,
					     dialog, NULL);
	populate_model (dialog);
}

static void
cookies_dialog_init (CookiesDialog *dialog)
{
	WebKitWebContext *web_context;
	EphyEmbedShell *shell = ephy_embed_shell_get_default ();

	dialog->priv = cookies_dialog_get_instance_private (dialog);
	gtk_widget_init_template (GTK_WIDGET (dialog));

	web_context = ephy_embed_shell_get_web_context (shell);
	dialog->priv->cookie_manager = webkit_web_context_get_cookie_manager (web_context);

	setup_page (dialog);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (cookies_dialog_response_cb), dialog);
}
