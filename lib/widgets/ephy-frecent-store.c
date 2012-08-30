/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2012 Igalia S.L.
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

#include "ephy-frecent-store.h"
#include "ephy-history-service.h"
#include "ephy-snapshot-service.h"

#define EPHY_FRECENT_STORE_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_FRECENT_STORE, EphyFrecentStorePrivate))

struct _EphyFrecentStorePrivate
{
  gint history_length;
};

enum
{
  PROP_0,
  PROP_HISTORY_LENGTH,
};

G_DEFINE_TYPE (EphyFrecentStore, ephy_frecent_store, EPHY_TYPE_OVERVIEW_STORE)

static void
on_find_urls_cb (EphyHistoryService *service,
                 gboolean success,
                 GList *urls,
                 EphyFrecentStore *store)
{
  EphyHistoryURL *url;
  char *old_url;
  GtkTreeIter treeiter;
  gboolean valid;
  GList *iter;
  gboolean peek_snapshot;

  if (success != TRUE)
    return;

  valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store),
                                         &treeiter);

  for (iter = urls; iter != NULL; iter = iter->next) {
    peek_snapshot = FALSE;
    url = (EphyHistoryURL *)iter->data;

    if (valid) {
      gtk_tree_model_get (GTK_TREE_MODEL (store), &treeiter,
                          EPHY_OVERVIEW_STORE_URI, &old_url,
                          -1);
      gtk_list_store_set (GTK_LIST_STORE (store), &treeiter,
                          EPHY_OVERVIEW_STORE_URI, url->url,
                          EPHY_OVERVIEW_STORE_LAST_VISIT, url->last_visit_time,
                          -1);
      if (g_strcmp0 (old_url, url->url) != 0) {
        gtk_list_store_set (GTK_LIST_STORE (store), &treeiter,
                            EPHY_OVERVIEW_STORE_TITLE, url->title,
                            -1);
        peek_snapshot = TRUE;
      }
      g_free (old_url);

      if (ephy_overview_store_needs_snapshot (EPHY_OVERVIEW_STORE (store), &treeiter))
        peek_snapshot = TRUE;
    } else {
      gtk_list_store_insert_with_values (GTK_LIST_STORE (store), &treeiter, -1,
                                         EPHY_OVERVIEW_STORE_TITLE, url->title,
                                         EPHY_OVERVIEW_STORE_URI, url->url,
                                         EPHY_OVERVIEW_STORE_LAST_VISIT, url->last_visit_time,
                                         -1);
      peek_snapshot = TRUE;
    }

    if (peek_snapshot)
      ephy_overview_store_peek_snapshot (EPHY_OVERVIEW_STORE (store),
                                         NULL, &treeiter);

    valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &treeiter);
  }

  g_list_free_full (urls, (GDestroyNotify)ephy_history_url_free);

  while (valid)
    valid = ephy_overview_store_remove (EPHY_OVERVIEW_STORE (store), &treeiter);
}

static void
ephy_frecent_store_fetch_urls (EphyFrecentStore *store,
                              EphyHistoryService *service)
{
  EphyHistoryQuery *query;

  query = ephy_history_query_new ();
  query->sort_type = EPHY_HISTORY_SORT_MV;
  query->limit = store->priv->history_length;
  query->ignore_hidden = TRUE;

  ephy_history_service_query_urls (service, query, NULL,
                                   (EphyHistoryJobCallback) on_find_urls_cb,
                                   store);
  ephy_history_query_free (query);
}

static gboolean
on_visit_url_cb (EphyHistoryService *service,
                 char *url,
                 EphyHistoryPageVisitType visit_type,
                 EphyFrecentStore *store)
{
  ephy_frecent_store_fetch_urls (store, service);

  return FALSE;
}

static void
on_cleared_cb (EphyHistoryService *service,
               EphyFrecentStore *store)
{
  /* Should probably emit a signal notifying that this is empty, this
     signal probably should live in EphyOverviewStore. */

  gtk_list_store_clear (GTK_LIST_STORE (store));
}

static void
on_url_title_changed (EphyHistoryService *service,
                      const char *url,
                      const char *title,
                      EphyFrecentStore *store)
{
  GtkTreeIter iter;
  gchar *iter_url;

  if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter))
    return;

  do {
    gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
                        EPHY_OVERVIEW_STORE_URI, &iter_url,
                        -1);
    if (g_strcmp0 (iter_url, url) == 0) {
      gtk_list_store_set (GTK_LIST_STORE (store), &iter,
                          EPHY_OVERVIEW_STORE_TITLE, title,
                          -1);
      g_free (iter_url);
      break;
    }
    g_free (iter_url);
  } while (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter));
}

static void
on_url_deleted (EphyHistoryService *service,
                const char *url,
                EphyFrecentStore *store)
{
  GtkTreeIter iter;
  gchar *iter_url;
  gboolean needs_update = FALSE;

  if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter))
    return;

  do {
    gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
                        EPHY_OVERVIEW_STORE_URI, &iter_url,
                        -1);
    if (g_strcmp0 (iter_url, url) == 0) {
      needs_update = TRUE;
      ephy_overview_store_remove (EPHY_OVERVIEW_STORE (store), &iter);
      g_free (iter_url);
      break;
    }
    g_free (iter_url);
  } while (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter));

  if (needs_update)
    ephy_frecent_store_fetch_urls (store, service);
}

static void
on_host_deleted (EphyHistoryService *service,
                 const char *deleted_url,
                 EphyFrecentStore *store)
{
  GtkTreeIter iter;
  gchar *iter_url;
  SoupURI *store_uri;
  SoupURI *deleted_uri;
  gboolean needs_update = FALSE;
  gboolean remove, valid;

  if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter))
    return;

  deleted_uri = soup_uri_new (deleted_url);

  do {
    gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
                        EPHY_OVERVIEW_STORE_URI, &iter_url,
                        -1);
    store_uri = soup_uri_new (iter_url);
    remove = g_strcmp0 (soup_uri_get_host (deleted_uri), soup_uri_get_host (store_uri)) == 0;
    soup_uri_free (store_uri);
    g_free (iter_url);

    if (remove) {
      valid = ephy_overview_store_remove (EPHY_OVERVIEW_STORE (store), &iter);
      needs_update = TRUE;
    } else
      valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter);
  } while (valid);

  soup_uri_free (deleted_uri);

  if (needs_update)
    ephy_frecent_store_fetch_urls (store, service);

}

static void
setup_history_service (EphyFrecentStore *store)
{
  EphyHistoryService *service;

  g_object_get (G_OBJECT (store), "history-service", &service, NULL);

  ephy_frecent_store_fetch_urls (store, service);

  g_signal_connect_after (service, "visit-url",
                    G_CALLBACK (on_visit_url_cb), store);
  g_signal_connect (service, "cleared",
                    G_CALLBACK (on_cleared_cb), store);
  g_signal_connect (service, "url-title-changed",
                    G_CALLBACK (on_url_title_changed), store);
  g_signal_connect (service, "url-deleted",
                    G_CALLBACK (on_url_deleted), store);
  g_signal_connect (service, "host-deleted",
                    G_CALLBACK (on_host_deleted), store);
  g_object_unref (service);
}

static void
ephy_frecent_store_notify (GObject *object,
                           GParamSpec *pspec)
{
  if (g_strcmp0 (pspec->name, "history-service") == 0)
    setup_history_service (EPHY_FRECENT_STORE (object));

  if (G_OBJECT_CLASS (ephy_frecent_store_parent_class)->notify)
    G_OBJECT_CLASS (ephy_frecent_store_parent_class)->notify (object, pspec);
}

static void
ephy_frecent_store_set_property (GObject *object,
                                 guint prop_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
  EphyFrecentStore *store = EPHY_FRECENT_STORE (object);

  switch (prop_id)
  {
  case PROP_HISTORY_LENGTH:
    ephy_frecent_store_set_history_length (store, g_value_get_int (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
ephy_frecent_store_get_property (GObject *object,
                                 guint prop_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
  EphyFrecentStore *store = EPHY_FRECENT_STORE (object);

  switch (prop_id)
  {
  case PROP_HISTORY_LENGTH:
    g_value_set_int (value, ephy_frecent_store_get_history_length (store));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
ephy_frecent_store_class_init (EphyFrecentStoreClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->notify = ephy_frecent_store_notify;
  object_class->set_property = ephy_frecent_store_set_property;
  object_class->get_property = ephy_frecent_store_get_property;

  g_object_class_install_property (object_class,
                                   PROP_HISTORY_LENGTH,
                                   g_param_spec_int ("history-length",
                                                     "History length",
                                                     "Length of the history list",
                                                     0, G_MAXINT, 12,
                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_type_class_add_private (object_class, sizeof(EphyFrecentStorePrivate));
}

static void
ephy_frecent_store_init (EphyFrecentStore *self)
{
  self->priv = EPHY_FRECENT_STORE_GET_PRIVATE (self);
}

EphyFrecentStore *
ephy_frecent_store_new (void)
{
  return g_object_new (EPHY_TYPE_FRECENT_STORE,
                       NULL);
}

static void
animated_remove_cb (EphyOverviewStore *store,
                    GtkTreeIter *iter,
                    gboolean valid,
                    EphyHistoryService *service)
{
  ephy_frecent_store_fetch_urls (EPHY_FRECENT_STORE (store), service);
}

static void
set_url_hidden_cb (EphyHistoryService *service,
                   gboolean success,
                   gpointer result_data,
                   GtkTreeRowReference *ref)
{
  EphyOverviewStore *store;
  GtkTreePath *path;
  GtkTreeIter iter;

  if (!success)
    return;

  store = EPHY_OVERVIEW_STORE (gtk_tree_row_reference_get_model (ref));
  path = gtk_tree_row_reference_get_path (ref);
  gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, path);
  gtk_tree_path_free (path);
  ephy_overview_store_animated_remove (store, ref,
                                       (EphyOverviewStoreAnimRemoveFunc)animated_remove_cb,
                                       service);
}

void
ephy_frecent_store_set_hidden (EphyFrecentStore *store,
                               GtkTreeIter *iter)
{
  EphyHistoryService *service;
  GtkTreeRowReference *ref;
  GtkTreePath *path;
  char *uri;
  GtkTreeModel *model;
  g_return_if_fail (EPHY_IS_FRECENT_STORE (store));
  g_return_if_fail (iter != NULL);

  model = GTK_TREE_MODEL (store);
  gtk_tree_model_get (model, iter,
                      EPHY_OVERVIEW_STORE_URI, &uri,
                      -1);
  g_object_get (store, "history-service", &service, NULL);

  path = gtk_tree_model_get_path (model, iter);
  ref = gtk_tree_row_reference_new (model, path);
  gtk_tree_path_free (path);

  ephy_history_service_set_url_hidden (service,
                                       uri, TRUE, NULL,
                                       (EphyHistoryJobCallback) set_url_hidden_cb,
                                       ref);
  g_free (uri);
  g_object_unref (service);
}

void
ephy_frecent_store_set_history_length (EphyFrecentStore *store,
                                       gint length)
{
  EphyHistoryService *service;

  g_return_if_fail (EPHY_IS_FRECENT_STORE (store));
  g_return_if_fail (length > 0);

  if (store->priv->history_length == length)
    return;

  store->priv->history_length = length;

  g_object_get (G_OBJECT (store), "history-service", &service, NULL);
  if (service) {
    ephy_frecent_store_fetch_urls (store, service);
    g_object_unref (service);
  }
}

int
ephy_frecent_store_get_history_length (EphyFrecentStore *store)
{
  g_return_val_if_fail (EPHY_IS_FRECENT_STORE (store), 0);

  return store->priv->history_length;
}
