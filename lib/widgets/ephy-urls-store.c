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
#include "ephy-urls-store.h"

#include <gtk/gtk.h>

G_DEFINE_TYPE (EphyURLsStore, ephy_urls_store, GTK_TYPE_LIST_STORE)

static void
ephy_urls_store_class_init (EphyURLsStoreClass *klass)
{
}

static void
ephy_urls_store_init (EphyURLsStore *self)
{
  GType types[EPHY_URLS_STORE_N_COLUMNS];

  types[EPHY_URLS_STORE_COLUMN_TITLE]   = G_TYPE_STRING;
  types[EPHY_URLS_STORE_COLUMN_ADDRESS] = G_TYPE_STRING;
  types[EPHY_URLS_STORE_COLUMN_DATE]    = G_TYPE_INT;

  gtk_list_store_set_column_types (GTK_LIST_STORE (self),
                                   EPHY_URLS_STORE_N_COLUMNS,
                                   types);
}

EphyURLsStore *
ephy_urls_store_new (void)
{
  return g_object_new (EPHY_TYPE_URLS_STORE, NULL);
}

void
ephy_urls_store_add_urls (EphyURLsStore *store,
                             GList *urls)
{
  EphyHistoryURL *url;
  GList *iter;

  for (iter = urls; iter != NULL; iter = iter->next) {
    url = (EphyHistoryURL *)iter->data;
    gtk_list_store_insert_with_values (GTK_LIST_STORE (store),
                                       NULL, G_MAXINT,
                                       EPHY_URLS_STORE_COLUMN_TITLE, url->title,
                                       EPHY_URLS_STORE_COLUMN_ADDRESS, url->url,
                                       EPHY_URLS_STORE_COLUMN_DATE, url->last_visit_time,
                                       -1);
  }
}

void
ephy_urls_store_add_url (EphyURLsStore *store, EphyHistoryURL *url)
{
  GList *urls = NULL;
  urls = g_list_append (urls, url);
  ephy_urls_store_add_urls (store, urls);
  g_list_free (urls);
}

EphyHistoryURL *
ephy_urls_store_get_url_from_path (EphyURLsStore *store,
                                   GtkTreePath *path)
{
  GtkTreeIter iter;

  EphyHistoryURL *url = ephy_history_url_new ("", "", 0, 0, 0);

  gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, path);
  gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
                      EPHY_URLS_STORE_COLUMN_TITLE, &url->title,
                      EPHY_URLS_STORE_COLUMN_ADDRESS, &url->url,
                      -1);
  return url;
}
