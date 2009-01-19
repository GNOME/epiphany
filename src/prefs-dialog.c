/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 200-2003 Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2005 Christian Persch
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
#include "ephy-embed-container.h"
#include "ephy-embed-shell.h"
#include "ephy-favicon-cache.h"
#include "ephy-session.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-single.h"
#include "ephy-shell.h"
#include "ephy-gui.h"
#include "eel-gconf-extensions.h"
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

static const char * const cookies_accept_enum [] =
{
	"anywhere", "current site", "nowhere"
};

enum
{
	WINDOW_PROP,
	NOTEBOOK_PROP,

	/* General */
	HOMEPAGE_ENTRY_PROP,
	HOMEPAGE_CURRENT_PROP,
	HOMEPAGE_BLANK_PROP,
	AUTO_OPEN_PROP,
	DOWNLOAD_PATH_HBOX_PROP,
	DOWNLOAD_PATH_LABEL_PROP,
	WEB_INSPECTOR_BUTTON_PROP,

	/* Fonts and Colors */
	USE_FONTS_PROP,
	MIN_SIZE_PROP,
	USE_COLORS_PROP,
	SMOOTHSCROLL_PROP,
	CSS_CONTAINER_PROP,
	CSS_CHECKBOX_PROP,
	CSS_EDIT_BOX_PROP,
	CSS_EDIT_BUTTON_PROP,

	/* Privacy */
	ALLOW_POPUPS_PROP,
	ALLOW_JAVA_PROP,
	ALLOW_JS_PROP,
	ACCEPT_COOKIES_PROP,
	REMEMBER_PASSWORDS_PROP,
	DISK_CACHE_PROP,
	CLEAR_CACHE_BUTTON_PROP,

	/* Language */
	AUTO_ENCODING_PROP,
	DEFAULT_ENCODING_PROP,
	DEFAULT_ENCODING_LABEL_PROP,
	AUTO_ENCODING_LABEL_PROP,
	LANGUAGE_ADD_BUTTON_PROP,
	LANGUAGE_REMOVE_BUTTON_PROP,
	LANGUAGE_UP_BUTTON_PROP,
	LANGUAGE_DOWN_BUTTON_PROP,
	LANGUAGE_TREEVIEW_PROP
};

static const
EphyDialogProperty properties [] =
{
	{ "prefs_dialog",	NULL, PT_NORMAL, 0},
	{ "prefs_notebook",	NULL, PT_NORMAL, 0},

	/* General */
	{ "homepage_entry",			CONF_GENERAL_HOMEPAGE,	    PT_AUTOAPPLY,  G_TYPE_STRING },
	{ "homepage_current_button",		NULL,			    PT_NORMAL,	   0 },
	{ "homepage_blank_button",		NULL,			    PT_NORMAL,	   0 },
	{ "automatic_downloads_checkbutton",	CONF_AUTO_DOWNLOADS,        PT_AUTOAPPLY,  0 },
	{ "download_button_hbox",		NULL,			    PT_NORMAL,	   0 },
	{ "download_button_label",		NULL,			    PT_NORMAL,	   0 },
	{ "web_inspector_checkbutton",	        CONF_WEB_INSPECTOR_ENABLED, PT_AUTOAPPLY,  0 },

	/* Fonts and Colors */
	{ "use_fonts_checkbutton",	CONF_RENDERING_USE_OWN_FONTS,	PT_AUTOAPPLY | PT_INVERTED,	0 },
	{ "min_size_spinbutton",	CONF_RENDERING_FONT_MIN_SIZE,	PT_AUTOAPPLY,	0 },
	{ "use_colors_checkbutton",	CONF_RENDERING_USE_OWN_COLORS,	PT_AUTOAPPLY | PT_INVERTED,	0 },
	{ "smoothscroll_button",	CONF_DISPLAY_SMOOTHSCROLL,	PT_AUTOAPPLY,	0 },
	{ "css_container",		NULL,				PT_NORMAL,	0 },
	{ "css_checkbox",		CONF_USER_CSS_ENABLED,		PT_AUTOAPPLY,	0 },
	{ "css_edit_box",		NULL,				PT_NORMAL,	0 },
	{ "css_edit_button",		NULL,				PT_NORMAL,	0 },

	/* Privacy */
	{ "popups_allow_checkbutton",		CONF_SECURITY_ALLOW_POPUPS,	  PT_AUTOAPPLY, 0 },
	{ "enable_java_checkbutton",		CONF_SECURITY_JAVA_ENABLED,	  PT_AUTOAPPLY, 0 },
	{ "enable_javascript_checkbutton",	CONF_SECURITY_JAVASCRIPT_ENABLED, PT_AUTOAPPLY, 0 },
	{ "cookies_radiobutton",		CONF_SECURITY_COOKIES_ACCEPT,	  PT_AUTOAPPLY, G_TYPE_STRING },
	{ "remember_passwords_checkbutton",	CONF_PRIVACY_REMEMBER_PASSWORDS,  PT_AUTOAPPLY, 0 },
	{ "disk_cache_spin",			CONF_NETWORK_CACHE_SIZE,	  PT_AUTOAPPLY, 0 },
	{ "clear_cache_button",			NULL,				  PT_NORMAL,	0 },

	/* Languages */
	{ "auto_encoding_combo",	CONF_LANGUAGE_AUTODETECT_ENCODING,	PT_AUTOAPPLY,	G_TYPE_STRING },
	{ "default_encoding_combo",	CONF_LANGUAGE_DEFAULT_ENCODING,		PT_AUTOAPPLY,	G_TYPE_STRING },
	{ "default_encoding_label",	NULL,					PT_NORMAL,	0 },
	{ "auto_encoding_label",	NULL,					PT_NORMAL,	0 },
	{ "lang_add_button",		NULL,					PT_NORMAL,	0 },
	{ "lang_remove_button",		NULL,					PT_NORMAL,	0 },
	{ "lang_up_button",		NULL,					PT_NORMAL,	0 },
	{ "lang_down_button",		NULL,					PT_NORMAL,	0 },
	{ "lang_treeview",		NULL,					PT_NORMAL,	0 },

	{ NULL }
};

enum
{
	LANGUAGE_DIALOG,
	LANGUAGE_PROP,
	LANGUAGE_ACCEPT_BUTTON_PROP
};

static const
EphyDialogProperty add_lang_props [] =
{
	{ "add_language_dialog", NULL,	PT_NORMAL,	0 },
	{ "languages_treeview",	 NULL,	PT_NORMAL,	G_TYPE_STRING },
	{ "add_button",		 NULL,	PT_NORMAL,	0 },

	{ NULL }
};

enum
{
	COL_LANG_NAME,
	COL_LANG_CODE
};

enum
{
	COL_ENC_NAME,
	COL_ENC_CODE
};

enum
{
	COL_AUTODETECTOR_NAME,
	COL_AUTODETECTOR_DATA,
	COL_AUTODETECTOR_IS_SEP
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
				  properties[WINDOW_PROP].id, &window,
				  properties[NOTEBOOK_PROP].id, &notebook,
				  NULL);

	id = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
	id = CLAMP (id, 0, 3);

	ephy_gui_help (GTK_WINDOW (window), "epiphany", help_preferences[id]);
}

static gboolean
row_is_separator (GtkTreeModel *model,
                  GtkTreeIter *iter,
                  gpointer data)
{
	int column = GPOINTER_TO_INT (data);
	gboolean is_sep;

	gtk_tree_model_get (model, iter, column, &is_sep, -1);

	return is_sep;
}

static void
css_checkbox_toggled (GtkToggleButton *button,
		      GtkWidget *widget)
{
	g_object_set (widget, "sensitive", gtk_toggle_button_get_active (button), NULL);
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

static void
create_node_combo (EphyDialog *dialog,
		   int prop,
		   EphyEncodings *encodings,
		   EphyNode *node,
		   const char *key,
		   const char *default_value)
{
	EphyTreeModelNode *nodemodel;
	GtkTreeModel *sortmodel;
	GtkComboBox *combo;
	GtkCellRenderer *renderer;
	char *code;
	int title_col, data_col;

	code = eel_gconf_get_string (key);
	if (code == NULL || ephy_encodings_get_node (encodings, code, FALSE) == NULL)
	{
		/* safe default */
		eel_gconf_set_string (key, default_value);
	}
	g_free (code);

	combo = GTK_COMBO_BOX (ephy_dialog_get_control (dialog, properties[prop].id));

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

	ephy_dialog_set_data_column (dialog, properties[prop].id, data_col);

	g_object_unref (nodemodel);
	g_object_unref (sortmodel);
}

static void
create_autodetectors_combo (EphyDialog *dialog,
			    int prop,
			    EphyEncodings *encodings,
			    EphyNode *node,
			    const char *key,
			    const char *default_value)
{
	GtkListStore *store;
	GtkTreeIter iter;
	GPtrArray *children;
	GtkComboBox *combo;
	GtkCellRenderer *renderer;
	char *code;
	const char *off_title = NULL;
	guint i;

	code = eel_gconf_get_string (key);
	if (code == NULL || ephy_encodings_get_node (encodings, code, FALSE) == NULL)
	{
		/* safe default */
		eel_gconf_set_string (key, default_value);
	}
	g_free (code);

	combo = GTK_COMBO_BOX (ephy_dialog_get_control (dialog, properties[prop].id));

	store = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);

	children = ephy_node_get_children (node);
	for (i = 0; i < children->len; ++i)
	{
		EphyNode *kid = g_ptr_array_index (children, i);
		const char *code, *title;

		code = ephy_node_get_property_string (kid, EPHY_NODE_ENCODING_PROP_ENCODING);
		g_return_if_fail (code != NULL);

		title = ephy_node_get_property_string (kid, EPHY_NODE_ENCODING_PROP_TITLE_ELIDED);
		if (code[0] == '\0')
		{
			off_title = title;
		}
		else
		{	
			gtk_list_store_insert_with_values (store, &iter, -1,
							   COL_AUTODETECTOR_NAME, title,
							   COL_AUTODETECTOR_DATA, code,
							   COL_AUTODETECTOR_IS_SEP, FALSE,
							   -1);
		}
	}

	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
					      COL_AUTODETECTOR_NAME,
					      GTK_SORT_ASCENDING);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
					      GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID,
					      GTK_SORT_ASCENDING);

	g_assert (off_title != NULL);

	gtk_list_store_insert_with_values (store, &iter, 0,
					   COL_AUTODETECTOR_NAME, off_title,
					   COL_AUTODETECTOR_DATA, "",
					   COL_AUTODETECTOR_IS_SEP, FALSE,
					   -1);
	gtk_list_store_insert_with_values (store, &iter, 1,
					   COL_AUTODETECTOR_NAME, NULL,
					   COL_AUTODETECTOR_DATA, "",
					   COL_AUTODETECTOR_IS_SEP, TRUE,
					   -1);

	gtk_combo_box_set_model (combo, GTK_TREE_MODEL (store));
	g_object_unref (store);

	gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (combo),
					      (GtkTreeViewRowSeparatorFunc) row_is_separator,
					      GINT_TO_POINTER (COL_AUTODETECTOR_IS_SEP), NULL);


	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer,
					"text", COL_AUTODETECTOR_NAME,
					NULL);

	ephy_dialog_set_data_column (dialog, properties[prop].id, COL_AUTODETECTOR_DATA);
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
	GSList *codes = NULL;

	if (gtk_tree_model_get_iter_first (pd->priv->lang_model, &iter))
	{
		do
		{
			char *code;
		
			gtk_tree_model_get (pd->priv->lang_model, &iter,
					    COL_LANG_CODE, &code,
					    -1);

			codes = g_slist_prepend (codes, code);
		}
		while (gtk_tree_model_iter_next (pd->priv->lang_model, &iter));
	}

	codes = g_slist_reverse (codes);
	eel_gconf_set_string_list (CONF_RENDERING_LANGUAGE, codes);

	g_slist_foreach (codes, (GFunc) g_free, NULL);
	g_slist_free (codes);
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

	button = ephy_dialog_get_control (dialog, add_lang_props[LANGUAGE_ACCEPT_BUTTON_PROP].id);

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
				(dialog, add_lang_props[LANGUAGE_PROP].id));
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

	parent = ephy_dialog_get_control (EPHY_DIALOG (pd), properties[WINDOW_PROP].id);

	dialog =  EPHY_DIALOG (g_object_new (EPHY_TYPE_DIALOG,
					     "parent-window", parent,
					     "default-width", 260,
					     "default-height", 230,
					     NULL));

	ephy_dialog_construct (dialog, 
			       add_lang_props,
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
				  add_lang_props[LANGUAGE_PROP].id, &treeview,
				  add_lang_props[LANGUAGE_DIALOG].id, &window,
				  NULL);

	gtk_window_group_add_window (GTK_WINDOW (parent)->group, GTK_WINDOW (window));
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
	GSList *list, *l, *ulist = NULL;

	pd->priv->iso_639_table = ephy_langs_iso_639_table ();
	pd->priv->iso_3166_table = ephy_langs_iso_3166_table ();

	ephy_dialog_get_controls
		(dialog,
		 properties[LANGUAGE_TREEVIEW_PROP].id, &treeview,
		 properties[LANGUAGE_ADD_BUTTON_PROP].id, &pd->priv->lang_add_button,
		 properties[LANGUAGE_REMOVE_BUTTON_PROP].id, &pd->priv->lang_remove_button,
		 properties[LANGUAGE_UP_BUTTON_PROP].id, &pd->priv->lang_up_button, 
		 properties[LANGUAGE_DOWN_BUTTON_PROP].id, &pd->priv->lang_down_button,
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

	gtk_tree_view_set_reorderable (GTK_TREE_VIEW (treeview), TRUE);

	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

	pd->priv->lang_model = GTK_TREE_MODEL (store);

	gtk_tree_view_set_model (treeview, pd->priv->lang_model);
	g_object_unref (store);

	gtk_tree_view_set_headers_visible (treeview, FALSE);

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

	list = eel_gconf_get_string_list (CONF_RENDERING_LANGUAGE);

	/* uniquify list */
	for (l = list; l != NULL; l = l->next)
	{
		if (g_slist_find_custom (ulist, l->data, (GCompareFunc) strcmp) == NULL)
		{
			ulist = g_slist_prepend (ulist, g_strdup (l->data));
		}
	}
	ulist = g_slist_reverse (ulist);

	/* if modified, write back */
	if (g_slist_length (ulist) != g_slist_length (list))
	{
		eel_gconf_set_string_list (CONF_RENDERING_LANGUAGE, ulist);
	}

	/* Fill languages editor */
	for (l = ulist; l != NULL; l = l->next)
	{
		const char *code = (const char *) l->data;

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

	language_editor_update_buttons (pd);

	/* Lockdown if key is not writable */
	if (eel_gconf_key_is_writable (CONF_RENDERING_LANGUAGE) == FALSE)
	{
		gtk_widget_set_sensitive (pd->priv->lang_add_button, FALSE);
		gtk_widget_set_sensitive (pd->priv->lang_remove_button, FALSE);
		gtk_widget_set_sensitive (pd->priv->lang_up_button, FALSE);
		gtk_widget_set_sensitive (pd->priv->lang_down_button, FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (pd->priv->lang_treeview), FALSE);
	}

	g_slist_foreach (list, (GFunc) g_free, NULL);
	g_slist_free (list);

	g_slist_foreach (ulist, (GFunc) g_free, NULL);
	g_slist_free (ulist);
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

	if (strcmp (dir, downloads_dir) == 0)
	{
		eel_gconf_set_string (CONF_STATE_DOWNLOAD_DIR, "Downloads");
	}
	else if (strcmp (dir, desktop_dir) == 0)
	{
		eel_gconf_set_string (CONF_STATE_DOWNLOAD_DIR, "Desktop");
	}
	else
	{
		eel_gconf_set_path (CONF_STATE_DOWNLOAD_DIR, dir);
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
				  properties[DOWNLOAD_PATH_HBOX_PROP].id, &hbox,
				  properties[DOWNLOAD_PATH_LABEL_PROP].id, &label,
				  properties[WINDOW_PROP].id, &parent,
				  NULL);

	fc = ephy_file_chooser_new (_("Select a Directory"),
				    parent,
				    GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
				    NULL, EPHY_FILE_FILTER_NONE);

	/* Unset the destroy-with-parent, since gtkfilechooserbutton doesn't expect this */
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

	gtk_widget_set_sensitive (button, eel_gconf_key_is_writable (CONF_STATE_DOWNLOAD_DIR));
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
prefs_clear_cache_button_clicked_cb (GtkWidget *button,
				     PrefsDialog *dialog)
{
	GtkWidget *parent;

	parent = ephy_dialog_get_control (EPHY_DIALOG (dialog),
					  properties[WINDOW_PROP].id);
	pdm_dialog_show_clear_all_dialog (EPHY_DIALOG (dialog),
					  parent,
					  CLEAR_ALL_CACHE);
}

static void
set_homepage_entry (EphyDialog *dialog,
		    const char *new_location)
{
	GValue value = { 0, };

	g_value_init (&value, G_TYPE_STRING);
	g_value_set_string (&value, new_location);
	ephy_dialog_set_value (dialog, properties[HOMEPAGE_ENTRY_PROP].id, &value);
	g_value_unset (&value);
}

static void
prefs_homepage_current_button_clicked_cb (GtkWidget *button,
					  EphyDialog *dialog)
{
	EphySession *session;
	EphyWindow *window;
	EphyEmbed *embed;

	session = EPHY_SESSION (ephy_shell_get_session (ephy_shell_get_default ()));
	window = ephy_session_get_active_window (session);

	/* can't do anything in this case */
	if (window == NULL) return;

	embed = ephy_embed_container_get_active_child 
          (EPHY_EMBED_CONTAINER (window));
	g_return_if_fail (embed != NULL);

	set_homepage_entry (dialog, ephy_embed_get_address (embed));
}

static void
prefs_homepage_blank_button_clicked_cb (GtkWidget *button,
					EphyDialog *dialog)
{
	set_homepage_entry (dialog, NULL);
}

static void
prefs_dialog_init (PrefsDialog *pd)
{
	EphyDialog *dialog = EPHY_DIALOG (pd);
	EphyEncodings *encodings;
	GtkWidget *window, *curr_button, *blank_button;
	GtkWidget *clear_cache_button;
	GtkWidget *css_checkbox, *css_edit_box, *css_edit_button, *css_container;
	gboolean sensitive;

	pd->priv = EPHY_PREFS_DIALOG_GET_PRIVATE (pd);

	ephy_dialog_construct (dialog,
			       properties,
			       ephy_file ("prefs-dialog.ui"),
			       "prefs_dialog",
			       NULL);

	ephy_dialog_add_enum (dialog, properties[ACCEPT_COOKIES_PROP].id,
			      G_N_ELEMENTS (cookies_accept_enum), cookies_accept_enum);

	ephy_dialog_get_controls
		(dialog,
		 properties[WINDOW_PROP].id, &window,
		 properties[HOMEPAGE_CURRENT_PROP].id, &curr_button,
		 properties[HOMEPAGE_BLANK_PROP].id, &blank_button,
		 properties[CSS_CHECKBOX_PROP].id, &css_checkbox,
		 properties[CSS_EDIT_BOX_PROP].id, &css_edit_box,
		 properties[CSS_EDIT_BUTTON_PROP].id, &css_edit_button,
		 properties[CSS_CONTAINER_PROP].id, &css_container,
		 properties[CLEAR_CACHE_BUTTON_PROP].id, &clear_cache_button,
		 NULL);

	ephy_gui_ensure_window_group (GTK_WINDOW (window));

	g_signal_connect (window, "response",
			  G_CALLBACK (prefs_dialog_response_cb), dialog);

	g_signal_connect (curr_button, "clicked",
			  G_CALLBACK (prefs_homepage_current_button_clicked_cb), dialog);
	g_signal_connect (blank_button, "clicked",
			  G_CALLBACK (prefs_homepage_blank_button_clicked_cb), dialog);

	/* set homepage button sensitivity */
	sensitive = eel_gconf_key_is_writable (CONF_GENERAL_HOMEPAGE);
	gtk_widget_set_sensitive (curr_button, sensitive);
	gtk_widget_set_sensitive (blank_button, sensitive);

	css_checkbox_toggled (GTK_TOGGLE_BUTTON (css_checkbox), css_edit_box);
	g_signal_connect (css_checkbox, "toggled",
			  G_CALLBACK (css_checkbox_toggled), css_edit_box);

	g_signal_connect (css_edit_button, "clicked",
			  G_CALLBACK (css_edit_button_clicked_cb), dialog);

	g_signal_connect (clear_cache_button, "clicked",
			  G_CALLBACK (prefs_clear_cache_button_clicked_cb), dialog);

	encodings = EPHY_ENCODINGS (ephy_embed_shell_get_encodings
					(EPHY_EMBED_SHELL (ephy_shell)));

	create_node_combo (dialog, DEFAULT_ENCODING_PROP, encodings,
			   ephy_encodings_get_all (encodings),
			   CONF_LANGUAGE_DEFAULT_ENCODING, "ISO-8859-1");
	create_autodetectors_combo (dialog, AUTO_ENCODING_PROP, encodings,
				    ephy_encodings_get_detectors (encodings),
				    CONF_LANGUAGE_AUTODETECT_ENCODING, "");

	create_language_section	(dialog);

	create_download_path_button (dialog);
}
