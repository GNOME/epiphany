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
#include "general-prefs.h"
#include "appearance-prefs.h"
#include "ephy-dialog.h"
#include "ephy-prefs.h"
#include "ephy-embed-prefs.h"
#include "ephy-shell.h"
#include "ephy-state.h"
#include "ephy-gui.h"

#include <bonobo/bonobo-i18n.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkradiobutton.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkimage.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkstock.h>

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


/* Proxy page */

enum
{
	CACHE_COMPARE_PROP,
	DISK_CACHE_PROP,
	MEMORY_CACHE_PROP
};

static const
EphyDialogProperty network_properties [] =
{
	{ CACHE_COMPARE_PROP, "cache_compare_radiobutton", CONF_NETWORK_CACHE_COMPARE, PT_AUTOAPPLY, NULL },
	{ DISK_CACHE_PROP, "disk_cache_spin", CONF_NETWORK_DISK_CACHE, PT_AUTOAPPLY, NULL },
	{ MEMORY_CACHE_PROP, "memory_cache_spin", CONF_NETWORK_MEMORY_CACHE, PT_AUTOAPPLY, NULL },

	{ -1, NULL, NULL }
};

enum
{
	OPEN_IN_TABS_PROP,
	JUMP_TO_PROP,
	POPUPS_PROP
};

static const
EphyDialogProperty ui_properties [] =
{
	{ OPEN_IN_TABS_PROP, "open_in_tabs_checkbutton", CONF_TABS_TABBED, PT_AUTOAPPLY, NULL },
	{ JUMP_TO_PROP, "jump_to_checkbutton", CONF_TABS_TABBED_AUTOJUMP, PT_AUTOAPPLY, NULL },
	{ POPUPS_PROP, "popups_checkbutton", CONF_TABS_TABBED_POPUPS, PT_AUTOAPPLY, NULL },

	{ -1, NULL, NULL }
};

struct PrefsDialogPrivate
{
	GtkWidget *notebook;
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

                prefs_dialog_type = g_type_register_static (GTK_TYPE_DIALOG,
							    "PrefsDialog",
							    &our_info, 0);
        }

        return prefs_dialog_type;

}

GtkDialog *
prefs_dialog_new (void)
{
        GtkDialog *dialog;

        dialog = GTK_DIALOG (g_object_new (PREFS_DIALOG_TYPE, NULL));

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
prefs_dialog_finalize (GObject *object)
{
        PrefsDialog *pd;

        g_return_if_fail (object != NULL);
        g_return_if_fail (IS_PREFS_DIALOG (object));

	pd = PREFS_DIALOG (object);

        g_return_if_fail (pd->priv != NULL);

        g_free (pd->priv);

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

static EphyDialog *
create_page (PrefsPageID id,
	     const char *page_widget,
	     const EphyDialogProperty *prop)
{
	EphyDialog *page = NULL;

	switch (id)
	{
	case PREFS_PAGE_GENERAL:
		page = general_prefs_new ();
		break;
	case PREFS_PAGE_APPEARANCE:
		page = appearance_prefs_new ();
		break;
	case PREFS_PAGE_UI:
	case PREFS_PAGE_ADVANCED:
		page = ephy_dialog_new ();
		ephy_dialog_construct (EPHY_DIALOG(page),
				       prop,
				       "prefs-dialog.glade",
				       page_widget);
		break;
	}

	return page;
}

static EphyDialog *
prefs_dialog_get_page (PrefsDialog *pd,
		       PrefsPageID id)
{
	const char *page_widget = NULL;
	EphyDialog *page;
	const EphyDialogProperty *prop = NULL;

	switch (id)
	{
	case PREFS_PAGE_APPEARANCE:
		page_widget = "appearance_page_box";
		break;
	case PREFS_PAGE_GENERAL:
		page_widget = "general_page_box";
		break;
	case PREFS_PAGE_UI:
		page_widget = "ui_page_box";
		prop = ui_properties;
		break;
	case PREFS_PAGE_ADVANCED:
		page_widget = "network_page_box";
		prop = network_properties;
		break;
	}

	g_assert (page_widget != NULL);

	page = create_page (id, page_widget, prop);
	g_assert (page != NULL);

	return page;
}

void
prefs_dialog_show_page (PrefsDialog *pd,
		        PrefsPageID id)
{
	gtk_notebook_set_current_page (GTK_NOTEBOOK (pd->priv->notebook), id);
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

static void
prefs_build_notebook (PrefsDialog *pd)
{
	int i;
	GtkWidget *nb;

	struct
	{
		char *name;
		int id;
	}
	pages[] =
	{
		{ _("General"), PREFS_PAGE_GENERAL },
		{ _("Appearance"), PREFS_PAGE_APPEARANCE },
		{ _("User Interface"), PREFS_PAGE_UI },
		{ _("Advanced"), PREFS_PAGE_ADVANCED },

		{ NULL, 0 }
	};

	gtk_dialog_add_button (GTK_DIALOG (pd), GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);

	gtk_dialog_add_button (GTK_DIALOG (pd), GTK_STOCK_HELP, GTK_RESPONSE_HELP);
	g_signal_connect (pd, "response",
			  G_CALLBACK (prefs_dialog_response_cb),
			  pd);

	gtk_container_set_border_width (GTK_CONTAINER (pd), 5);

	nb = gtk_notebook_new ();
	gtk_container_set_border_width (GTK_CONTAINER (nb), 5);
	gtk_widget_show (nb);
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG (pd)->vbox), nb);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (nb), TRUE);
	pd->priv->notebook = nb;

	for (i = 0; pages[i].name != NULL; i++)
	{
		GtkWidget *label, *child;
		EphyDialog *page;

		page = prefs_dialog_get_page (pd, pages[i].id);

		child = gtk_hbox_new (FALSE, 0);
		gtk_widget_show (child);
		label = gtk_label_new (pages[i].name);
		gtk_notebook_append_page (GTK_NOTEBOOK (nb),
					  child, label);

		ephy_dialog_show_embedded (page, child);
	}
}

static void
prefs_dialog_init (PrefsDialog *pd)
{
	GdkPixbuf *icon;
	pd->priv = g_new0 (PrefsDialogPrivate, 1);

	gtk_window_set_title (GTK_WINDOW(pd), _("Preferences"));
	gtk_dialog_set_has_separator (GTK_DIALOG(pd), FALSE);

	ephy_state_add_window (GTK_WIDGET(pd),
			       "prefs_dialog", -1, -1);
	
	icon = gtk_widget_render_icon (GTK_WIDGET(pd),
						      GTK_STOCK_PREFERENCES,
						      GTK_ICON_SIZE_MENU,
						      "prefs_dialog");
	gtk_window_set_icon (GTK_WINDOW(pd), icon);
	g_object_unref(icon);

	prefs_build_notebook (pd);
}

/* Network page callbacks */

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
