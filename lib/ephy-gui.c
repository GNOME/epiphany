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
 *
 *  $Id$
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ephy-gui.h"
#include "eel-gconf-extensions.h"

#include <ctype.h>
#include <string.h>
#include <glib/gi18n.h>
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
	gboolean rtl;

	rtl = (gtk_widget_get_direction (w) == GTK_TEXT_DIR_RTL);

	gdk_window_get_origin (w->window, x, y);
	gtk_widget_size_request (GTK_WIDGET (menu), &requisition);

	/* FIXME multihead */
	screen_width = gdk_screen_width ();
	screen_height = gdk_screen_height ();

	if (rtl)
	{
		*x += w->allocation.x + w->allocation.width - requisition.width;
	}
	else
	{
		*x += w->allocation.x;
	}

	*y += w->allocation.y + w->allocation.height;

	*x = CLAMP (*x, 0, MAX (0, screen_width - requisition.width));
	*y = CLAMP (*y, 0, MAX (0, screen_height - requisition.height));
}

gboolean
ephy_gui_confirm_overwrite_file (GtkWidget *parent, const char *filename)
{
	char *question, *converted;
	GtkWidget *dialog;
	gboolean res;

	if (filename == NULL) return FALSE;

	if (!g_file_test (filename, G_FILE_TEST_EXISTS))
	{
		return TRUE;
	}

	converted = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
	if (converted == NULL) return FALSE;

	question = g_strdup_printf (_("File %s will be overwritten.\n"
				    "If you choose yes, the contents will be lost.\n\n"
				    "Do you want to continue?"), converted);
	dialog = gtk_message_dialog_new (parent ? GTK_WINDOW(parent) : NULL,
			                 GTK_DIALOG_MODAL,
				         GTK_MESSAGE_QUESTION,
				         GTK_BUTTONS_YES_NO,
				         question);
	res = (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_YES);
	gtk_widget_destroy (dialog);
	g_free (question);
	g_free (converted);

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
