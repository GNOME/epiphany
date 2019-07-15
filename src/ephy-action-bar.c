/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2018 Purism SPC
 *  Copyright © 2018 Adrien Plazas <kekun.plazas@laposte.net>
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

#include "ephy-action-bar.h"
#include "ephy-pages-button.h"
#include "ephy-pages-popover.h"
#include "ephy-settings.h"
#include "ephy-shell.h"

enum {
  PROP_0,
  PROP_WINDOW,
  N_PROPERTIES
};

static GParamSpec *object_properties[N_PROPERTIES] = { NULL, };

struct _EphyActionBar {
  GtkRevealer parent_instance;

  EphyWindow *window;
  EphyActionBarStart *action_bar_start;
  EphyActionBarEnd *action_bar_end;
  EphyPagesButton *pages_button;
  GtkNotebook *notebook;
};

G_DEFINE_TYPE (EphyActionBar, ephy_action_bar, GTK_TYPE_REVEALER)

static void
sync_chromes_visibility (EphyActionBar *action_bar)
{
  EphyWindowChrome chrome;

  chrome = ephy_window_get_chrome (action_bar->window);

  gtk_widget_set_visible (ephy_action_bar_start_get_navigation_box (action_bar->action_bar_start),
                          chrome & EPHY_WINDOW_CHROME_HEADER_BAR);
  ephy_action_bar_end_set_show_bookmarks_button (action_bar->action_bar_end,
                                                 chrome & EPHY_WINDOW_CHROME_BOOKMARKS);
}

static void
update_pages_button (EphyActionBar *action_bar)
{
  int n_pages;

  n_pages = gtk_notebook_get_n_pages (action_bar->notebook);
  ephy_pages_button_set_n_pages (action_bar->pages_button, n_pages);
}

static void
ephy_action_bar_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  EphyActionBar *action_bar = EPHY_ACTION_BAR (object);

  switch (property_id) {
    case PROP_WINDOW:
      action_bar->window = EPHY_WINDOW (g_value_get_object (value));
      g_object_notify_by_pspec (object, object_properties[PROP_WINDOW]);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
ephy_action_bar_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  EphyActionBar *action_bar = EPHY_ACTION_BAR (object);

  switch (property_id) {
    case PROP_WINDOW:
      g_value_set_object (value, action_bar->window);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
ephy_action_bar_constructed (GObject *object)
{
  EphyActionBar *action_bar = EPHY_ACTION_BAR (object);

  G_OBJECT_CLASS (ephy_action_bar_parent_class)->constructed (object);

  action_bar->notebook = GTK_NOTEBOOK (ephy_window_get_notebook (action_bar->window));
  update_pages_button (action_bar);

  g_signal_connect_object (action_bar->window, "notify::chrome",
                           G_CALLBACK (sync_chromes_visibility), action_bar,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (action_bar->notebook, "page-added",
                           G_CALLBACK (update_pages_button), action_bar,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (action_bar->notebook, "page-removed",
                           G_CALLBACK (update_pages_button), action_bar,
                           G_CONNECT_SWAPPED);
}

static void
ephy_action_bar_class_init (EphyActionBarClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->set_property = ephy_action_bar_set_property;
  gobject_class->get_property = ephy_action_bar_get_property;
  gobject_class->constructed = ephy_action_bar_constructed;

  object_properties[PROP_WINDOW] =
    g_param_spec_object ("window",
                         "Window",
                         "The action_bar's EphyWindow",
                         EPHY_TYPE_WINDOW,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class,
                                     N_PROPERTIES,
                                     object_properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/action-bar.ui");

  gtk_widget_class_bind_template_child (widget_class,
                                        EphyActionBar,
                                        action_bar_start);
  gtk_widget_class_bind_template_child (widget_class,
                                        EphyActionBar,
                                        pages_button);
  gtk_widget_class_bind_template_child (widget_class,
                                        EphyActionBar,
                                        action_bar_end);
}

static void
ephy_action_bar_init (EphyActionBar *action_bar)
{
  EphyEmbedShellMode mode;

  /* Ensure the types used by the template have been initialized. */
  EPHY_TYPE_ACTION_BAR_END;
  EPHY_TYPE_ACTION_BAR_START;
  EPHY_TYPE_PAGES_BUTTON;

  gtk_widget_init_template (GTK_WIDGET (action_bar));

  mode = ephy_embed_shell_get_mode (EPHY_EMBED_SHELL (ephy_shell_get_default ()));
  gtk_widget_set_visible (GTK_WIDGET (action_bar->pages_button),
                          mode != EPHY_EMBED_SHELL_MODE_APPLICATION);
}

EphyActionBar *
ephy_action_bar_new (EphyWindow *window)
{
  return g_object_new (EPHY_TYPE_ACTION_BAR,
                       "window", window,
                       NULL);
}

EphyActionBarStart *
ephy_action_bar_get_action_bar_start (EphyActionBar *action_bar)
{
  return action_bar->action_bar_start;
}

EphyActionBarEnd *
ephy_action_bar_get_action_bar_end (EphyActionBar *action_bar)
{
  return action_bar->action_bar_end;
}

void
ephy_action_bar_set_adaptive_mode (EphyActionBar    *action_bar,
                                   EphyAdaptiveMode  adaptive_mode)
{
  switch (adaptive_mode) {
    case EPHY_ADAPTIVE_MODE_NORMAL:
      gtk_revealer_set_reveal_child (GTK_REVEALER (action_bar), FALSE);

      break;
    case EPHY_ADAPTIVE_MODE_NARROW:
      gtk_revealer_set_reveal_child (GTK_REVEALER (action_bar), TRUE);

      break;
  }
}
