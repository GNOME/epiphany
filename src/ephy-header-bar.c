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
#include "ephy-desktop-utils.h"
#include "ephy-embed-utils.h"
#include "ephy-file-helpers.h"
#include "ephy-flatpak-utils.h"
#include "ephy-location-entry.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-title-box.h"
#include "ephy-title-widget.h"
#include "ephy-type-builtins.h"

#include <glib/gi18n.h>
#include <handy.h>

#define POPOVER_HIDE_DELAY 300

enum {
  PROP_0,
  PROP_WINDOW,
  N_PROPERTIES
};

static GParamSpec *object_properties[N_PROPERTIES] = { NULL, };

/* Translators: tooltip for the refresh button */
static const char *REFRESH_BUTTON_TOOLTIP = N_("Reload the current page");

struct _EphyHeaderBar {
  GtkBin parent_instance;

  GtkWidget *header_bar;
  EphyWindow *window;
  EphyTitleWidget *title_widget;
  GtkRevealer *start_revealer;
  GtkRevealer *end_revealer;
  EphyActionBarStart *action_bar_start;
  EphyActionBarEnd *action_bar_end;
  GtkWidget *page_menu_button;
  GtkWidget *zoom_level_label;
  GtkWidget *restore_button;
  GtkWidget *combined_stop_reload_button;
  GtkWidget *combined_stop_reload_image;
  GtkWidget *page_menu_popover;

  guint popover_hide_timeout_id;
};

G_DEFINE_TYPE (EphyHeaderBar, ephy_header_bar, GTK_TYPE_BIN)

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
}

static gboolean
hide_timeout_cb (EphyHeaderBar *header_bar)
{
  gtk_popover_popdown (GTK_POPOVER (header_bar->page_menu_popover));

  header_bar->popover_hide_timeout_id = 0;

  return G_SOURCE_REMOVE;
}

void
fullscreen_changed_cb (EphyHeaderBar *header_bar)
{
  gboolean fullscreen;

  g_object_get (header_bar->window, "fullscreen", &fullscreen, NULL);

  gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (header_bar->header_bar), !fullscreen);
  gtk_widget_set_visible (header_bar->restore_button, fullscreen);

  if (fullscreen) {
    g_clear_handle_id (&header_bar->popover_hide_timeout_id, g_source_remove);

    header_bar->popover_hide_timeout_id =
      g_timeout_add (POPOVER_HIDE_DELAY, (GSourceFunc)hide_timeout_cb, header_bar);
  }
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
restore_button_clicked_cb (GtkButton     *button,
                           EphyHeaderBar *header_bar)
{
  GActionGroup *action_group;
  GAction *action;

  action_group = gtk_widget_get_action_group (GTK_WIDGET (header_bar->window), "win");
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "fullscreen");

  g_action_activate (action, NULL);
}

static void
update_revealer_visibility (GtkRevealer *revealer)
{
  gtk_widget_set_visible (GTK_WIDGET (revealer),
                          gtk_revealer_get_reveal_child (revealer) ||
                          gtk_revealer_get_child_revealed (revealer));
}

static void
ephy_header_bar_constructed (GObject *object)
{
  EphyHeaderBar *header_bar = EPHY_HEADER_BAR (object);
  GtkWidget *button;
  GtkWidget *event_box;
  GtkBuilder *builder;
  EphyEmbedShell *embed_shell;
  GtkSizeGroup *downloads_size_group;

  G_OBJECT_CLASS (ephy_header_bar_parent_class)->constructed (object);

  g_signal_connect_object (header_bar->window, "notify::chrome",
                           G_CALLBACK (sync_chromes_visibility), header_bar,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (header_bar->window, "notify::fullscreen",
                           G_CALLBACK (fullscreen_changed_cb), header_bar,
                           G_CONNECT_SWAPPED);

  /* Header bar */
  header_bar->header_bar = gtk_header_bar_new ();
  gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (header_bar->header_bar), TRUE);
  gtk_widget_show (header_bar->header_bar);
  gtk_container_add (GTK_CONTAINER (header_bar), header_bar->header_bar);

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

  gtk_header_bar_pack_start (GTK_HEADER_BAR (header_bar->header_bar),
                             GTK_WIDGET (header_bar->start_revealer));

  embed_shell = ephy_embed_shell_get_default ();

  /* Title widget (location entry or title box) */
  if (ephy_embed_shell_get_mode (embed_shell) == EPHY_EMBED_SHELL_MODE_APPLICATION)
    header_bar->title_widget = EPHY_TITLE_WIDGET (ephy_title_box_new ());
  else {
    header_bar->title_widget = EPHY_TITLE_WIDGET (ephy_location_entry_new ());
  }

  event_box = gtk_event_box_new ();
  gtk_widget_add_events (event_box, GDK_ALL_EVENTS_MASK);
  gtk_widget_show (event_box);
  gtk_header_bar_set_custom_title (GTK_HEADER_BAR (header_bar->header_bar), event_box);
  gtk_widget_set_name (event_box, "title-box-container");

  if (is_desktop_pantheon ()) {
    /* Use a full-width entry on Pantheon */
    gtk_widget_set_hexpand (GTK_WIDGET (header_bar->title_widget), TRUE);
    gtk_widget_set_margin_start (GTK_WIDGET (header_bar->title_widget), 6);
    gtk_widget_set_margin_end (GTK_WIDGET (header_bar->title_widget), 6);

    gtk_container_add (GTK_CONTAINER (event_box), GTK_WIDGET (header_bar->title_widget));
  } else {
    GtkWidget *clamp;

    clamp = hdy_clamp_new ();
    gtk_widget_set_hexpand (GTK_WIDGET (clamp), TRUE);
    gtk_widget_show (clamp);
    hdy_clamp_set_maximum_size (HDY_CLAMP (clamp), 860);
    hdy_clamp_set_tightening_threshold (HDY_CLAMP (clamp), 560);
    gtk_container_add (GTK_CONTAINER (clamp), GTK_WIDGET (header_bar->title_widget));

    gtk_container_add (GTK_CONTAINER (event_box), clamp);
  }

  gtk_widget_show (GTK_WIDGET (header_bar->title_widget));

  if (EPHY_IS_LOCATION_ENTRY (header_bar->title_widget)) {
    EphyLocationEntry *lentry = EPHY_LOCATION_ENTRY (header_bar->title_widget);
    GtkWidget *popover = ephy_add_bookmark_popover_new (ephy_location_entry_get_bookmark_widget (lentry), GTK_WIDGET (header_bar->window));

    g_signal_connect_object (popover, "update-state", G_CALLBACK (ephy_window_sync_bookmark_state), header_bar, G_CONNECT_SWAPPED);
    ephy_location_entry_set_add_bookmark_popover (lentry, GTK_POPOVER (popover));

    g_signal_connect_object (header_bar->title_widget,
                             "bookmark-clicked",
                             G_CALLBACK (add_bookmark_button_clicked_cb),
                             header_bar,
                             0);
  }

  /* Fullscreen restore button */
  header_bar->restore_button = gtk_button_new_from_icon_name ("view-restore-symbolic",
                                                              GTK_ICON_SIZE_BUTTON);
  g_signal_connect_object (header_bar->restore_button, "clicked",
                           G_CALLBACK (restore_button_clicked_cb),
                           header_bar, 0);
  gtk_header_bar_pack_end (GTK_HEADER_BAR (header_bar->header_bar),
                           GTK_WIDGET (header_bar->restore_button));

  /* Page Menu */
  button = gtk_menu_button_new ();
  header_bar->page_menu_button = button;
  gtk_button_set_image (GTK_BUTTON (button),
                        gtk_image_new_from_icon_name ("open-menu-symbolic", GTK_ICON_SIZE_BUTTON));
  g_type_ensure (G_TYPE_THEMED_ICON);
  builder = gtk_builder_new_from_resource ("/org/gnome/epiphany/gtk/page-menu-popover.ui");
  header_bar->page_menu_popover = GTK_WIDGET (gtk_builder_get_object (builder, "page-menu-popover"));
  header_bar->zoom_level_label = GTK_WIDGET (gtk_builder_get_object (builder, "zoom-level"));
  if (ephy_embed_shell_get_mode (embed_shell) == EPHY_EMBED_SHELL_MODE_APPLICATION) {
    gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "new-window-separator")));
    gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "new-window-button")));
    gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "new-incognito-window-button")));
    gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "reopen-closed-tab-button")));
    gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "save-as-application-separator")));
    gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "save-as-application-button")));
    gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "application-manager-button")));
    gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "override-text-encoding-separator")));
    gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "override-text-encoding-button")));
    gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "keyboard-shortcuts-button")));
    gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "help-button")));
    gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "firefox-sync-separator")));
    gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "firefox-sync-button")));
    gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "import-export-menu")));
  } else if (ephy_is_running_inside_sandbox ()) {
    gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "run-in-background-separator")));
    gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "run-in-background-button")));
    gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "save-as-application-separator")));
    gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "save-as-application-button")));
    gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "application-manager-button")));

    if (is_desktop_pantheon ())
      gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "help-button")));
  } else {
    gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "run-in-background-separator")));
    gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "run-in-background-button")));
  }

  header_bar->combined_stop_reload_button = GTK_WIDGET (gtk_builder_get_object (builder, "combined_stop_reload_button"));
  header_bar->combined_stop_reload_image = GTK_WIDGET (gtk_builder_get_object (builder, "combined_stop_reload_image"));
  gtk_widget_set_tooltip_text (header_bar->combined_stop_reload_button, _(REFRESH_BUTTON_TOOLTIP));

  if (is_desktop_pantheon ()) {
    gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "about-button")));

    gtk_button_set_image (GTK_BUTTON (button),
                          gtk_image_new_from_icon_name ("open-menu",
                                                        GTK_ICON_SIZE_LARGE_TOOLBAR));
  }
  g_settings_bind (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_ENABLE_WEBEXTENSIONS, gtk_builder_get_object (builder, "extensions-button"), "visible", G_SETTINGS_BIND_DEFAULT);

  gtk_menu_button_set_popover (GTK_MENU_BUTTON (button), header_bar->page_menu_popover);
  g_object_unref (builder);

  gtk_header_bar_pack_end (GTK_HEADER_BAR (header_bar->header_bar), button);

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

  gtk_header_bar_pack_end (GTK_HEADER_BAR (header_bar->header_bar),
                           GTK_WIDGET (header_bar->end_revealer));

  /* Sync the size of placeholder in EphyActionBarStart with downloads button */
  downloads_size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
  gtk_size_group_add_widget (downloads_size_group,
                             ephy_action_bar_start_get_placeholder (header_bar->action_bar_start));
  gtk_size_group_add_widget (downloads_size_group,
                             ephy_action_bar_end_get_downloads_revealer (header_bar->action_bar_end));
  g_object_unref (downloads_size_group);

  if (ephy_profile_dir_is_web_application ()) {
    GtkWidget *navigation_box = ephy_action_bar_start_get_navigation_box (header_bar->action_bar_start);

    g_settings_bind (EPHY_SETTINGS_WEB_APP, EPHY_PREFS_WEB_APP_SHOW_NAVIGATION_BUTTONS, navigation_box, "visible", G_SETTINGS_BIND_GET | G_SETTINGS_BIND_INVERT_BOOLEAN);
  }
}

static void
ephy_header_bar_dispose (GObject *object)
{
  EphyHeaderBar *header_bar = EPHY_HEADER_BAR (object);

  g_clear_handle_id (&header_bar->popover_hide_timeout_id, g_source_remove);

  G_OBJECT_CLASS (ephy_header_bar_parent_class)->dispose (object);
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
  gobject_class->dispose = ephy_header_bar_dispose;

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
                                   "window", window,
                                   NULL));
}

EphyTitleWidget *
ephy_header_bar_get_title_widget (EphyHeaderBar *header_bar)
{
  return header_bar->title_widget;
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
  ephy_action_bar_end_set_show_bookmark_button (header_bar->action_bar_end,
                                                adaptive_mode == EPHY_ADAPTIVE_MODE_NARROW);

  switch (adaptive_mode) {
    case EPHY_ADAPTIVE_MODE_NORMAL:
      gtk_revealer_set_reveal_child (GTK_REVEALER (header_bar->start_revealer), TRUE);
      gtk_revealer_set_reveal_child (GTK_REVEALER (header_bar->end_revealer), TRUE);
      gtk_widget_set_visible (header_bar->combined_stop_reload_button, FALSE);

      break;
    case EPHY_ADAPTIVE_MODE_NARROW:
      gtk_revealer_set_reveal_child (GTK_REVEALER (header_bar->start_revealer), FALSE);
      gtk_revealer_set_reveal_child (GTK_REVEALER (header_bar->end_revealer), FALSE);
      gtk_widget_set_visible (header_bar->combined_stop_reload_button, TRUE);

      break;
  }

  if (ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) != EPHY_EMBED_SHELL_MODE_APPLICATION)
    ephy_location_entry_set_adaptive_mode (EPHY_LOCATION_ENTRY (header_bar->title_widget), adaptive_mode);
}

void
ephy_header_bar_start_change_combined_stop_reload_state (EphyHeaderBar *header_bar,
                                                         gboolean       loading)
{
  if (loading) {
    gtk_image_set_from_icon_name (GTK_IMAGE (header_bar->combined_stop_reload_image),
                                  "process-stop-symbolic",
                                  get_icon_size ());
    /* Translators: tooltip for the stop button */
    gtk_widget_set_tooltip_text (header_bar->combined_stop_reload_button,
                                 _("Stop loading the current page"));
  } else {
    gtk_image_set_from_icon_name (GTK_IMAGE (header_bar->combined_stop_reload_image),
                                  "view-refresh-symbolic",
                                  get_icon_size ());
    gtk_widget_set_tooltip_text (header_bar->combined_stop_reload_button,
                                 _(REFRESH_BUTTON_TOOLTIP));
  }
}

void
ephy_header_bar_set_zoom_level (EphyHeaderBar *header_bar,
                                gdouble        zoom)
{
  g_autofree gchar *zoom_level = g_strdup_printf ("%2.0f%%", zoom * 100);

  gtk_label_set_label (GTK_LABEL (header_bar->zoom_level_label), zoom_level);
}

void
ephy_header_bar_add_browser_action (EphyHeaderBar *header_bar,
                                    GtkWidget     *action)
{
  ephy_action_bar_end_add_browser_action (header_bar->action_bar_end, action);
}
