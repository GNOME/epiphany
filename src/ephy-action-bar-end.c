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

#include "ephy-action-bar-end.h"

#include "ephy-downloads-popover.h"
#include "ephy-downloads-progress-icon.h"
#include "ephy-shell.h"

struct _EphyActionBarEnd {
  GtkBox parent_instance;

  GtkWidget *bookmarks_button;
  GtkWidget *new_tab_revealer;
  GtkWidget *new_tab_button;
  GtkWidget *downloads_revealer;
  GtkWidget *downloads_button;
  GtkWidget *downloads_popover;
};

G_DEFINE_TYPE (EphyActionBarEnd, ephy_action_bar_end, GTK_TYPE_BOX)

static gboolean
is_for_active_window (EphyActionBarEnd *action_bar_end)
{
  EphyShell *shell = ephy_shell_get_default ();
  GtkWidget *ancestor;
  GtkWindow*active_window;

  ancestor = gtk_widget_get_ancestor (GTK_WIDGET (action_bar_end), GTK_TYPE_WINDOW);
  active_window = gtk_application_get_active_window (GTK_APPLICATION (shell));

  return active_window == GTK_WINDOW (ancestor);
}

static void
download_added_cb (EphyDownloadsManager *manager,
                   EphyDownload         *download,
                   EphyActionBarEnd     *action_bar_end)
{
  if (!action_bar_end->downloads_popover) {
    action_bar_end->downloads_popover = ephy_downloads_popover_new (action_bar_end->downloads_button);
    gtk_menu_button_set_popover (GTK_MENU_BUTTON (action_bar_end->downloads_button),
                                 action_bar_end->downloads_popover);
  }

  gtk_revealer_set_reveal_child (GTK_REVEALER (action_bar_end->downloads_revealer), TRUE);

  if (is_for_active_window (action_bar_end) &&
      gtk_widget_get_mapped (GTK_WIDGET (action_bar_end)))
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (action_bar_end->downloads_button), TRUE);
}

static void
download_completed_cb (EphyDownloadsManager *manager,
                       EphyDownload         *download,
                       EphyActionBarEnd     *action_bar_end)
{
  if (is_for_active_window (action_bar_end) &&
      gtk_widget_get_mapped (GTK_WIDGET (action_bar_end)))
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (action_bar_end->downloads_button), TRUE);
}

static void
download_removed_cb (EphyDownloadsManager *manager,
                     EphyDownload         *download,
                     EphyActionBarEnd     *action_bar_end)
{
  if (!ephy_downloads_manager_get_downloads (manager))
    gtk_revealer_set_reveal_child (GTK_REVEALER (action_bar_end->downloads_revealer), FALSE);
}

static void
downloads_estimated_progress_cb (EphyDownloadsManager *manager,
                                 EphyActionBarEnd     *action_bar_end)
{
  gtk_widget_queue_draw (gtk_button_get_image (GTK_BUTTON (action_bar_end->downloads_button)));
}

static void
ephy_action_bar_end_class_init (EphyActionBarEndClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/action-bar-end.ui");

  gtk_widget_class_bind_template_child (widget_class,
                                        EphyActionBarEnd,
                                        bookmarks_button);
  gtk_widget_class_bind_template_child (widget_class,
                                        EphyActionBarEnd,
                                        new_tab_revealer);
  gtk_widget_class_bind_template_child (widget_class,
                                        EphyActionBarEnd,
                                        new_tab_button);
  gtk_widget_class_bind_template_child (widget_class,
                                        EphyActionBarEnd,
                                        downloads_revealer);
  gtk_widget_class_bind_template_child (widget_class,
                                        EphyActionBarEnd,
                                        downloads_button);
}

static void
ephy_action_bar_end_init (EphyActionBarEnd *action_bar_end)
{
  GObject *object = G_OBJECT (action_bar_end);
  EphyDownloadsManager *downloads_manager;

  /* Ensure the types used by the template have been initialized. */
  EPHY_TYPE_DOWNLOADS_PROGRESS_ICON;

  gtk_widget_init_template (GTK_WIDGET (action_bar_end));

  /* Downloads */
  downloads_manager = ephy_embed_shell_get_downloads_manager (ephy_embed_shell_get_default ());

  gtk_revealer_set_reveal_child (GTK_REVEALER (action_bar_end->downloads_revealer),
                                 ephy_downloads_manager_get_downloads (downloads_manager) != NULL);

  if (ephy_downloads_manager_get_downloads (downloads_manager)) {
    action_bar_end->downloads_popover = ephy_downloads_popover_new (action_bar_end->downloads_button);
    gtk_menu_button_set_popover (GTK_MENU_BUTTON (action_bar_end->downloads_button), action_bar_end->downloads_popover);
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
}

EphyActionBarEnd *
ephy_action_bar_end_new (void)
{
  return g_object_new (EPHY_TYPE_ACTION_BAR_END,
                       NULL);
}

void
ephy_action_bar_end_set_show_bookmarks_button (EphyActionBarEnd *action_bar_end,
                                               gboolean          show)
{
  gtk_widget_set_visible (action_bar_end->bookmarks_button, show);
}

void
ephy_action_bar_end_set_show_new_tab_button (EphyActionBarEnd *action_bar_end,
                                             gboolean          show)
{
  gtk_widget_set_visible (action_bar_end->new_tab_revealer, show);
  gtk_revealer_set_reveal_child (GTK_REVEALER (action_bar_end->new_tab_revealer), show);
}

GtkWidget *
ephy_action_bar_end_get_bookmarks_button (EphyActionBarEnd *action_bar_end)
{
  return action_bar_end->bookmarks_button;
}
