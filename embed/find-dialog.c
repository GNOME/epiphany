/*
 *  Copyright (C) 2002 Jorn Baayen
 *  Copyright (C) 2003 Marco Pesenti Gritti
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

#include "config.h"

#include "find-dialog.h"
#include "ephy-file-helpers.h"
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

#define EPHY_FIND_DIALOG_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_FIND_DIALOG, FindDialogPrivate))

struct FindDialogPrivate
{
	EphyEmbed *old_embed;
	gboolean initialised;
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
	{ "find_dialog",	NULL,			PT_NORMAL, 0 },
	{ "case_check",		CONF_FIND_MATCH_CASE,	PT_NORMAL, 0 },
	{ "wrap_check",		CONF_FIND_AUTOWRAP,	PT_NORMAL, 0 },
	{ "find_entry",		CONF_FIND_WORD,		PT_NORMAL, 0 },
	{ "back_button",	NULL,			PT_NORMAL, 0 },
	{ "forward_button",	NULL,			PT_NORMAL, 0 },
	{ NULL }
};

GType
find_dialog_get_type (void)
{
        static GType type = 0;

        if (G_UNLIKELY (type == 0))
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

                type = g_type_register_static (EPHY_TYPE_EMBED_DIALOG,
					       "FindDialog",
					       &our_info, 0);
        }

        return type;

}

static void
update_navigation_controls (FindDialog *dialog, gboolean prev, gboolean next)
{
	GtkWidget *button;

	button = ephy_dialog_get_control (EPHY_DIALOG (dialog),
					  properties[BACK_BUTTON].id);
	gtk_widget_set_sensitive (button, prev);

	button = ephy_dialog_get_control (EPHY_DIALOG (dialog),
					  properties[FORWARD_BUTTON].id);
	gtk_widget_set_sensitive (button, next);
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

	if (!find_dialog->priv->initialised) return;

        /* get the search string from the entry field */
	ephy_dialog_get_value (dialog, properties[WORD_PROP].id, &word);
        search_string = g_strdup (g_value_get_string (&word));
	g_value_unset (&word);

        /* don't do null searches */
        if (search_string == NULL || search_string[0] == '\0')
        {
		update_navigation_controls (find_dialog, FALSE, FALSE);
		g_free (search_string);

                return;
        }

	ephy_dialog_get_value (dialog, properties[MATCH_CASE_PROP].id, &match_case);
        b_match_case = g_value_get_boolean (&match_case);
	g_value_unset (&match_case);

	ephy_dialog_get_value (dialog, properties[AUTOWRAP_PROP].id, &wrap);
        b_wrap = g_value_get_boolean (&wrap);
	g_value_unset (&wrap);

	embed = ephy_embed_dialog_get_embed (EPHY_EMBED_DIALOG(dialog));
	g_return_if_fail (embed != NULL);

        ephy_embed_find_set_properties (embed, search_string,
					b_match_case, b_wrap);

	g_free (search_string);
}

static void
impl_show (EphyDialog *dialog)
{
	EPHY_DIALOG_CLASS (parent_class)->show (dialog);

	EPHY_FIND_DIALOG (dialog)->priv->initialised = TRUE;

	set_properties (EPHY_FIND_DIALOG (dialog));
	
	/* Focus the text entry.  This will correctly select or leave
	 * unselected the existing text in the entry depending on the
	 * 'gtk-entry-select-on-focus = 0 / 1' setting in user's gtkrc.
	 */
	gtk_widget_grab_focus (ephy_dialog_get_control
					(dialog, properties[WORD_PROP].id));
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

	g_type_class_add_private (object_class, sizeof (FindDialogPrivate));
}

static void
sync_page_change (EphyEmbed *embed, const char *address, FindDialog *dialog)
{
	g_return_if_fail (EPHY_IS_EMBED (embed));

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
	g_return_if_fail (EPHY_IS_EMBED (embed));
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
	GtkWidget *window;

	dialog->priv = EPHY_FIND_DIALOG_GET_PRIVATE (dialog);

	ephy_dialog_construct (EPHY_DIALOG(dialog),
			       properties,
			       ephy_file ("epiphany.glade"),
			       "find_dialog",
			       NULL);

	update_navigation_controls (dialog, TRUE, TRUE);

	window = ephy_dialog_get_control (EPHY_DIALOG (dialog),
					  properties[WINDOW_PROP].id);
	gtk_window_set_icon_name (GTK_WINDOW (window), GTK_STOCK_FIND);

	g_signal_connect_object (dialog, "notify::embed",
				 G_CALLBACK (sync_embed), NULL, 0);
}

static void
find_dialog_finalize (GObject *object)
{
	FindDialog *dialog;

        g_return_if_fail (EPHY_IS_FIND_DIALOG (object));

	dialog = EPHY_FIND_DIALOG (object);

	g_signal_handlers_disconnect_by_func (dialog, G_CALLBACK (sync_embed), NULL);

	unset_old_embed (dialog);

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

EphyDialog *
find_dialog_new (EphyEmbed *embed)
{
	FindDialog *dialog;

	dialog = EPHY_FIND_DIALOG (g_object_new (EPHY_TYPE_FIND_DIALOG,
						 "embed", embed,
						 NULL));

	return EPHY_DIALOG(dialog);
}

EphyDialog *
find_dialog_new_with_parent (GtkWidget *window,
			     EphyEmbed *embed)
{
	FindDialog *dialog;

	dialog = EPHY_FIND_DIALOG (g_object_new (EPHY_TYPE_FIND_DIALOG,
						 "embed", embed,
						 "parent-window", window,
						 NULL));

	return EPHY_DIALOG(dialog);
}

static void
find_dialog_go_next (FindDialog *dialog)
{
	EphyEmbed *embed;

	g_return_if_fail (EPHY_IS_FIND_DIALOG (dialog));

	embed = ephy_embed_dialog_get_embed (EPHY_EMBED_DIALOG(dialog));
	g_return_if_fail (embed != NULL);

        if (ephy_embed_find_next (embed, FALSE))
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
	EphyEmbed *embed;

	g_return_if_fail (EPHY_IS_FIND_DIALOG (dialog));

	embed = ephy_embed_dialog_get_embed (EPHY_EMBED_DIALOG(dialog));
	g_return_if_fail (embed != NULL);

	if (ephy_embed_find_next (embed, TRUE))
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
	find_dialog_go_next (EPHY_FIND_DIALOG(dialog));
}

void
find_prev_button_clicked_cb (GtkWidget *button,
			     EphyDialog *dialog)
{
	find_dialog_go_prev (EPHY_FIND_DIALOG(dialog));
}

void
find_entry_changed_cb  (GtkWidget *editable,
			EphyDialog *dialog)
{
	FindDialog *find_dialog = EPHY_FIND_DIALOG(dialog);

	update_navigation_controls (find_dialog, TRUE, TRUE);
	set_properties (find_dialog);
}

void
find_check_toggled_cb (GtkWidget *toggle,
                       EphyDialog *dialog)
{
	FindDialog *find_dialog = EPHY_FIND_DIALOG(dialog);

	update_navigation_controls (find_dialog, TRUE, TRUE);
	set_properties (find_dialog);
}
