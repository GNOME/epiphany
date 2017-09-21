/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2002 Marco Pesenti Gritti
 *
 *  This file is part of Epiphany.
 *
 *  Epiphany is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Epiphany is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Epiphany.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "ephy-gui.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

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
ephy_gui_menu_position_under_widget (GtkMenu  *menu,
                                     gint     *x,
                                     gint     *y,
                                     gboolean *push_in,
                                     gpointer  user_data)
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
  if (!gtk_widget_get_has_window (widget)) {
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
  if (group == NULL) {
    group = gtk_window_group_new ();
    gtk_window_group_add_window (group, window);
    g_object_unref (group);
  }

  return group;
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
ephy_gui_help (GtkWidget  *parent,
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

  if (error != NULL) {
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
                            guint        *ostate,
                            guint        *obutton)
{
  GdkEvent *event;
  GdkEventType type = GDK_NOTHING;
  guint state = 0, button = (guint) - 1;

  event = gtk_get_current_event ();
  if (event != NULL) {
    type = event->type;

    if (type == GDK_KEY_PRESS ||
        type == GDK_KEY_RELEASE) {
      state = event->key.state;
    } else if (type == GDK_BUTTON_PRESS ||
               type == GDK_BUTTON_RELEASE ||
               type == GDK_2BUTTON_PRESS ||
               type == GDK_3BUTTON_PRESS) {
      button = event->button.button;
      state = event->button.state;
    }

    gdk_event_free (event);
  }

  if (otype)
    *otype = type;
  if (ostate)
    *ostate = state & gtk_accelerator_get_default_mod_mask ();
  if (obutton)
    *obutton = button;
}
