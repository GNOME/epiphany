/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2012 Igalia S.L.
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
#include "ephy-page-menu-action.h"

G_DEFINE_TYPE (EphyPageMenuAction, ephy_page_menu_action, EPHY_TYPE_WINDOW_ACTION);

static void
menu_position_func (GtkMenu           *menu,
                    int               *x,
                    int               *y,
                    gboolean          *push_in,
                    GtkButton         *button)
{
  GtkAllocation allocation;
  GtkWidget *widget = GTK_WIDGET (button);
  GtkRequisition menu_req;
  GtkTextDirection direction;
  GdkWindow *window;

  gtk_widget_get_preferred_size (GTK_WIDGET (menu),
                                 &menu_req, NULL);

  direction = gtk_widget_get_direction (widget);
  window = gtk_widget_get_window (widget);

  gtk_widget_get_allocation (widget, &allocation);

  gdk_window_get_origin (window, x, y);
  *x += allocation.x;
  *y += allocation.y + allocation.height;

  if (direction == GTK_TEXT_DIR_LTR)
      *x += allocation.width - menu_req.width;

  *push_in = FALSE;
}

static void
ephy_page_menu_action_activate (GtkAction *action)
{
    GtkWidget *menu;
    EphyWindow *window;
    GtkUIManager *manager;
    GSList *list;
    GtkWidget *button;
    GdkEvent *event;
    guint activate_button = 1;
    guint32 activate_time = 0;

    window = ephy_window_action_get_window (EPHY_WINDOW_ACTION (action));
    manager = GTK_UI_MANAGER (ephy_window_get_ui_manager (window));
    menu = gtk_ui_manager_get_widget (manager, "/ui/PagePopup");

    list = gtk_action_get_proxies (action);
    if (GTK_IS_BUTTON (list->data))
        button = GTK_WIDGET (list->data);

    g_return_if_fail (GTK_IS_BUTTON (button));

    event = gtk_get_current_event ();
    if (event && event->type == GDK_BUTTON_PRESS) {
      activate_button = event->button.button;
      activate_time = event->button.time;

      gdk_event_free (event);
    }

    gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
                    (GtkMenuPositionFunc)menu_position_func, button,
                    activate_button, activate_time);
}

static void
ephy_page_menu_action_class_init (EphyPageMenuActionClass *klass)
{
    GtkActionClass *action_class = GTK_ACTION_CLASS (klass);

    action_class->activate = ephy_page_menu_action_activate;
}

static void
ephy_page_menu_action_init (EphyPageMenuAction *self)
{
}
