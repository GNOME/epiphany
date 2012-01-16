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

#define EPHY_PAGE_MENU_ACTION_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), EPHY_TYPE_PAGE_MENU_ACTION, EphyPageMenuActionPrivate))

struct _EphyPageMenuActionPrivate {
  GtkWidget *menu;
};

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
visible_cb (GtkWidget *menu, GParamSpec *pspec, gpointer user_data)
{
  if (gtk_widget_get_visible (menu))
    gtk_style_context_add_class (gtk_widget_get_style_context (menu),
                                 "active-menu");
  else
    gtk_style_context_remove_class (gtk_widget_get_style_context (menu),
                                    "active-menu");
}

static void
button_press_cb (GtkWidget *button, GdkEventButton *event, EphyPageMenuAction *action)
{
  GtkWidget *menu;
  EphyWindow *window;
  GtkUIManager *manager;
  guint event_button = 1;
  guint32 event_time = 0;

  if (!action->priv->menu) {
    window = ephy_window_action_get_window (EPHY_WINDOW_ACTION (action));
    manager = GTK_UI_MANAGER (ephy_window_get_ui_manager (window));
    menu = gtk_ui_manager_get_widget (manager, "/ui/PagePopup");

    g_signal_connect (menu, "notify::visible",
                      G_CALLBACK (visible_cb), NULL);

    action->priv->menu = menu;
  }
    

  if (!button) {
    GSList *l = gtk_action_get_proxies (GTK_ACTION (action));
    if (GTK_IS_BUTTON (l->data))
      button = GTK_WIDGET (l->data);
  }

  g_return_if_fail (GTK_IS_BUTTON (button));

  if (event) {
    event_button = event->button;
    event_time = event->time;
  }

  gtk_menu_popup (GTK_MENU (action->priv->menu),
                  NULL, NULL,
                  (GtkMenuPositionFunc)menu_position_func, button,
                  event_button, event_time);
}

static void
ephy_page_menu_action_activate (GtkAction *action)
{
  button_press_cb (NULL, NULL, EPHY_PAGE_MENU_ACTION (action));
}

static void
ephy_page_menu_action_connect_proxy (GtkAction *action,
                                     GtkWidget *proxy)
{
  if (GTK_IS_BUTTON (proxy))
    g_signal_connect (proxy, "button-press-event",
                      G_CALLBACK (button_press_cb), action);

  GTK_ACTION_CLASS (ephy_page_menu_action_parent_class)->connect_proxy (action, proxy);
}

static void
ephy_page_menu_action_disconnect_proxy (GtkAction *action,
                                        GtkWidget *proxy)
{
  if (GTK_IS_BUTTON (proxy))
    g_signal_handlers_disconnect_by_func (proxy, G_CALLBACK (button_press_cb), action);

  GTK_ACTION_CLASS (ephy_page_menu_action_parent_class)->disconnect_proxy (action, proxy);
}

static void
ephy_page_menu_action_class_init (EphyPageMenuActionClass *klass)
{
  GtkActionClass *action_class = GTK_ACTION_CLASS (klass);

  action_class->activate = ephy_page_menu_action_activate;
  action_class->connect_proxy = ephy_page_menu_action_connect_proxy;
  action_class->disconnect_proxy = ephy_page_menu_action_disconnect_proxy;

  g_type_class_add_private (klass, sizeof (EphyPageMenuActionPrivate));
}

static void
ephy_page_menu_action_init (EphyPageMenuAction *self)
{
  self->priv = EPHY_PAGE_MENU_ACTION_GET_PRIVATE (self);
}
