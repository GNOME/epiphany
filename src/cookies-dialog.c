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

struct _EphyCookiesDialog
{
	GtkDialog parent_instance;

	GtkWidget *cookies_treeview;
	GtkTreeSelection *tree_selection;
	GtkWidget *liststore;
	GtkWidget *treemodelfilter;
	GtkWidget *treemodelsort;

	GActionGroup *action_group;

	WebKitCookieManager *cookie_manager;
	gboolean filled;

	char *search_text;
};

G_DEFINE_TYPE (EphyCookiesDialog, ephy_cookies_dialog, GTK_TYPE_DIALOG)

static void populate_model    (EphyCookiesDialog   *dialog);
static void cookie_changed_cb (WebKitCookieManager *cookie_manager,
                               EphyCookiesDialog   *dialog);

static void
reload_model (EphyCookiesDialog *dialog)
{
	g_signal_handlers_disconnect_by_func (dialog->cookie_manager, cookie_changed_cb, dialog);
	gtk_list_store_clear (GTK_LIST_STORE (dialog->liststore));
	dialog->filled = FALSE;
	populate_model (dialog);
}

static void
cookie_changed_cb (WebKitCookieManager *cookie_manager,
                   EphyCookiesDialog   *dialog)
{
	reload_model (dialog);
}

static void
ephy_cookies_dialog_dispose (GObject *object)
{
	g_signal_handlers_disconnect_by_func (EPHY_COOKIES_DIALOG (object)->cookie_manager, cookie_changed_cb, object);
	G_OBJECT_CLASS (ephy_cookies_dialog_parent_class)->dispose (object);
}

static void
ephy_cookies_dialog_finalize (GObject *object)
{
	g_free (EPHY_COOKIES_DIALOG (object)->search_text);
	G_OBJECT_CLASS (ephy_cookies_dialog_parent_class)->finalize (object);
}

static void
cookie_remove (EphyCookiesDialog *dialog,
	       gpointer data)
{
	const char *domain = (const char *) data;

	webkit_cookie_manager_delete_cookies_for_domain (dialog->cookie_manager, domain);
}

static void
forget (GSimpleAction     *action,
        GVariant          *parameter,
        gpointer           user_data)
{
	EphyCookiesDialog *dialog = EPHY_COOKIES_DIALOG (user_data);
	GList *llist, *rlist = NULL, *l, *r;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter, iter2;
	GtkTreeRowReference *row_ref = NULL;

	llist = gtk_tree_selection_get_selected_rows (dialog->tree_selection, &model);

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

		GtkTreeIter filter_iter;
		GtkTreeIter child_iter;

		path = gtk_tree_row_reference_get_path ((GtkTreeRowReference *)r->data);
		gtk_tree_model_get_iter (model, &iter, path);
		gtk_tree_model_get_value (model, &iter, COL_COOKIES_HOST, &val);
		cookie_remove (dialog, (gpointer)g_value_get_string (&val));
		g_value_unset (&val);

		gtk_tree_model_sort_convert_iter_to_child_iter (GTK_TREE_MODEL_SORT (dialog->treemodelsort),
								&filter_iter,
								&iter);

		gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (dialog->treemodelfilter),
								  &child_iter,
								  &filter_iter);

		gtk_list_store_remove (GTK_LIST_STORE (dialog->liststore), &child_iter);

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
			gtk_tree_view_set_cursor (GTK_TREE_VIEW (dialog->cookies_treeview), path, NULL, FALSE);
			gtk_tree_path_free (path);
		}

		gtk_tree_row_reference_free (row_ref);
	}
}

static void
update_selection_actions (GActionMap *action_map,
                          gboolean    has_selection)
{
	GAction *forget_action;

	forget_action = g_action_map_lookup_action (action_map, "forget");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (forget_action), has_selection);
}

static void
on_treeview_selection_changed (GtkTreeSelection  *selection,
                               EphyCookiesDialog *dialog)
{
	update_selection_actions (G_ACTION_MAP (dialog->action_group),
	                          gtk_tree_selection_count_selected_rows (selection) > 0);
}

static void
on_search_entry_changed (GtkSearchEntry    *entry,
                         EphyCookiesDialog *dialog)
{
	const char *text;

	text = gtk_entry_get_text (GTK_ENTRY (entry));
	g_free (dialog->search_text);
	dialog->search_text = g_strdup (text);
	gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (dialog->treemodelfilter));
}

static void
forget_all (GSimpleAction     *action,
            GVariant          *parameter,
            gpointer           user_data)
{
	EphyCookiesDialog *dialog = EPHY_COOKIES_DIALOG (user_data);

	webkit_cookie_manager_delete_all_cookies (dialog->cookie_manager);
	reload_model (dialog);
}

static void
ephy_cookies_dialog_class_init (EphyCookiesDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->dispose = ephy_cookies_dialog_dispose;
	object_class->finalize = ephy_cookies_dialog_finalize;

	gtk_widget_class_set_template_from_resource (widget_class,
	                                             "/org/gnome/epiphany/cookies-dialog.ui");

	gtk_widget_class_bind_template_child (widget_class, EphyCookiesDialog, liststore);
	gtk_widget_class_bind_template_child (widget_class, EphyCookiesDialog, treemodelfilter);
	gtk_widget_class_bind_template_child (widget_class, EphyCookiesDialog, treemodelsort);
	gtk_widget_class_bind_template_child (widget_class, EphyCookiesDialog, cookies_treeview);
	gtk_widget_class_bind_template_child (widget_class, EphyCookiesDialog, tree_selection);

	gtk_widget_class_bind_template_callback (widget_class, on_treeview_selection_changed);
	gtk_widget_class_bind_template_callback (widget_class, on_search_entry_changed);
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
cookie_add (EphyCookiesDialog *dialog,
	    gpointer data)
{
	char *domain = (char *) data;
	GtkListStore *store;
	GtkTreeIter iter;
	int column[3] = { COL_COOKIES_HOST, COL_COOKIES_HOST_KEY, COL_COOKIES_DATA };
	GValue value[3] = { { 0, }, { 0, }, { 0, } };

	store = GTK_LIST_STORE (dialog->liststore);

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
                             GAsyncResult        *result,
                             EphyCookiesDialog   *dialog)
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
	gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (dialog->liststore),
					 COL_COOKIES_HOST_KEY,
					 (GtkTreeIterCompareFunc) compare_cookie_host_keys,
					 NULL, NULL);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (dialog->liststore),
					      COL_COOKIES_HOST_KEY,
					      GTK_SORT_ASCENDING);

	g_signal_connect (cookie_manager,
			  "changed",
			  G_CALLBACK (cookie_changed_cb),
			  dialog);

	dialog->filled = TRUE;
}

static gboolean
row_visible_func (GtkTreeModel      *model,
                  GtkTreeIter       *iter,
                  EphyCookiesDialog *dialog)
{
	gboolean visible = FALSE;
	gchar *host;

	if (dialog->search_text == NULL)
		return TRUE;

	gtk_tree_model_get (model, iter,
			    COL_COOKIES_HOST, &host,
			    -1);

	if (host != NULL && strstr (host, dialog->search_text) != NULL)
		visible = TRUE;

	g_free (host);

	return visible;
}

static void
populate_model (EphyCookiesDialog *dialog)
{
	g_assert (dialog->filled == FALSE);

	webkit_cookie_manager_get_domains_with_cookies (dialog->cookie_manager,
							NULL,
							(GAsyncReadyCallback) get_domains_with_cookies_cb,
							dialog);
}

static void
setup_page (EphyCookiesDialog *dialog)
{
	gtk_tree_view_set_search_equal_func (GTK_TREE_VIEW (dialog->cookies_treeview),
					     (GtkTreeViewSearchEqualFunc) cookie_search_equal,
					     dialog, NULL);
	populate_model (dialog);
}

static GActionGroup *
create_action_group (EphyCookiesDialog *dialog)
{
	const GActionEntry entries[] = {
		{ "forget", forget },
		{ "forget-all", forget_all }
	};

	GSimpleActionGroup *group;

	group = g_simple_action_group_new ();
	g_action_map_add_action_entries (G_ACTION_MAP (group), entries, G_N_ELEMENTS (entries), dialog);

	return G_ACTION_GROUP (group);
}

static void
ephy_cookies_dialog_init (EphyCookiesDialog *dialog)
{
	WebKitWebContext *web_context;
	EphyEmbedShell *shell = ephy_embed_shell_get_default ();

	gtk_widget_init_template (GTK_WIDGET (dialog));

	gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (dialog->treemodelfilter),
						(GtkTreeModelFilterVisibleFunc)row_visible_func,
						dialog,
						NULL);

	web_context = ephy_embed_shell_get_web_context (shell);
	dialog->cookie_manager = webkit_web_context_get_cookie_manager (web_context);

	setup_page (dialog);

	dialog->action_group = create_action_group (dialog);
	gtk_widget_insert_action_group (GTK_WIDGET (dialog), "cookies", dialog->action_group);

	update_selection_actions (G_ACTION_MAP (dialog->action_group), FALSE);
}

EphyCookiesDialog *
ephy_cookies_dialog_new (void)
{
	return g_object_new (EPHY_TYPE_COOKIES_DIALOG,
	                     "use-header-bar", TRUE,
	                     NULL);
}
