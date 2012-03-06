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
#include "ephy-hosts-store.h"

G_DEFINE_TYPE (EphyHostsStore, ephy_hosts_store, GTK_TYPE_LIST_STORE)

static void
ephy_hosts_store_class_init (EphyHostsStoreClass *klass)
{
}

static void
ephy_hosts_store_init (EphyHostsStore *self)
{
  GType types[EPHY_HOSTS_STORE_N_COLUMNS];

  types[EPHY_HOSTS_STORE_COLUMN_ID]          = G_TYPE_INT;
  types[EPHY_HOSTS_STORE_COLUMN_TITLE]       = G_TYPE_STRING;
  types[EPHY_HOSTS_STORE_COLUMN_ADDRESS]     = G_TYPE_STRING;
  types[EPHY_HOSTS_STORE_COLUMN_VISIT_COUNT] = G_TYPE_INT;

  gtk_list_store_set_column_types (GTK_LIST_STORE (self),
                                   EPHY_HOSTS_STORE_N_COLUMNS,
                                   types);
}

EphyHostsStore *
ephy_hosts_store_new (void)
{
  return g_object_new (EPHY_TYPE_HOSTS_STORE,
                       NULL);
}

void
ephy_hosts_store_add_hosts (EphyHostsStore *store,
                            GList *hosts)
{
  EphyHistoryHost *host;
  GList *iter;

  for (iter = hosts; iter != NULL; iter = iter->next) {
    host = (EphyHistoryHost *)iter->data;
    gtk_list_store_insert_with_values (GTK_LIST_STORE (store),
                                       NULL, -1,
                                       EPHY_HOSTS_STORE_COLUMN_ID, host->id,
                                       EPHY_HOSTS_STORE_COLUMN_TITLE, host->title,
                                       EPHY_HOSTS_STORE_COLUMN_ADDRESS, host->url,
                                       EPHY_HOSTS_STORE_COLUMN_VISIT_COUNT, host->visit_count,
                                       -1);
  }
}

void
ephy_hosts_store_add_host (EphyHostsStore *store, EphyHistoryHost *host)
{
  GList *hosts = NULL;
  hosts = g_list_append (hosts, host);
  ephy_hosts_store_add_hosts (store, hosts);
  g_list_free (hosts);
}

EphyHistoryHost *
ephy_hosts_store_get_host_from_path (EphyHostsStore *store,
                                     GtkTreePath *path)
{
  GtkTreeIter iter;

  EphyHistoryHost *host = ephy_history_host_new ("", "", 0, 1.0);

  gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, path);
  gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
                      EPHY_HOSTS_STORE_COLUMN_ID, &host->id,
                      EPHY_HOSTS_STORE_COLUMN_TITLE, &host->title,
                      EPHY_HOSTS_STORE_COLUMN_ADDRESS, &host->url,
                      EPHY_HOSTS_STORE_COLUMN_VISIT_COUNT, &host->visit_count,
                      -1);
  return host;
}
