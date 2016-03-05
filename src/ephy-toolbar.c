/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2012 Igalia S.L
 *  Copyright © 2013 Yosef Or Boczko <yoseforb@gmail.com>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "ephy-toolbar.h"

#include "ephy-location-entry.h"
#include "ephy-middle-clickable-button.h"
#include "ephy-private.h"
#include "ephy-downloads-popover.h"
#include "ephy-downloads-progress-icon.h"

enum {
  PROP_0,
  PROP_WINDOW,
  N_PROPERTIES
};

static GParamSpec *object_properties[N_PROPERTIES] = { NULL, };

struct _EphyToolbar {
  GtkHeaderBar parent_instance;

  EphyWindow *window;
  EphyTitleBox *title_box;
  GtkWidget *entry;
  GtkWidget *navigation_box;
  GtkWidget *page_menu_button;
  GtkWidget *new_tab_button;
  GtkWidget *downloads_revealer;
  GtkWidget *downloads_button;
  GtkWidget *downloads_popover;
};

G_DEFINE_TYPE (EphyToolbar, ephy_toolbar, GTK_TYPE_HEADER_BAR)

static void
download_added_cb (EphyDownloadsManager *manager,
                   EphyDownload *download,
                   EphyToolbar *toolbar)
{
  if (!toolbar->downloads_popover) {
    toolbar->downloads_popover = ephy_downloads_popover_new (toolbar->downloads_button);
    gtk_menu_button_set_popover (GTK_MENU_BUTTON (toolbar->downloads_button),
                                 toolbar->downloads_popover);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toolbar->downloads_button), TRUE);
  }

  gtk_revealer_set_reveal_child (GTK_REVEALER (toolbar->downloads_revealer), TRUE);
}

static void
download_completed_cb (EphyDownloadsManager *manager,
                       EphyDownload *download,
                       EphyToolbar *toolbar)
{
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toolbar->downloads_button), TRUE);
}

static void
download_removed_cb (EphyDownloadsManager *manager,
                     EphyDownload *download,
                     EphyToolbar *toolbar)
{
  if (!ephy_downloads_manager_get_downloads (manager))
    gtk_revealer_set_reveal_child (GTK_REVEALER (toolbar->downloads_revealer), FALSE);
}

static void
downloads_estimated_progress_cb (EphyDownloadsManager *manager,
                                 EphyToolbar *toolbar)
{
  gtk_widget_queue_draw (gtk_button_get_image (GTK_BUTTON (toolbar->downloads_button)));
}

static void
ephy_toolbar_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
  EphyToolbar *toolbar = EPHY_TOOLBAR (object);

  switch (property_id) {
  case PROP_WINDOW:
    toolbar->window = EPHY_WINDOW (g_value_get_object (value));
    g_object_notify_by_pspec (object, object_properties[PROP_WINDOW]);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
ephy_toolbar_get_property (GObject *object,
                           guint property_id,
                           GValue *value,
                           GParamSpec *pspec)
{
  EphyToolbar *toolbar = EPHY_TOOLBAR (object);

  switch (property_id) {
  case PROP_WINDOW:
    g_value_set_object (value, toolbar->window);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
sync_chromes_visibility (EphyToolbar *toolbar)
{
  EphyWindowChrome chrome;

  chrome = ephy_window_get_chrome (toolbar->window);

  gtk_widget_set_visible (toolbar->navigation_box, chrome & EPHY_WINDOW_CHROME_TOOLBAR);
  gtk_widget_set_visible (toolbar->page_menu_button, chrome & EPHY_WINDOW_CHROME_MENU);
  gtk_widget_set_visible (toolbar->new_tab_button, chrome & EPHY_WINDOW_CHROME_TABSBAR);
}

static void
ephy_toolbar_constructed (GObject *object)
{
  EphyToolbar *toolbar = EPHY_TOOLBAR (object);
  GtkActionGroup *action_group;
  GtkAction *action;
  GtkUIManager *manager;
  GtkWidget *box, *button, *menu;
  EphyDownloadsManager *downloads_manager;

  G_OBJECT_CLASS (ephy_toolbar_parent_class)->constructed (object);

  g_signal_connect_swapped (toolbar->window, "notify::chrome",
                            G_CALLBACK (sync_chromes_visibility), toolbar);

  /* Back and Forward */
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  toolbar->navigation_box = box;

  /* Back */
  button = ephy_middle_clickable_button_new ();
  /* FIXME: apparently we need an image inside the button for the action
   * icon to appear. */
  gtk_button_set_image (GTK_BUTTON (button), gtk_image_new ());
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  action_group = ephy_window_get_toolbar_action_group (toolbar->window);
  action = gtk_action_group_get_action (action_group, "NavigationBack");
  gtk_activatable_set_related_action (GTK_ACTIVATABLE (button),
                                      action);
  gtk_button_set_label (GTK_BUTTON (button), NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (button), "image-button");
  gtk_container_add (GTK_CONTAINER (box), button);

  /* Forward */
  button = ephy_middle_clickable_button_new ();
  /* FIXME: apparently we need an image inside the button for the action
   * icon to appear. */
  gtk_button_set_image (GTK_BUTTON (button), gtk_image_new ());
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  action = gtk_action_group_get_action (action_group, "NavigationForward");
  gtk_activatable_set_related_action (GTK_ACTIVATABLE (button),
                                      action);
  gtk_button_set_label (GTK_BUTTON (button), NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (button), "image-button");
  gtk_container_add (GTK_CONTAINER (box), button);

  gtk_style_context_add_class (gtk_widget_get_style_context (box),
                               "raised");
  gtk_style_context_add_class (gtk_widget_get_style_context (box),
                               "linked");

  gtk_header_bar_pack_start (GTK_HEADER_BAR (toolbar), box);

  /* Reload/Stop */
  button = gtk_button_new ();
  /* FIXME: apparently we need an image inside the button for the action
   * icon to appear. */
  gtk_button_set_image (GTK_BUTTON (button), gtk_image_new ());
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  action = gtk_action_group_get_action (action_group, "ViewCombinedStopReload");
  gtk_activatable_set_related_action (GTK_ACTIVATABLE (button),
                                      action);
  gtk_header_bar_pack_start (GTK_HEADER_BAR (toolbar), button);

  /* New Tab */
  button = gtk_button_new ();
  toolbar->new_tab_button = button;
  /* FIXME: apparently we need an image inside the button for the action
   * icon to appear. */
  gtk_button_set_image (GTK_BUTTON (button), gtk_image_new ());
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  action = gtk_action_group_get_action (action_group, "FileNewTab");
  gtk_activatable_set_related_action (GTK_ACTIVATABLE (button),
                                      action);
  gtk_button_set_label (GTK_BUTTON (button), NULL);
  gtk_header_bar_pack_start (GTK_HEADER_BAR (toolbar), button);


  /* Location bar + Title */
  toolbar->title_box = ephy_title_box_new (toolbar->window);
  toolbar->entry = ephy_title_box_get_location_entry (toolbar->title_box);
  gtk_header_bar_set_custom_title (GTK_HEADER_BAR (toolbar), GTK_WIDGET (toolbar->title_box));
  gtk_widget_show (GTK_WIDGET (toolbar->title_box));

  /* Page Menu */
  button = gtk_menu_button_new ();
  toolbar->page_menu_button = button;
  gtk_button_set_image (GTK_BUTTON (button), gtk_image_new_from_icon_name ("open-menu-symbolic", GTK_ICON_SIZE_BUTTON));
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  manager = ephy_window_get_ui_manager (toolbar->window);
  menu = gtk_ui_manager_get_widget (manager, "/ui/PagePopup");
  gtk_widget_set_halign (menu, GTK_ALIGN_END);
  gtk_menu_button_set_popup (GTK_MENU_BUTTON (button), menu);
  gtk_header_bar_pack_end (GTK_HEADER_BAR (toolbar), button);

  /* Downloads */
  downloads_manager = ephy_embed_shell_get_downloads_manager (ephy_embed_shell_get_default ());

  toolbar->downloads_revealer = gtk_revealer_new ();
  gtk_revealer_set_transition_type (GTK_REVEALER (toolbar->downloads_revealer), GTK_REVEALER_TRANSITION_TYPE_CROSSFADE);
  gtk_revealer_set_reveal_child (GTK_REVEALER (toolbar->downloads_revealer),
                                 ephy_downloads_manager_get_downloads (downloads_manager) != NULL);

  toolbar->downloads_button = gtk_menu_button_new ();
  gtk_button_set_image (GTK_BUTTON (toolbar->downloads_button), ephy_downloads_progress_icon_new ());
  gtk_widget_set_valign (toolbar->downloads_button, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (toolbar->downloads_revealer), toolbar->downloads_button);
  gtk_widget_show (toolbar->downloads_button);

  if (ephy_downloads_manager_get_downloads (downloads_manager)) {
    toolbar->downloads_popover = ephy_downloads_popover_new (toolbar->downloads_button);
    gtk_menu_button_set_popover (GTK_MENU_BUTTON (toolbar->downloads_button),
                                 toolbar->downloads_popover);
  }

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

  gtk_header_bar_pack_end (GTK_HEADER_BAR (toolbar), toolbar->downloads_revealer);
  gtk_widget_show (toolbar->downloads_revealer);
}

static void
ephy_toolbar_class_init (EphyToolbarClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->set_property = ephy_toolbar_set_property;
    gobject_class->get_property = ephy_toolbar_get_property;
    gobject_class->constructed = ephy_toolbar_constructed;

    object_properties[PROP_WINDOW] =
      g_param_spec_object ("window",
                           "Window",
                           "The toolbar's EphyWindow",
                           EPHY_TYPE_WINDOW,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (gobject_class,
                                       N_PROPERTIES,
                                       object_properties);
}

static void
ephy_toolbar_init (EphyToolbar *toolbar)
{
}

GtkWidget*
ephy_toolbar_new (EphyWindow *window)
{
    g_return_val_if_fail (EPHY_IS_WINDOW (window), NULL);

    return GTK_WIDGET (g_object_new (EPHY_TYPE_TOOLBAR,
                                     "show-close-button", TRUE,
                                     "window", window,
                                     NULL));
}

GtkWidget *
ephy_toolbar_get_location_entry (EphyToolbar *toolbar)
{
  return toolbar->entry;
}

EphyTitleBox *
ephy_toolbar_get_title_box (EphyToolbar *toolbar)
{
  return toolbar->title_box;
}
