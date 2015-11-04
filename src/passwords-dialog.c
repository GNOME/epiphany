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

struct _EphyPasswordsDialog
{
	GtkDialog parent_instance;

	GtkWidget *passwords_treeview;
	GtkTreeSelection *tree_selection;
	GtkWidget *liststore;
	GtkWidget *treemodelfilter;
	GtkWidget *treemodelsort;
	GtkWidget *show_passwords_button;
	GtkWidget *password_column;
	GtkWidget *password_renderer;
	GMenuModel *treeview_popup_menu_model;

	GActionGroup *action_group;

	SecretService *ss;
	GCancellable *ss_cancellable;
	gboolean filled;

	char *search_text;
};

G_DEFINE_TYPE (EphyPasswordsDialog, ephy_passwords_dialog, GTK_TYPE_DIALOG)

static void populate_model (EphyPasswordsDialog *dialog);

static void
reload_model (EphyPasswordsDialog *dialog)
{
	gtk_list_store_clear (GTK_LIST_STORE (dialog->liststore));
	dialog->filled = FALSE;
	populate_model (dialog);
}

static void
ephy_passwords_dialog_dispose (GObject *object)
{
	EphyPasswordsDialog *dialog = EPHY_PASSWORDS_DIALOG (object);

	if (dialog->ss_cancellable != NULL) {
		g_cancellable_cancel (dialog->ss_cancellable);
		g_clear_object (&(dialog->ss_cancellable));
	}

	g_clear_object (&(dialog->ss));
	g_free (dialog->search_text);
	dialog->search_text = NULL;

	G_OBJECT_CLASS (ephy_passwords_dialog_parent_class)->dispose (object);
}

static void
secret_remove_ready_cb (GObject             *source,
                        GAsyncResult        *res,
                        EphyPasswordsDialog *dialog)
{
	secret_item_delete_finish (SECRET_ITEM (source), res, NULL);
}

static void
secret_remove (EphyPasswordsDialog *dialog,
               SecretItem          *item)
{
	secret_item_delete (item, NULL, (GAsyncReadyCallback)secret_remove_ready_cb, dialog);
}

static void
forget (GSimpleAction       *action,
        GVariant            *parameter,
        EphyPasswordsDialog *dialog)
{
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
		SecretItem *item;
		GtkTreeIter filter_iter;
		GtkTreeIter child_iter;

		path = gtk_tree_row_reference_get_path ((GtkTreeRowReference *)r->data);
		gtk_tree_model_get_iter (model, &iter, path);
		gtk_tree_model_get_value (model, &iter, COL_PASSWORDS_DATA, &val);
		item = g_value_get_object (&val);
		secret_remove (dialog, item);
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
			gtk_tree_view_set_cursor (GTK_TREE_VIEW (dialog->passwords_treeview), path, NULL, FALSE);
			gtk_tree_path_free (path);
		}

		gtk_tree_row_reference_free (row_ref);
	}
}

static void
show_passwords (GSimpleAction       *action,
                GVariant            *parameter,
                EphyPasswordsDialog *dialog)
{
	gboolean active;

	active = gtk_toggle_button_get_active (dialog->show_passwords_button);

	gtk_tree_view_column_set_attributes (GTK_TREE_VIEW_COLUMN (dialog->password_column),
					     GTK_CELL_RENDERER (dialog->password_renderer),
					     "text", (active ? COL_PASSWORDS_PASSWORD : COL_PASSWORDS_INVISIBLE),
					     NULL);
	gtk_widget_queue_draw (dialog->passwords_treeview);
}

static void
update_selection_actions (GActionMap *action_map,
                          gboolean    has_selection)
{
	GSimpleAction *forget_action;

	forget_action = g_action_map_lookup_action (action_map, "forget");
	g_simple_action_set_enabled (forget_action, has_selection);
}

static void
on_treeview_selection_changed (GtkTreeSelection    *selection,
                               EphyPasswordsDialog *dialog)
{
	update_selection_actions (G_ACTION_MAP (dialog->action_group),
	                          gtk_tree_selection_count_selected_rows (selection) > 0);
}

static void
on_search_entry_changed (GtkSearchEntry      *entry,
                         EphyPasswordsDialog *dialog)
{
	const char *text;

	text = gtk_entry_get_text (GTK_ENTRY (entry));
	g_free (dialog->search_text);
	dialog->search_text = g_strdup (text);
	gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (dialog->treemodelfilter));
}

static char *
get_selected_item (EphyPasswordsDialog *dialog,
		   PasswordsDialogColumn column)
{
	GtkTreeModel *model;
	GList *selected;
	GtkTreeIter iter;
	char *value;

	selected = gtk_tree_selection_get_selected_rows (dialog->tree_selection, &model);
	gtk_tree_model_get_iter (model, &iter, selected->data);
	gtk_tree_model_get (model, &iter,
			    column, &value,
			    -1);
	g_list_free_full (selected, (GDestroyNotify) gtk_tree_path_free);

	return value;
}

static void
copy_password (GSimpleAction       *action,
               GVariant            *parameter,
               EphyPasswordsDialog *dialog)
{
	char *password;

	password = get_selected_item (dialog, COL_PASSWORDS_PASSWORD);
	if (password != NULL) {
		gtk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (dialog),
								  GDK_SELECTION_CLIPBOARD),
					password, -1);
	}
	g_free (password);
}

static void
copy_username (GSimpleAction       *action,
               GVariant            *parameter,
               EphyPasswordsDialog *dialog)
{
	char *username;

	username = get_selected_item (dialog, COL_PASSWORDS_USER);
	if (username != NULL) {
		gtk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (dialog),
								  GDK_SELECTION_CLIPBOARD),
					username, -1);
	}
	g_free (username);
}

static void
update_popup_menu_actions (GActionGroup *action_group,
                           gboolean      only_one_selected_item)
{
	GSimpleAction *copy_password_action;
	GSimpleAction *copy_username_action;

	copy_password_action = g_action_map_lookup_action (action_group, "copy-password");
	copy_username_action = g_action_map_lookup_action (action_group, "copy-username");

	g_simple_action_set_enabled (copy_password_action, only_one_selected_item);
	g_simple_action_set_enabled (copy_username_action, only_one_selected_item);
}

static gboolean
on_passwords_treeview_button_press_event (GtkWidget           *widget,
                                          GdkEventButton      *event,
                                          EphyPasswordsDialog *dialog)
{
	if (event->button == 3) {
		int n;
		GtkMenu *menu;

		n = gtk_tree_selection_count_selected_rows (dialog->tree_selection);
		if (n == 0)
			return FALSE;

		update_popup_menu_actions (G_ACTION_MAP (dialog->action_group), (n == 1));

		menu = gtk_menu_new_from_model (dialog->treeview_popup_menu_model);
		gtk_menu_attach_to_widget (menu, dialog, NULL);
		gtk_menu_popup (menu, NULL, NULL, NULL, NULL, event->button, event->time);
		return TRUE;
	}

	return FALSE;
}

static void
ephy_passwords_dialog_class_init (EphyPasswordsDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->dispose = ephy_passwords_dialog_dispose;

	gtk_widget_class_set_template_from_resource (widget_class,
	                                             "/org/gnome/epiphany/passwords-dialog.ui");

	gtk_widget_class_bind_template_child (widget_class, EphyPasswordsDialog, liststore);
	gtk_widget_class_bind_template_child (widget_class, EphyPasswordsDialog, treemodelfilter);
	gtk_widget_class_bind_template_child (widget_class, EphyPasswordsDialog, treemodelsort);
	gtk_widget_class_bind_template_child (widget_class, EphyPasswordsDialog, passwords_treeview);
	gtk_widget_class_bind_template_child (widget_class, EphyPasswordsDialog, tree_selection);
	gtk_widget_class_bind_template_child (widget_class, EphyPasswordsDialog, show_passwords_button);
	gtk_widget_class_bind_template_child (widget_class, EphyPasswordsDialog, password_column);
	gtk_widget_class_bind_template_child (widget_class, EphyPasswordsDialog, password_renderer);
	gtk_widget_class_bind_template_child (widget_class, EphyPasswordsDialog, treeview_popup_menu_model);

	gtk_widget_class_bind_template_callback (widget_class, on_passwords_treeview_button_press_event);
	gtk_widget_class_bind_template_callback (widget_class, on_treeview_selection_changed);
	gtk_widget_class_bind_template_callback (widget_class, on_search_entry_changed);
}

static void
delete_all_passwords_ready_cb (GObject             *source_object,
                               GAsyncResult        *res,
                               EphyPasswordsDialog *dialog)
{
	secret_service_clear_finish (dialog->ss, res, NULL);
	reload_model (dialog);
}

static void
forget_all (GSimpleAction       *action,
            GVariant            *parameter,
            EphyPasswordsDialog *dialog)
{
	GHashTable *attributes;

	attributes = secret_attributes_build (EPHY_FORM_PASSWORD_SCHEMA, NULL);
	secret_service_clear (dialog->ss,
			      EPHY_FORM_PASSWORD_SCHEMA,
			      attributes,
			      dialog->ss_cancellable,
			      (GAsyncReadyCallback)delete_all_passwords_ready_cb,
			      dialog);
	g_hash_table_unref (attributes);
}

static void
secrets_search_ready_cb (GObject             *source_object,
                         GAsyncResult        *res,
                         EphyPasswordsDialog *dialog)
{
	GList *matches;
	GList *l;

	matches = secret_service_search_finish (dialog->ss, res, NULL);

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

		gtk_list_store_insert_with_values (GTK_LIST_STORE (dialog->liststore),
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
populate_model (EphyPasswordsDialog *dialog)
{
	GHashTable *attributes;

	g_assert (dialog->filled == FALSE);

	attributes = secret_attributes_build (EPHY_FORM_PASSWORD_SCHEMA, NULL);

	secret_service_search (dialog->ss,
			       EPHY_FORM_PASSWORD_SCHEMA,
			       attributes,
			       SECRET_SEARCH_ALL | SECRET_SEARCH_UNLOCK | SECRET_SEARCH_LOAD_SECRETS,
			       dialog->ss_cancellable,
			       (GAsyncReadyCallback)secrets_search_ready_cb,
			       dialog);

	g_hash_table_unref (attributes);
}

static void
secrets_ready_cb (GObject             *source_object,
                  GAsyncResult        *res,
                  EphyPasswordsDialog *dialog)
{
	dialog->ss = secret_service_get_finish (res, NULL);
	populate_model (dialog);
}

static gboolean
row_visible_func (GtkTreeModel        *model,
                  GtkTreeIter         *iter,
                  EphyPasswordsDialog *dialog)
{
	char *username;
	char *host;
	gboolean visible = FALSE;

	if (dialog->search_text == NULL)
		return TRUE;

	gtk_tree_model_get (model, iter,
			    COL_PASSWORDS_HOST, &host,
			    COL_PASSWORDS_USER, &username,
			    -1);

	if (host != NULL && g_strrstr (host, dialog->search_text) != NULL)
		visible = TRUE;
	else if (username != NULL && g_strrstr (username, dialog->search_text) != NULL)
		visible = TRUE;

	g_free (host);
	g_free (username);

	return visible;
}

static GActionGroup *
create_action_group (EphyPasswordsDialog *dialog)
{
	const GActionEntry entries[] = {
		{ "copy-password", copy_password },
		{ "copy-username", copy_username },
		{ "forget", forget },
		{ "forget-all", forget_all },
		{ "show-passwords", show_passwords }
	};

	GSimpleActionGroup *group;

	group = g_simple_action_group_new ();
	g_action_map_add_action_entries (G_ACTION_MAP (group), entries, G_N_ELEMENTS (entries), dialog);

	return G_ACTION_GROUP (group);
}

static void
ephy_passwords_dialog_init (EphyPasswordsDialog *dialog)
{
	gtk_widget_init_template (GTK_WIDGET (dialog));

	gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (dialog->treemodelfilter),
						(GtkTreeModelFilterVisibleFunc)row_visible_func,
						dialog,
						NULL);

	dialog->ss_cancellable = g_cancellable_new ();
	secret_service_get (SECRET_SERVICE_OPEN_SESSION | SECRET_SERVICE_LOAD_COLLECTIONS,
			    dialog->ss_cancellable,
			    (GAsyncReadyCallback)secrets_ready_cb,
			    dialog);

	dialog->action_group = create_action_group (dialog);
	gtk_widget_insert_action_group (dialog, "passwords", dialog->action_group);

	update_selection_actions (G_ACTION_MAP (dialog->action_group), FALSE);
}

EphyPasswordsDialog *
ephy_passwords_dialog_new (void)
{
	return g_object_new (EPHY_TYPE_PASSWORDS_DIALOG,
	                     "use-header-bar", TRUE,
	                     NULL);
}
