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
#include "ephy-urls-view.h"

#include "ephy-gui.h"
#include "ephy-urls-store.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

G_DEFINE_TYPE (EphyURLsView, ephy_urls_view, EPHY_TYPE_HISTORY_VIEW)

static void
ephy_urls_view_class_init (EphyURLsViewClass *klass)
{
}

static void
ephy_urls_view_init (EphyURLsView *self)
{
  GtkTreeViewColumn *column;

  column = gtk_tree_view_column_new_with_attributes (_("Title"),
                                                     g_object_new (GTK_TYPE_CELL_RENDERER_TEXT,
                                                                   "ellipsize-set", TRUE,
                                                                   "ellipsize", PANGO_ELLIPSIZE_END, NULL),
                                                     "text", EPHY_URLS_STORE_COLUMN_TITLE,
                                                     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (self), column);

  column = gtk_tree_view_column_new_with_attributes (_("Address"),
                                                     g_object_new (GTK_TYPE_CELL_RENDERER_TEXT,
                                                                   "ellipsize-set", TRUE,
                                                                   "ellipsize", PANGO_ELLIPSIZE_END, NULL),
                                                     "text", EPHY_URLS_STORE_COLUMN_ADDRESS,
                                                     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (self), column);

  column = gtk_tree_view_column_new_with_attributes (_("Date"),
                                                     gtk_cell_renderer_text_new (),
                                                     "text", EPHY_URLS_STORE_COLUMN_DATE,
                                                     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (self), column);
}

GtkWidget *
ephy_urls_view_new (void)
{
  return g_object_new (EPHY_TYPE_URLS_VIEW, NULL);
}

static void
get_selection (GtkTreeModel *model,
               GtkTreePath *path,
               GtkTreeIter *iter,
               gpointer *data)
{
  EphyHistoryURL *url;

  url = ephy_urls_store_get_url_from_path (EPHY_URLS_STORE (model), path);
  *data = g_list_prepend (*data, url);
}

GList *
ephy_urls_view_get_selection (EphyURLsView *view)
{
  GtkTreeSelection *selection;
  GList *list = NULL;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
  gtk_tree_selection_selected_foreach (selection,
                                       (GtkTreeSelectionForeachFunc) get_selection,
                                       &list);

  return g_list_reverse (list);
}
