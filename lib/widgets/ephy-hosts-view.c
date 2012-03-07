/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2011, 2012 Igalia S.L.
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
#include "ephy-hosts-view.h"

#include "ephy-gui.h"
#include "ephy-hosts-store.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

G_DEFINE_TYPE (EphyHostsView, ephy_hosts_view, EPHY_TYPE_HISTORY_VIEW)

static void
ephy_hosts_view_class_init (EphyHostsViewClass *klass)
{
}

static void
ephy_hosts_view_init (EphyHostsView *self)
{
  GtkTreeViewColumn *column;

  column = gtk_tree_view_column_new_with_attributes (_("Sites"),
                                                     gtk_cell_renderer_text_new (),
                                                     "text", EPHY_HOSTS_STORE_COLUMN_TITLE,
                                                     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (self), column);
}

GtkWidget *
ephy_hosts_view_new (void)
{
  return g_object_new (EPHY_TYPE_HOSTS_VIEW, NULL);
}

/**
 * ephy_hosts_view_select_host:
 * @view: A #EphyHostView
 * @host: a @host or %NULL to select the first item
 *
 * Selects the row pointed by @host or, when not found or row is
 * %NULL, select the first item ("All sites").
 *
 * Returns: whether @host was found.
 **/
gboolean
ephy_hosts_view_select_host (EphyHostsView *view,
                             EphyHistoryHost *host)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  gint id;
  gboolean found = FALSE;

  g_return_val_if_fail (EPHY_IS_HOSTS_VIEW (view), FALSE);

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));

  gtk_tree_model_get_iter_first (model, &iter);

  if (host != NULL) {
    do {
      gtk_tree_model_get (model, &iter,
                          EPHY_HOSTS_STORE_COLUMN_ID, &id,
                          -1);
      if (id == host->id) {
        found = TRUE;
        break;
      }
    } while (gtk_tree_model_iter_next (model, &iter));
  }

  if (host == NULL || found == FALSE) {
    gtk_tree_model_get_iter_first (model, &iter);
  }

  gtk_tree_selection_select_iter (
    gtk_tree_view_get_selection (GTK_TREE_VIEW (view)), &iter);

  return found;
}
