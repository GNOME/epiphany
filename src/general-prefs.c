/*
 *  Copyright (C) 2002 Marco Pesenti Gritti
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

#include "general-prefs.h"
#include "ephy-shell.h"
#include "ephy-prefs.h"
#include "ephy-embed-prefs.h"
#include "ephy-shell.h"
#include "eel-gconf-extensions.h"
#include "language-editor.h"

#include <string.h>
#include <gtk/gtkeditable.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkmenuitem.h>
#include <bonobo/bonobo-i18n.h>

static void general_prefs_class_init (GeneralPrefsClass *klass);
static void general_prefs_init (GeneralPrefs *dialog);
static void general_prefs_finalize (GObject *object);

/* Glade callbacks */
void
prefs_homepage_current_button_clicked_cb (GtkWidget *button,
					  EphyDialog *dialog);
void
prefs_homepage_blank_button_clicked_cb (GtkWidget *button,
					EphyDialog *dialog);
void
prefs_language_more_button_clicked_cb (GtkWidget *button,
				       EphyDialog *dialog);

static GObjectClass *parent_class = NULL;

struct GeneralPrefsPrivate
{
	gpointer dummy;
};

enum
{
	HOMEPAGE_ENTRY_PROP,
	NEW_PAGE_PROP,
	AUTOCHARSET_PROP,
	DEFAULT_CHARSET_PROP,
	LANGUAGE_PROP
};

static const
EphyDialogProperty properties [] =
{
	{ HOMEPAGE_ENTRY_PROP, "homepage_entry", CONF_GENERAL_HOMEPAGE, PT_AUTOAPPLY, NULL },
	{ NEW_PAGE_PROP, "new_page_show_homepage", CONF_GENERAL_NEWPAGE_TYPE, PT_AUTOAPPLY, NULL },
	{ AUTOCHARSET_PROP, "autocharset_optionmenu", CONF_LANGUAGE_AUTODETECT_CHARSET, PT_AUTOAPPLY, NULL },
	{ DEFAULT_CHARSET_PROP, "default_charset_optionmenu", NULL, PT_NORMAL, NULL },
	{ LANGUAGE_PROP, "language_optionmenu", NULL, PT_NORMAL, NULL },

	{ -1, NULL, NULL }
};

static const
struct
{
	char *name;
	char *code;
}
languages [] =
{
	{ _("Afrikaans"), "ak" },
	{ _("Albanian"), "sq" },
	{ _("Arabic"), "ar" },
	{ _("Azerbaijani"), "az" },
	{ _("Basque"), "eu" },
	{ _("Breton"), "br" },
	{ _("Bulgarian"), "bg" },
	{ _("Byelorussian"), "be" },
	{ _("Catalan"), "ca" },
	{ _("Chinese"), "zh" },
	{ _("Croatian"), "hr" },
	{ _("Czech"), "cs" },
	{ _("Danish"), "da" },
	{ _("Dutch"), "nl" },
	{ _("English"), "en" },
	{ _("Esperanto"), "eo" },
	{ _("Estonian"), "et" },
	{ _("Faeroese"), "fo" },
	{ _("Finnish"), "fi" },
	{ _("French"), "fr" },
	{ _("Galician"), "gl" },
	{ _("German"), "de" },
	{ _("Greek"), "el" },
	{ _("Hebrew"), "he" },
	{ _("Hungarian"), "hu" },
	{ _("Icelandic"), "is" },
	{ _("Indonesian"), "id" },
	{ _("Irish"), "ga" },
	{ _("Italian"), "it" },
	{ _("Japanese"), "ja" },
	{ _("Korean"), "ko" },
	{ _("Latvian"), "lv" },
	{ _("Lithuanian"), "lt" },
	{ _("Macedonian"), "mk" },
	{ _("Malay"), "ms" },
	{ _("Norwegian/Nynorsk"), "nn" },
	{ _("Norwegian/Bokmaal"), "nb" },
	{ _("Norwegian"), "no" },
	{ _("Polish"), "pl" },
	{ _("Portuguese"), "pt" },
	{ _("Portuguese of Brazil"), "pt-BR" },
	{ _("Romanian"), "ro" },
	{ _("Russian"), "ru" },
	{ _("Scottish"), "gd" },
	{ _("Serbian"), "sr" },
	{ _("Slovak"), "sk" },
	{ _("Slovenian"), "sl" },
	{ _("Spanish"), "es" },
	{ _("Swedish"), "sv" },
	{ _("Tamil"), "ta" },
	{ _("Turkish"), "tr" },
	{ _("Ukrainian"), "uk" },
	{ _("Vietnamian"), "vi" },
	{ _("Walloon"), "wa" },
	{ NULL, NULL }
};

GType
general_prefs_get_type (void)
{
        static GType general_prefs_type = 0;

        if (general_prefs_type == 0)
        {
                static const GTypeInfo our_info =
                {
                        sizeof (GeneralPrefsClass),
                        NULL, /* base_init */
                        NULL, /* base_finalize */
                        (GClassInitFunc) general_prefs_class_init,
                        NULL,
                        NULL, /* class_data */
                        sizeof (GeneralPrefs),
                        0, /* n_preallocs */
                        (GInstanceInitFunc) general_prefs_init
                };

                general_prefs_type = g_type_register_static (EPHY_DIALOG_TYPE,
						              "GeneralPrefs",
						              &our_info, 0);
        }

        return general_prefs_type;

}

static void
general_prefs_class_init (GeneralPrefsClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = general_prefs_finalize;
}

static void
default_charset_menu_changed_cb (GtkOptionMenu *option_menu,
				 EphyEmbedShell *shell)
{
	GList *charsets;
	int i;
	CharsetInfo *info;

	ephy_embed_shell_get_charset_titles (shell, NULL, &charsets);

	i = gtk_option_menu_get_history (option_menu);
	charsets = g_list_nth (charsets, i);
	g_assert (charsets != NULL);
	info = (CharsetInfo *) charsets->data;
	eel_gconf_set_string (CONF_LANGUAGE_DEFAULT_CHARSET,
			      info->name);

	g_list_free (charsets);
}

static gint
find_charset_in_list_cmp (gconstpointer a,
                          gconstpointer b)
{
	CharsetInfo *info = (CharsetInfo *)a;
	const char *value = b;

	return (strcmp (info->name, value));
}

static void
create_default_charset_menu (GeneralPrefs *dialog)
{
	EphyEmbedShell *shell;
	GList *l;
	GList *charsets;
	GtkWidget *menu;
	GtkWidget *optionmenu;
	char *value;

	shell = EPHY_EMBED_SHELL (ephy_shell);
	ephy_embed_shell_get_charset_titles (shell, NULL, &l);

	menu = gtk_menu_new ();

	optionmenu = ephy_dialog_get_control (EPHY_DIALOG (dialog),
					      DEFAULT_CHARSET_PROP);

	for (charsets = l; charsets != NULL; charsets = charsets->next)
	{
		CharsetInfo *info = (CharsetInfo *) charsets->data;
		GtkWidget *item;

		item = gtk_menu_item_new_with_label (info->title);
		gtk_menu_shell_append (GTK_MENU_SHELL(menu),
				       item);
		gtk_widget_show (item);
	}

	gtk_option_menu_set_menu (GTK_OPTION_MENU(optionmenu), menu);

	/* init value */
	charsets = l;
	value = eel_gconf_get_string (CONF_LANGUAGE_DEFAULT_CHARSET);
	g_return_if_fail (value != NULL);
	charsets = g_list_find_custom (charsets, (gconstpointer)value,
				       (GCompareFunc)find_charset_in_list_cmp);
	gtk_option_menu_set_history (GTK_OPTION_MENU(optionmenu),
				     g_list_position (l, charsets));
	g_free (value);

	g_signal_connect (optionmenu, "changed",
			  G_CALLBACK (default_charset_menu_changed_cb),
			  shell);

	g_list_free (l);
}

static GtkWidget *
general_prefs_new_language_menu (GeneralPrefs *dialog)
{
	int i;
	GtkWidget *menu;

	menu = gtk_menu_new ();

	for (i = 0; languages[i].name != NULL; i++)
	{
		GtkWidget *item;

		item = gtk_menu_item_new_with_label (languages[i].name);
		gtk_menu_shell_append (GTK_MENU_SHELL(menu),
				       item);
		gtk_widget_show (item);
		g_object_set_data (G_OBJECT(item), "desc",
				   languages[i].name);
	}

	return menu;
}

static void
language_menu_changed_cb (GtkOptionMenu *option_menu,
		          gpointer data)
{
	GSList *list;
	GSList *l = NULL;
	int history;

	list = eel_gconf_get_string_list (CONF_RENDERING_LANGUAGE);
	l = g_slist_copy (list);

	/* Subst the first item according to the optionmenu */
	history = gtk_option_menu_get_history (option_menu);
	l->data = languages [history].code;

	eel_gconf_set_string_list (CONF_RENDERING_LANGUAGE, l);

	g_slist_free (l);
}

static void
create_language_menu (GeneralPrefs *dialog)
{
	GtkWidget *optionmenu;
	GtkWidget *menu;
	const char *value;
	int i;
	GSList *list;

	optionmenu = ephy_dialog_get_control (EPHY_DIALOG (dialog),
					      LANGUAGE_PROP);

	menu = general_prefs_new_language_menu (dialog);

	gtk_option_menu_set_menu (GTK_OPTION_MENU(optionmenu), menu);

	/* init value */
	list = eel_gconf_get_string_list (CONF_RENDERING_LANGUAGE);
	g_return_if_fail (list != NULL);
	value = (const char *)list->data;

	i = 0;
	while (languages[i].code && strcmp (languages[i].code, value) != 0)
	{
		i++;
	}

	gtk_option_menu_set_history (GTK_OPTION_MENU(optionmenu), i);

	g_signal_connect (optionmenu, "changed",
			  G_CALLBACK (language_menu_changed_cb),
			  NULL);
}

static void
general_prefs_init (GeneralPrefs *dialog)
{
	dialog->priv = g_new0 (GeneralPrefsPrivate, 1);

	ephy_dialog_construct (EPHY_DIALOG(dialog),
				 properties,
				 "prefs-dialog.glade",
				 "general_page_box");

	create_default_charset_menu (dialog);
	create_language_menu (dialog);
}

static void
general_prefs_finalize (GObject *object)
{
	GeneralPrefs *dialog;

        g_return_if_fail (object != NULL);
        g_return_if_fail (IS_GENERAL_PREFS (object));

	dialog = GENERAL_PREFS (object);

        g_return_if_fail (dialog->priv != NULL);

        g_free (dialog->priv);

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

EphyDialog *
general_prefs_new (void)
{
	GeneralPrefs *dialog;

	dialog = GENERAL_PREFS (g_object_new (GENERAL_PREFS_TYPE,
				               NULL));

	return EPHY_DIALOG(dialog);
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
fill_language_editor (LanguageEditor *le)
{
	GSList *strings;
	GSList *tmp;
	int i;

	/* Fill the list */
	strings = eel_gconf_get_string_list (CONF_RENDERING_LANGUAGE);

	for (tmp = strings; tmp != NULL; tmp = g_slist_next (tmp))
	{
		char *value = (char *)tmp->data;

		i = 0;
		while (languages[i].code && strcmp (languages[i].code, value) != 0)
		{
			i++;
		}

		/* FIXME unsafe, bad prefs could cause it to access random memory */
		language_editor_add (le, languages[i].name, i);
	}
}

static void
language_dialog_changed_cb (LanguageEditor *le,
			    GSList *list,
			    GeneralPrefs *dialog)
{
	GtkWidget *optionmenu;
	const GSList *l;
	GSList *langs = NULL;

	optionmenu = ephy_dialog_get_control (EPHY_DIALOG (dialog),
						LANGUAGE_PROP);
	gtk_option_menu_set_history (GTK_OPTION_MENU(optionmenu),
				     GPOINTER_TO_INT(list->data));

	for (l = list; l != NULL; l = l->next)
	{
		int i = GPOINTER_TO_INT (l->data);
		langs = g_slist_append (langs, languages[i].code);
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

	menu = general_prefs_new_language_menu (GENERAL_PREFS(dialog));

	toplevel = gtk_widget_get_toplevel (button);
	editor = language_editor_new (toplevel);
	language_editor_set_menu (editor, menu);
	fill_language_editor (editor);
	ephy_dialog_set_modal (EPHY_DIALOG(editor), TRUE);

	g_signal_connect (editor, "changed",
			  G_CALLBACK(language_dialog_changed_cb),
			  dialog);

	ephy_dialog_show (EPHY_DIALOG(editor));
}
