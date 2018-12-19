/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2012 Igalia S.L
 *  Copyright © 2013 Yosef Or Boczko <yoseforb@gmail.com>
 *  Copyright © 2016 Iulian-Gabriel Radu <iulian.radu67@gmail.com>
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
#include "ephy-header-bar.h"

#include "ephy-add-bookmark-popover.h"
#include "ephy-embed-utils.h"
#include "ephy-flatpak-utils.h"
#include "ephy-location-entry.h"
#include "ephy-shell.h"
#include "ephy-title-box.h"
#include "ephy-title-widget.h"
#include "ephy-type-builtins.h"

#include <glib/gi18n.h>
#define HANDY_USE_UNSTABLE_API
#include <handy.h>

enum {
  PROP_0,
  PROP_WINDOW,
  N_PROPERTIES
};

static GParamSpec *object_properties[N_PROPERTIES] = { NULL, };

struct _EphyHeaderBar {
  GtkHeaderBar parent_instance;

  EphyWindow *window;
  EphyTitleWidget *title_widget;
  GtkRevealer *start_revealer;
  GtkRevealer *end_revealer;
  EphyActionBarStart *action_bar_start;
  EphyActionBarEnd *action_bar_end;
  GtkWidget *navigation_box;
  GtkWidget *reader_mode_revealer;
  GtkWidget *reader_mode_button;
  GtkWidget *new_tab_revealer;
  GtkWidget *new_tab_button;
  GtkWidget *bookmarks_button;
  GtkWidget *page_menu_button;
  GtkWidget *zoom_level_button;
};

G_DEFINE_TYPE (EphyHeaderBar, ephy_header_bar, GTK_TYPE_HEADER_BAR)

static void
ephy_header_bar_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  EphyHeaderBar *header_bar = EPHY_HEADER_BAR (object);

  switch (property_id) {
    case PROP_WINDOW:
      header_bar->window = EPHY_WINDOW (g_value_get_object (value));
      g_object_notify_by_pspec (object, object_properties[PROP_WINDOW]);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
ephy_header_bar_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  EphyHeaderBar *header_bar = EPHY_HEADER_BAR (object);

  switch (property_id) {
    case PROP_WINDOW:
      g_value_set_object (value, header_bar->window);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
sync_chromes_visibility (EphyHeaderBar *header_bar)
{
  EphyWindowChrome chrome;

  chrome = ephy_window_get_chrome (header_bar->window);

  gtk_widget_set_visible (ephy_action_bar_start_get_navigation_box (header_bar->action_bar_start),
                          chrome & EPHY_WINDOW_CHROME_HEADER_BAR);
  ephy_action_bar_end_set_show_bookmarks_button (header_bar->action_bar_end,
                                                 chrome & EPHY_WINDOW_CHROME_BOOKMARKS);
  gtk_widget_set_visible (header_bar->page_menu_button, chrome & EPHY_WINDOW_CHROME_MENU);
  ephy_action_bar_end_set_show_new_tab_button (header_bar->action_bar_end,
                                               chrome & EPHY_WINDOW_CHROME_TABSBAR);
}

static void
add_bookmark_button_clicked_cb (EphyLocationEntry *entry,
                                gpointer          *user_data)
{
  EphyHeaderBar *header_bar = EPHY_HEADER_BAR (user_data);
  GActionGroup *action_group;
  GAction *action;

  action_group = gtk_widget_get_action_group (GTK_WIDGET (header_bar->window), "win");
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "bookmark-page");

  g_action_activate (action, NULL);
}

static void
update_revealer_visibility (GtkRevealer *revealer)
{
  gtk_widget_set_visible (GTK_WIDGET (revealer),
                          gtk_revealer_get_reveal_child (revealer) ||
                          gtk_revealer_get_child_revealed (revealer));
}

static gboolean
is_desktop_pantheon (void)
{
  const gchar *xdg_current_desktop = g_environ_getenv (g_get_environ (), "XDG_CURRENT_DESKTOP");

  return strstr (xdg_current_desktop, "Pantheon") != NULL;
}

static void
ephy_header_bar_constructed (GObject *object)
{
  EphyHeaderBar *header_bar = EPHY_HEADER_BAR (object);
  GtkWidget *button;
  GtkWidget *page_menu_popover;
  GtkBuilder *builder;
  EphyEmbedShell *embed_shell;
  HdyColumn *column;

  G_OBJECT_CLASS (ephy_header_bar_parent_class)->constructed (object);

  g_signal_connect_object (header_bar->window, "notify::chrome",
                           G_CALLBACK (sync_chromes_visibility), header_bar,
                           G_CONNECT_SWAPPED);

  /* Start action elements */
  header_bar->action_bar_start = ephy_action_bar_start_new ();
  gtk_widget_show (GTK_WIDGET (header_bar->action_bar_start));
  header_bar->start_revealer = GTK_REVEALER (gtk_revealer_new ());
  g_signal_connect (header_bar->start_revealer, "notify::child-revealed",
                    G_CALLBACK (update_revealer_visibility), NULL);
  g_signal_connect (header_bar->start_revealer, "notify::reveal-child",
                    G_CALLBACK (update_revealer_visibility), NULL);
  gtk_revealer_set_transition_type (GTK_REVEALER (header_bar->start_revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT);
  gtk_container_add (GTK_CONTAINER (header_bar->start_revealer), GTK_WIDGET (header_bar->action_bar_start));

  gtk_header_bar_pack_start (GTK_HEADER_BAR (header_bar),
                             GTK_WIDGET (header_bar->start_revealer));

  embed_shell = ephy_embed_shell_get_default ();

  /* Title widget (location entry or title box) */
  if (ephy_embed_shell_get_mode (embed_shell) == EPHY_EMBED_SHELL_MODE_APPLICATION)
    header_bar->title_widget = EPHY_TITLE_WIDGET (ephy_title_box_new ());
  else
    header_bar->title_widget = EPHY_TITLE_WIDGET (ephy_location_entry_new ());
  column = hdy_column_new ();
  gtk_widget_set_hexpand (GTK_WIDGET (column), TRUE);
  gtk_widget_show (GTK_WIDGET (column));
  hdy_column_set_maximum_width (column, 860);
  hdy_column_set_linear_growth_width (column, 560);
  gtk_container_add (GTK_CONTAINER (column), GTK_WIDGET (header_bar->title_widget));
  gtk_header_bar_set_custom_title (GTK_HEADER_BAR (header_bar), GTK_WIDGET (column));
  gtk_widget_show (GTK_WIDGET (header_bar->title_widget));

  if (EPHY_IS_LOCATION_ENTRY (header_bar->title_widget)) {
    ephy_location_entry_set_add_bookmark_popover (EPHY_LOCATION_ENTRY (header_bar->title_widget),
                                                  GTK_POPOVER (ephy_add_bookmark_popover_new (header_bar)));

    g_signal_connect_object (header_bar->title_widget,
                             "bookmark-clicked",
                             G_CALLBACK (add_bookmark_button_clicked_cb),
                             header_bar,
                             0);
  }

  /* Page Menu */
  button = gtk_menu_button_new ();
  header_bar->page_menu_button = button;
  gtk_button_set_image (GTK_BUTTON (button),
                        gtk_image_new_from_icon_name ("open-menu-symbolic", GTK_ICON_SIZE_BUTTON));
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  g_type_ensure (G_TYPE_THEMED_ICON);
  builder = gtk_builder_new_from_resource ("/org/gnome/epiphany/gtk/page-menu-popover.ui");
  page_menu_popover = GTK_WIDGET (gtk_builder_get_object (builder, "page-menu-popover"));
  header_bar->zoom_level_button = GTK_WIDGET (gtk_builder_get_object (builder, "zoom-level"));
  if (ephy_embed_shell_get_mode (embed_shell) == EPHY_EMBED_SHELL_MODE_APPLICATION) {
    gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "new-window-separator")));
    gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "new-window-button")));
    gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "new-incognito-window-button")));
    gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "reopen-closed-tab-button")));
    gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "bookmarks-separator")));
    gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "import-bookmarks-button")));
    gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "export-bookmarks-button")));
    gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "save-as-application-separator")));
    gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "save-as-application-button")));
    gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "override-text-encoding-separator")));
    gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "override-text-encoding-button")));
    gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "keyboard-shortcuts-button")));
    gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "help-button")));
  } else {
    if (ephy_is_running_inside_flatpak ()) {
      gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "save-as-application-separator")));
      gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "save-as-application-button")));
    }

    if (is_desktop_pantheon ()) {
      gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "help-button")));
    }
  }

  gtk_menu_button_set_popover (GTK_MENU_BUTTON (button), page_menu_popover);
  g_object_unref (builder);

  gtk_header_bar_pack_end (GTK_HEADER_BAR (header_bar), button);

  /* End action elements */
  header_bar->action_bar_end = ephy_action_bar_end_new ();
  gtk_widget_show (GTK_WIDGET (header_bar->action_bar_end));
  header_bar->end_revealer = GTK_REVEALER (gtk_revealer_new ());
  g_signal_connect (header_bar->end_revealer, "notify::child-revealed",
                    G_CALLBACK (update_revealer_visibility), NULL);
  g_signal_connect (header_bar->end_revealer, "notify::reveal-child",
                    G_CALLBACK (update_revealer_visibility), NULL);
  gtk_revealer_set_transition_type (GTK_REVEALER (header_bar->end_revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT);
  gtk_container_add (GTK_CONTAINER (header_bar->end_revealer), GTK_WIDGET (header_bar->action_bar_end));

  gtk_header_bar_pack_end (GTK_HEADER_BAR (header_bar),
                           GTK_WIDGET (header_bar->end_revealer));
}

static void
ephy_header_bar_init (EphyHeaderBar *header_bar)
{
}

static void
ephy_header_bar_class_init (EphyHeaderBarClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = ephy_header_bar_set_property;
  gobject_class->get_property = ephy_header_bar_get_property;
  gobject_class->constructed = ephy_header_bar_constructed;

  object_properties[PROP_WINDOW] =
    g_param_spec_object ("window",
                         "Window",
                         "The header_bar's EphyWindow",
                         EPHY_TYPE_WINDOW,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class,
                                     N_PROPERTIES,
                                     object_properties);
}

GtkWidget *
ephy_header_bar_new (EphyWindow *window)
{
  g_assert (EPHY_IS_WINDOW (window));

  return GTK_WIDGET (g_object_new (EPHY_TYPE_HEADER_BAR,
                                   "show-close-button", TRUE,
                                   "window", window,
                                   NULL));
}

EphyTitleWidget *
ephy_header_bar_get_title_widget (EphyHeaderBar *header_bar)
{
  return header_bar->title_widget;
}

GtkWidget *
ephy_header_bar_get_zoom_level_button (EphyHeaderBar *header_bar)
{
  return header_bar->zoom_level_button;
}

GtkWidget *
ephy_header_bar_get_page_menu_button (EphyHeaderBar *header_bar)
{
  return header_bar->page_menu_button;
}

EphyWindow *
ephy_header_bar_get_window (EphyHeaderBar *header_bar)
{
  return header_bar->window;
}

EphyActionBarStart *
ephy_header_bar_get_action_bar_start (EphyHeaderBar *header_bar)
{
  return header_bar->action_bar_start;
}

EphyActionBarEnd *
ephy_header_bar_get_action_bar_end (EphyHeaderBar *header_bar)
{
  return header_bar->action_bar_end;
}

void
ephy_header_bar_set_adaptive_mode (EphyHeaderBar    *header_bar,
                                   EphyAdaptiveMode  adaptive_mode)
{
  switch (adaptive_mode) {
  case EPHY_ADAPTIVE_MODE_NORMAL:
    gtk_revealer_set_reveal_child (GTK_REVEALER (header_bar->start_revealer), TRUE);
    gtk_revealer_set_reveal_child (GTK_REVEALER (header_bar->end_revealer), TRUE);

    break;
  case EPHY_ADAPTIVE_MODE_NARROW:
    gtk_revealer_set_reveal_child (GTK_REVEALER (header_bar->start_revealer), FALSE);
    gtk_revealer_set_reveal_child (GTK_REVEALER (header_bar->end_revealer), FALSE);

    break;
  }
}
