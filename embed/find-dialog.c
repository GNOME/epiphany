/*
 *  Copyright (C) 2002 Jorn Baayen
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

#include "find-dialog.h"
#include "ephy-prefs.h"
#include "ephy-embed.h"
#include "ephy-debug.h"

#include <gtk/gtk.h>

#define CONF_FIND_MATCH_CASE "/apps/epiphany/dialogs/find_match_case"
#define CONF_FIND_AUTOWRAP "/apps/epiphany/dialogs/find_autowrap"
#define CONF_FIND_WORD "/apps/epiphany/dialogs/find_word"

static void find_dialog_class_init (FindDialogClass *klass);
static void find_dialog_init (FindDialog *dialog);
static void find_dialog_finalize (GObject *object);

/* Glade callbacks */
void find_close_button_clicked_cb (GtkWidget *button, EphyDialog *dialog);
void find_next_button_clicked_cb (GtkWidget *button, EphyDialog *dialog);
void find_prev_button_clicked_cb (GtkWidget *button, EphyDialog *dialog);
void find_entry_changed_cb  (GtkWidget *editable, EphyDialog *dialog);
void find_check_toggled_cb (GtkWidget *toggle, EphyDialog *dialog);

static GObjectClass *parent_class = NULL;

struct FindDialogPrivate
{
	EphyEmbed *old_embed;
};

enum
{
	WINDOW_PROP,
	MATCH_CASE_PROP,
	AUTOWRAP_PROP,
	WORD_PROP,
	BACK_BUTTON,
	FORWARD_BUTTON
};

static const
EphyDialogProperty properties [] =
{
	{ WINDOW_PROP, "find_dialog", NULL, PT_NORMAL, NULL },
	{ MATCH_CASE_PROP, "case_check", CONF_FIND_MATCH_CASE, PT_NORMAL, NULL },
	{ AUTOWRAP_PROP, "wrap_check", CONF_FIND_AUTOWRAP, PT_NORMAL, NULL },
	{ WORD_PROP, "find_entry", CONF_FIND_WORD, PT_NORMAL, NULL },
	{ BACK_BUTTON, "back_button", NULL, PT_NORMAL, NULL },
	{ FORWARD_BUTTON, "forward_button", NULL, PT_NORMAL, NULL },
	{ -1, NULL, NULL }
};

GType
find_dialog_get_type (void)
{
        static GType find_dialog_type = 0;

        if (find_dialog_type == 0)
        {
                static const GTypeInfo our_info =
                {
                        sizeof (FindDialogClass),
                        NULL, /* base_init */
                        NULL, /* base_finalize */
                        (GClassInitFunc) find_dialog_class_init,
                        NULL,
                        NULL, /* class_data */
                        sizeof (FindDialog),
                        0, /* n_preallocs */
                        (GInstanceInitFunc) find_dialog_init
                };

                find_dialog_type = g_type_register_static (EPHY_EMBED_DIALOG_TYPE,
						           "FindDialog",
						           &our_info, 0);
        }

        return find_dialog_type;

}

static void
update_navigation_controls (FindDialog *dialog, gboolean prev, gboolean next)
{
	GtkWidget *button;

	button = ephy_dialog_get_control (EPHY_DIALOG (dialog), BACK_BUTTON);
	gtk_widget_set_sensitive (button, prev);

	button = ephy_dialog_get_control (EPHY_DIALOG (dialog), FORWARD_BUTTON);
	gtk_widget_set_sensitive (button, next);
}

static void
impl_show (EphyDialog *dialog)
{

	EPHY_DIALOG_CLASS (parent_class)->show (dialog);

	/* Focus the text entry.  This will correctly select or leave
	 * unselected the existing text in the entry depending on the
	 * 'gtk-entry-select-on-focus = 0 / 1' setting in user's gtkrc.
	 */
	gtk_widget_grab_focus (ephy_dialog_get_control (dialog, WORD_PROP));
}

static void
find_dialog_class_init (FindDialogClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
	EphyDialogClass *ephy_dialog_class;

        parent_class = g_type_class_peek_parent (klass);
	ephy_dialog_class = EPHY_DIALOG_CLASS (klass);

        object_class->finalize = find_dialog_finalize;

	ephy_dialog_class->show = impl_show;
}

static void
set_properties (FindDialog *find_dialog)
{
        char *search_string;
	GValue match_case = {0, };
	GValue wrap = {0, };
	GValue word = {0, };
	gboolean b_match_case;
	gboolean b_wrap;
	EphyDialog *dialog = EPHY_DIALOG (find_dialog);
	EphyEmbed *embed;

        /* get the search string from the entry field */
	ephy_dialog_get_value (dialog, WORD_PROP, &word);
        search_string = g_strdup (g_value_get_string (&word));
	g_value_unset (&word);

        /* don't do null searches */
        if (search_string == NULL || search_string[0] == '\0')
        {
		update_navigation_controls (find_dialog, FALSE, FALSE);
		g_free (search_string);

                return;
        }

	ephy_dialog_get_value (dialog, MATCH_CASE_PROP, &match_case);
        b_match_case = g_value_get_boolean (&match_case);

	ephy_dialog_get_value (dialog, AUTOWRAP_PROP, &wrap);
        b_wrap = g_value_get_boolean (&wrap);

	embed = ephy_embed_dialog_get_embed (EPHY_EMBED_DIALOG(dialog));
	g_return_if_fail (embed != NULL);

        ephy_embed_find_set_properties (embed, search_string,
					b_match_case, b_wrap);
}

static void
sync_page_change (EphyEmbed *embed, const char *address, FindDialog *dialog)
{
	g_return_if_fail (IS_EPHY_EMBED (embed));

	update_navigation_controls (dialog, TRUE, TRUE);
	set_properties (dialog);
}

static void
unset_old_embed (FindDialog *dialog)
{
	if (dialog->priv->old_embed != NULL)
	{
		g_signal_handlers_disconnect_by_func (dialog->priv->old_embed,
						      G_CALLBACK (sync_page_change),
						      dialog);
		g_object_remove_weak_pointer (G_OBJECT (dialog->priv->old_embed),
					      (gpointer *)&dialog->priv->old_embed);

		dialog->priv->old_embed = NULL;
	}
}

static void
sync_embed (FindDialog *dialog, GParamSpec *pspec, gpointer data)
{
	EphyEmbed *embed;

	unset_old_embed (dialog);

	embed = ephy_embed_dialog_get_embed (EPHY_EMBED_DIALOG (dialog));
	g_return_if_fail (IS_EPHY_EMBED (embed));
	dialog->priv->old_embed = embed;

	g_signal_connect (G_OBJECT (embed), "ge_location",
			  G_CALLBACK (sync_page_change), dialog);

	g_object_add_weak_pointer (G_OBJECT (embed),
				   (gpointer *)&dialog->priv->old_embed);

	update_navigation_controls (dialog, TRUE, TRUE);
	set_properties (dialog);
}

static void
find_dialog_init (FindDialog *dialog)
{
	GdkPixbuf *icon;
	GtkWidget *window;

	dialog->priv = g_new0 (FindDialogPrivate, 1);

	dialog->priv->old_embed = NULL;

	ephy_dialog_construct (EPHY_DIALOG(dialog),
			       properties,
			       "epiphany.glade",
			       "find_dialog");
	update_navigation_controls (dialog, TRUE, TRUE);

	window = ephy_dialog_get_control (EPHY_DIALOG (dialog), WINDOW_PROP);
	icon = gtk_widget_render_icon (window,
				       GTK_STOCK_FIND,
				       GTK_ICON_SIZE_MENU,
				       "find_dialog");
	gtk_window_set_icon (GTK_WINDOW(window), icon);
	g_object_unref (icon);

	g_signal_connect_object (dialog, "notify::embed",
				 G_CALLBACK (sync_embed), NULL, 0);
}

static void
find_dialog_finalize (GObject *object)
{
	FindDialog *dialog;

        g_return_if_fail (object != NULL);
        g_return_if_fail (IS_FIND_DIALOG (object));

	dialog = FIND_DIALOG (object);

	g_signal_handlers_disconnect_by_func (dialog, G_CALLBACK (sync_embed), NULL);

        g_return_if_fail (dialog->priv != NULL);

	unset_old_embed (dialog);

        g_free (dialog->priv);

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

EphyDialog *
find_dialog_new (EphyEmbed *embed)
{
	FindDialog *dialog;

	dialog = FIND_DIALOG (g_object_new (FIND_DIALOG_TYPE,
				     "embed", embed,
				     NULL));

	return EPHY_DIALOG(dialog);
}

EphyDialog *
find_dialog_new_with_parent (GtkWidget *window,
			     EphyEmbed *embed)
{
	FindDialog *dialog;

	dialog = FIND_DIALOG (g_object_new (FIND_DIALOG_TYPE,
				     "embed", embed,
				     "ParentWindow", window,
				     NULL));

	return EPHY_DIALOG(dialog);
}

static void
find_dialog_go_next (FindDialog *dialog)
{
	gresult result;
	EphyEmbed *embed;

	g_return_if_fail (IS_FIND_DIALOG (dialog));

	embed = ephy_embed_dialog_get_embed (EPHY_EMBED_DIALOG(dialog));
	g_return_if_fail (embed != NULL);

        result = ephy_embed_find_next (embed, FALSE);

	if (result == G_OK)
	{
		update_navigation_controls (dialog, TRUE, TRUE);
	}
	else
	{
		update_navigation_controls (dialog, TRUE, FALSE);
	}
}

static void
find_dialog_go_prev (FindDialog *dialog)
{
	gresult result;
	EphyEmbed *embed;

	g_return_if_fail (IS_FIND_DIALOG (dialog));

	embed = ephy_embed_dialog_get_embed (EPHY_EMBED_DIALOG(dialog));
	g_return_if_fail (embed != NULL);

	result = ephy_embed_find_next (embed, TRUE);

	if (result == G_OK)
	{
		update_navigation_controls (dialog, TRUE, TRUE);
	}
	else
	{
		update_navigation_controls (dialog, FALSE, TRUE);
	}
}

void
find_close_button_clicked_cb (GtkWidget *button,
			      EphyDialog *dialog)
{
	g_object_unref (dialog);
}

void find_next_button_clicked_cb (GtkWidget *button,
				  EphyDialog *dialog)
{
	find_dialog_go_next (FIND_DIALOG(dialog));
}

void
find_prev_button_clicked_cb (GtkWidget *button,
			     EphyDialog *dialog)
{
	find_dialog_go_prev (FIND_DIALOG(dialog));
}

void
find_entry_changed_cb  (GtkWidget *editable,
			EphyDialog *dialog)
{
	FindDialog *find_dialog = FIND_DIALOG(dialog);

	update_navigation_controls (find_dialog, TRUE, TRUE);
	set_properties (find_dialog);
}

void
find_check_toggled_cb (GtkWidget *toggle,
                       EphyDialog *dialog)
{
	FindDialog *find_dialog = FIND_DIALOG(dialog);

	update_navigation_controls (find_dialog, TRUE, TRUE);
	set_properties (find_dialog);
}
