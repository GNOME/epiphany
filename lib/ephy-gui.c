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

#include "config.h"

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
#include <gtk/gtktreeselection.h>

void
ephy_gui_sanitise_popup_position (GtkMenu *menu,
				  GtkWidget *widget,
				  gint *x,
				  gint *y)
{
	GdkScreen *screen = gtk_widget_get_screen (widget);
	gint monitor_num;
	GdkRectangle monitor;
	GtkRequisition req;

	g_return_if_fail (widget != NULL);

	gtk_widget_size_request (GTK_WIDGET (menu), &req);

	monitor_num = gdk_screen_get_monitor_at_point (screen, *x, *y);
	gtk_menu_set_monitor (menu, monitor_num);
	gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);

	*x = CLAMP (*x, monitor.x, monitor.x + MAX (0, monitor.width - req.width));
	*y = CLAMP (*y, monitor.y, monitor.y + MAX (0, monitor.height - req.height));
}

void
ephy_gui_menu_position_tree_selection (GtkMenu   *menu,
				       gint      *x,
				       gint      *y,
				       gboolean  *push_in,
				       gpointer  user_data)
{
	GtkTreeSelection *selection;
	GList *selected_rows;
	GtkTreeModel *model;
	GtkTreeView *tree_view = GTK_TREE_VIEW (user_data);
	GtkWidget *widget = GTK_WIDGET (user_data);
	GtkRequisition req;
	GdkRectangle visible;

	gtk_widget_size_request (GTK_WIDGET (menu), &req);
	gdk_window_get_origin (widget->window, x, y);

	*x += (widget->allocation.width - req.width) / 2;

	/* Add on height for the treeview title */
	gtk_tree_view_get_visible_rect (tree_view, &visible);
	*y += widget->allocation.height - visible.height;

	selection = gtk_tree_view_get_selection (tree_view);
	selected_rows = gtk_tree_selection_get_selected_rows (selection, &model);
	if (selected_rows)
	{
		GdkRectangle cell_rect;

		gtk_tree_view_get_cell_area (tree_view, selected_rows->data,
					     NULL, &cell_rect);

		*y += CLAMP (cell_rect.y + cell_rect.height, 0, visible.height);

		g_list_foreach (selected_rows, (GFunc)gtk_tree_path_free, NULL);
		g_list_free (selected_rows);
	}

	ephy_gui_sanitise_popup_position (menu, widget, x, y);
}

/**
 * ephy_gui_menu_position_under_widget:
 */
void
ephy_gui_menu_position_under_widget (GtkMenu   *menu,
				     gint      *x,
				     gint      *y,
				     gboolean  *push_in,
				     gpointer	user_data)
{
	GtkWidget *w = GTK_WIDGET (user_data);
	GtkRequisition requisition;
	gboolean rtl;

	rtl = (gtk_widget_get_direction (w) == GTK_TEXT_DIR_RTL);

	gdk_window_get_origin (w->window, x, y);
	gtk_widget_size_request (GTK_WIDGET (menu), &requisition);

	if (rtl)
	{
		*x += w->allocation.x + w->allocation.width - requisition.width;
	}
	else
	{
		*x += w->allocation.x;
	}

	*y += w->allocation.y + w->allocation.height;

	ephy_gui_sanitise_popup_position (menu, w, x, y);
}

gboolean
ephy_gui_confirm_overwrite_file (GtkWidget *parent, const char *filename)
{
	GtkWidget *dialog;
	char *display_name;
	gboolean retval;

	if (filename == NULL) return FALSE;

	if (!g_file_test (filename, G_FILE_TEST_EXISTS))
	{
		return TRUE;
	}

	display_name = g_filename_display_name (filename);

	dialog = gtk_message_dialog_new
		(parent ? GTK_WINDOW (parent) : NULL,
		 GTK_DIALOG_MODAL,
		 GTK_MESSAGE_WARNING,
		 GTK_BUTTONS_CANCEL,
		 _("A file %s already exists."), display_name);

	gtk_message_dialog_format_secondary_text
		(GTK_MESSAGE_DIALOG (dialog),
		 _("If you choose to overwrite this file, "
		   "the contents will be lost."));

	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       _("_Overwrite"), GTK_RESPONSE_ACCEPT);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Overwrite File"));
	gtk_window_set_icon_name (GTK_WINDOW (dialog), "web-browser");

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);

	retval = (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT);

	gtk_widget_destroy (dialog);

	g_free (display_name);

	return retval;
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
