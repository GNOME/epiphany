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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ephy-gui.h"
#include "eel-gconf-extensions.h"

#include <ctype.h>
#include <string.h>
#include <bonobo/bonobo-i18n.h>
#include <libgnome/gnome-help.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtkmessagedialog.h>

/* Styles for tab labels */
GtkStyle *loading_text_style = NULL;
GtkStyle *new_text_style = NULL;

/**
 * gul_gui_menu_position_under_widget:
 */
void
ephy_gui_menu_position_under_widget (GtkMenu   *menu,
				     gint      *x,
				     gint      *y,
				     gboolean  *push_in,
				     gpointer	user_data)
{
	GtkWidget *w = GTK_WIDGET (user_data);
	gint screen_width, screen_height;
	GtkRequisition requisition;

	gdk_window_get_origin (w->window, x, y);
	*x += w->allocation.x;
	*y += w->allocation.y + w->allocation.height;

	gtk_widget_size_request (GTK_WIDGET (menu), &requisition);

	screen_width = gdk_screen_width ();
	screen_height = gdk_screen_height ();

	*x = CLAMP (*x, 0, MAX (0, screen_width - requisition.width));
	*y = CLAMP (*y, 0, MAX (0, screen_height - requisition.height));
	g_print ("result %d\n", *y);
}

/**
 * gul_gui_gtk_radio_button_get: get the active member of a radiobutton
 * group from one of the buttons in the group. This should be in GTK+!
 */
gint
ephy_gui_gtk_radio_button_get (GtkRadioButton *radio_button)
{
	GtkToggleButton *toggle_button;
	gint i, length;
        GSList *list;

	/* get group list */
        list = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radio_button));
        length = g_slist_length (list);

	/* iterate over list to find active button */
	for (i = 0; list != NULL; i++, list = g_slist_next (list))
	{
		/* get button and text */
		toggle_button = GTK_TOGGLE_BUTTON (list->data);
		if (gtk_toggle_button_get_active (toggle_button))
		{
			break;
		}
	}

	/* check we didn't run off end */
	g_assert (list != NULL);

	/* return index (reverse order!) */
	return (length - 1) - i;
}

/**
 * gul_gui_gtk_radio_button_set: set the active member of a radiobutton
 * group from one of the buttons in the group. This should be in GTK+!
 */
void
ephy_gui_gtk_radio_button_set (GtkRadioButton *radio_button, gint index)
{
	GtkToggleButton *button;
	GSList *list;
	gint length;

	/* get the list */
        list = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radio_button));

	/* check out the length */
        length = g_slist_length (list);

        /* new buttons are *preppended* to the list, so button added as first
         * has last position in the list */
        index = (length - 1) - index;

	/* find the right button */
        button = GTK_TOGGLE_BUTTON (g_slist_nth_data (list, index));

	/* set it... this will de-activate the others in the group */
	if (gtk_toggle_button_get_active (button) == FALSE)
	{
		gtk_toggle_button_set_active (button, TRUE);
	}
}

gboolean
ephy_gui_confirm_overwrite_file (GtkWidget *parent, const char *filename)
{
	char *question;
	GtkWidget *dialog;
	gboolean res;

	if (!g_file_test (filename, G_FILE_TEST_EXISTS))
	{
		return TRUE;
	}

	question = g_strdup_printf (_("File %s will be overwritten.\n"
				    "If you choose yes, the contents will be lost.\n\n"
				    "Do you want to continue?"), filename);
	dialog = gtk_message_dialog_new (parent ? GTK_WINDOW(parent) : NULL,
			                 GTK_DIALOG_MODAL,
				         GTK_MESSAGE_QUESTION,
				         GTK_BUTTONS_YES_NO,
				         question);
	res = (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_YES);
	gtk_widget_destroy (dialog);
	g_free (question);

	return res;
}

void		
ephy_gui_help (GtkWindow *parent,
	       const char *file_name,
               const char *link_id)
{
	GError *err = NULL;

	gnome_help_display (file_name, link_id, &err);

	if (err != NULL)
	{
		GtkWidget *dialog;
		dialog = gtk_message_dialog_new (parent,
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_OK,
						 _("Could not display help: %s"), err->message);
		g_signal_connect (G_OBJECT (dialog), "response",
				  G_CALLBACK (gtk_widget_destroy),
				  NULL);
		gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
		gtk_widget_show (dialog);
		g_error_free (err);
	}
}
