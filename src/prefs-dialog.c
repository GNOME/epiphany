/*
 *  Copyright (C) 200-2003 Marco Pesenti Gritti
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
#include "language-editor.h"
#include "ephy-langs.h"
#include "ephy-encodings.h"
#include "ephy-debug.h"
#include "ephy-ellipsizing-label.h"
#include "ephy-file-chooser.h"
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
						 EphyDialog *dialog);

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
	LANGUAGE_PROP,
	LANGUAGE_LABEL_PROP,
	DEFAULT_ENCODING_LABEL_PROP,
	AUTO_ENCODING_LABEL_PROP
};

static const
EphyDialogProperty properties [] =
{
	{ "prefs_dialog",	NULL, PT_NORMAL, 0},
	{ "prefs_notebook",	NULL, PT_NORMAL, 0},

	/* General */
	{ "homepage_entry",			CONF_GENERAL_HOMEPAGE,	  PT_AUTOAPPLY,	G_TYPE_STRING },
	{ "auto_open_downloads_checkbutton",	CONF_AUTO_OPEN_DOWNLOADS, PT_AUTOAPPLY,	0 },
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
	{ "language_combo",		NULL,					PT_NORMAL,	G_TYPE_STRING },
	{ "language_label",		NULL,					PT_NORMAL,	0 },
	{ "default_encoding_label",	NULL,					PT_NORMAL,	0 },
	{ "auto_encoding_label",	NULL,					PT_NORMAL,	0 },

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
prefs_dialog_class_init (PrefsDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	g_type_class_add_private (object_class, sizeof(PrefsDialogPrivate));
}

static void
prefs_dialog_show_help (EphyDialog *dialog)
{
	GtkWidget *window, *notebook;
	int id;

	/* FIXME: Once we actually have documentation we
	 * should point these at the correct links.
	 */
	char *help_preferences[] = {
		"setting-preferences",
		"setting-preferences",
		"setting-preferences",
		"setting-preferences"
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
	GtkTreeIter iter;
	GList *fonts, *l;
	char *name;
	char key[255];
	EphyEmbedSingle *single;

	single = ephy_embed_shell_get_embed_single
		(EPHY_EMBED_SHELL (ephy_shell));
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

	ephy_dialog_set_pref (dialog, properties[prop].id, NULL);

	gtk_combo_box_set_model (GTK_COMBO_BOX (combo), GTK_TREE_MODEL (store));

	ephy_dialog_set_pref (dialog, properties[prop].id, key);
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
}

static void
create_node_combo (EphyDialog *dialog,
		   int prop,
		   EphyEncodings *encodings,
		   EphyNode *node,
		   const char *key,
		   const char *default_value)
{
	EphyTreeModelNode *node_model;
	GtkTreeModel *sort_model;
	GtkComboBox *combo;
	GtkCellRenderer *renderer;
	char *code;
	int title_col, data_col;

	code = eel_gconf_get_string (key);
	if (code == NULL || ephy_encodings_get_node (encodings, code) == NULL)
	{
		/* safe default */
		eel_gconf_set_string (key, default_value);
	}
	g_free (code);

	combo = GTK_COMBO_BOX (ephy_dialog_get_control (dialog, properties[prop].id));

	node_model = ephy_tree_model_node_new (node, NULL);

	title_col = ephy_tree_model_node_add_prop_column
			(node_model, G_TYPE_STRING, EPHY_NODE_ENCODING_PROP_TITLE_ELIDED);
	data_col = ephy_tree_model_node_add_prop_column
			(node_model, G_TYPE_STRING, EPHY_NODE_ENCODING_PROP_ENCODING);

	sort_model = ephy_tree_model_sort_new (GTK_TREE_MODEL (node_model));

	gtk_tree_sortable_set_sort_column_id
		(GTK_TREE_SORTABLE (sort_model), title_col, GTK_SORT_ASCENDING);

	gtk_combo_box_set_model (combo, GTK_TREE_MODEL (sort_model));

        renderer = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer,
                                        "text", title_col,
                                        NULL);

	ephy_dialog_set_data_column (dialog, properties[prop].id, data_col);
}

static void
language_combo_changed_cb (GtkComboBox *combo,
			   EphyDialog *dialog)
{
	GValue value = { 0, };
	GSList *list;

	list = eel_gconf_get_string_list (CONF_RENDERING_LANGUAGE);
	/* delete the first list item */
	list = g_slist_remove_link (list, list);

	ephy_dialog_get_value (dialog, properties[LANGUAGE_PROP].id, &value);

	/* add the new language code upfront */
	list = g_slist_prepend (list, g_value_dup_string (&value));
	g_value_unset (&value);

	eel_gconf_set_string_list (CONF_RENDERING_LANGUAGE, list);

	g_slist_foreach (list, (GFunc) g_free, NULL);
	g_slist_free (list);
}

static void
create_language_menu (EphyDialog *dialog)
{
	GtkComboBox *combo;
	GtkListStore *store;
	GtkTreeModel *sortmodel;
	GtkTreeIter iter;
	GtkCellRenderer *renderer;
	int i;
	GSList *list, *l, *ulist = NULL;

	/* init value from first element of the list */
	list = eel_gconf_get_string_list (CONF_RENDERING_LANGUAGE);

	/* uniquify list */
	for (l = list; l != NULL; l = l->next)
	{
		if (g_slist_find_custom (ulist, l->data, (GCompareFunc) strcmp) == NULL)
		{
			ulist = g_slist_prepend (ulist, l->data);
		}
		else
		{
			g_free (l->data);
		}
	}
	g_slist_free (list);
	list = g_slist_reverse (ulist);
	eel_gconf_set_string_list (CONF_RENDERING_LANGUAGE, list);

	combo = GTK_COMBO_BOX (ephy_dialog_get_control
			(dialog, properties[LANGUAGE_PROP].id));

	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

	for (i = 0; i < n_languages; i++)
	{
		gtk_list_store_append (store, &iter);

		gtk_list_store_set (store, &iter,
				    COL_LANG_NAME, _(languages[i].name),
				    COL_LANG_CODE, languages[i].code,
				    -1);
	}

	/* add additional list items */
	for (l = list; l != NULL; l = l->next)
	{
		const char *code = (const char *) l->data;
		int i;

		if (code == NULL) continue;

		for (i = 0; i < n_languages; i++)
		{
			if (strcmp (languages[i].code, code) == 0) break;
		}

		/* code isn't in stock list */
		if (i == n_languages)
		{
			char *text;

			text = g_strdup_printf (_("Custom [%s]"), code);
			gtk_list_store_append (store, &iter);
			gtk_list_store_set (store, &iter,
					    COL_LANG_NAME, text,
					    COL_LANG_CODE, code,
					    -1);
			g_free (text);
		}
	}

	sortmodel = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (store));
	gtk_tree_sortable_set_sort_column_id
		(GTK_TREE_SORTABLE (sortmodel), COL_LANG_NAME, GTK_SORT_ASCENDING);

	gtk_combo_box_set_model (combo, sortmodel);

	ephy_dialog_set_data_column
		(dialog, properties[LANGUAGE_PROP].id, COL_LANG_CODE);

        renderer = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer,
                                        "text", COL_LANG_NAME,
                                        NULL);

	g_signal_connect (combo, "changed",
			  G_CALLBACK (language_combo_changed_cb), dialog);

	/* set combo from first element of the list */
	if (list && list->data)
	{
		const char *code = (const char *) list->data;
		GValue value = { 0, };

		g_value_init (&value, G_TYPE_STRING);
		g_value_set_string (&value, code);
		ephy_dialog_set_value (dialog, properties[LANGUAGE_PROP].id, &value);
		g_value_unset (&value);
	}

	g_slist_foreach (list, (GFunc) g_free, NULL);
	g_slist_free (list);
}

static char*
get_download_button_label ()
{
	char *label, *key, *desktop_path, *tmp;

	key = eel_gconf_get_string (CONF_STATE_DOWNLOAD_DIR);
	tmp = g_build_filename (g_get_home_dir (), "Desktop", NULL);
	desktop_path = g_filename_to_utf8 (tmp, -1, NULL, NULL, NULL);
	g_free (tmp);
	if (g_utf8_collate (key, desktop_path) == 0)
	{
		g_free (key);
		label = g_strdup (_("Desktop")); 
	}
	else
	{
		label = key;
	}

	g_free (desktop_path);
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
}
	
static void
prefs_dialog_init (PrefsDialog *pd)
{
	EphyDialog *dialog = EPHY_DIALOG (pd);
	EphyEncodings *encodings;
	GtkWidget *window;
	GdkPixbuf *icon;
	GtkCellRenderer *renderer;
	GtkWidget *combo;

	pd->priv = EPHY_PREFS_DIALOG_GET_PRIVATE (pd);

	ephy_dialog_construct (dialog,
			       properties,
			       "prefs-dialog.glade",
			       "prefs_dialog");

	ephy_dialog_add_enum (dialog, properties[ACCEPT_COOKIES_PROP].id,
			      n_cookies_accept_enum, cookies_accept_enum);

	ephy_dialog_set_size_group (dialog,
				    properties[LANGUAGE_LABEL_PROP].id,
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

	create_language_menu (dialog);

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

	single = ephy_embed_shell_get_embed_single
		(EPHY_EMBED_SHELL (ephy_shell));

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
language_dialog_changed_cb (LanguageEditor *le,
			    GSList *list,
			    EphyDialog *dialog)
{
	LOG ("language_dialog_changed_cb")

	if (list && list->data)
	{
		GValue value = { 0, };

		g_value_init (&value, G_TYPE_STRING);
		g_value_set_string (&value, (char *) list->data);
		ephy_dialog_set_value (dialog, properties[LANGUAGE_PROP].id, &value);
		g_value_unset (&value);
	}

	eel_gconf_set_string_list (CONF_RENDERING_LANGUAGE, list);
}

void
prefs_language_more_button_clicked_cb (GtkWidget *button,
				       EphyDialog *dialog)
{
	LanguageEditor *editor;
	GtkWidget *window, *combo;
	GtkTreeModel *model;
	GSList *codes, *l;

	window = ephy_dialog_get_control (dialog, properties[WINDOW_PROP].id);
	combo = ephy_dialog_get_control (dialog, properties[LANGUAGE_PROP].id);
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));

	editor = language_editor_new (window);
	language_editor_set_model (editor, model);

	codes = eel_gconf_get_string_list (CONF_RENDERING_LANGUAGE);
	for (l = codes; l != NULL; l = l->next)
	{
		const char *code = (const char *) l->data;
		int i;

		if (code == NULL) continue;

		for (i = 0; i < n_languages; i++)
		{
			if (strcmp (languages[i].code, code) == 0) break;
		}

		if (i == n_languages)
		{
			char *desc;
			desc = g_strdup_printf (_("Custom [%s]"), code);
			language_editor_add (editor, code, desc);\
			g_free (desc);
		}
		else
		{
			language_editor_add (editor, code, _(languages[i].name));
		}
	}
	g_slist_foreach (codes, (GFunc) g_free, NULL);
	g_slist_free (codes);

	/* FIXME: make it only modal to prefs dialogue, not to all windows */
	ephy_dialog_set_modal (EPHY_DIALOG (editor), TRUE);

	g_signal_connect (editor, "list-changed",
			  G_CALLBACK (language_dialog_changed_cb),
			  dialog);

	ephy_dialog_show (EPHY_DIALOG (editor));
}

static void
download_path_response_cb (GtkDialog *fc, gint response, EphyDialog *dialog)
{
	if (response == EPHY_RESPONSE_OPEN)
	{
		char *dir;

		dir = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (fc));
		if (dir != NULL)
		{
			GtkWidget *button;
			char *label;

			eel_gconf_set_string (CONF_STATE_DOWNLOAD_DIR, dir);
			
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
				       EphyDialog *dialog)
{
	GtkWidget *parent;
	EphyFileChooser *fc;

	parent = ephy_dialog_get_control (dialog, properties[WINDOW_PROP].id);

	fc = ephy_file_chooser_new (_("Select a directory"),
				    GTK_WIDGET (parent),
				    GTK_FILE_CHOOSER_ACTION_OPEN,
				    NULL);
	gtk_file_chooser_set_folder_mode (GTK_FILE_CHOOSER (fc), TRUE);

	g_signal_connect (GTK_DIALOG (fc), "response",
			    G_CALLBACK (download_path_response_cb),
			    dialog);

	gtk_widget_show (GTK_WIDGET (fc));
}
