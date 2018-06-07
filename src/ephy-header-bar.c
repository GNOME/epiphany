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

#include "ephy-action-helper.h"
#include "ephy-add-bookmark-popover.h"
#include "ephy-bookmarks-popover.h"
#include "ephy-bookmark-properties-grid.h"
#include "ephy-downloads-popover.h"
#include "ephy-downloads-progress-icon.h"
#include "ephy-embed.h"
#include "ephy-embed-container.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-utils.h"
#include "ephy-favicon-helpers.h"
#include "ephy-flatpak-utils.h"
#include "ephy-gui.h"
#include "ephy-history-service.h"
#include "ephy-location-entry.h"
#include "ephy-middle-clickable-button.h"
#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-title-box.h"
#include "ephy-title-widget.h"
#include "ephy-type-builtins.h"

#include <glib/gi18n.h>
#include <webkit2/webkit2.h>

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
  EphyActionBarStart *action_bar_start;
  GtkWidget *navigation_box;
  GtkWidget *reader_mode_revealer;
  GtkWidget *reader_mode_button;
  GtkWidget *new_tab_revealer;
  GtkWidget *new_tab_button;
  GtkWidget *bookmarks_button;
  GtkWidget *page_menu_button;
  GtkWidget *downloads_revealer;
  GtkWidget *downloads_button;
  GtkWidget *downloads_popover;
  GtkWidget *zoom_level_button;
};

G_DEFINE_TYPE (EphyHeaderBar, ephy_header_bar, GTK_TYPE_HEADER_BAR)

static gboolean
header_bar_is_for_active_window (EphyHeaderBar *header_bar)
{
  EphyShell *shell = ephy_shell_get_default ();
  GtkWindow *active_window;

  active_window = gtk_application_get_active_window (GTK_APPLICATION (shell));

  return active_window == GTK_WINDOW (header_bar->window);
}

static void
download_added_cb (EphyDownloadsManager *manager,
                   EphyDownload         *download,
                   EphyHeaderBar        *header_bar)
{
  if (!header_bar->downloads_popover) {
    header_bar->downloads_popover = ephy_downloads_popover_new (header_bar->downloads_button);
    gtk_menu_button_set_popover (GTK_MENU_BUTTON (header_bar->downloads_button),
                                 header_bar->downloads_popover);
  }

  gtk_revealer_set_reveal_child (GTK_REVEALER (header_bar->downloads_revealer), TRUE);

  if (header_bar_is_for_active_window (header_bar))
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (header_bar->downloads_button), TRUE);
}

static void
download_completed_cb (EphyDownloadsManager *manager,
                       EphyDownload         *download,
                       EphyHeaderBar        *header_bar)
{
  if (header_bar_is_for_active_window (header_bar))
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (header_bar->downloads_button), TRUE);
}

static void
download_removed_cb (EphyDownloadsManager *manager,
                     EphyDownload         *download,
                     EphyHeaderBar        *header_bar)
{
  if (!ephy_downloads_manager_get_downloads (manager))
    gtk_revealer_set_reveal_child (GTK_REVEALER (header_bar->downloads_revealer), FALSE);
}

static void
downloads_estimated_progress_cb (EphyDownloadsManager *manager,
                                 EphyHeaderBar        *header_bar)
{
  gtk_widget_queue_draw (gtk_button_get_image (GTK_BUTTON (header_bar->downloads_button)));
}

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
  gtk_widget_set_visible (header_bar->bookmarks_button, chrome & EPHY_WINDOW_CHROME_BOOKMARKS);
  gtk_widget_set_visible (header_bar->page_menu_button, chrome & EPHY_WINDOW_CHROME_MENU);
  gtk_widget_set_visible (header_bar->new_tab_button, chrome & EPHY_WINDOW_CHROME_TABSBAR);
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
notebook_show_tabs_changed_cb (GtkNotebook   *notebook,
                               GParamSpec    *pspec,
                               EphyHeaderBar *header_bar)
{
  gboolean visible = !gtk_notebook_get_show_tabs (notebook);

  if (visible) {
    gtk_widget_show (header_bar->new_tab_revealer);
    gtk_revealer_set_reveal_child (GTK_REVEALER (header_bar->new_tab_revealer), TRUE);
  } else {
    /* Note the animation here doesn't actually work, since we hide the revealer
     * right away. That's not ideal, but not much we can do about it, since
     * hiding the revealer results in the location entry expanding, and that
     * needs to happen immediately or it looks pretty bad, so we can't wait
     * until the animation completes. Using the revealer is still worthwhile
     * because the new tab button reveal animation is more important.
     */
    gtk_revealer_set_reveal_child (GTK_REVEALER (header_bar->new_tab_revealer), FALSE);
    gtk_widget_hide (header_bar->new_tab_revealer);
  }
}

static void
ephy_header_bar_constructed (GObject *object)
{
  EphyHeaderBar *header_bar = EPHY_HEADER_BAR (object);
  GtkWidget *button, *notebook;
  GtkWidget *page_menu_popover;
  EphyDownloadsManager *downloads_manager;
  GtkBuilder *builder;
  EphyEmbedShell *embed_shell;

  G_OBJECT_CLASS (ephy_header_bar_parent_class)->constructed (object);

  g_signal_connect_swapped (header_bar->window, "notify::chrome",
                            G_CALLBACK (sync_chromes_visibility), header_bar);

  /* Start action elements */
  header_bar->action_bar_start = ephy_action_bar_start_new ();
  gtk_widget_show (GTK_WIDGET (header_bar->action_bar_start));

  gtk_header_bar_pack_start (GTK_HEADER_BAR (header_bar),
                             GTK_WIDGET (header_bar->action_bar_start));

  embed_shell = ephy_embed_shell_get_default ();

  /* Title widget (location entry or title box) */
  if (ephy_embed_shell_get_mode (embed_shell) == EPHY_EMBED_SHELL_MODE_APPLICATION)
    header_bar->title_widget = EPHY_TITLE_WIDGET (ephy_title_box_new ());
  else
    header_bar->title_widget = EPHY_TITLE_WIDGET (ephy_location_entry_new ());
  gtk_widget_set_margin_start (GTK_WIDGET (header_bar->title_widget), 54);
  gtk_widget_set_margin_end (GTK_WIDGET (header_bar->title_widget), 54);
  gtk_header_bar_set_custom_title (GTK_HEADER_BAR (header_bar), GTK_WIDGET (header_bar->title_widget));
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
  /* FIXME: This is horrible, but it doesn't seem possible to hide a single menu item of an existing menu.
   * Calling gtk_widget_hide() on the child menu item somehow hides the entire menu! */
  if (ephy_is_running_inside_flatpak ())
    builder = gtk_builder_new_from_resource ("/org/gnome/epiphany/gtk/page-menu-popover-flatpak.ui");
  else
    builder = gtk_builder_new_from_resource ("/org/gnome/epiphany/gtk/page-menu-popover.ui");
  page_menu_popover = GTK_WIDGET (gtk_builder_get_object (builder, "page-menu-popover"));
  header_bar->zoom_level_button = GTK_WIDGET (gtk_builder_get_object (builder, "zoom-level"));
  gtk_menu_button_set_popover (GTK_MENU_BUTTON (button), page_menu_popover);
  g_object_unref (builder);

  gtk_header_bar_pack_end (GTK_HEADER_BAR (header_bar), button);

  /* Bookmarks button */
  button = gtk_menu_button_new ();
  header_bar->bookmarks_button = button;
  gtk_button_set_image (GTK_BUTTON (button),
                        gtk_image_new_from_icon_name ("ephy-bookmarks-symbolic", GTK_ICON_SIZE_BUTTON));
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  /* Translators: tooltip for the bookmarks popover button */
  gtk_widget_set_tooltip_text (button, _("View and manage your bookmarks"));
  gtk_menu_button_set_popover (GTK_MENU_BUTTON (button), GTK_WIDGET (ephy_bookmarks_popover_new ()));
  gtk_header_bar_pack_end (GTK_HEADER_BAR (header_bar), button);

  /* Downloads */
  downloads_manager = ephy_embed_shell_get_downloads_manager (ephy_embed_shell_get_default ());

  header_bar->downloads_revealer = gtk_revealer_new ();
  gtk_revealer_set_transition_type (GTK_REVEALER (header_bar->downloads_revealer), GTK_REVEALER_TRANSITION_TYPE_CROSSFADE);
  gtk_revealer_set_reveal_child (GTK_REVEALER (header_bar->downloads_revealer),
                                 ephy_downloads_manager_get_downloads (downloads_manager) != NULL);

  button = gtk_menu_button_new ();
  header_bar->downloads_button = button;
  gtk_button_set_image (GTK_BUTTON (button), ephy_downloads_progress_icon_new ());
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  /* Translators: tooltip for the downloads button */
  gtk_widget_set_tooltip_text (button, _("View downloads"));
  gtk_container_add (GTK_CONTAINER (header_bar->downloads_revealer), button);
  gtk_widget_show (button);

  if (ephy_downloads_manager_get_downloads (downloads_manager)) {
    header_bar->downloads_popover = ephy_downloads_popover_new (button);
    gtk_menu_button_set_popover (GTK_MENU_BUTTON (button), header_bar->downloads_popover);
  }

  /* New Tab */
  header_bar->new_tab_revealer = gtk_revealer_new ();
  gtk_revealer_set_transition_type (GTK_REVEALER (header_bar->new_tab_revealer), GTK_REVEALER_TRANSITION_TYPE_CROSSFADE);
  gtk_header_bar_pack_end (GTK_HEADER_BAR (header_bar), header_bar->new_tab_revealer);

  button = gtk_button_new ();
  header_bar->new_tab_button = button;
  gtk_button_set_image (GTK_BUTTON (button),
                        gtk_image_new_from_icon_name ("tab-new-symbolic", GTK_ICON_SIZE_BUTTON));
  /* Translators: tooltip for the new tab button */
  gtk_widget_set_tooltip_text (button, _("Open a new tab"));
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  gtk_actionable_set_action_name (GTK_ACTIONABLE (button), "win.new-tab");
  gtk_container_add (GTK_CONTAINER (header_bar->new_tab_revealer), button);
  gtk_widget_show (button);

  notebook = ephy_window_get_notebook (header_bar->window);
  gtk_revealer_set_reveal_child (GTK_REVEALER (header_bar->new_tab_revealer),
                                 !gtk_notebook_get_show_tabs (GTK_NOTEBOOK (notebook)));
  gtk_widget_set_visible (header_bar->new_tab_revealer,
                          !gtk_notebook_get_show_tabs (GTK_NOTEBOOK (notebook)));
  g_signal_connect_object (notebook, "notify::show-tabs",
                           G_CALLBACK (notebook_show_tabs_changed_cb), header_bar, 0);

  g_signal_connect_object (downloads_manager, "download-added",
                           G_CALLBACK (download_added_cb),
                           object, 0);
  g_signal_connect_object (downloads_manager, "download-completed",
                           G_CALLBACK (download_completed_cb),
                           object, 0);
  g_signal_connect_object (downloads_manager, "download-removed",
                           G_CALLBACK (download_removed_cb),
                           object, 0);
  g_signal_connect_object (downloads_manager, "estimated-progress-changed",
                           G_CALLBACK (downloads_estimated_progress_cb),
                           object, 0);

  gtk_header_bar_pack_end (GTK_HEADER_BAR (header_bar), header_bar->downloads_revealer);
  gtk_widget_show (header_bar->downloads_revealer);
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
