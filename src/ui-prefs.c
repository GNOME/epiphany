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

#include "ui-prefs.h"
#include "ephy-shell.h"
#include "ephy-prefs.h"
#include "eel-gconf-extensions.h"
#include "ephy-spinner.h"

#include <string.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-icon-list.h>

static void ui_prefs_class_init (UIPrefsClass *klass);
static void ui_prefs_init (UIPrefs *dialog);
static void ui_prefs_finalize (GObject *object);

/* Glade callbacks */
void
spinners_iconlist_select_icon_cb (GtkWidget *iconlist, gint num,
				  GdkEvent *event, UIPrefs *dialog);

static GObjectClass *parent_class = NULL;

struct UIPrefsPrivate
{
	gpointer dummy;
	GList *spinner_list;
};

enum
{
	SPINNERS_PROP,
	OPEN_IN_TABS_PROP,
	JUMP_TO_PROP,
	POPUPS_PROP
};

static const
EphyDialogProperty properties [] =
{
	{ SPINNERS_PROP, "spinners_iconlist", NULL, PT_NORMAL, NULL },
	{ OPEN_IN_TABS_PROP, "open_in_tabs_checkbutton", CONF_TABS_TABBED, PT_AUTOAPPLY, NULL },
	{ JUMP_TO_PROP, "jump_to_checkbutton", CONF_TABS_TABBED_AUTOJUMP, PT_AUTOAPPLY, NULL },
	{ POPUPS_PROP, "popups_checkbutton", CONF_TABS_TABBED_POPUPS, PT_AUTOAPPLY, NULL },

	{ -1, NULL, NULL }
};

GType
ui_prefs_get_type (void)
{
        static GType ui_prefs_type = 0;

        if (ui_prefs_type == 0)
        {
                static const GTypeInfo our_info =
                {
                        sizeof (UIPrefsClass),
                        NULL, /* base_init */
                        NULL, /* base_finalize */
                        (GClassInitFunc) ui_prefs_class_init,
                        NULL,
                        NULL, /* class_data */
                        sizeof (UIPrefs),
                        0, /* n_preallocs */
                        (GInstanceInitFunc) ui_prefs_init
                };

                ui_prefs_type = g_type_register_static (EPHY_DIALOG_TYPE,
						              "UIPrefs",
						              &our_info, 0);
        }

        return ui_prefs_type;

}

static void
ui_prefs_class_init (UIPrefsClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = ui_prefs_finalize;
}

/**
 * Free any existing spinner list.
 */
static void
free_spinner_list (UIPrefs *dialog)
{
        GList *node;

        for (node = dialog->priv->spinner_list; node; node = node->next)
                g_free(node->data);

        g_list_free(dialog->priv->spinner_list);
        dialog->priv->spinner_list = NULL;
}

/**
 * spinner_get_path_from_index: used in prefs_callbacks.c to get the
 * path of selected icon
 */
static const gchar *
spinner_get_path_from_index (UIPrefs *dialog, gint index)
{
        gchar *path;

        path = g_list_nth_data (dialog->priv->spinner_list, index);

        return path;
}

/*
 * spinner_fill_iconlist: fill a gnome icon list with icons of available spinners
 */
static void
spinner_fill_iconlist (UIPrefs *dialog, GnomeIconList *icon_list)
{
	GList *spinners, *tmp;
        gchar *pref_spinner_path;
	gint index;

        /* clear spinner list */
        free_spinner_list (dialog);
        gnome_icon_list_clear (GNOME_ICON_LIST (icon_list));

        pref_spinner_path =
		eel_gconf_get_string (CONF_TOOLBAR_SPINNER_THEME);
        index = gnome_icon_list_get_num_icons (icon_list);

	spinners = ephy_spinner_list_spinners ();
	for (tmp = spinners; tmp != NULL; tmp = g_list_next (tmp))
	{
		EphySpinnerInfo *info = tmp->data;

		dialog->priv->spinner_list =
			g_list_append (dialog->priv->spinner_list,
				       g_strdup (info->name));

		gnome_icon_list_append (icon_list, info->filename, info->name);

		/* Select the icon configured in prefs */
		if (pref_spinner_path &&
	            strcmp (pref_spinner_path, info->name) == 0)
		{
			gnome_icon_list_select_icon (icon_list, index);
		}
		index++;
	}
	g_list_foreach (spinners, (GFunc)ephy_spinner_info_free, NULL);
	g_list_free (spinners);

	g_free (pref_spinner_path);
}

static void
ui_prefs_init (UIPrefs *dialog)
{
	GtkWidget *icon_list;

	dialog->priv = g_new0 (UIPrefsPrivate, 1);
	dialog->priv->spinner_list = NULL;

	ephy_dialog_construct (EPHY_DIALOG(dialog),
				 properties,
				 "prefs-dialog.glade",
				 "ui_page_box");

	icon_list = ephy_dialog_get_control (EPHY_DIALOG(dialog),
					       SPINNERS_PROP);

	spinner_fill_iconlist (dialog, GNOME_ICON_LIST (icon_list));
}

static void
ui_prefs_finalize (GObject *object)
{
	UIPrefs *dialog;

        g_return_if_fail (object != NULL);
        g_return_if_fail (IS_UI_PREFS (object));

	dialog = UI_PREFS (object);

        g_return_if_fail (dialog->priv != NULL);

	free_spinner_list (dialog);

        g_free (dialog->priv);

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

EphyDialog *
ui_prefs_new (void)
{
	UIPrefs *dialog;

	dialog = UI_PREFS (g_object_new (UI_PREFS_TYPE,
				               NULL));

	return EPHY_DIALOG(dialog);
}

void
spinners_iconlist_select_icon_cb (GtkWidget *iconlist, gint num,
				  GdkEvent *event, UIPrefs *dialog)
{
	const char *path;
	path = spinner_get_path_from_index (dialog, num);
	eel_gconf_set_string (CONF_TOOLBAR_SPINNER_THEME, path);
}
