/*
 *  Copyright (C) 200-2003 Marco Pesenti Gritti
 *  Copyright (C) 2003, 2004 Christian Persch
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

#include "prefs-dialog.h"
#include "ephy-dialog.h"
#include "ephy-prefs.h"
#include "ephy-embed-shell.h"
#include "ephy-shell.h"
#include "ephy-session.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-single.h"
#include "ephy-shell.h"
#include "ephy-gui.h"
#include "eel-gconf-extensions.h"
#include "ephy-langs.h"
#include "ephy-encodings.h"
#include "ephy-debug.h"
#include "ephy-ellipsizing-label.h"
#include "ephy-file-chooser.h"
#include "ephy-file-helpers.h"
#include "ephy-tree-model-node.h"
#include "ephy-tree-model-sort.h"

#include <glib/gi18n.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkradiobutton.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkcombobox.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtkimage.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkstock.h>
#include <string.h>

#define CONF_FONTS_FOR_LANGUAGE	"/apps/epiphany/dialogs/preferences_font_language"

static void prefs_dialog_class_init	(PrefsDialogClass *klass);
static void prefs_dialog_init		(PrefsDialog *pd);

/* Glade callbacks */
void prefs_proxy_auto_url_reload_cb		(GtkWidget *button,
						 EphyDialog *dialog);
void prefs_clear_cache_button_clicked_cb	(GtkWidget *button,
						 gpointer data);
void prefs_dialog_response_cb			(GtkDialog *widget,
						 gint response_id,
						 EphyDialog *dialog);
void prefs_homepage_current_button_clicked_cb	(GtkWidget *button,
						 EphyDialog *dialog);
void prefs_homepage_blank_button_clicked_cb	(GtkWidget *button,
						 EphyDialog *dialog);
void prefs_language_more_button_clicked_cb	(GtkWidget *button,
						 EphyDialog *dialog);
void prefs_download_path_button_clicked_cb	(GtkWidget *button,
						 PrefsDialog *dialog);
void language_editor_add_button_clicked_cb	(GtkWidget *button,
						 PrefsDialog *pd);
void language_editor_remove_button_clicked_cb	(GtkWidget *button,
						 PrefsDialog *pd);
void language_editor_up_button_clicked_cb	(GtkWidget *button,
						 PrefsDialog *pd);
void language_editor_down_button_clicked_cb	(GtkWidget *button,
						 PrefsDialog *pd);

static const
struct
{
	char *name;
	char *code;
}
languages [] =
{
	/**
	 * please translate like this: "<your language> (System setting)"
	 * Examples:
	 * "de"    translation: "Deutsch (Systemeinstellung)"
	 * "en_AU" translation: "English, Australia (System setting)" or
	 *                      "Australian English (System setting)"
	 */ 
	{ N_("System language"), "system" },
	{ N_("Afrikaans"), "ak" },
	{ N_("Albanian"), "sq" },
	{ N_("Arabic"), "ar" },
	{ N_("Azerbaijani"), "az" },
	{ N_("Basque"), "eu" },
	{ N_("Breton"), "br" },
	{ N_("Bulgarian"), "bg" },
	{ N_("Byelorussian"), "be" },
	{ N_("Catalan"), "ca" },
	{ N_("Simplified Chinese"), "zh-cn" },
	{ N_("Traditional Chinese"), "zh-tw" },
	{ N_("Chinese"), "zh" },
	{ N_("Croatian"), "hr" },
	{ N_("Czech"), "cs" },
	{ N_("Danish"), "da" },
	{ N_("Dutch"), "nl" },
	{ N_("English"), "en" },
	{ N_("Esperanto"), "eo" },
	{ N_("Estonian"), "et" },
	{ N_("Faeroese"), "fo" },
	{ N_("Finnish"), "fi" },
	{ N_("French"), "fr" },
	{ N_("Galician"), "gl" },
	{ N_("German"), "de" },
	{ N_("Greek"), "el" },
	{ N_("Hebrew"), "he" },
	{ N_("Hungarian"), "hu" },
	{ N_("Icelandic"), "is" },
	{ N_("Indonesian"), "id" },
	{ N_("Irish"), "ga" },
	{ N_("Italian"), "it" },
	{ N_("Japanese"), "ja" },
	{ N_("Korean"), "ko" },
	{ N_("Latvian"), "lv" },
	{ N_("Lithuanian"), "lt" },
	{ N_("Macedonian"), "mk" },
	{ N_("Malay"), "ms" },
	{ N_("Norwegian/Nynorsk"), "nn" },
	{ N_("Norwegian/Bokmal"), "nb" },
	{ N_("Norwegian"), "no" },
	{ N_("Polish"), "pl" },
	{ N_("Portuguese"), "pt" },
	{ N_("Portuguese of Brazil"), "pt-br" },
	{ N_("Romanian"), "ro" },
	{ N_("Russian"), "ru" },
	{ N_("Scottish"), "gd" },
	{ N_("Serbian"), "sr" },
	{ N_("Slovak"), "sk" },
	{ N_("Slovenian"), "sl" },
	{ N_("Spanish"), "es" },
	{ N_("Swedish"), "sv" },
	{ N_("Tamil"), "ta" },
	{ N_("Turkish"), "tr" },
	{ N_("Ukrainian"), "uk" },
	{ N_("Vietnamese"), "vi" },
	{ N_("Walloon"), "wa" }
};
static guint n_languages = G_N_ELEMENTS (languages);

static const
char *cookies_accept_enum [] =
{
	"anywhere", "current site", "nowhere"
};
static guint n_cookies_accept_enum = G_N_ELEMENTS (cookies_accept_enum);

enum
{
	FONT_TYPE_VARIABLE,
	FONT_TYPE_MONOSPACE
};

const
char *fonts_types[] =
{
	"variable",
	"monospace"
};

enum
{
	FONT_SIZE_FIXED,
	FONT_SIZE_VAR,
	FONT_SIZE_MIN
};

const
char *size_prefs [] =
{
	CONF_RENDERING_FONT_FIXED_SIZE,
	CONF_RENDERING_FONT_VAR_SIZE,
	CONF_RENDERING_FONT_MIN_SIZE
};

const
int default_size [] =
{
	10,
	11,
	7
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
	DOWNLOAD_PATH_BUTTON_PROP,

	/* Fonts and Colors */
	FONTS_LANGUAGE_PROP,
	VARIABLE_PROP,
	MONOSPACE_PROP,
	FIXED_SIZE_PROP,
	VARIABLE_SIZE_PROP,
	MIN_SIZE_PROP,
	USE_COLORS_PROP,
	USE_FONTS_PROP,

	/* Privacy */
	ALLOW_POPUPS_PROP,
	ALLOW_JAVA_PROP,
	ALLOW_JS_PROP,
	ACCEPT_COOKIES_PROP,
	DISK_CACHE_PROP,

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
	{ "homepage_entry",			CONF_GENERAL_HOMEPAGE,	  PT_AUTOAPPLY,	G_TYPE_STRING },
	{ "homepage_current_button",		NULL,			  PT_NORMAL,	0 },
	{ "homepage_blank_button",		NULL,			  PT_NORMAL,	0 },
	{ "automatic_downloads_checkbutton",	CONF_AUTO_DOWNLOADS,      PT_AUTOAPPLY,	0 },
	{ "download_path_button",		NULL,			  PT_NORMAL,	0 },

	/* Fonts and Colors */
	{ "fonts_language_combo",	CONF_FONTS_FOR_LANGUAGE,	PT_AUTOAPPLY,	G_TYPE_STRING },
	{ "variable_combo",		NULL,				PT_AUTOAPPLY,	G_TYPE_STRING },
	{ "monospace_combo",		NULL,				PT_AUTOAPPLY,	G_TYPE_STRING },
	{ "fixed_size_spinbutton",	NULL,				PT_AUTOAPPLY,	0 },
	{ "variable_size_spinbutton",	NULL,				PT_AUTOAPPLY,	0 },
	{ "min_size_spinbutton",	NULL,				PT_AUTOAPPLY,	0 },
	{ "use_colors_checkbutton",	CONF_RENDERING_USE_OWN_COLORS,	PT_AUTOAPPLY,	0 },
	{ "use_fonts_checkbutton",	CONF_RENDERING_USE_OWN_FONTS,	PT_AUTOAPPLY,	0 },

	/* Privacy */
	{ "popups_allow_checkbutton",		CONF_SECURITY_ALLOW_POPUPS,	  PT_AUTOAPPLY, 0 },
	{ "enable_java_checkbutton",		CONF_SECURITY_JAVA_ENABLED,	  PT_AUTOAPPLY, 0 },
	{ "enable_javascript_checkbutton",	CONF_SECURITY_JAVASCRIPT_ENABLED, PT_AUTOAPPLY, 0 },
	{ "cookies_radiobutton",		CONF_SECURITY_COOKIES_ACCEPT,	  PT_AUTOAPPLY, G_TYPE_STRING },
	{ "disk_cache_spin",			CONF_NETWORK_CACHE_SIZE,	  PT_AUTOAPPLY, 0 },

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
	LANGUAGE_PROP
};

static const
EphyDialogProperty add_lang_props [] =
{
	{ "add_language_dialog", NULL,	PT_NORMAL,	0 },
	{ "languages_treeview",	 NULL,	PT_NORMAL,	G_TYPE_STRING },

	{ NULL }
};

enum
{
	COL_FONTS_LANG_NAME,
	COL_FONTS_LANG_CODE
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

#define EPHY_PREFS_DIALOG_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_PREFS_DIALOG, PrefsDialogPrivate))

struct PrefsDialogPrivate
{
	GtkWidget *download_dir_chooser;
	GtkTreeView *lang_treeview;
	GtkTreeModel *lang_model;
	EphyDialog *add_lang_dialog;
	GtkWidget *lang_add_button;
	GtkWidget *lang_remove_button;
	GtkWidget *lang_up_button;
	GtkWidget *lang_down_button;
};

static GObjectClass *parent_class = NULL;

GType
prefs_dialog_get_type (void)
{
	static GType type = 0;

	if (type == 0)
	{
		static const GTypeInfo our_info =
		{
			sizeof (PrefsDialogClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) prefs_dialog_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (PrefsDialog),
			0, /* n_preallocs */
			(GInstanceInitFunc) prefs_dialog_init
		};

		type = g_type_register_static (EPHY_TYPE_DIALOG,
					       "PrefsDialog",
					       &our_info, 0);
	}

	return type;
}

static void
prefs_dialog_finalize (GObject *object)
{
	PrefsDialog *dialog = EPHY_PREFS_DIALOG (object);

	if (dialog->priv->add_lang_dialog != NULL)
	{
		g_object_remove_weak_pointer
			(G_OBJECT (dialog->priv->add_lang_dialog),
			(gpointer *) &dialog->priv->add_lang_dialog);
		g_object_unref (dialog->priv->add_lang_dialog);
	}

	if (dialog->priv->download_dir_chooser != NULL)
	{
		g_object_remove_weak_pointer
			(G_OBJECT (dialog->priv->download_dir_chooser),
			 (gpointer *) &dialog->priv->download_dir_chooser);
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
prefs_dialog_class_init (PrefsDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = prefs_dialog_finalize;

	g_type_class_add_private (object_class, sizeof(PrefsDialogPrivate));
}

static void
prefs_dialog_show_help (EphyDialog *dialog)
{
	GtkWidget *window, *notebook;
	int id;

	char *help_preferences[] = {
		"general-preferences",
		"fonts-and-colors-preferences",
		"privacy-preferences",
		"language-preferences"
	};

	window = ephy_dialog_get_control (dialog, properties[WINDOW_PROP].id);
	notebook = ephy_dialog_get_control (dialog, properties[NOTEBOOK_PROP].id);

	id = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
	id = CLAMP (id, 0, 3);

	ephy_gui_help (GTK_WINDOW (window), "epiphany", help_preferences[id]);
}

static void
setup_font_combo (EphyDialog *dialog,
		  const char *type,
		  const char *code,
		  int prop)
{
	GtkWidget *combo;
	GtkListStore *store;
	GtkTreeModel *sortmodel;
	GtkTreeIter iter;
	GList *fonts, *l;
	char *name;
	char key[255];
	EphyEmbedSingle *single;

	single = EPHY_EMBED_SINGLE (ephy_embed_shell_get_embed_single (embed_shell));
	fonts = ephy_embed_single_get_font_list (single, code);

	g_snprintf (key, 255, "%s_%s_%s", CONF_RENDERING_FONT, type, code);
	name = eel_gconf_get_string (key);

	/* sanitise the pref */
	if (name == NULL || name[0] == '\0'
	    || g_list_find_custom (fonts, name, (GCompareFunc) strcmp) == NULL)
	{
		if (prop == VARIABLE_PROP)
		{
			eel_gconf_set_string (key, "sans-serif");
		}
		else
		{
			eel_gconf_set_string (key, "monospace");
		}
	}
	g_free (name);

	combo = ephy_dialog_get_control (dialog, properties[prop].id);
	store = gtk_list_store_new (1, G_TYPE_STRING);

	for (l = fonts; l != NULL; l = l->next)
	{
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, (char *) l->data, -1);
	}
	g_list_foreach (fonts, (GFunc) g_free, NULL);
	g_list_free (fonts);

	sortmodel = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (store));
	gtk_tree_sortable_set_sort_column_id
		(GTK_TREE_SORTABLE (sortmodel), 0, GTK_SORT_ASCENDING);

	ephy_dialog_set_pref (dialog, properties[prop].id, NULL);

	gtk_combo_box_set_model (GTK_COMBO_BOX (combo), sortmodel);

	ephy_dialog_set_pref (dialog, properties[prop].id, key);

	g_object_unref (store);
	g_object_unref (sortmodel);
}

static void
fonts_language_changed_cb (EphyDialog *dialog,
			   const GValue *value,
			   gpointer data)
{
	const char *code;
	char key[128];
	int size;

	code = g_value_get_string (value);

	LOG ("fonts language combo changed, new code '%s'", code)

	setup_font_combo (dialog, "variable", code, VARIABLE_PROP);
	setup_font_combo (dialog, "monospace", code, MONOSPACE_PROP);

	g_snprintf (key, sizeof (key), "%s_%s", size_prefs[FONT_SIZE_VAR], code);
	size = eel_gconf_get_integer (key);
	if (size <= 0)
	{
		eel_gconf_set_integer (key, default_size[FONT_SIZE_VAR]);
	}
	ephy_dialog_set_pref (dialog, properties[VARIABLE_SIZE_PROP].id, key);

	g_snprintf (key, sizeof (key), "%s_%s", size_prefs[FONT_SIZE_FIXED], code);
	size = eel_gconf_get_integer (key);
	if (size <= 0)
	{
		eel_gconf_set_integer (key, default_size[FONT_SIZE_FIXED]);
	}
	ephy_dialog_set_pref (dialog, properties[FIXED_SIZE_PROP].id, key);

	g_snprintf (key, sizeof (key), "%s_%s", size_prefs[FONT_SIZE_MIN], code);
	size = eel_gconf_get_integer (key);
	if (size <= 0)
	{
		eel_gconf_set_integer (key, default_size[FONT_SIZE_MIN]);
	}
	ephy_dialog_set_pref (dialog, properties[MIN_SIZE_PROP].id, key);
}

static void
create_fonts_language_menu (EphyDialog *dialog)
{
	GtkWidget *combo;
	GtkCellRenderer *renderer;
	GtkListStore *store;
	GtkTreeModel *sortmodel;
	GtkTreeIter iter;
	guint n_fonts_languages, i;
	const EphyFontsLanguageInfo *fonts_languages;

	combo = ephy_dialog_get_control (dialog, properties[FONTS_LANGUAGE_PROP].id);

	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

	fonts_languages = ephy_font_languages ();
	n_fonts_languages = ephy_font_n_languages ();
	
	for (i = 0; i < n_fonts_languages; i++)
	{
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    COL_FONTS_LANG_NAME, _(fonts_languages[i].title),
				    COL_FONTS_LANG_CODE, fonts_languages[i].code,
				    -1);
	}

	sortmodel = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (store));
	gtk_tree_sortable_set_sort_column_id
		(GTK_TREE_SORTABLE (sortmodel), COL_FONTS_LANG_NAME, GTK_SORT_ASCENDING);

	gtk_combo_box_set_model (GTK_COMBO_BOX (combo), sortmodel);

        renderer = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer,
                                        "text", COL_FONTS_LANG_NAME,
                                        NULL);

	ephy_dialog_set_data_column (dialog, properties[FONTS_LANGUAGE_PROP].id, COL_FONTS_LANG_CODE);

	g_signal_connect (dialog, "changed::fonts_language_combo",
			  G_CALLBACK (fonts_language_changed_cb),
			  NULL);

	g_object_unref (store);
	g_object_unref (sortmodel);
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

	nodemodel = ephy_tree_model_node_new (node, NULL);

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
	int selected;

	selection = gtk_tree_view_get_selection (dialog->priv->lang_treeview);

	gtk_widget_set_sensitive (dialog->priv->lang_remove_button, FALSE);
	gtk_widget_set_sensitive (dialog->priv->lang_up_button, FALSE);
	gtk_widget_set_sensitive (dialog->priv->lang_down_button, FALSE);
	
	if (gtk_tree_selection_get_selected (selection, &model, &iter))
	{
		path = gtk_tree_model_get_path (model, &iter);
	
		selected = gtk_tree_path_get_indices (path)[0];
	
		gtk_widget_set_sensitive (dialog->priv->lang_remove_button, TRUE);
		gtk_widget_set_sensitive (dialog->priv->lang_up_button, selected > 0);
		gtk_widget_set_sensitive (dialog->priv->lang_down_button,
			selected < gtk_tree_model_iter_n_children (model, NULL) - 1);

		gtk_tree_path_free (path);
	}
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

	if (response == GTK_RESPONSE_OK)
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

static EphyDialog *
setup_add_language_dialog (PrefsDialog *pd)
{
	EphyDialog *dialog;
	GtkWidget *window;
	GtkListStore *store;
	GtkTreeModel *sortmodel;
	GtkTreeView *treeview;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	int i;

	window = ephy_dialog_get_control (EPHY_DIALOG (pd), properties[WINDOW_PROP].id);

	dialog =  EPHY_DIALOG (g_object_new (EPHY_TYPE_DIALOG,
					     "parent-window", window,
					     "default-width", 260,
					     "default-height", 230,
					     NULL));

	ephy_dialog_construct (dialog, 
			       add_lang_props,
			       ephy_file ("prefs-dialog.glade"),
			       "add_language_dialog",
			       NULL);

	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

	for (i = 0; i < n_languages; i++)
	{
		gtk_list_store_append (store, &iter);

		gtk_list_store_set (store, &iter,
				    COL_LANG_NAME, _(languages[i].name),
				    COL_LANG_CODE, languages[i].code,
				    -1);
	}

	sortmodel = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (store));
	gtk_tree_sortable_set_sort_column_id
		(GTK_TREE_SORTABLE (sortmodel), COL_LANG_NAME, GTK_SORT_ASCENDING);

	treeview = GTK_TREE_VIEW (ephy_dialog_get_control
			(dialog, add_lang_props[LANGUAGE_PROP].id));

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

	window = ephy_dialog_get_control (dialog, add_lang_props[LANGUAGE_DIALOG].id);
	g_signal_connect (window, "response",
			  G_CALLBACK (add_lang_dialog_response_cb), pd);

	g_object_unref (store);
	g_object_unref (sortmodel);

	return dialog;
}

void
language_editor_add_button_clicked_cb (GtkWidget *button,
				       PrefsDialog *pd)
{
	if (pd->priv->add_lang_dialog == NULL)
	{
		pd->priv->add_lang_dialog = setup_add_language_dialog (pd);

		g_object_add_weak_pointer
			(G_OBJECT (pd->priv->add_lang_dialog),
			(gpointer *) &pd->priv->add_lang_dialog);;
	}

	ephy_dialog_show (pd->priv->add_lang_dialog);
}

void
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

void
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

void
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

	/* setup the languages treeview */
	treeview = GTK_TREE_VIEW (ephy_dialog_get_control
			(dialog, properties[LANGUAGE_TREEVIEW_PROP].id));
	pd->priv->lang_treeview = treeview;

	gtk_tree_view_set_reorderable (GTK_TREE_VIEW (treeview), TRUE);

	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

	pd->priv->lang_model = GTK_TREE_MODEL (store);

	gtk_tree_view_set_model (treeview, pd->priv->lang_model);
	g_object_unref (store);

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
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

	/* Connect treeview signals */
	g_signal_connect (G_OBJECT (treeview), "drag_end",
			  G_CALLBACK (language_editor_treeview_drag_end_cb), pd);
	g_signal_connect (G_OBJECT (selection), "changed",
			  G_CALLBACK (language_editor_selection_changed_cb), pd);

	pd->priv->lang_add_button = ephy_dialog_get_control
		(dialog, properties[LANGUAGE_ADD_BUTTON_PROP].id);
	pd->priv->lang_remove_button = ephy_dialog_get_control
		(dialog, properties[LANGUAGE_REMOVE_BUTTON_PROP].id);
	pd->priv->lang_up_button = ephy_dialog_get_control
		(dialog, properties[LANGUAGE_UP_BUTTON_PROP].id);
	pd->priv->lang_down_button = ephy_dialog_get_control
		(dialog, properties[LANGUAGE_DOWN_BUTTON_PROP].id);

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
		int i;

		for (i = 0; i < n_languages; i++)
		{
			if (strcmp (languages[i].code, code) == 0) break;
		}

		/* code isn't in stock list */
		if (i == n_languages)
		{
			char *text;

			text = g_strdup_printf (_("Custom [%s]"), code);
			language_editor_add (pd, code, text);
			g_free (text);
		}
		else
		{
			language_editor_add (pd, code, _(languages[i].name));
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

static char*
get_download_button_label ()
{
	char *key, *label, *downloads_path, *converted_dp;

	key = eel_gconf_get_string (CONF_STATE_DOWNLOAD_DIR);

	downloads_path = ephy_file_downloads_dir ();
	converted_dp = g_filename_to_utf8 (downloads_path, -1, NULL, NULL, NULL);

	if (key == NULL || g_utf8_collate (key, "~") == 0)
	{
		/* Note that this does NOT refer to the home page but to a
		 * user's home folder. It should be translated by the same
		 * term as GTK+'s "Home" string to be consistent with the
		 * filechooser */
		label = g_strdup (_("Home"));
	}
	else if ((converted_dp != NULL && g_utf8_collate (key, converted_dp) == 0) ||
		 g_utf8_collate (key, "Downloads") == 0)
	{
		label = g_strdup (_("Downloads"));
	}
	else if (g_utf8_collate (key, "~/Desktop") == 0)
	{
		label = g_strdup (_("Desktop")); 
	}
	else
	{
		label = g_strdup (key);
	}

	g_free (downloads_path);
	g_free (converted_dp);
	g_free (key);

	return label;
}
	
static void
create_download_path_label (EphyDialog *dialog)
{
	GtkWidget *button, *label;
	char *dir;

	button = ephy_dialog_get_control (dialog, properties[DOWNLOAD_PATH_BUTTON_PROP].id);
	
	dir = get_download_button_label ();
	label = ephy_ellipsizing_label_new (dir);
	ephy_ellipsizing_label_set_mode ((EphyEllipsizingLabel*)label,
					  EPHY_ELLIPSIZE_START);
	gtk_container_add (GTK_CONTAINER (button), label);
	g_free (dir);
	gtk_widget_show (label);

	gtk_widget_set_sensitive (button, eel_gconf_key_is_writable (CONF_STATE_DOWNLOAD_DIR));
}
	
static void
prefs_dialog_init (PrefsDialog *pd)
{
	EphyDialog *dialog = EPHY_DIALOG (pd);
	EphyEncodings *encodings;
	GtkWidget *window, *button, *combo;
	GdkPixbuf *icon;
	GtkCellRenderer *renderer;
	gboolean sensitive;

	pd->priv = EPHY_PREFS_DIALOG_GET_PRIVATE (pd);

	ephy_dialog_construct (dialog,
			       properties,
			       ephy_file ("prefs-dialog.glade"),
			       "prefs_dialog",
			       NULL);

	ephy_dialog_add_enum (dialog, properties[ACCEPT_COOKIES_PROP].id,
			      n_cookies_accept_enum, cookies_accept_enum);

	ephy_dialog_set_size_group (dialog,
				    properties[DEFAULT_ENCODING_LABEL_PROP].id,
				    properties[AUTO_ENCODING_LABEL_PROP].id,
				    NULL);

	window = ephy_dialog_get_control (dialog, properties[WINDOW_PROP].id);

	icon = gtk_widget_render_icon (window,
				       GTK_STOCK_PREFERENCES,
				       GTK_ICON_SIZE_MENU,
				       "prefs_dialog");
	gtk_window_set_icon (GTK_WINDOW (window), icon);
	g_object_unref(icon);

	/* set homepage button sensitivity */
	sensitive = eel_gconf_key_is_writable (CONF_GENERAL_HOMEPAGE);
	button = ephy_dialog_get_control (dialog, properties[HOMEPAGE_CURRENT_PROP].id);
	gtk_widget_set_sensitive (button, sensitive);
	button = ephy_dialog_get_control (dialog, properties[HOMEPAGE_BLANK_PROP].id);
	gtk_widget_set_sensitive (button, sensitive);

	combo = ephy_dialog_get_control (dialog, properties[VARIABLE_PROP].id);
        renderer = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer,
                                        "text", 0,
                                        NULL);
	ephy_dialog_set_data_column (dialog, properties[VARIABLE_PROP].id, 0);
	combo = ephy_dialog_get_control (dialog, properties[MONOSPACE_PROP].id);
        renderer = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer,
                                        "text", 0,
                                        NULL);
	ephy_dialog_set_data_column (dialog, properties[MONOSPACE_PROP].id, 0);

	create_fonts_language_menu (dialog);

	encodings = EPHY_ENCODINGS (ephy_embed_shell_get_encodings
					(EPHY_EMBED_SHELL (ephy_shell)));

	create_node_combo (dialog, DEFAULT_ENCODING_PROP, encodings,
			   ephy_encodings_get_all (encodings),
			   CONF_LANGUAGE_DEFAULT_ENCODING, "ISO-8859-1");
	create_node_combo (dialog, AUTO_ENCODING_PROP, encodings,
			   ephy_encodings_get_detectors (encodings),
			   CONF_LANGUAGE_AUTODETECT_ENCODING, "");

	create_language_section	(dialog);

	create_download_path_label (dialog);
}

void
prefs_dialog_response_cb (GtkDialog *widget,
			  gint response_id,
			  EphyDialog *dialog)
{
	if (response_id == GTK_RESPONSE_CLOSE)
	{
		g_object_unref (dialog);
	}
	else if (response_id == GTK_RESPONSE_HELP)
	{
		prefs_dialog_show_help (dialog);
	}
}

void
prefs_clear_cache_button_clicked_cb (GtkWidget *button,
				     gpointer data)
{
	EphyEmbedSingle *single;

	single = EPHY_EMBED_SINGLE (ephy_embed_shell_get_embed_single (embed_shell));
	ephy_embed_single_clear_cache (single);
}

static void
set_homepage_entry (EphyDialog *dialog,
		    char *new_location)
{
	GValue value = { 0, };

	g_value_init (&value, G_TYPE_STRING);
	g_value_take_string (&value, new_location);
	ephy_dialog_set_value (dialog, properties[HOMEPAGE_ENTRY_PROP].id, &value);
	g_value_unset (&value);
}

void
prefs_homepage_current_button_clicked_cb (GtkWidget *button,
					  EphyDialog *dialog)
{
	EphySession *session;
	EphyWindow *window;
	EphyEmbed *embed;
	char *location;

	session = EPHY_SESSION (ephy_shell_get_session (ephy_shell));
	window = ephy_session_get_active_window (session);
	g_return_if_fail (window != NULL);

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	location = ephy_embed_get_location (embed, TRUE);
	set_homepage_entry (dialog, location);
}

void
prefs_homepage_blank_button_clicked_cb (GtkWidget *button,
					EphyDialog *dialog)
{
	set_homepage_entry (dialog, NULL);
}

static void
download_path_response_cb (GtkDialog *fc, gint response, EphyDialog *dialog)
{
	if (response == EPHY_RESPONSE_OPEN)
	{
		char *dir;

		dir = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (fc));
		if (dir != NULL)
		{
			GtkWidget *button;
			char *label;

			eel_gconf_set_path (CONF_STATE_DOWNLOAD_DIR, dir);

			button = ephy_dialog_get_control (dialog, properties[DOWNLOAD_PATH_BUTTON_PROP].id);
			label = get_download_button_label ();
			ephy_ellipsizing_label_set_text ((EphyEllipsizingLabel*) GTK_BIN (button)->child,
							  label);

			g_free (dir);
			g_free (label);
		}
	}

	gtk_widget_destroy (GTK_WIDGET (fc));
}

void
prefs_download_path_button_clicked_cb (GtkWidget *button,
				       PrefsDialog *dialog)
{
	if (dialog->priv->download_dir_chooser == NULL)
	{
		GtkWidget *parent;
		EphyFileChooser *fc;

		parent = ephy_dialog_get_control
			(EPHY_DIALOG (dialog), properties[WINDOW_PROP].id);
	
		fc = ephy_file_chooser_new (_("Select a directory"),
					    GTK_WIDGET (parent),
					    GTK_FILE_CHOOSER_ACTION_OPEN,
					    NULL);
		gtk_file_chooser_set_folder_mode (GTK_FILE_CHOOSER (fc), TRUE);
	
		g_signal_connect (GTK_DIALOG (fc), "response",
				    G_CALLBACK (download_path_response_cb),
				    dialog);
		dialog->priv->download_dir_chooser = GTK_WIDGET (fc);
		g_object_add_weak_pointer
			(G_OBJECT (fc), (gpointer *) &dialog->priv->download_dir_chooser);
	}

	gtk_widget_show (dialog->priv->download_dir_chooser);
}
