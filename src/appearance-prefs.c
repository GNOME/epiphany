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

#include "appearance-prefs.h"
#include "ephy-shell.h"
#include "ephy-prefs.h"
#include "ephy-embed-prefs.h"
#include "eel-gconf-extensions.h"

#include <string.h>
#include <gtk/gtkcombo.h>
#include <gtk/gtkspinbutton.h>
#include <gtk/gtkeditable.h>
#include <gtk/gtkoptionmenu.h>

static void appearance_prefs_class_init (AppearancePrefsClass *klass);
static void appearance_prefs_init (AppearancePrefs *dialog);
static void appearance_prefs_finalize (GObject *object);

/* Glade callbacks */
void
fonts_language_optionmenu_changed_cb (GtkWidget *optionmenu, EphyDialog *dialog);

static GObjectClass *parent_class = NULL;

struct AppearancePrefsPrivate
{
	int language;
	gboolean switching;
};

enum
{
	SERIF_PROP,
	SANSSERIF_PROP,
	MONOSPACE_PROP,
	FIXED_SIZE_PROP,
	VARIABLE_SIZE_PROP,
	MIN_SIZE_PROP,
	PROPORTIONAL_PROP,
	BACKGROUND_PROP,
	TEXT_PROP,
	UNVISITED_PROP,
	VISITED_PROP,
	USE_SYSCOLORS_PROP,
	USE_COLORS_PROP,
	USE_FONTS_PROP,
};

static const
EphyDialogProperty properties [] =
{
	{ SERIF_PROP, "serif_combo", NULL, PT_NORMAL, NULL },
	{ SANSSERIF_PROP, "sansserif_combo", NULL, PT_NORMAL, NULL },
	{ MONOSPACE_PROP, "monospace_combo", NULL, PT_NORMAL, NULL },
	{ FIXED_SIZE_PROP, "fixed_size_spinbutton", NULL, PT_NORMAL, NULL },
	{ VARIABLE_SIZE_PROP, "variable_size_spinbutton", NULL, PT_NORMAL, NULL },
	{ MIN_SIZE_PROP, "min_size_spinbutton", NULL, PT_NORMAL, NULL },
	{ PROPORTIONAL_PROP, "proportional_optionmenu", CONF_RENDERING_DEFAULT_FONT, PT_AUTOAPPLY, NULL },
	{ BACKGROUND_PROP, "background_cpick", CONF_RENDERING_BG_COLOR, PT_AUTOAPPLY, NULL },
	{ TEXT_PROP, "text_cpick", CONF_RENDERING_TEXT_COLOR, PT_AUTOAPPLY, NULL },
	{ UNVISITED_PROP, "unvisited_cpick", CONF_RENDERING_UNVISITED_LINKS, PT_AUTOAPPLY, NULL },
	{ VISITED_PROP, "visited_cpick", CONF_RENDERING_VISITED_LINKS, PT_AUTOAPPLY, NULL },
	{ USE_SYSCOLORS_PROP, "use_syscolors_checkbutton", CONF_RENDERING_USE_SYSTEM_COLORS, PT_AUTOAPPLY, NULL },
	{ USE_COLORS_PROP, "use_colors_checkbutton", CONF_RENDERING_USE_OWN_COLORS, PT_AUTOAPPLY, NULL },
	{ USE_FONTS_PROP, "use_fonts_checkbutton", CONF_RENDERING_USE_OWN_FONTS, PT_AUTOAPPLY, NULL },

	{ -1, NULL, NULL }
};

/* FIXME duped in mozilla/ */
const
char *lang_encode[] =
{
        "x-western",
        "x-central-euro",
        "ja",
        "zh-TW",
        "zh-CN",
        "ko",
        "x-cyrillic",
        "x-baltic",
        "el",
        "tr",
        "x-unicode",
        "th",
        "he",
        "ar"
};

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

GType
appearance_prefs_get_type (void)
{
        static GType appearance_prefs_type = 0;

        if (appearance_prefs_type == 0)
        {
                static const GTypeInfo our_info =
                {
                        sizeof (AppearancePrefsClass),
                        NULL, /* base_init */
                        NULL, /* base_finalize */
                        (GClassInitFunc) appearance_prefs_class_init,
                        NULL,
                        NULL, /* class_data */
                        sizeof (AppearancePrefs),
                        0, /* n_preallocs */
                        (GInstanceInitFunc) appearance_prefs_init
                };

                appearance_prefs_type = g_type_register_static (EPHY_DIALOG_TYPE,
						              "AppearancePrefs",
						              &our_info, 0);
        }

        return appearance_prefs_type;

}

static void
appearance_prefs_class_init (AppearancePrefsClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = appearance_prefs_finalize;
}

static void
setup_font_menu (AppearancePrefs *dialog,
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
					 lang_encode[dialog->priv->language],
					 type, &fonts, &default_font);

	/* Get the default font */
	sprintf (key, "%s_%s_%s", CONF_RENDERING_FONT, type,
		 lang_encode[dialog->priv->language]);
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

	/* FIXME free the list */
}

static void
save_font_menu (AppearancePrefs *dialog,
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
		 lang_encode[dialog->priv->language]);
	eel_gconf_set_string (key, name);
	g_free (name);
}

static void
font_entry_changed_cb (GtkWidget *entry, AppearancePrefs *dialog)
{
	int type;

	if (dialog->priv->switching) return;

	type = GPOINTER_TO_INT (g_object_get_data (G_OBJECT(entry),
						    "type"));
	save_font_menu (dialog, type, entry);
}

static void
attach_font_signal (AppearancePrefs *dialog, int prop,
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
attach_fonts_signals (AppearancePrefs *dialog)
{
	attach_font_signal (dialog, SERIF_PROP,
			    GINT_TO_POINTER(FONT_TYPE_SERIF));
	attach_font_signal (dialog, SANSSERIF_PROP,
			    GINT_TO_POINTER(FONT_TYPE_SANSSERIF));
	attach_font_signal (dialog, MONOSPACE_PROP,
			    GINT_TO_POINTER(FONT_TYPE_MONOSPACE));
}

static void
size_spinbutton_changed_cb (GtkWidget *spin, AppearancePrefs *dialog)
{
	int type;
	char key[255];

	if (dialog->priv->switching) return;

	type = GPOINTER_TO_INT(g_object_get_data (G_OBJECT(spin), "type"));

	sprintf (key, "%s_%s",
		 size_prefs[type],
		 lang_encode[dialog->priv->language]);
	eel_gconf_set_integer (key, gtk_spin_button_get_value (GTK_SPIN_BUTTON (spin)));
}

static void
attach_size_controls_signals (AppearancePrefs *dialog)
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
setup_size_control (AppearancePrefs *dialog,
		    const char *pref,
		    int default_size,
		    GtkWidget *spin)
{
	char key[255];
	int size;

	sprintf (key, "%s_%s", pref,
		 lang_encode[dialog->priv->language]);
	size = eel_gconf_get_integer (key);

	if (size == 0) size = default_size;

	gtk_spin_button_set_value (GTK_SPIN_BUTTON(spin), size);
}

static void
setup_size_controls (AppearancePrefs *dialog)
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
setup_fonts (AppearancePrefs *dialog)
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
appearance_prefs_init (AppearancePrefs *dialog)
{
	dialog->priv = g_new0 (AppearancePrefsPrivate, 1);
	dialog->priv->switching = FALSE;

	ephy_dialog_construct (EPHY_DIALOG(dialog),
			       properties,
			       "prefs-dialog.glade",
			       "appearance_page_box");

	setup_fonts (dialog);
	setup_size_controls (dialog);
	attach_fonts_signals (dialog);
	attach_size_controls_signals (dialog);
}

static void
appearance_prefs_finalize (GObject *object)
{
	AppearancePrefs *dialog;

        g_return_if_fail (object != NULL);
        g_return_if_fail (IS_appearance_PREFS (object));

	dialog = appearance_PREFS (object);

        g_return_if_fail (dialog->priv != NULL);

        g_free (dialog->priv);

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

EphyDialog *
appearance_prefs_new (void)
{
	AppearancePrefs *dialog;

	dialog = appearance_PREFS (g_object_new (appearance_PREFS_TYPE,
				                  NULL));

	return EPHY_DIALOG(dialog);
}

void
fonts_language_optionmenu_changed_cb (GtkWidget *optionmenu,
				      EphyDialog *dialog)
{
	int i;

	i = gtk_option_menu_get_history
		(GTK_OPTION_MENU (optionmenu));

	appearance_PREFS(dialog)->priv->language = i;

	setup_fonts (appearance_PREFS(dialog));
	setup_size_controls (appearance_PREFS(dialog));
}
