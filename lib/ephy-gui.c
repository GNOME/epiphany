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
#include <gtk/gtkhbox.h>
#include <gtk/gtkimage.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkversion.h>
#include <gtk/gtkicontheme.h>

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
	char *primary_text, *question, *converted;
	GtkWidget *dialog, *hbox, *label;
	GtkWidget *image;
	gboolean res;

	if (filename == NULL) return FALSE;

	if (!g_file_test (filename, G_FILE_TEST_EXISTS))
	{
		return TRUE;
	}

	converted = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
	if (converted == NULL) return FALSE;

	primary_text = g_strdup_printf (_("A file %s already exists."), 
	                                converted);

	question = g_strdup_printf ("<b><big>%s</big></b>\n\n%s", primary_text,
	                            _("If you choose to overwrite this file, "
				      "the contents will be lost."));

	dialog = gtk_dialog_new_with_buttons (_("Overwrite File"),
                                        parent ? GTK_WINDOW (parent) : NULL,
                                        GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR,
                                        GTK_STOCK_CANCEL,
					GTK_RESPONSE_CANCEL,
					_("_Overwrite"), GTK_RESPONSE_ACCEPT,
					NULL);
	ephy_gui_set_default_window_icon (GTK_WINDOW (dialog));

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, TRUE, TRUE, 12);
	image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_WARNING, GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment (GTK_MISC (image), 0.5, 0.0);
	gtk_box_pack_start (GTK_BOX (hbox), image, TRUE, TRUE, 0);

	label = gtk_label_new (NULL);
	gtk_label_set_selectable (GTK_LABEL (label), TRUE);
	gtk_label_set_markup (GTK_LABEL (label), question);
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
	gtk_widget_show_all (hbox);

	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	gtk_window_set_skip_taskbar_hint (GTK_WINDOW (dialog), TRUE);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dialog)), 6);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
        res = (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) ? TRUE : FALSE;

	gtk_widget_destroy (dialog);

	g_free (primary_text);
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

gboolean
ephy_gui_select_row_by_key (GtkTreeView *treeview, gint column, guint32 unicode)
{
	GtkTreeModel *model;
	GtkTreeIter iter, last_iter;
	GtkTreePath *path;
	GValue value = {0, };
	char *string;
	char *event_string;
	gboolean found = FALSE;
	char outbuf[6];
	int length;

	model = gtk_tree_view_get_model (treeview);

	length = g_unichar_to_utf8 (unicode, outbuf);
	event_string = g_utf8_casefold (outbuf, length);

	if (!gtk_tree_model_get_iter_first (model, &iter))
	{
		g_free (event_string);
		return FALSE;
	}

	do
	{
		last_iter = iter;
		gtk_tree_model_get_value (model, &iter, column, &value);

		string = g_utf8_casefold (g_value_get_string (&value), -1);
		g_utf8_strncpy (string, string, 1);
		found = (g_utf8_collate (string, event_string) == 0);

		g_free (string);
		g_value_unset (&value);
	}
	while (!found && gtk_tree_model_iter_next (model, &iter));

	if (!found)
	{
		iter = last_iter;
	}

	path = gtk_tree_model_get_path (model, &iter);
	gtk_tree_view_set_cursor (GTK_TREE_VIEW (treeview), path, NULL, FALSE);
	gtk_tree_path_free (path);
	g_free (event_string);

	return TRUE;
}

gboolean
ephy_gui_is_middle_click (void)
{
	gboolean new_tab = FALSE;
	GdkEvent *event;

	event = gtk_get_current_event ();
	if (event != NULL)
	{
		if (event->type == GDK_BUTTON_RELEASE)
		{
			guint modifiers, button, state;

			modifiers = gtk_accelerator_get_default_mod_mask ();
			button = event->button.button;
			state = event->button.state;

			/* middle-click or control-click */
			if ((button == 1 && ((state & modifiers) == GDK_CONTROL_MASK)) ||
			    (button == 2))
			{
				new_tab = TRUE;
			}
		}

		gdk_event_free (event);
	}

	return new_tab;
}

void
ephy_gui_set_default_window_icon (GtkWindow *window)
{
#if GTK_CHECK_VERSION (2, 5, 4)
	gtk_window_set_icon_name (window, "web-browser");
#else
	const char *icon_path;
	GdkPixbuf *icon = NULL;
	GtkIconTheme *icon_theme;
	GtkIconInfo *icon_info;

	icon_theme = gtk_icon_theme_get_default ();
	icon_info = gtk_icon_theme_lookup_icon (icon_theme, "web-browser", -1, 0);

	if (icon_info != NULL)
	{
		icon_path = gtk_icon_info_get_filename (icon_info);

		if (icon_path != NULL)
		{
			icon = gdk_pixbuf_new_from_file (icon_path, NULL);
		}

		gtk_icon_info_free (icon_info);
	}
	else
	{
		g_warning ("Web browser gnome icon not found");
	}

	gtk_window_set_icon (GTK_WINDOW (window), icon);

	if (icon != NULL)
	{
		g_object_unref (icon);
	}
#endif
}
