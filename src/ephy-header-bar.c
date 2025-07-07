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

#include "ephy-desktop-utils.h"
#include "ephy-embed-utils.h"
#include "ephy-file-helpers.h"
#include "ephy-flatpak-utils.h"
#include "ephy-location-entry.h"
#include "ephy-page-menu-button.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-title-box.h"
#include "ephy-title-widget.h"
#include "ephy-type-builtins.h"

#include <adwaita.h>
#include <glib/gi18n.h>

#define POPOVER_HIDE_DELAY 300

enum {
  PROP_0,
  PROP_WINDOW,
  N_PROPERTIES
};

static GParamSpec *object_properties[N_PROPERTIES] = { NULL, };

struct _EphyHeaderBar {
  AdwBin parent_instance;

  GtkWidget *header_bar;
  EphyWindow *window;
  EphyTitleWidget *title_widget;
  EphyActionBarStart *action_bar_start;
  EphyActionBarEnd *action_bar_end;
  GtkWidget *page_menu_button;
  GtkWidget *zoom_level_label;
  GtkWidget *restore_button;
  GtkWidget *combined_stop_reload_button;
  GtkWidget *page_menu_popover;

  guint popover_hide_timeout_id;
};

G_DEFINE_FINAL_TYPE (EphyHeaderBar, ephy_header_bar, ADW_TYPE_BIN)

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

static void
hide_timeout_cb (EphyHeaderBar *header_bar)
{
  ephy_page_menu_button_popdown (EPHY_PAGE_MENU_BUTTON (header_bar->page_menu_button));

  header_bar->popover_hide_timeout_id = 0;
}

void
fullscreen_changed_cb (EphyHeaderBar *header_bar)
{
  gboolean fullscreen;

  g_object_get (header_bar->window, "fullscreened", &fullscreen, NULL);

  adw_header_bar_set_show_start_title_buttons (ADW_HEADER_BAR (header_bar->header_bar), !fullscreen);
  adw_header_bar_set_show_end_title_buttons (ADW_HEADER_BAR (header_bar->header_bar), !fullscreen);
  gtk_widget_set_visible (header_bar->restore_button, fullscreen);

  if (fullscreen) {
    g_clear_handle_id (&header_bar->popover_hide_timeout_id, g_source_remove);

    header_bar->popover_hide_timeout_id =
      g_timeout_add_once (POPOVER_HIDE_DELAY, (GSourceOnceFunc)hide_timeout_cb, header_bar);
  }
}

static void
ephy_header_bar_constructed (GObject *object)
{
  EphyHeaderBar *header_bar = EPHY_HEADER_BAR (object);
  GtkWidget *event_box;
  EphyEmbedShell *embed_shell;
  GtkSizeGroup *downloads_size_group;

  G_OBJECT_CLASS (ephy_header_bar_parent_class)->constructed (object);

  g_signal_connect_object (header_bar->window, "notify::chrome",
                           G_CALLBACK (sync_chromes_visibility), header_bar,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (header_bar->window, "notify::fullscreened",
                           G_CALLBACK (fullscreen_changed_cb), header_bar,
                           G_CONNECT_SWAPPED);

  /* Header bar */
  header_bar->header_bar = adw_header_bar_new ();
  adw_bin_set_child (ADW_BIN (header_bar), header_bar->header_bar);

  /* Start action elements */
  header_bar->action_bar_start = ephy_action_bar_start_new ();
  adw_header_bar_pack_start (ADW_HEADER_BAR (header_bar->header_bar),
                             GTK_WIDGET (header_bar->action_bar_start));

  embed_shell = ephy_embed_shell_get_default ();

  /* Title widget (location entry or title box) */
  if (ephy_embed_shell_get_mode (embed_shell) == EPHY_EMBED_SHELL_MODE_APPLICATION)
    header_bar->title_widget = EPHY_TITLE_WIDGET (ephy_title_box_new ());
  else
    header_bar->title_widget = EPHY_TITLE_WIDGET (ephy_location_entry_new ());

  event_box = adw_bin_new ();
  adw_header_bar_set_title_widget (ADW_HEADER_BAR (header_bar->header_bar), event_box);
  gtk_widget_set_name (event_box, "title-box-container");

  if (is_desktop_pantheon ()) {
    /* Use a full-width entry on Pantheon */
    gtk_widget_set_hexpand (GTK_WIDGET (header_bar->title_widget), TRUE);
    gtk_widget_set_margin_start (GTK_WIDGET (header_bar->title_widget), 6);
    gtk_widget_set_margin_end (GTK_WIDGET (header_bar->title_widget), 6);

    adw_bin_set_child (ADW_BIN (event_box), GTK_WIDGET (header_bar->title_widget));
  } else {
    GtkWidget *clamp;

    clamp = adw_clamp_new ();
    gtk_widget_set_hexpand (GTK_WIDGET (clamp), TRUE);
    adw_clamp_set_maximum_size (ADW_CLAMP (clamp), 860);
    adw_clamp_set_tightening_threshold (ADW_CLAMP (clamp), 560);
    adw_clamp_set_child (ADW_CLAMP (clamp), GTK_WIDGET (header_bar->title_widget));

    adw_bin_set_child (ADW_BIN (event_box), clamp);
  }

  /* Fullscreen restore button */
  header_bar->restore_button = gtk_button_new_from_icon_name ("view-restore-symbolic");
  gtk_widget_set_tooltip_text (header_bar->restore_button, _("Exit Fullscreen"));
  gtk_widget_set_visible (header_bar->restore_button, FALSE);
  gtk_actionable_set_action_name (GTK_ACTIONABLE (header_bar->restore_button),
                                  "win.fullscreen");
  adw_header_bar_pack_end (ADW_HEADER_BAR (header_bar->header_bar),
                           GTK_WIDGET (header_bar->restore_button));

  /* Page Menu */
  header_bar->page_menu_button = GTK_WIDGET (ephy_page_menu_button_new ());
  adw_header_bar_pack_end (ADW_HEADER_BAR (header_bar->header_bar), header_bar->page_menu_button);

  /* End action elements */
  header_bar->action_bar_end = ephy_action_bar_end_new ();
  adw_header_bar_pack_end (ADW_HEADER_BAR (header_bar->header_bar),
                           GTK_WIDGET (header_bar->action_bar_end));

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
                         NULL, NULL,
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
  switch (adaptive_mode) {
    case EPHY_ADAPTIVE_MODE_NORMAL:
      gtk_widget_set_visible (GTK_WIDGET (header_bar->action_bar_start), TRUE);
      gtk_widget_set_visible (GTK_WIDGET (header_bar->action_bar_end), TRUE);
      gtk_widget_set_visible (header_bar->page_menu_button, TRUE);
      adw_header_bar_set_show_end_title_buttons (ADW_HEADER_BAR (header_bar->header_bar), TRUE);

      break;
    case EPHY_ADAPTIVE_MODE_NARROW:
      gtk_widget_set_visible (GTK_WIDGET (header_bar->action_bar_start), FALSE);
      gtk_widget_set_visible (GTK_WIDGET (header_bar->action_bar_end), FALSE);
      gtk_widget_set_visible (header_bar->page_menu_button, FALSE);
      adw_header_bar_set_show_end_title_buttons (ADW_HEADER_BAR (header_bar->header_bar), FALSE);

      break;
  }

  if (ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) != EPHY_EMBED_SHELL_MODE_APPLICATION)
    ephy_location_entry_set_adaptive_mode (EPHY_LOCATION_ENTRY (header_bar->title_widget), adaptive_mode);
}
