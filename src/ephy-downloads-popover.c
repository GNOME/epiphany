/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2015 Igalia S.L.
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
#include "ephy-downloads-popover.h"

#include "ephy-downloads-manager.h"
#include "ephy-download-widget.h"
#include "ephy-embed-shell.h"

#include <glib/gi18n.h>

struct _EphyDownloadsPopover {
  GtkPopover parent_instance;

  GtkWidget *downloads_box;
  GtkWidget *clear_button;
};

G_DEFINE_FINAL_TYPE (EphyDownloadsPopover, ephy_downloads_popover, GTK_TYPE_POPOVER)

#define DOWNLOADS_BOX_MIN_SIZE 330

static void
download_box_row_activated_cb (EphyDownloadsPopover *popover,
                               GtkListBoxRow        *row)
{
  EphyDownloadWidget *widget;
  EphyDownload *download;

  widget = EPHY_DOWNLOAD_WIDGET (gtk_list_box_row_get_child (GTK_LIST_BOX_ROW (row)));
  download = ephy_download_widget_get_download (widget);
  if (!ephy_download_succeeded (download))
    return;

  ephy_download_do_download_action (download,
                                    EPHY_DOWNLOAD_ACTION_OPEN);

  gtk_popover_popdown (GTK_POPOVER (popover));
}

static void
download_completed_cb (EphyDownloadsPopover *popover)
{
  gtk_widget_set_sensitive (popover->clear_button, TRUE);
}

static void
download_failed_cb (EphyDownloadsPopover *popover,
                    GError               *error)
{
  if (!g_error_matches (error, WEBKIT_DOWNLOAD_ERROR, WEBKIT_DOWNLOAD_ERROR_CANCELLED_BY_USER))
    gtk_widget_set_sensitive (popover->clear_button, TRUE);
}

static void
download_added_cb (EphyDownloadsPopover *popover,
                   EphyDownload         *download)
{
  GtkWidget *row;
  GtkWidget *widget;

  row = gtk_list_box_row_new ();
  gtk_list_box_prepend (GTK_LIST_BOX (popover->downloads_box), row);

  widget = ephy_download_widget_new (download);
  g_signal_connect_object (download, "completed",
                           G_CALLBACK (download_completed_cb),
                           popover, G_CONNECT_SWAPPED);
  g_signal_connect_object (download, "error",
                           G_CALLBACK (download_failed_cb),
                           popover, G_CONNECT_SWAPPED);
  gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), widget);
}

static void
download_removed_cb (EphyDownloadsPopover *popover,
                     EphyDownload         *download)
{
  EphyDownloadsManager *manager;
  GtkListBoxRow *row;
  int i = 0;

  /* Hide the popover before removing the last download widget so it "crumples"
   * more smoothly */
  if (!gtk_list_box_get_row_at_index (GTK_LIST_BOX (popover->downloads_box), 2))
    gtk_widget_set_visible (GTK_WIDGET (popover), FALSE);

  while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (popover->downloads_box), i++))) {
    GtkWidget *widget = gtk_list_box_row_get_child (row);
    if (!EPHY_IS_DOWNLOAD_WIDGET (widget))
      continue;

    if (ephy_download_widget_get_download (EPHY_DOWNLOAD_WIDGET (widget)) == download) {
      gtk_list_box_remove (GTK_LIST_BOX (popover->downloads_box), GTK_WIDGET (row));
      break;
    }
  }

  manager = ephy_embed_shell_get_downloads_manager (ephy_embed_shell_get_default ());
  gtk_widget_set_sensitive (popover->clear_button, !ephy_downloads_manager_has_active_downloads (manager));
}

static void
clear_button_clicked_cb (EphyDownloadsPopover *popover)
{
  EphyDownloadsManager *manager;
  GtkListBoxRow *row;
  int i = 0;

  gtk_widget_set_visible (GTK_WIDGET (popover), FALSE);

  manager = ephy_embed_shell_get_downloads_manager (ephy_embed_shell_get_default ());
  g_signal_handlers_block_by_func (manager, download_removed_cb, popover);

  while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (popover->downloads_box), i))) {
    GtkWidget *widget;
    EphyDownload *download;

    widget = gtk_list_box_row_get_child (row);
    download = ephy_download_widget_get_download (EPHY_DOWNLOAD_WIDGET (widget));

    if (ephy_download_is_active (download)) {
      i++;
    } else {
      ephy_downloads_manager_remove_download (manager, download);
      gtk_list_box_remove (GTK_LIST_BOX (popover->downloads_box), GTK_WIDGET (row));
    }
  }
  gtk_widget_set_sensitive (popover->clear_button, FALSE);

  g_signal_handlers_unblock_by_func (manager, download_removed_cb, popover);
}

static void
ephy_downloads_popover_class_init (EphyDownloadsPopoverClass *klass)
{
}

static void
ephy_downloads_popover_init (EphyDownloadsPopover *popover)
{
  GtkWidget *scrolled_window;
  GtkWidget *vbox;
  GList *downloads, *l;
  EphyDownloadsManager *manager = ephy_embed_shell_get_downloads_manager (ephy_embed_shell_get_default ());

  gtk_widget_add_css_class (GTK_WIDGET (popover), "menu");

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  scrolled_window = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (scrolled_window),
                                              DOWNLOADS_BOX_MIN_SIZE);

  popover->downloads_box = gtk_list_box_new ();
  g_signal_connect_swapped (popover->downloads_box, "row-activated",
                            G_CALLBACK (download_box_row_activated_cb),
                            popover);
  gtk_list_box_set_activate_on_single_click (GTK_LIST_BOX (popover->downloads_box), TRUE);
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (popover->downloads_box), GTK_SELECTION_NONE);
  gtk_widget_add_css_class (popover->downloads_box, "background");
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled_window), popover->downloads_box);

  downloads = ephy_downloads_manager_get_downloads (manager);
  for (l = downloads; l; l = g_list_next (l)) {
    EphyDownload *download = (EphyDownload *)l->data;
    GtkWidget *row;
    GtkWidget *widget;

    g_signal_connect_object (download, "completed",
                             G_CALLBACK (download_completed_cb),
                             popover, G_CONNECT_SWAPPED);
    g_signal_connect_object (download, "error",
                             G_CALLBACK (download_failed_cb),
                             popover, G_CONNECT_SWAPPED);

    row = gtk_list_box_row_new ();
    gtk_list_box_prepend (GTK_LIST_BOX (popover->downloads_box), row);

    widget = ephy_download_widget_new (download);
    gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), widget);
  }

  g_signal_connect_object (manager, "download-added",
                           G_CALLBACK (download_added_cb),
                           popover, G_CONNECT_SWAPPED);
  g_signal_connect_object (manager, "download-removed",
                           G_CALLBACK (download_removed_cb),
                           popover, G_CONNECT_SWAPPED);

  gtk_box_append (GTK_BOX (vbox), scrolled_window);

  popover->clear_button = gtk_button_new_with_mnemonic (_("_Clear All"));
  gtk_widget_set_sensitive (popover->clear_button, !ephy_downloads_manager_has_active_downloads (manager));
  g_signal_connect_swapped (popover->clear_button, "clicked",
                            G_CALLBACK (clear_button_clicked_cb),
                            popover);
  gtk_widget_set_halign (popover->clear_button, GTK_ALIGN_END);
  gtk_widget_set_margin_start (popover->clear_button, 6);
  gtk_widget_set_margin_end (popover->clear_button, 6);
  gtk_widget_set_margin_top (popover->clear_button, 6);
  gtk_widget_set_margin_bottom (popover->clear_button, 6);
  gtk_box_append (GTK_BOX (vbox), popover->clear_button);

  gtk_popover_set_child (GTK_POPOVER (popover), vbox);
}

GtkWidget *
ephy_downloads_popover_new (void)
{
  return GTK_WIDGET (g_object_new (EPHY_TYPE_DOWNLOADS_POPOVER, NULL));
}
