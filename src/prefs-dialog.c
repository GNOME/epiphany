/*
 *  Copyright (C) 2000, 2001, 2002 Marco Pesenti Gritti
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "prefs-dialog.h"
#include "ephy-dialog.h"
#include "ephy-prefs.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-single.h"
#include "ephy-shell.h"
#include "ephy-gui.h"
#include "eel-gconf-extensions.h"
#include "language-editor.h"
#include "ephy-langs.h"

#include <bonobo/bonobo-i18n.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkradiobutton.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkimage.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkstock.h>
#include <string.h>

static void
prefs_dialog_class_init (PrefsDialogClass *klass);
static void
prefs_dialog_init (PrefsDialog *pd);
static void
prefs_dialog_finalize (GObject *object);

/* Glade callbacks */
void
prefs_proxy_auto_url_reload_cb (GtkWidget *button,
			        EphyDialog *dialog);
void
prefs_clear_memory_cache_button_clicked_cb (GtkWidget *button,
					    gpointer data);
void
prefs_clear_disk_cache_button_clicked_cb (GtkWidget *button,
					  gpointer data);
void
prefs_dialog_response_cb (GtkDialog *dialog, gint response_id, gpointer data);
void
fonts_language_optionmenu_changed_cb (GtkWidget *optionmenu, EphyDialog *dialog);
void
prefs_homepage_current_button_clicked_cb (GtkWidget *button,
					  EphyDialog *dialog);
void
prefs_homepage_blank_button_clicked_cb (GtkWidget *button,
					EphyDialog *dialog);
void
prefs_language_more_button_clicked_cb (GtkWidget *button,
				       EphyDialog *dialog);

static const
struct
{
	char *name;
	char *code;
}
languages [] =
{
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
	{ N_("Norwegian/Bokmaal"), "nb" },
	{ N_("Norwegian"), "no" },
	{ N_("Polish"), "pl" },
	{ N_("Portuguese"), "pt" },
	{ N_("Portuguese of Brazil"), "pt-BR" },
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
	{ N_("Vietnamian"), "vi" },
	{ N_("Walloon"), "wa" }
};
static guint n_languages = G_N_ELEMENTS (languages);

enum
{
	FONT_TYPE_SERIF,
	FONT_TYPE_SANSSERIF,
	FONT_TYPE_MONOSPACE
};

const
char *fonts_types[] =
{
	"serif",
	"sans-serif",
	"monospace"
};

enum
{
	FONT_SIZE_FIXED,
	FONT_SIZE_VAR,
	FONT_SIZE_MIN
};

const
char *size_prefs[] =
{
	CONF_RENDERING_FONT_FIXED_SIZE,
	CONF_RENDERING_FONT_VAR_SIZE,
	CONF_RENDERING_FONT_MIN_SIZE
};

enum
{
	WINDOW_PROP,
	NOTEBOOK_PROP,

	/* General */
	OPEN_IN_TABS_PROP,
	HOMEPAGE_ENTRY_PROP,
	AUTO_ENCODING_PROP,
	DEFAULT_ENCODING_PROP,
	LANGUAGE_PROP,

	/* Appeareance */
	SERIF_PROP,
	SANSSERIF_PROP,
	MONOSPACE_PROP,
	FIXED_SIZE_PROP,
	VARIABLE_SIZE_PROP,
	MIN_SIZE_PROP,
	PROPORTIONAL_PROP,
	USE_COLORS_PROP,
	USE_FONTS_PROP,

	/* Security */
	ALLOW_POPUPS_PROP,
	ALLOW_JAVA_PROP,
	ALLOW_JS_PROP,
	ACCEPT_COOKIES_PROP,

	/* Advanced */
	CACHE_COMPARE_PROP,
	DISK_CACHE_PROP,
	MEMORY_CACHE_PROP
};

static const
EphyDialogProperty properties [] =
{
	{ WINDOW_PROP, "prefs_dialog", NULL, PT_NORMAL, NULL },
	{ NOTEBOOK_PROP, "prefs_notebook", NULL, PT_NORMAL, NULL },

	/* General */
	{ OPEN_IN_TABS_PROP, "open_in_tabs_checkbutton", CONF_TABS_TABBED, PT_AUTOAPPLY, NULL },
	{ HOMEPAGE_ENTRY_PROP, "homepage_entry", CONF_GENERAL_HOMEPAGE, PT_AUTOAPPLY, NULL },
	{ AUTO_ENCODING_PROP, "auto_encoding_optionmenu", CONF_LANGUAGE_AUTODETECT_ENCODING, PT_AUTOAPPLY, NULL },
	{ DEFAULT_ENCODING_PROP, "default_encoding_optionmenu", NULL, PT_NORMAL, NULL },
	{ LANGUAGE_PROP, "language_optionmenu", NULL, PT_NORMAL, NULL },

	/* Appeareance */
	{ SERIF_PROP, "serif_combo", NULL, PT_NORMAL, NULL },
	{ SANSSERIF_PROP, "sansserif_combo", NULL, PT_NORMAL, NULL },
	{ MONOSPACE_PROP, "monospace_combo", NULL, PT_NORMAL, NULL },
	{ FIXED_SIZE_PROP, "fixed_size_spinbutton", NULL, PT_NORMAL, NULL },
	{ VARIABLE_SIZE_PROP, "variable_size_spinbutton", NULL, PT_NORMAL, NULL },
	{ MIN_SIZE_PROP, "min_size_spinbutton", NULL, PT_NORMAL, NULL },
	{ PROPORTIONAL_PROP, "proportional_optionmenu", CONF_RENDERING_DEFAULT_FONT, PT_AUTOAPPLY, NULL },
	{ USE_COLORS_PROP, "use_colors_checkbutton", CONF_RENDERING_USE_OWN_COLORS, PT_AUTOAPPLY, NULL },
	{ USE_FONTS_PROP, "use_fonts_checkbutton", CONF_RENDERING_USE_OWN_FONTS, PT_AUTOAPPLY, NULL },

	/* Security */
	{ ALLOW_POPUPS_PROP, "popups_allow_checkbutton", CONF_SECURITY_ALLOW_POPUPS, PT_AUTOAPPLY, NULL },
	{ ALLOW_JAVA_PROP, "enable_java_checkbutton", CONF_SECURITY_JAVA_ENABLED, PT_AUTOAPPLY, NULL },
	{ ALLOW_JS_PROP, "enable_javascript_checkbutton", CONF_SECURITY_JAVASCRIPT_ENABLED, PT_AUTOAPPLY, NULL },
	{ ACCEPT_COOKIES_PROP, "cookies_radiobutton", CONF_SECURITY_COOKIES_ACCEPT, PT_AUTOAPPLY, NULL },

	/* Advanced */
	{ CACHE_COMPARE_PROP, "cache_compare_radiobutton", CONF_NETWORK_CACHE_COMPARE, PT_AUTOAPPLY, NULL },
	{ DISK_CACHE_PROP, "disk_cache_spin", CONF_NETWORK_DISK_CACHE, PT_AUTOAPPLY, NULL },
	{ MEMORY_CACHE_PROP, "memory_cache_spin", CONF_NETWORK_MEMORY_CACHE, PT_AUTOAPPLY, NULL },

	{ -1, NULL, NULL }
};

typedef struct
{
	gchar *name;
	gchar *key;
	gchar *code;
} EphyLangItem;

struct PrefsDialogPrivate
{
	GtkWidget *notebook;
	GtkWidget *window;

	GList *langs;
	GList *encodings;

	int language;
	gboolean switching;
};

static GObjectClass *parent_class = NULL;

GType
prefs_dialog_get_type (void)
{
        static GType prefs_dialog_type = 0;

        if (prefs_dialog_type == 0)
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

                prefs_dialog_type = g_type_register_static (EPHY_DIALOG_TYPE,
							    "PrefsDialog",
							    &our_info, 0);
        }

        return prefs_dialog_type;

}

EphyDialog *
prefs_dialog_new (GtkWidget *parent)
{
        EphyDialog *dialog;

        dialog = EPHY_DIALOG (g_object_new (PREFS_DIALOG_TYPE,
					    "ParentWindow", parent,
					    NULL));

        return dialog;
}

static void
prefs_dialog_class_init (PrefsDialogClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = prefs_dialog_finalize;
}

static void
free_lang_item (EphyLangItem *item, gpointer user_data)
{
	if (item == NULL) return;
		
	g_free (item->name);
	g_free (item->key);
	g_free (item->code);
	g_free (item);
}

static void
prefs_dialog_finalize (GObject *object)
{
        PrefsDialog *pd;

        g_return_if_fail (object != NULL);
        g_return_if_fail (IS_PREFS_DIALOG (object));

	pd = PREFS_DIALOG (object);

        g_return_if_fail (pd->priv != NULL);

	g_list_foreach (pd->priv->langs, (GFunc) free_lang_item, NULL);
	g_list_free (pd->priv->langs);

	g_list_foreach (pd->priv->encodings, (GFunc) encoding_info_free, NULL);
	g_list_free (pd->priv->encodings);

        g_free (pd->priv);

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
prefs_dialog_show_help (PrefsDialog *pd)
{
	gint id;

	/* FIXME: Once we actually have documentation we
	 * should point these at the correct links.
	 */
	gchar *help_preferences[] = {
		"setting-preferences",
		"setting-preferences",
		"setting-preferences",
		"setting-preferences"
	};

	id = gtk_notebook_get_current_page (GTK_NOTEBOOK (pd->priv->notebook));

	ephy_gui_help (GTK_WINDOW (pd), "epiphany", help_preferences[id]);
}

static void
setup_font_menu (PrefsDialog *dialog,
		 const char *type,
		 GtkWidget *combo)
{
	char *default_font;
	GList *fonts;
	const char *name;
	char key[255];
	int pos;
	GtkWidget *entry = GTK_COMBO(combo)->entry;
	EphyEmbedSingle *single;

	single = ephy_embed_shell_get_embed_single
		(EPHY_EMBED_SHELL (ephy_shell));

	ephy_embed_single_get_font_list (single,
					 lang_encode_item[dialog->priv->language],
					 type, &fonts, &default_font);

	/* Get the default font */
	sprintf (key, "%s_%s_%s", CONF_RENDERING_FONT, type,
		 lang_encode_item[dialog->priv->language]);
	name = eel_gconf_get_string (key);
	if (name == NULL)
	{
		name = default_font;
	}

	/* set popdown doesnt like NULL */
	if (fonts == NULL)
	{
		fonts = g_list_alloc ();
	}

	gtk_combo_set_popdown_strings (GTK_COMBO(combo), fonts);

	/* set the default value */
	if (name != NULL)
	{
		gtk_editable_delete_text (GTK_EDITABLE(entry), 0, -1);
		gtk_editable_insert_text (GTK_EDITABLE(entry),
					  name, g_utf8_strlen (name, -1),
					  &pos);
	}

	g_free (default_font);

	g_list_foreach (fonts, (GFunc)g_free, NULL);
	g_list_free (fonts);
}

static void
save_font_menu (PrefsDialog *dialog,
		int type,
		GtkWidget *entry)
{
	char *name;
	char key[255];

	name = gtk_editable_get_chars
		(GTK_EDITABLE(entry), 0, -1);

	/* do not save empty fonts */
	if (!name || *name == '\0')
	{
		g_free (name);
		return;
	}

	sprintf (key, "%s_%s_%s", CONF_RENDERING_FONT,
		 fonts_types[type],
		 lang_encode_item[dialog->priv->language]);
	eel_gconf_set_string (key, name);
	g_free (name);
}

static void
font_entry_changed_cb (GtkWidget *entry, PrefsDialog *dialog)
{
	int type;

	if (dialog->priv->switching) return;

	type = GPOINTER_TO_INT (g_object_get_data (G_OBJECT(entry),
						    "type"));
	save_font_menu (dialog, type, entry);
}

static void
attach_font_signal (PrefsDialog *dialog, int prop,
		    gpointer type)
{
	GtkWidget *combo;
	GtkWidget *entry;

	combo = ephy_dialog_get_control (EPHY_DIALOG(dialog),
					   prop);
	entry = GTK_COMBO(combo)->entry;
	g_object_set_data (G_OBJECT(entry), "type", type);
	g_signal_connect (entry, "changed",
			  G_CALLBACK(font_entry_changed_cb),
			  dialog);
}

static void
attach_fonts_signals (PrefsDialog *dialog)
{
	attach_font_signal (dialog, SERIF_PROP,
			    GINT_TO_POINTER(FONT_TYPE_SERIF));
	attach_font_signal (dialog, SANSSERIF_PROP,
			    GINT_TO_POINTER(FONT_TYPE_SANSSERIF));
	attach_font_signal (dialog, MONOSPACE_PROP,
			    GINT_TO_POINTER(FONT_TYPE_MONOSPACE));
}

static void
size_spinbutton_changed_cb (GtkWidget *spin, PrefsDialog *dialog)
{
	int type;
	char key[255];

	if (dialog->priv->switching) return;

	type = GPOINTER_TO_INT(g_object_get_data (G_OBJECT(spin), "type"));

	sprintf (key, "%s_%s",
		 size_prefs[type],
		 lang_encode_item[dialog->priv->language]);
	eel_gconf_set_integer (key, gtk_spin_button_get_value (GTK_SPIN_BUTTON (spin)));
}

static void
attach_size_controls_signals (PrefsDialog *dialog)
{
	GtkWidget *spin;

	spin = ephy_dialog_get_control (EPHY_DIALOG(dialog),
					  FIXED_SIZE_PROP);
	g_object_set_data (G_OBJECT(spin), "type",
			   GINT_TO_POINTER(FONT_SIZE_FIXED));
	g_signal_connect (spin, "value_changed",
			  G_CALLBACK(size_spinbutton_changed_cb),
			  dialog);

	spin = ephy_dialog_get_control (EPHY_DIALOG(dialog),
					  VARIABLE_SIZE_PROP);
	g_object_set_data (G_OBJECT(spin), "type",
			   GINT_TO_POINTER(FONT_SIZE_VAR));
	g_signal_connect (spin, "value_changed",
			  G_CALLBACK(size_spinbutton_changed_cb),
			  dialog);

	spin = ephy_dialog_get_control (EPHY_DIALOG(dialog),
					  MIN_SIZE_PROP);
	g_object_set_data (G_OBJECT(spin), "type",
			   GINT_TO_POINTER(FONT_SIZE_MIN));
	g_signal_connect (spin, "value_changed",
			  G_CALLBACK(size_spinbutton_changed_cb),
			  dialog);
}

static void
setup_size_control (PrefsDialog *dialog,
		    const char *pref,
		    int default_size,
		    GtkWidget *spin)
{
	char key[255];
	int size;

	sprintf (key, "%s_%s", pref,
		 lang_encode_item[dialog->priv->language]);
	size = eel_gconf_get_integer (key);

	if (size == 0) size = default_size;

	gtk_spin_button_set_value (GTK_SPIN_BUTTON(spin), size);
}

static void
setup_size_controls (PrefsDialog *dialog)
{
	GtkWidget *spin;

	spin = ephy_dialog_get_control (EPHY_DIALOG(dialog),
					  FIXED_SIZE_PROP);
	setup_size_control (dialog, CONF_RENDERING_FONT_FIXED_SIZE, 12, spin);

	spin = ephy_dialog_get_control (EPHY_DIALOG(dialog),
					  VARIABLE_SIZE_PROP);
	setup_size_control (dialog, CONF_RENDERING_FONT_VAR_SIZE, 16, spin);

	spin = ephy_dialog_get_control (EPHY_DIALOG(dialog),
					  MIN_SIZE_PROP);
	setup_size_control (dialog, CONF_RENDERING_FONT_MIN_SIZE, 0, spin);
}

static void
setup_fonts (PrefsDialog *dialog)
{
	GtkWidget *combo;

	dialog->priv->switching = TRUE;

	combo = ephy_dialog_get_control (EPHY_DIALOG(dialog),
					 SERIF_PROP);
	setup_font_menu (dialog, "serif", combo);

	combo = ephy_dialog_get_control (EPHY_DIALOG(dialog),
					 SANSSERIF_PROP);
	setup_font_menu (dialog, "sans-serif", combo);

	combo = ephy_dialog_get_control (EPHY_DIALOG(dialog),
					 MONOSPACE_PROP);
	setup_font_menu (dialog, "monospace", combo);

	dialog->priv->switching = FALSE;
}

static void
default_encoding_menu_changed_cb (GtkOptionMenu *option_menu,
				  PrefsDialog *dialog)
{
	GList *encoding;
	gint i;
	EncodingInfo *info;

	i = gtk_option_menu_get_history (option_menu);
	encoding = g_list_nth (dialog->priv->encodings, i);
	g_assert (encoding != NULL);

	info = (EncodingInfo *) encoding->data;
	eel_gconf_set_string (CONF_LANGUAGE_DEFAULT_ENCODING, info->encoding);
}

static gint
find_encoding_in_list_cmp (const EncodingInfo *info, const gchar *encoding)
{
	return strcmp (info->encoding, encoding);
}

static void
create_default_encoding_menu (PrefsDialog *dialog)
{
	GList *l;
	GtkWidget *menu, *optionmenu;
	gchar *encoding;
	EphyEmbedSingle *single;

	single = ephy_embed_shell_get_embed_single
		(EPHY_EMBED_SHELL (ephy_shell));

	ephy_embed_single_get_encodings (single, LG_ALL, TRUE,
					 &dialog->priv->encodings);

	menu = gtk_menu_new ();

	optionmenu = ephy_dialog_get_control (EPHY_DIALOG (dialog),
					      DEFAULT_ENCODING_PROP);

	for (l = dialog->priv->encodings; l != NULL; l = l->next)
	{
		EncodingInfo *info = (EncodingInfo *) l->data;
		GtkWidget *item;

		item = gtk_menu_item_new_with_label (info->title);
		gtk_menu_shell_append (GTK_MENU_SHELL(menu), item);
		gtk_widget_show (item);
	}

	gtk_option_menu_set_menu (GTK_OPTION_MENU(optionmenu), menu);

	/* init value */
	encoding = eel_gconf_get_string (CONF_LANGUAGE_DEFAULT_ENCODING);
	/* fallback */
	if (encoding == NULL) encoding = g_strdup ("ISO-8859-1");

	l = g_list_find_custom (dialog->priv->encodings, encoding,
				(GCompareFunc) find_encoding_in_list_cmp);
	gtk_option_menu_set_history (GTK_OPTION_MENU(optionmenu),
				     g_list_position (dialog->priv->encodings, l));
	g_free (encoding);

	g_signal_connect (optionmenu, "changed",
			  G_CALLBACK (default_encoding_menu_changed_cb),
			  dialog);
}

static gint
compare_lang_items (const EphyLangItem *i1, const EphyLangItem *i2)
{
	return strcmp (i1->key, i2->key);
}

static gint
find_lang_code (const EphyLangItem *i1, const gchar *code)
{
	return strcmp (i1->code, code);
}

static void
create_languages_list (PrefsDialog *dialog)
{
	GList *list = NULL, *lang;
	GSList *pref_list, *l;
	EphyLangItem *item;
	const gchar *code;
	guint i;

	for (i = 0; i < n_languages; i++)
	{		
		item = g_new0 (EphyLangItem, 1);

		item->name = g_strdup (_(languages[i].name));
		item->key  = g_utf8_collate_key (item->name, -1);
		item->code = g_strdup (languages[i].code);

		list = g_list_prepend (list, item);
	}

	/* add custom languages */
	pref_list = eel_gconf_get_string_list (CONF_RENDERING_LANGUAGE);

	for (l = pref_list; l != NULL; l = l->next)
	{
		code = (const gchar*) l->data;

		lang = g_list_find_custom (list, code,
					   (GCompareFunc) find_lang_code);

		if (lang == NULL)
		{
			/* not found in list */
			item = g_new0 (EphyLangItem, 1);

			item->name = g_strdup_printf (_("Custom [%s]"), code);
			item->key  = g_utf8_collate_key (item->name, -1);
			item->code = g_strdup (code);

			list = g_list_prepend (list, item);
		}
	}

	if (pref_list)
	{
		g_slist_foreach (pref_list, (GFunc) g_free, NULL);
		g_slist_free (pref_list);
	}

	list = g_list_sort (list, (GCompareFunc) compare_lang_items);

	dialog->priv->langs = list;
}

static GtkWidget *
general_prefs_new_language_menu (PrefsDialog *dialog)
{
	GList *l;
	GtkWidget *menu;
	EphyLangItem *li;

	menu = gtk_menu_new ();

	for (l = dialog->priv->langs; l != NULL; l = l->next)
	{
		GtkWidget *item;

		li = (EphyLangItem*) l->data;
		item = gtk_menu_item_new_with_label (li->name);
		gtk_menu_shell_append (GTK_MENU_SHELL(menu), item);
		gtk_widget_show (item);
		g_object_set_data (G_OBJECT (item), "desc", li->name);
	}

	return menu;
}

static void
language_menu_changed_cb (GtkOptionMenu *option_menu,
		          gpointer data)
{
	gint i;
	GSList *list = NULL;
	GList *lang = NULL;

	g_return_if_fail (IS_PREFS_DIALOG (data));

	list = eel_gconf_get_string_list (CONF_RENDERING_LANGUAGE);
	g_return_if_fail (list != NULL);

	/* Subst the first item according to the optionmenu */
	i = gtk_option_menu_get_history (option_menu);

	lang = g_list_nth (PREFS_DIALOG (data)->priv->langs, i);

	if (lang)
	{
		g_free (list->data);
		list->data = g_strdup (((EphyLangItem *) lang->data)->code);

		eel_gconf_set_string_list (CONF_RENDERING_LANGUAGE, list);
	}

	g_slist_foreach (list, (GFunc) g_free, NULL);
	g_slist_free (list);
}

static void
create_language_menu (PrefsDialog *dialog)
{
	GtkWidget *optionmenu;
	GtkWidget *menu;
	const gchar *code;
	gint i = 0;
	GSList *list;
	GList *lang;

	optionmenu = ephy_dialog_get_control (EPHY_DIALOG (dialog),
					      LANGUAGE_PROP);

	menu = general_prefs_new_language_menu (dialog);

	gtk_option_menu_set_menu (GTK_OPTION_MENU(optionmenu), menu);

	/* init value from first element of the list */
	list = eel_gconf_get_string_list (CONF_RENDERING_LANGUAGE);
	g_return_if_fail (list != NULL); /* FIXME: doesn't connect the handler! */

	code = (const gchar *) list->data;
	lang = g_list_find_custom (dialog->priv->langs, code,
				   (GCompareFunc)find_lang_code);

	if (lang)
	{
		i = g_list_position (dialog->priv->langs, lang);
	}

	gtk_option_menu_set_history (GTK_OPTION_MENU(optionmenu), i);

	g_slist_foreach (list, (GFunc) g_free, NULL);
	g_slist_free (list);

	g_signal_connect (optionmenu, "changed",
			  G_CALLBACK (language_menu_changed_cb),
			  dialog);
}

static void
set_homepage_entry (EphyDialog *dialog,
		    const char *new_location)
{
	GtkWidget *entry;
	int pos;

	entry = ephy_dialog_get_control (dialog, HOMEPAGE_ENTRY_PROP);

	gtk_editable_delete_text (GTK_EDITABLE (entry), 0, -1);
	gtk_editable_insert_text (GTK_EDITABLE (entry), new_location,
				  g_utf8_strlen (new_location, -1),
				  &pos);
}


static void
prefs_dialog_init (PrefsDialog *pd)
{
	EphyDialog *dialog = EPHY_DIALOG (pd);
	GdkPixbuf *icon;

	pd->priv = g_new0 (PrefsDialogPrivate, 1);

	ephy_dialog_construct (EPHY_DIALOG (pd),
			       properties,
			       "prefs-dialog.glade",
			       "prefs_dialog");

	pd->priv->window = ephy_dialog_get_control (dialog, WINDOW_PROP);
	pd->priv->notebook = ephy_dialog_get_control (dialog, NOTEBOOK_PROP);
	pd->priv->langs = NULL;
	pd->priv->encodings = NULL;

	icon = gtk_widget_render_icon (pd->priv->window,
				       GTK_STOCK_PREFERENCES,
				       GTK_ICON_SIZE_MENU,
				       "prefs_dialog");
	gtk_window_set_icon (GTK_WINDOW (pd->priv->window), icon);
	g_object_unref(icon);

	pd->priv->switching = FALSE;
	setup_fonts (pd);
	setup_size_controls (pd);
	attach_fonts_signals (pd);
	attach_size_controls_signals (pd);
	create_languages_list (pd);
	create_default_encoding_menu (pd);
	create_language_menu (pd);
}

/* Network page callbacks */

void
prefs_dialog_response_cb (GtkDialog *dialog, gint response_id, gpointer data)
{
	if (response_id == GTK_RESPONSE_CLOSE)
	{
		gtk_widget_destroy (GTK_WIDGET(dialog));
	}
	else if (response_id == GTK_RESPONSE_HELP)
	{
		PrefsDialog *pd = (PrefsDialog *)data;
		prefs_dialog_show_help (pd);
	}
}

void
prefs_clear_memory_cache_button_clicked_cb (GtkWidget *button,
					    gpointer data)
{
	EphyEmbedSingle *single;

	single = ephy_embed_shell_get_embed_single
		(EPHY_EMBED_SHELL (ephy_shell));

	ephy_embed_single_clear_cache (single, MEMORY_CACHE);
}

void
prefs_clear_disk_cache_button_clicked_cb (GtkWidget *button,
					  gpointer data)
{
	EphyEmbedSingle *single;

	single = ephy_embed_shell_get_embed_single
		(EPHY_EMBED_SHELL (ephy_shell));

	ephy_embed_single_clear_cache (single, DISK_CACHE);
}

void
fonts_language_optionmenu_changed_cb (GtkWidget *optionmenu,
				      EphyDialog *dialog)
{
	int i;
	PrefsDialog *pd = PREFS_DIALOG (dialog);

	i = gtk_option_menu_get_history
		(GTK_OPTION_MENU (optionmenu));

	pd->priv->language = i;

	setup_fonts (pd);
	setup_size_controls (pd);
}

void
prefs_homepage_current_button_clicked_cb (GtkWidget *button,
					  EphyDialog *dialog)
{
	EphyWindow *window;
	EphyTab *tab;

	window = ephy_shell_get_active_window (ephy_shell);
	g_return_if_fail (window != NULL);

	tab = ephy_window_get_active_tab (window);
	g_return_if_fail (tab != NULL);

	set_homepage_entry (dialog, ephy_tab_get_location (tab));
}

void
prefs_homepage_blank_button_clicked_cb (GtkWidget *button,
					EphyDialog *dialog)
{
	set_homepage_entry (dialog, "about:blank");
}

static void
fill_language_editor (LanguageEditor *le, PrefsDialog *dialog)
{
	GSList *strings;
	GSList *tmp;
	GList *lang;
	gint i;
	const gchar *code;
	EphyLangItem *li;

	strings = eel_gconf_get_string_list (CONF_RENDERING_LANGUAGE);
	g_return_if_fail (strings != NULL);

	for (tmp = strings; tmp != NULL; tmp = g_slist_next (tmp))
	{
		code = (const gchar *) tmp->data;

		lang = g_list_find_custom (dialog->priv->langs, code,
					   (GCompareFunc) find_lang_code);

		if (lang)
		{
			i = g_list_position (dialog->priv->langs, lang);
			li = (EphyLangItem *) lang->data;
			
			language_editor_add (le, li->name, i);
		}
	}
	
	g_slist_foreach (strings, (GFunc) g_free, NULL);
	g_slist_free (strings);
}

static void
language_dialog_changed_cb (LanguageEditor *le,
			    GSList *list,
			    PrefsDialog *dialog)
{
	GtkWidget *optionmenu;
	const GSList *l;
	GSList *langs = NULL;
	GList *lang;
	gint i;
	EphyLangItem *li;

	optionmenu = ephy_dialog_get_control (EPHY_DIALOG (dialog),
						LANGUAGE_PROP);
	gtk_option_menu_set_history (GTK_OPTION_MENU(optionmenu),
				     GPOINTER_TO_INT(list->data));

	for (l = list; l != NULL; l = l->next)
	{
		i = GPOINTER_TO_INT (l->data);
		lang = g_list_nth (dialog->priv->langs, i);

		if (lang)
		{
			li = (EphyLangItem *) lang->data;

			langs = g_slist_append (langs, li->code);
		}
	}

	eel_gconf_set_string_list (CONF_RENDERING_LANGUAGE, langs);
	g_slist_free (langs);
}

void
prefs_language_more_button_clicked_cb (GtkWidget *button,
				       EphyDialog *dialog)
{
	LanguageEditor *editor;
	GtkWidget *menu;
	GtkWidget *toplevel;

	menu = general_prefs_new_language_menu (PREFS_DIALOG (dialog));

	toplevel = gtk_widget_get_toplevel (button);
	editor = language_editor_new (toplevel);
	language_editor_set_menu (editor, menu);
	fill_language_editor (editor, PREFS_DIALOG (dialog));
	ephy_dialog_set_modal (EPHY_DIALOG(editor), TRUE);

	g_signal_connect (editor, "changed",
			  G_CALLBACK(language_dialog_changed_cb),
			  dialog);

	ephy_dialog_show (EPHY_DIALOG(editor));
}
