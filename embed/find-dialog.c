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
#include <gtk/gtk.h>

#define CONF_FIND_MATCH_CASE "/apps/epiphany/find/match_case"
#define CONF_FIND_AUTOWRAP "/apps/epiphany/find/autowrap"
#define CONF_FIND_WORD "/apps/epiphany/find/word"

static void find_dialog_class_init (FindDialogClass *klass);
static void find_dialog_init (FindDialog *dialog);
static void find_dialog_finalize (GObject *object);

static void
impl_construct (EphyDialog *dialog,
                const EphyDialogProperty *properties,
                const char *file,
                const char *name);
static void
impl_destruct (EphyDialog *dialog);
static void
impl_show (EphyDialog *dialog);


/* Glade callbacks */
void find_close_button_clicked_cb (GtkWidget *button, EphyDialog *dialog);
void find_next_button_clicked_cb (GtkWidget *button, EphyDialog *dialog);
void find_prev_button_clicked_cb (GtkWidget *button, EphyDialog *dialog);
void find_entry_changed_cb  (GtkWidget *editable, EphyDialog *dialog);
void find_check_toggled_cb (GtkWidget *toggle, EphyDialog *dialog);

static GObjectClass *parent_class = NULL;

struct FindDialogPrivate
{
	EmbedFindInfo *properties;
	GtkWidget *window;
	gboolean can_go_prev;
	gboolean can_go_next;
	gboolean constructed;
};

enum
{
	SEARCH,
	LAST_SIGNAL
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

static guint find_dialog_signals[LAST_SIGNAL] = { 0 };

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
find_dialog_class_init (FindDialogClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
	EphyDialogClass *ephy_dialog_class;

        parent_class = g_type_class_peek_parent (klass);
	ephy_dialog_class = EPHY_DIALOG_CLASS (klass);

        object_class->finalize = find_dialog_finalize;

	ephy_dialog_class->construct = impl_construct;
	ephy_dialog_class->destruct = impl_destruct;
	ephy_dialog_class->show = impl_show;

	find_dialog_signals[SEARCH] =
                g_signal_new ("search",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (FindDialogClass, search),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE,
                              0);
}

static void
find_update_nav (EphyDialog *dialog)
{
	GtkWidget *forward_button;
	GtkWidget *back_button;

	g_signal_emit (G_OBJECT (dialog), find_dialog_signals[SEARCH], 0);

	if (!FIND_DIALOG(dialog)->priv->constructed) return;

	forward_button = ephy_dialog_get_control (dialog, FORWARD_BUTTON);
        gtk_widget_set_sensitive (forward_button,
				  FIND_DIALOG(dialog)->priv->can_go_next);

	back_button = ephy_dialog_get_control (dialog, BACK_BUTTON);
        gtk_widget_set_sensitive (back_button,
				  FIND_DIALOG(dialog)->priv->can_go_prev);
}

static void
ensure_constructed (FindDialog *dialog)
{
	if (!dialog->priv->constructed)
	{
		ephy_dialog_construct (EPHY_DIALOG(dialog),
				       properties,
				       "epiphany.glade",
				       "find_dialog");
	}
}

static void
find_dialog_init (FindDialog *dialog)
{
	dialog->priv = g_new0 (FindDialogPrivate, 1);

	dialog->priv->properties = NULL;
	dialog->priv->can_go_prev = TRUE;
	dialog->priv->can_go_next = TRUE;
	dialog->priv->constructed = FALSE;

	ensure_constructed (dialog);
}

static void
impl_construct (EphyDialog *dialog,
                const EphyDialogProperty *properties,
                const char *file,
                const char *name)
{
	FIND_DIALOG(dialog)->priv->constructed = TRUE;

	EPHY_DIALOG_CLASS (parent_class)->construct (dialog, properties, file, name);
}

static void
impl_destruct (EphyDialog *dialog)
{
	FIND_DIALOG(dialog)->priv->constructed = FALSE;

	EPHY_DIALOG_CLASS (parent_class)->destruct (dialog);
}

static void
find_get_info (EphyDialog *dialog)
{
        EmbedFindInfo *properties;
        char *search_string;
	GValue word = {0, };
	GValue match_case = {0, };
	GValue wrap = {0, };
	FindDialog *find_dialog = FIND_DIALOG(dialog);

        /* get the search string from the entry field */
	ephy_dialog_get_value (dialog, WORD_PROP, &word);
        search_string = g_strdup(g_value_get_string (&word));
	g_value_unset (&word);

        /* don't do null searches */
        if (search_string[0] == '\0')
        {
		find_dialog->priv->can_go_prev = FALSE;
		find_dialog->priv->can_go_next = FALSE;
                return;
        }

	if (find_dialog->priv->properties != NULL)
	{
		g_free (find_dialog->priv->properties->search_string);
		g_free (find_dialog->priv->properties);
	}

        /* build search structure */
        properties = g_new0 (EmbedFindInfo,1);
        properties->search_string = search_string;

	ephy_dialog_get_value (dialog, MATCH_CASE_PROP, &match_case);
        properties->match_case = g_value_get_boolean (&match_case);

	ephy_dialog_get_value (dialog, AUTOWRAP_PROP, &wrap);
        properties->wrap = g_value_get_boolean (&wrap);

        properties->entire_word = FALSE;
	properties->search_frames = TRUE;

	find_dialog->priv->properties = properties;
}

static void
dialog_constrain_height (FindDialog *dialog)
{
	GdkGeometry geometry;
	GtkWindow *window = GTK_WINDOW (dialog->priv->window);

	/* Do not allow to resize the widget vertically */
	geometry.max_height  = 0;
	geometry.max_width = gdk_screen_get_width
			(gtk_widget_get_screen (GTK_WIDGET (window)));
	gtk_window_set_geometry_hints (window, GTK_WIDGET (window),
				       &geometry,
				       GDK_HINT_MAX_SIZE);
}

static void
impl_show (EphyDialog *dialog)
{
	GdkPixbuf *icon;
	FindDialog *find_dialog = FIND_DIALOG(dialog);
	ensure_constructed (find_dialog);

	find_dialog->priv->can_go_prev = TRUE;
	find_dialog->priv->can_go_next = TRUE;
	find_dialog->priv->window = ephy_dialog_get_control (dialog, WINDOW_PROP);
	find_get_info (dialog);
	find_update_nav (dialog);

	/* Focus the text entry.  This will correctly select or leave
	 * unselected the existing text in the entry depending on the
	 * 'gtk-entry-select-on-focus = 0 / 1' setting in user's gtkrc.
	 */
	gtk_widget_grab_focus (ephy_dialog_get_control (dialog, WORD_PROP));

	
	icon = gtk_widget_render_icon (find_dialog->priv->window, 
						      GTK_STOCK_FIND,
						      GTK_ICON_SIZE_MENU,
						      "find_dialog");
	gtk_window_set_icon (GTK_WINDOW(find_dialog->priv->window), icon);
	g_object_unref (icon);
	
	dialog_constrain_height (find_dialog);
	
	EPHY_DIALOG_CLASS (parent_class)->show (dialog);
}

static void
find_dialog_finalize (GObject *object)
{
	FindDialog *dialog;

        g_return_if_fail (object != NULL);
        g_return_if_fail (IS_FIND_DIALOG (object));

	dialog = FIND_DIALOG (object);

        g_return_if_fail (dialog->priv != NULL);

        g_free (dialog->priv);

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

EphyDialog *
find_dialog_new (EphyEmbed *embed)
{
	FindDialog *dialog;

	dialog = FIND_DIALOG (g_object_new (FIND_DIALOG_TYPE,
				     "EphyEmbed", embed,
				     NULL));

	return EPHY_DIALOG(dialog);
}

EphyDialog *
find_dialog_new_with_parent (GtkWidget *window,
			     EphyEmbed *embed)
{
	FindDialog *dialog;

	dialog = FIND_DIALOG (g_object_new (FIND_DIALOG_TYPE,
				     "EphyEmbed", embed,
				     "ParentWindow", window,
				     NULL));

	return EPHY_DIALOG(dialog);
}

gboolean
find_dialog_can_go_next (FindDialog *dialog)
{
	return dialog->priv->can_go_next;
}

gboolean
find_dialog_can_go_prev (FindDialog *dialog)
{
	return dialog->priv->can_go_prev;
}

void
find_dialog_go_next (FindDialog *dialog,
		     gboolean interactive)
{
	gresult result;
	EphyEmbed *embed;

	if (!find_dialog_can_go_next (dialog)) return;

	dialog->priv->properties->backwards = FALSE;
	dialog->priv->properties->interactive = interactive;

	embed = ephy_embed_dialog_get_embed (EPHY_EMBED_DIALOG(dialog));
	g_return_if_fail (embed != NULL);

        result = ephy_embed_find (embed,
				  dialog->priv->properties);

	dialog->priv->can_go_prev = TRUE;
	if (result != G_OK)
	{
		dialog->priv->can_go_next = FALSE;
	}

	find_update_nav (EPHY_DIALOG(dialog));
}

void
find_dialog_go_prev (FindDialog *dialog,
		     gboolean interactive)
{
	gresult result;
	EphyEmbed *embed;

	if (!find_dialog_can_go_prev (dialog)) return;

	dialog->priv->properties->backwards = TRUE;
	dialog->priv->properties->interactive = interactive;

	embed = ephy_embed_dialog_get_embed (EPHY_EMBED_DIALOG(dialog));
	g_return_if_fail (embed != NULL);

	result = ephy_embed_find (embed,
				  dialog->priv->properties);

	dialog->priv->can_go_next = TRUE;
	if (result != G_OK)
	{
		dialog->priv->can_go_prev = FALSE;
	}

	find_update_nav (EPHY_DIALOG(dialog));
}

void
find_close_button_clicked_cb (GtkWidget *button,
			      EphyDialog *dialog)
{
	ephy_dialog_destruct (dialog);
	g_object_unref (dialog);
}

void find_next_button_clicked_cb (GtkWidget *button,
				  EphyDialog *dialog)
{
	find_dialog_go_next (FIND_DIALOG(dialog), TRUE);
}

void
find_prev_button_clicked_cb (GtkWidget *button,
			     EphyDialog *dialog)
{
	find_dialog_go_prev (FIND_DIALOG(dialog), TRUE);
}

void
find_entry_changed_cb  (GtkWidget *editable,
			EphyDialog *dialog)
{
	FindDialog *find_dialog = FIND_DIALOG(dialog);

	find_dialog->priv->can_go_prev = TRUE;
	find_dialog->priv->can_go_next = TRUE;

	find_get_info (dialog);

	find_update_nav (dialog);
}

void
find_check_toggled_cb (GtkWidget *toggle,
                       EphyDialog *dialog)
{
	FindDialog *find_dialog = FIND_DIALOG(dialog);

	find_dialog->priv->can_go_prev = TRUE;
	find_dialog->priv->can_go_next = TRUE;

	find_get_info (dialog);

        find_update_nav (dialog);
}
