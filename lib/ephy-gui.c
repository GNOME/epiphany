/*
 *  Copyright © 2002 Marco Pesenti Gritti
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "ephy-gui.h"

#include "ephy-debug.h"

#include <ctype.h>
#include <gdk/gdkx.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>
#include <unistd.h>

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

	gtk_widget_get_preferred_size (GTK_WIDGET (menu), &req, NULL);

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
	GtkAllocation allocation;
	GdkRectangle visible;

	gtk_widget_get_preferred_size (GTK_WIDGET (menu), &req, NULL);
	gdk_window_get_origin (gtk_widget_get_window (widget), x, y);
	gtk_widget_get_allocation (widget, &allocation);

	*x += (allocation.width - req.width) / 2;

	/* Add on height for the treeview title */
	gtk_tree_view_get_visible_rect (tree_view, &visible);
	*y += allocation.height - visible.height;

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
 * @menu:
 * @x:
 * @y:
 * @push_in:
 * @user_data: a #GtkWidget
 * 
 * Positions @menu under (or over, depending on space available) the widget
 * @user_data.
 */
void
ephy_gui_menu_position_under_widget (GtkMenu   *menu,
				     gint      *x,
				     gint      *y,
				     gboolean  *push_in,
				     gpointer	user_data)
{
	/* Adapted from gtktoolbar.c */
	GtkWidget *widget = GTK_WIDGET (user_data);
	GtkWidget *container;
	GtkRequisition req;
	GtkRequisition menu_req;
	GtkAllocation allocation;
	GdkRectangle monitor;
	GdkWindow *window;
	int monitor_num;
	GdkScreen *screen;
  
	g_return_if_fail (GTK_IS_WIDGET (widget));

	container = gtk_widget_get_ancestor (widget, GTK_TYPE_CONTAINER);
	g_return_if_fail (container != NULL);

	gtk_widget_get_preferred_size (widget, &req, NULL);
	gtk_widget_get_preferred_size (GTK_WIDGET (menu), &menu_req, NULL);

	screen = gtk_widget_get_screen (GTK_WIDGET (menu));
	window = gtk_widget_get_window (widget);
	monitor_num = gdk_screen_get_monitor_at_window (screen, window);
	if (monitor_num < 0)
		monitor_num = 0;
	gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);

	gtk_widget_get_allocation (widget, &allocation);
	gdk_window_get_origin (window, x, y);
	if (!gtk_widget_get_has_window (widget))
	{
		*x += allocation.x;
		*y += allocation.y;
	}

	if (gtk_widget_get_direction (container) == GTK_TEXT_DIR_LTR) 
		*x += allocation.width - req.width;
	else 
		*x += req.width - menu_req.width;

	if ((*y + allocation.height + menu_req.height) <= monitor.y + monitor.height)
		*y += allocation.height;
	else if ((*y - menu_req.height) >= monitor.y)
		*y -= menu_req.height;
	else if (monitor.y + monitor.height - (*y + allocation.height) > *y)
		*y += allocation.height;
	else
		*y -= menu_req.height;

	*push_in = FALSE;
}

GtkWindowGroup *
ephy_gui_ensure_window_group (GtkWindow *window)
{
	GtkWindowGroup *group;

	group = gtk_window_get_group (window);
	if (group == NULL)
	{
		group = gtk_window_group_new ();
		gtk_window_group_add_window (group, window);
		g_object_unref (group);
	}

	return group;
}

gboolean
ephy_gui_check_location_writable (GtkWidget *parent,
				  const char *filename)
{
	GtkWidget *dialog;
	char *display_name;

	if (filename == NULL) return FALSE;

	if (!g_file_test (filename, G_FILE_TEST_EXISTS))
	{
		char *path = g_path_get_dirname (filename);
		gboolean writable = access (path, W_OK) == 0;

		/* check if path is writable to */
		if (!writable)
		{
			dialog = gtk_message_dialog_new (
					parent ? GTK_WINDOW(parent) : NULL,
					GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_MESSAGE_ERROR,
					GTK_BUTTONS_CLOSE,
					_("Directory “%s” is not writable"), path);

			gtk_message_dialog_format_secondary_text (
					GTK_MESSAGE_DIALOG (dialog),
					_("You do not have permission to "
					  "create files in this directory."));

			gtk_window_set_title (GTK_WINDOW (dialog), _("Directory not Writable"));

			if (parent != NULL)
			{
				gtk_window_group_add_window (
						ephy_gui_ensure_window_group (GTK_WINDOW (parent)),
						GTK_WINDOW (dialog));
			}

			gtk_dialog_run (GTK_DIALOG (dialog));

			gtk_widget_destroy (dialog);		
		}

		g_free (path);

		return writable;
	}

	display_name = g_filename_display_basename (filename);

	/* check if file is writable */
	if (access (filename, W_OK) != 0)
	{
		dialog = gtk_message_dialog_new (
				parent ? GTK_WINDOW(parent) : NULL,
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_CLOSE,
				_("Cannot overwrite existing file “%s”"), display_name);

		gtk_message_dialog_format_secondary_text (
				GTK_MESSAGE_DIALOG (dialog),
				_("A file with this name already exists and "
				  "you don't have permission to overwrite it."));

		gtk_window_set_title (GTK_WINDOW (dialog), _("Cannot Overwrite File"));

		if (parent != NULL)
		{
			gtk_window_group_add_window (
					ephy_gui_ensure_window_group (GTK_WINDOW (parent)),
					GTK_WINDOW (dialog));
		}

		gtk_dialog_run (GTK_DIALOG (dialog));

		gtk_widget_destroy (dialog);		

		return FALSE;
	}


	return TRUE;
}

/**
 * ephy_gui_help:
 * @parent: the parent window where help is being called
 * @page: help page to open or %NULL
 *
 * Displays Epiphany's help, opening the page indicated by @page.
 *
 * Note that @parent is used to know the #GdkScreen where to open the help
 * window.
 **/
void
ephy_gui_help (GtkWidget *parent,
	       const char *page)
{
	GError *error = NULL;
	GdkScreen *screen;
	char *url;

	if (page)
		url = g_strdup_printf ("help:epiphany/%s", page);
	else
		url = g_strdup ("help:epiphany");

	if (parent)
	    screen = gtk_widget_get_screen (parent);
	else
	    screen = gdk_screen_get_default ();

	gtk_show_uri (screen, url, gtk_get_current_event_time (), &error);

	if (error != NULL)
	{
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new (GTK_WINDOW (parent),
						 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_OK,
						 _("Could not display help: %s"),
						 error->message);
		g_error_free (error);

		g_signal_connect (dialog, "response",
				  G_CALLBACK (gtk_widget_destroy), NULL);
		gtk_widget_show (dialog);
	}

	g_free (url);
}

void
ephy_gui_get_current_event (GdkEventType *otype,
			    guint *ostate,
			    guint *obutton)
{
	GdkEvent *event;
	GdkEventType type = GDK_NOTHING;
	guint state = 0, button = (guint) -1;

	event = gtk_get_current_event ();
	if (event != NULL)
	{
		type = event->type;

		if (type == GDK_KEY_PRESS ||
		    type == GDK_KEY_RELEASE)
		{
			state = event->key.state;
		}
		else if (type == GDK_BUTTON_PRESS ||
			 type == GDK_BUTTON_RELEASE ||
			 type == GDK_2BUTTON_PRESS ||
			 type == GDK_3BUTTON_PRESS)
		{
			button = event->button.button;
			state = event->button.state;
		}

		gdk_event_free (event);
	}
	
	if (otype) *otype = type;
	if (ostate) *ostate = state & gtk_accelerator_get_default_mod_mask ();
	if (obutton) *obutton = button;
}

gboolean
ephy_gui_is_middle_click (void)
{
	gboolean is_middle_click = FALSE;
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
			    (button == 2 && ((state & modifiers) == 0)))
			{
				is_middle_click = TRUE;
			}
		}

		gdk_event_free (event);
	}

	return is_middle_click;
}

void
ephy_gui_window_update_user_time (GtkWidget *window,
				  guint32 user_time)
{
	LOG ("updating user time on window %p to %d", window, user_time);

	if (user_time != 0)
	{
		gtk_widget_realize (window);

#ifdef GDK_WINDOWING_X11
		if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
		{
			gdk_x11_window_set_user_time (gtk_widget_get_window (window),
						      user_time);
		}
#endif
	}

}

