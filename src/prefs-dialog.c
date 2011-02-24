/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 200-2003 Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2005 Christian Persch
 *  Copyright © 2010 Igalia S.L.
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

#include "prefs-dialog.h"
#include "ephy-dialog.h"
#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-embed-container.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-utils.h"
#include "ephy-favicon-cache.h"
#include "ephy-session.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-single.h"
#include "ephy-shell.h"
#include "ephy-gui.h"
#include "ephy-langs.h"
#include "ephy-encodings.h"
#include "ephy-debug.h"
#include "ephy-file-chooser.h"
#include "ephy-file-helpers.h"
#include "ephy-tree-model-node.h"
#include "ephy-tree-model-sort.h"
#include "pdm-dialog.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

#define DOWNLOAD_BUTTON_WIDTH	8

static void prefs_dialog_class_init	(PrefsDialogClass *klass);
static void prefs_dialog_init		(PrefsDialog *pd);

#include "languages.h"

enum
{
	COL_LANG_NAME,
	COL_LANG_CODE
};

#define EPHY_PREFS_DIALOG_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_PREFS_DIALOG, PrefsDialogPrivate))

struct PrefsDialogPrivate
{
	GtkTreeView *lang_treeview;
	GtkTreeModel *lang_model;
	EphyDialog *add_lang_dialog;
	GtkWidget *lang_add_button;
	GtkWidget *lang_remove_button;
	GtkWidget *lang_up_button;
	GtkWidget *lang_down_button;
	GHashTable *iso_639_table;
	GHashTable *iso_3166_table;
};

G_DEFINE_TYPE (PrefsDialog, prefs_dialog, EPHY_TYPE_DIALOG)

static void
prefs_dialog_finalize (GObject *object)
{
	PrefsDialog *dialog = EPHY_PREFS_DIALOG (object);
	PrefsDialogPrivate *priv = dialog->priv;

	if (priv->add_lang_dialog != NULL)
	{
		EphyDialog **add_lang_dialog = &priv->add_lang_dialog;

		g_object_remove_weak_pointer
				(G_OBJECT (priv->add_lang_dialog),
				 (gpointer *) add_lang_dialog);
		g_object_unref (priv->add_lang_dialog);
	}

	g_hash_table_destroy (priv->iso_639_table);
	g_hash_table_destroy (priv->iso_3166_table);

	G_OBJECT_CLASS (prefs_dialog_parent_class)->finalize (object);
}

static void
prefs_dialog_class_init (PrefsDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = prefs_dialog_finalize;

	g_type_class_add_private (object_class, sizeof(PrefsDialogPrivate));
}

static void
prefs_dialog_show_help (EphyDialog *dialog)
{
	GtkWidget *window, *notebook;
	int id;

	static const char help_preferences[][28] = {
		"general-preferences",
		"fonts-and-style-preferences",
		"privacy-preferences",
		"language-preferences"
	};

	ephy_dialog_get_controls (dialog,
				  "prefs_dialog", &window,
				  "prefs_notebook", &notebook,
				  NULL);

	id = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
	id = CLAMP (id, 0, 3);

	ephy_gui_help (window, help_preferences[id]);
}

static void
css_edit_button_clicked_cb (GtkWidget *button,
			    PrefsDialog *pd)
{
	GFile *css_file;

	css_file = g_file_new_for_path (g_build_filename (ephy_dot_dir (),
							  USER_STYLESHEET_FILENAME,
							  NULL));

	ephy_file_launch_handler ("text/plain", css_file,
				  gtk_get_current_event_time ());
	g_object_unref (css_file);
}

static gboolean
combo_get_mapping (GValue *value,
		   GVariant *variant,
		   gpointer user_data)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean valid = FALSE;
	const char *settings_name;
	int i;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (user_data));
	valid = gtk_tree_model_get_iter_first (model, &iter);
	settings_name = g_variant_get_string (variant, NULL);
	i = 0;

	while (valid)
	{
		char *item_name;
		gtk_tree_model_get (model, &iter, 1, &item_name, -1);

		if (g_strcmp0 (item_name, settings_name) == 0)
		{
			g_value_set_int (value, i);
			break;
		}

		i++;
		valid = gtk_tree_model_iter_next (model, &iter);
		g_free (item_name);
	}

	return TRUE;
}

static GVariant *
combo_set_mapping (const GValue *value,
		   const GVariantType *expected_type,
		   gpointer user_data)
{
	GVariant *variant = NULL;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean valid = FALSE;
	int n;

	n = g_value_get_int (value);
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (user_data));
	valid = gtk_tree_model_iter_nth_child (model, &iter, NULL, n);

	if (valid)
	{
		char *item_name;
		gtk_tree_model_get (model, &iter, 1, &item_name, -1);

		variant = g_variant_new_string (item_name);

		g_free (item_name);
	}

	return variant;
}

static void
create_node_combo (EphyDialog *dialog,
		   EphyEncodings *encodings,
		   EphyNode *node,
		   const char *default_value)
{
	EphyTreeModelNode *nodemodel;
	GtkTreeModel *sortmodel;
	GtkComboBox *combo;
	GtkCellRenderer *renderer;
	char *code;
	int title_col, data_col;

	code = g_settings_get_string (EPHY_SETTINGS_WEB,
				      EPHY_PREFS_WEB_DEFAULT_ENCODING);
	if (code == NULL || ephy_encodings_get_node (encodings, code, FALSE) == NULL)
	{
		/* safe default */
		g_settings_set_string (EPHY_SETTINGS_WEB,
				       EPHY_PREFS_WEB_DEFAULT_ENCODING,
				       default_value);
	}
	g_free (code);

	combo = GTK_COMBO_BOX (ephy_dialog_get_control (dialog,
							"default_encoding_combo"));

	nodemodel = ephy_tree_model_node_new (node);

	title_col = ephy_tree_model_node_add_prop_column
			(nodemodel, G_TYPE_STRING, EPHY_NODE_ENCODING_PROP_TITLE_ELIDED);
	data_col = ephy_tree_model_node_add_prop_column
			(nodemodel, G_TYPE_STRING, EPHY_NODE_ENCODING_PROP_ENCODING);

	sortmodel = ephy_tree_model_sort_new (GTK_TREE_MODEL (nodemodel));

	gtk_tree_sortable_set_sort_column_id
		(GTK_TREE_SORTABLE (sortmodel), title_col, GTK_SORT_ASCENDING);

	gtk_combo_box_set_model (combo, GTK_TREE_MODEL (sortmodel));

        renderer = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer,
                                        "text", title_col,
                                        NULL);

	g_settings_bind_with_mapping (EPHY_SETTINGS_WEB,
				      EPHY_PREFS_WEB_DEFAULT_ENCODING,
				      combo, "active",
				      G_SETTINGS_BIND_DEFAULT,
				      combo_get_mapping,
				      combo_set_mapping,
				      combo,
				      NULL);

	g_object_unref (nodemodel);
	g_object_unref (sortmodel);
}

static void
language_editor_add (PrefsDialog *pd,
		     const char *code,
		     const char *desc)
{
	GtkTreeIter iter;

	g_return_if_fail (code != NULL && desc != NULL);

	if (gtk_tree_model_get_iter_first (pd->priv->lang_model, &iter))
	{
		do
		{
			char *c;

			gtk_tree_model_get (pd->priv->lang_model, &iter,
					    COL_LANG_CODE, &c,
					    -1);

			if (strcmp (code, c) == 0)
			{
				g_free (c);

				/* already in list, don't allow a duplicate */
				return;
			}
			g_free (c);
		}
		while (gtk_tree_model_iter_next (pd->priv->lang_model, &iter));
	}

	gtk_list_store_append (GTK_LIST_STORE (pd->priv->lang_model), &iter);
	
	gtk_list_store_set (GTK_LIST_STORE (pd->priv->lang_model), &iter,
			    COL_LANG_NAME, desc,
			    COL_LANG_CODE, code,
			    -1);
}

static void
language_editor_update_pref (PrefsDialog *pd)
{
	GtkTreeIter iter;
	GVariantBuilder builder;

	if (gtk_tree_model_get_iter_first (pd->priv->lang_model, &iter))
	{
		g_variant_builder_init (&builder, G_VARIANT_TYPE_STRING_ARRAY);

		do
		{
			char *code;
		
			gtk_tree_model_get (pd->priv->lang_model, &iter,
					    COL_LANG_CODE, &code,
					    -1);
			g_variant_builder_add (&builder, "s", code);
			g_free (code);
		}
		while (gtk_tree_model_iter_next (pd->priv->lang_model, &iter));

		g_settings_set (EPHY_SETTINGS_WEB,
				EPHY_PREFS_WEB_LANGUAGE,
				"as", &builder);
	}
}

static void
language_editor_update_buttons (PrefsDialog *dialog)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	gboolean can_remove = FALSE, can_move_up = FALSE, can_move_down = FALSE;
	int selected;

	selection = gtk_tree_view_get_selection (dialog->priv->lang_treeview);

	if (gtk_tree_selection_get_selected (selection, &model, &iter))
	{
		path = gtk_tree_model_get_path (model, &iter);
	
		selected = gtk_tree_path_get_indices (path)[0];

		can_remove = TRUE;
		can_move_up = selected > 0;
		can_move_down = 
			selected < gtk_tree_model_iter_n_children (model, NULL) - 1;

		gtk_tree_path_free (path);
	}

	gtk_widget_set_sensitive (dialog->priv->lang_remove_button, can_remove);
	gtk_widget_set_sensitive (dialog->priv->lang_up_button, can_move_up);
	gtk_widget_set_sensitive (dialog->priv->lang_down_button, can_move_down);
}

static void
add_lang_dialog_selection_changed (GtkTreeSelection *selection,
				   EphyDialog *dialog)
{
	GtkWidget *button;
	int n_selected;

	button = ephy_dialog_get_control (dialog, "add_button");

	n_selected = gtk_tree_selection_count_selected_rows (selection);
	gtk_widget_set_sensitive (button, n_selected > 0);
}

static void
add_lang_dialog_response_cb (GtkWidget *widget,
			     int response,
			     PrefsDialog *pd)
{
	EphyDialog *dialog = pd->priv->add_lang_dialog;
	GtkTreeView *treeview;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GList *rows, *r;

	g_return_if_fail (dialog != NULL);

	if (response == GTK_RESPONSE_ACCEPT)
	{
		treeview = GTK_TREE_VIEW (ephy_dialog_get_control
				(dialog, "languages_treeview"));
		selection = gtk_tree_view_get_selection (treeview);

		rows = gtk_tree_selection_get_selected_rows (selection, &model);

		for (r = rows; r != NULL; r = r->next)
		{
			GtkTreePath *path = (GtkTreePath *) r->data;

			if (gtk_tree_model_get_iter (model, &iter, path))
			{
				char *code, *desc;
				
				gtk_tree_model_get (model, &iter,
						    COL_LANG_NAME, &desc,
						    COL_LANG_CODE, &code,
						    -1);

				language_editor_add (pd, code, desc);
		
				g_free (desc);
				g_free (code);
			}
		}			

		g_list_foreach (rows, (GFunc) gtk_tree_path_free, NULL);
		g_list_free (rows);

		language_editor_update_pref (pd);
		language_editor_update_buttons (pd);
	}

	g_object_unref (dialog);
}

static char *
get_name_for_lang_code (PrefsDialog *pd,
			const char *code)
{
	char **str;
	char *name;
	const char *langname, *localename;
	int len;

	str = g_strsplit (code, "-", -1);
	len = g_strv_length (str);
	g_return_val_if_fail (len != 0, NULL);

	langname = (const char *) g_hash_table_lookup (pd->priv->iso_639_table, str[0]);

	if (len == 1 && langname != NULL)
	{
		name = g_strdup (dgettext (ISO_639_DOMAIN, langname));
	}
	else if (len == 2 && langname != NULL)
	{
		localename = (const char *) g_hash_table_lookup (pd->priv->iso_3166_table, str[1]);

		if (localename != NULL)
		{
			/* Translators: the first %s is the language name, and the
			 * second %s is the locale name. Example:
			 * "French (France)"
			 */
			name = g_strdup_printf (C_("language", "%s (%s)"),
						dgettext (ISO_639_DOMAIN, langname),
						dgettext (ISO_3166_DOMAIN, localename));
		}
		else
		{
			name = g_strdup_printf (C_("language", "%s (%s)"),
						dgettext (ISO_639_DOMAIN, langname), str[1]);
		}
	}
	else
	{
		/* Translators: this refers to a user-define language code
		 * (one which isn't in our built-in list).
		 */
		name = g_strdup_printf (C_("language", "User defined (%s)"), code);
	}

	g_strfreev (str);

	return name;
}

static void
add_system_language_entry (GtkListStore *store)
{
	GtkTreeIter iter;
	char **sys_langs;
	char *system, *text;
	int n_sys_langs;

	sys_langs = ephy_langs_get_languages ();
	n_sys_langs = g_strv_length (sys_langs);

	system = g_strjoinv (", ", sys_langs);

	text = g_strdup_printf
		(ngettext ("System language (%s)",
			   "System languages (%s)", n_sys_langs), system);

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter,
			    COL_LANG_NAME, text,
			    COL_LANG_CODE, "system",
			    -1);

	g_strfreev (sys_langs);
	g_free (system);
	g_free (text);
}

static EphyDialog *
setup_add_language_dialog (PrefsDialog *pd)
{
	EphyDialog *dialog;
	GtkWidget *window, *parent;
	GtkListStore *store;
	GtkTreeModel *sortmodel;
	GtkTreeView *treeview;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	int i;

	parent = ephy_dialog_get_control (EPHY_DIALOG (pd), "prefs_dialog");

	dialog =  EPHY_DIALOG (g_object_new (EPHY_TYPE_DIALOG,
					     "parent-window", parent,
					     "default-width", 260,
					     "default-height", 230,
					     NULL));

	ephy_dialog_construct (dialog, 
			       ephy_file ("prefs-dialog.ui"),
			       "add_language_dialog",
			       NULL);

	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

	for (i = 0; i < G_N_ELEMENTS (languages); i++)
	{
		const char *code = languages[i];
		char *name;

		name = get_name_for_lang_code (pd, code);
		
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    COL_LANG_NAME, name,
				    COL_LANG_CODE, code,
				    -1);
		g_free (name);
	}

	add_system_language_entry (store);

	sortmodel = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (store));
	gtk_tree_sortable_set_sort_column_id
		(GTK_TREE_SORTABLE (sortmodel), COL_LANG_NAME, GTK_SORT_ASCENDING);

	ephy_dialog_get_controls (dialog,
				  "languages_treeview", &treeview,
				  "add_language_dialog", &window,
				  NULL);

	gtk_window_group_add_window (gtk_window_get_group (GTK_WINDOW (parent)),
				     GTK_WINDOW (window));
	gtk_window_set_modal (GTK_WINDOW (window), TRUE);

	gtk_tree_view_set_reorderable (GTK_TREE_VIEW (treeview), FALSE);

	gtk_tree_view_set_model (treeview, sortmodel);

	gtk_tree_view_set_headers_visible (treeview, FALSE);

	renderer = gtk_cell_renderer_text_new ();

	gtk_tree_view_insert_column_with_attributes (treeview,
						     0, "Language",
						     renderer,
						     "text", 0,
						     NULL);
	column = gtk_tree_view_get_column (treeview, 0);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_sort_column_id (column, COL_LANG_NAME);

	selection = gtk_tree_view_get_selection (treeview);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
	
	add_lang_dialog_selection_changed (GTK_TREE_SELECTION (selection), dialog);
	g_signal_connect (selection, "changed",
			  G_CALLBACK (add_lang_dialog_selection_changed), dialog);

	g_signal_connect (window, "response",
			  G_CALLBACK (add_lang_dialog_response_cb), pd);

	g_object_unref (store);
	g_object_unref (sortmodel);

	return dialog;
}

static void
language_editor_add_button_clicked_cb (GtkWidget *button,
				       PrefsDialog *pd)
{
	if (pd->priv->add_lang_dialog == NULL)
	{
		EphyDialog **add_lang_dialog;

		pd->priv->add_lang_dialog = setup_add_language_dialog (pd);

		add_lang_dialog = &pd->priv->add_lang_dialog;

		g_object_add_weak_pointer
			(G_OBJECT (pd->priv->add_lang_dialog),
			(gpointer *) add_lang_dialog);
	}

	ephy_dialog_show (pd->priv->add_lang_dialog);
}

static void
language_editor_remove_button_clicked_cb (GtkWidget *button,
					  PrefsDialog *pd)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;

	selection = gtk_tree_view_get_selection (pd->priv->lang_treeview);

	if (gtk_tree_selection_get_selected (selection, &model, &iter))
	{
		gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
	}

	language_editor_update_pref (pd);
	language_editor_update_buttons (pd);
}

static void
language_editor_up_button_clicked_cb (GtkWidget *button,
				      PrefsDialog *pd)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter, iter_prev;
	GtkTreePath *path;

	selection = gtk_tree_view_get_selection (pd->priv->lang_treeview);

	if (gtk_tree_selection_get_selected (selection, &model, &iter))
	{
		path = gtk_tree_model_get_path (model, &iter);

		if (!gtk_tree_path_prev (path))
		{
			gtk_tree_path_free (path);
			return;
		}

		gtk_tree_model_get_iter (model, &iter_prev, path);

		gtk_list_store_swap (GTK_LIST_STORE (model), &iter_prev, &iter);

		gtk_tree_path_free (path);
	}

	language_editor_update_pref (pd);
	language_editor_update_buttons (pd);
}

static void
language_editor_down_button_clicked_cb (GtkWidget *button,
					PrefsDialog *pd)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter, iter_next;
	GtkTreePath *path;


	selection = gtk_tree_view_get_selection (pd->priv->lang_treeview);

	if (gtk_tree_selection_get_selected (selection, &model, &iter))
	{
		path = gtk_tree_model_get_path (model, &iter);

		gtk_tree_path_next (path);

		gtk_tree_model_get_iter (model, &iter_next, path);

		gtk_list_store_swap (GTK_LIST_STORE (model), &iter, &iter_next);

		gtk_tree_path_free (path);
	}

	language_editor_update_pref (pd);
	language_editor_update_buttons (pd);
}

static void
language_editor_treeview_drag_end_cb (GtkWidget *widget,
				      GdkDragContext *context,
				      PrefsDialog *dialog)
{
	language_editor_update_pref (dialog);
	language_editor_update_buttons (dialog);
}

static void
language_editor_selection_changed_cb (GtkTreeSelection *selection,
				      PrefsDialog *dialog)
{
	language_editor_update_buttons (dialog);
}

static void
create_language_section (EphyDialog *dialog)
{
	PrefsDialog *pd = EPHY_PREFS_DIALOG (dialog);
	GtkListStore *store;
	GtkTreeView *treeview;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;
	char **list = NULL;
	int i;

	pd->priv->iso_639_table = ephy_langs_iso_639_table ();
	pd->priv->iso_3166_table = ephy_langs_iso_3166_table ();

	ephy_dialog_get_controls
		(dialog,
		 "lang_treeview", &treeview,
		 "lang_add_button", &pd->priv->lang_add_button,
		 "lang_remove_button", &pd->priv->lang_remove_button,
		 "lang_up_button", &pd->priv->lang_up_button,
		 "lang_down_button", &pd->priv->lang_down_button,
		 NULL);

	g_signal_connect (pd->priv->lang_add_button, "clicked",
			  G_CALLBACK (language_editor_add_button_clicked_cb), dialog);
	g_signal_connect (pd->priv->lang_remove_button, "clicked",
			  G_CALLBACK (language_editor_remove_button_clicked_cb), dialog);
	g_signal_connect (pd->priv->lang_up_button, "clicked",
			  G_CALLBACK (language_editor_up_button_clicked_cb), dialog);
	g_signal_connect (pd->priv->lang_down_button, "clicked",
			  G_CALLBACK (language_editor_down_button_clicked_cb), dialog);

	/* setup the languages treeview */
	pd->priv->lang_treeview = treeview;

	gtk_tree_view_set_reorderable (treeview, TRUE);
	gtk_tree_view_set_headers_visible (treeview, FALSE);

	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

	pd->priv->lang_model = GTK_TREE_MODEL (store);
	gtk_tree_view_set_model (treeview, pd->priv->lang_model);

	renderer = gtk_cell_renderer_text_new ();

	gtk_tree_view_insert_column_with_attributes (treeview,
						     0, _("Language"),
						     renderer,
						     "text", 0,
						     NULL);
	column = gtk_tree_view_get_column (treeview, 0);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_sort_column_id (column, COL_LANG_NAME);

	selection = gtk_tree_view_get_selection (treeview);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

	/* Connect treeview signals */
	g_signal_connect (G_OBJECT (treeview), "drag_end",
			  G_CALLBACK (language_editor_treeview_drag_end_cb), pd);
	g_signal_connect (G_OBJECT (selection), "changed",
			  G_CALLBACK (language_editor_selection_changed_cb), pd);

	list = g_settings_get_strv (EPHY_SETTINGS_WEB,
				    EPHY_PREFS_WEB_LANGUAGE);

	/* Fill languages editor */
	for (i = 0; list[i]; i++)
	{
		const char *code = (const char *) list[i];

		if (strcmp (code, "system") == 0)
		{
			add_system_language_entry (store);
		}
		else if (code[0] != '\0')
		{
			char *text;

			text = get_name_for_lang_code (pd, code);
			language_editor_add (pd, code, text);
			g_free (text);
		}
	}
	g_object_unref (store);

	language_editor_update_buttons (pd);
	g_strfreev (list);

	/* Lockdown if key is not writable */
	g_settings_bind_writable (EPHY_SETTINGS_WEB,
				  EPHY_PREFS_WEB_LANGUAGE,
				  pd->priv->lang_add_button, "sensitive", FALSE);
	g_settings_bind_writable (EPHY_SETTINGS_WEB,
				  EPHY_PREFS_WEB_LANGUAGE,
				  pd->priv->lang_remove_button, "sensitive", FALSE);
	g_settings_bind_writable (EPHY_SETTINGS_WEB,
				  EPHY_PREFS_WEB_LANGUAGE,
				  pd->priv->lang_up_button, "sensitive", FALSE);
	g_settings_bind_writable (EPHY_SETTINGS_WEB,
				  EPHY_PREFS_WEB_LANGUAGE,
				  pd->priv->lang_down_button, "sensitive", FALSE);
	g_settings_bind_writable (EPHY_SETTINGS_WEB,
				  EPHY_PREFS_WEB_LANGUAGE,
				  pd->priv->lang_treeview, "sensitive", FALSE);
}

static void
download_path_changed_cb (GtkFileChooser *button)
{
	char *dir, *downloads_dir, *desktop_dir;

	/* FIXME: use _uri variant when we support downloading 
	 * to gnome-vfs remote locations
	 */
	dir = gtk_file_chooser_get_filename (button);
	if (dir == NULL) return;

	downloads_dir = ephy_file_downloads_dir ();
	desktop_dir = ephy_file_desktop_dir ();
	g_return_if_fail (downloads_dir != NULL && desktop_dir != NULL);

	/* Check if the dir matches the default downloads_dir or desktop_dir to
	 * store the english name instead of the localized one reported by the
	 * two ephy_file_ functions. */
	if (strcmp (dir, downloads_dir) == 0)
	{
		g_settings_set_string (EPHY_SETTINGS_STATE,
				       EPHY_PREFS_STATE_DOWNLOAD_DIR,
				       _("Downloads"));
	}
	else if (strcmp (dir, desktop_dir) == 0)
	{
		g_settings_set_string (EPHY_SETTINGS_STATE,
				       EPHY_PREFS_STATE_DOWNLOAD_DIR,
				       _("Desktop"));
	}
	else
	{
		g_settings_set_string (EPHY_SETTINGS_STATE,
				       EPHY_PREFS_STATE_DOWNLOAD_DIR, dir);
	}

	g_free (dir);
	g_free (downloads_dir);
	g_free (desktop_dir);
}

static void
create_download_path_button (EphyDialog *dialog)
{
	GtkWidget *parent, *hbox, *label, *button;
	EphyFileChooser *fc;
	char *dir;

	dir = ephy_file_get_downloads_dir ();

	ephy_dialog_get_controls (dialog,
				  "download_button_hbox", &hbox,
				  "download_button_label", &label,
				  "prefs_dialog", &parent,
				  NULL);

	fc = ephy_file_chooser_new (_("Select a Directory"),
				    parent,
				    GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
				    NULL, EPHY_FILE_FILTER_NONE);

	/* Unset the destroy-with-parent, since gtkfilechooserbutton doesn't
	 * expect this */
	gtk_window_set_destroy_with_parent (GTK_WINDOW (fc), FALSE);

	button = gtk_file_chooser_button_new_with_dialog (GTK_WIDGET (fc));
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (button), dir);
	gtk_file_chooser_button_set_width_chars (GTK_FILE_CHOOSER_BUTTON (button),
						 DOWNLOAD_BUTTON_WIDTH);
	g_signal_connect (button, "current-folder-changed",
			  G_CALLBACK (download_path_changed_cb), dialog);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), button);
	gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
	gtk_widget_show (button);

	g_settings_bind_writable (EPHY_SETTINGS_STATE,
				  EPHY_PREFS_STATE_DOWNLOAD_DIR,
				  button, "sensitive", FALSE);
	g_free (dir);
}

static void
prefs_dialog_response_cb (GtkDialog *widget,
			  int response,
			  EphyDialog *dialog)
{
	if (response == GTK_RESPONSE_HELP)
	{
		prefs_dialog_show_help (dialog);
		return;
	}
		
	g_object_unref (dialog);
}

static void
clear_cache_button_clicked_cb (GtkWidget *button,
			       PrefsDialog *dialog)
{
	GtkWidget *parent;

	parent = ephy_dialog_get_control (EPHY_DIALOG (dialog),
					  "prefs_dialog");
	pdm_dialog_show_clear_all_dialog (EPHY_DIALOG (dialog), parent,
					  CLEAR_ALL_CACHE);
}

static void
homepage_current_button_clicked_cb (GtkWidget *button,
				    EphyDialog *dialog)
{
	EphySession *session;
	EphyWindow *window;
	EphyEmbed *embed;
	EphyWebView *view;

	session = EPHY_SESSION (ephy_shell_get_session (ephy_shell_get_default ()));
	window = ephy_session_get_active_window (session);

	/* can't do anything in this case */
	if (window == NULL) return;

	embed = ephy_embed_container_get_active_child 
          (EPHY_EMBED_CONTAINER (window));
	g_return_if_fail (embed != NULL);

	view = ephy_embed_get_web_view (embed);

	g_settings_set_string (EPHY_SETTINGS_MAIN, EPHY_PREFS_HOMEPAGE_URL,
			       ephy_web_view_get_address (view));
}

static void
homepage_blank_button_clicked_cb (GtkWidget *button,
				  EphyDialog *dialog)
{
	g_settings_set_string (EPHY_SETTINGS_MAIN,
			       EPHY_PREFS_HOMEPAGE_URL, "");
}

static gboolean
cookies_get_mapping (GValue *value,
		     GVariant *variant,
		     gpointer user_data)
{
	const char *setting;
	const char *name;

	setting = g_variant_get_string (variant, NULL);
	name = gtk_buildable_get_name (GTK_BUILDABLE (user_data));

	/* If the button name matches the setting, it should be active. */
	if (g_strcmp0 (name, setting) == 0)
		g_value_set_boolean (value, TRUE);

	return TRUE;
}

static GVariant *
cookies_set_mapping (const GValue *value,
		     const GVariantType *expected_type,
		     gpointer user_data)
{
	GVariant *variant = NULL;
	const char *name;

	/* Don't act unless the button has been activated (turned ON). */
	if (!g_value_get_boolean (value))
		return NULL;

	name = gtk_buildable_get_name (GTK_BUILDABLE (user_data));
	variant = g_variant_new_string (name);

	return variant;
}

typedef struct
{
	char *obj;
	char *prop;
	char *schema;
	char *key;
	GSettingsBindFlags flags;
	GSettingsBindGetMapping get_mapping;
	GSettingsBindSetMapping set_mapping;
} PrefsDialogPreference;

static const PrefsDialogPreference preferences[] =
{
	{ "homepage_entry", "text",
	  EPHY_PREFS_SCHEMA, EPHY_PREFS_HOMEPAGE_URL,
	  G_SETTINGS_BIND_DEFAULT, NULL, NULL },
	{ "automatic_downloads_checkbutton", "active",
	  EPHY_PREFS_SCHEMA, EPHY_PREFS_AUTO_DOWNLOADS,
	  G_SETTINGS_BIND_DEFAULT, NULL, NULL },
	{ "remember_passwords_checkbutton", "active",
	  EPHY_PREFS_SCHEMA, EPHY_PREFS_REMEMBER_PASSWORDS,
	  G_SETTINGS_BIND_DEFAULT, NULL, NULL },

	{ "disk_cache_spinbutton", "value",
	  EPHY_PREFS_WEB_SCHEMA, EPHY_PREFS_CACHE_SIZE,
	  G_SETTINGS_BIND_DEFAULT, NULL, NULL },
	{ "use_gnome_fonts_checkbutton", "active",
	  EPHY_PREFS_WEB_SCHEMA, EPHY_PREFS_WEB_USE_GNOME_FONTS,
	  G_SETTINGS_BIND_DEFAULT, NULL, NULL },
	{ "min_size_spinbutton", "value",
	  EPHY_PREFS_WEB_SCHEMA, EPHY_PREFS_WEB_FONT_MIN_SIZE,
	  G_SETTINGS_BIND_DEFAULT, NULL, NULL },
	{ "popups_allow_checkbutton", "active",
	  EPHY_PREFS_WEB_SCHEMA, EPHY_PREFS_WEB_ENABLE_POPUPS,
	  G_SETTINGS_BIND_DEFAULT, NULL, NULL },
	{ "enable_plugins_checkbutton", "active",
	  EPHY_PREFS_WEB_SCHEMA, EPHY_PREFS_WEB_ENABLE_PLUGINS,
	  G_SETTINGS_BIND_DEFAULT, NULL, NULL },
	{ "enable_javascript_checkbutton", "active",
	  EPHY_PREFS_WEB_SCHEMA, EPHY_PREFS_WEB_ENABLE_JAVASCRIPT,
	  G_SETTINGS_BIND_DEFAULT, NULL, NULL },
	{ "css_checkbox", "active",
	  EPHY_PREFS_WEB_SCHEMA, EPHY_PREFS_WEB_ENABLE_USER_CSS,
	  G_SETTINGS_BIND_DEFAULT, NULL, NULL },
	{ "css_edit_button", "sensitive",
	  EPHY_PREFS_WEB_SCHEMA, EPHY_PREFS_WEB_ENABLE_USER_CSS,
	  G_SETTINGS_BIND_GET, NULL, NULL },

	/* Font buttons */
	{ "custom_fonts_table", "sensitive",
	  EPHY_PREFS_WEB_SCHEMA, EPHY_PREFS_WEB_USE_GNOME_FONTS,
	  G_SETTINGS_BIND_GET | G_SETTINGS_BIND_INVERT_BOOLEAN, NULL, NULL },

	{ "sans_fontbutton", "font-name",
	  EPHY_PREFS_WEB_SCHEMA, EPHY_PREFS_WEB_SANS_SERIF_FONT,
	  G_SETTINGS_BIND_DEFAULT, NULL, NULL },
	{ "serif_fontbutton", "font-name",
	  EPHY_PREFS_WEB_SCHEMA, EPHY_PREFS_WEB_SERIF_FONT,
	  G_SETTINGS_BIND_DEFAULT, NULL, NULL },
	{ "mono_fontbutton", "font-name",
	  EPHY_PREFS_WEB_SCHEMA, EPHY_PREFS_WEB_MONOSPACE_FONT,
	  G_SETTINGS_BIND_DEFAULT, NULL, NULL },

	/* Has mapping */
	{ "always", "active",
	  EPHY_PREFS_WEB_SCHEMA, EPHY_PREFS_WEB_COOKIES_POLICY,
	  G_SETTINGS_BIND_DEFAULT, cookies_get_mapping, cookies_set_mapping },
	{ "no-third-party", "active",
	  EPHY_PREFS_WEB_SCHEMA, EPHY_PREFS_WEB_COOKIES_POLICY,
	  G_SETTINGS_BIND_DEFAULT, cookies_get_mapping, cookies_set_mapping },
	{ "never", "active",
	  EPHY_PREFS_WEB_SCHEMA, EPHY_PREFS_WEB_COOKIES_POLICY,
	  G_SETTINGS_BIND_DEFAULT, cookies_get_mapping, cookies_set_mapping },
};

static void
prefs_dialog_init (PrefsDialog *pd)
{
	EphyDialog *dialog = EPHY_DIALOG (pd);
	EphyEncodings *encodings;
	GtkWidget *window, *curr_button, *blank_button;
	GtkWidget *clear_cache_button;
	GtkWidget *css_checkbox, *css_edit_button;
	int i;

	pd->priv = EPHY_PREFS_DIALOG_GET_PRIVATE (pd);

	ephy_dialog_construct (dialog,
			       ephy_file ("prefs-dialog.ui"),
			       "prefs_dialog",
			       NULL);

	for (i = 0; i < G_N_ELEMENTS (preferences); i++)
	{
		PrefsDialogPreference pref;
		GtkWidget *widget;
		GSettings *settings;

		pref = preferences[i];
		settings = ephy_settings_get (pref.schema);
		widget = ephy_dialog_get_control (dialog, pref.obj);

		if (pref.set_mapping != NULL || pref.get_mapping != NULL)
			g_settings_bind_with_mapping (settings, pref.key,
						      widget, pref.prop,
						      pref.flags,
						      pref.get_mapping,
						      pref.set_mapping,
						      widget, NULL);
		else
			g_settings_bind (settings, pref.key,
					 widget, pref.prop,
					 pref.flags);
	}

	ephy_dialog_get_controls (dialog,
				  "prefs_dialog", &window,
				  "homepage_current_button", &curr_button,
				  "homepage_blank_button", &blank_button,
				  "css_checkbox", &css_checkbox,
				  "css_edit_button", &css_edit_button,
				  "clear_cache_button", &clear_cache_button,
				  NULL);

	ephy_gui_ensure_window_group (GTK_WINDOW (window));

	g_signal_connect (window, "response",
			  G_CALLBACK (prefs_dialog_response_cb), dialog);

	g_signal_connect (curr_button, "clicked",
			  G_CALLBACK (homepage_current_button_clicked_cb),
			  dialog);
	g_signal_connect (blank_button, "clicked",
			  G_CALLBACK (homepage_blank_button_clicked_cb),
			  dialog);

	/* set homepage button sensitivity */
	g_settings_bind_writable (EPHY_SETTINGS_MAIN,
				  EPHY_PREFS_HOMEPAGE_URL,
				  curr_button, "sensitive", FALSE);
	g_settings_bind_writable (EPHY_SETTINGS_MAIN,
				  EPHY_PREFS_HOMEPAGE_URL,
				  blank_button, "sensitive", FALSE);

	g_signal_connect (css_edit_button, "clicked",
			  G_CALLBACK (css_edit_button_clicked_cb), dialog);

	g_signal_connect (clear_cache_button, "clicked",
			  G_CALLBACK (clear_cache_button_clicked_cb), dialog);

	encodings = EPHY_ENCODINGS (ephy_embed_shell_get_encodings
					(EPHY_EMBED_SHELL (ephy_shell)));

	create_node_combo (dialog, encodings,
			   ephy_encodings_get_all (encodings), "ISO-8859-1");

	create_language_section	(dialog);

	create_download_path_button (dialog);
}
