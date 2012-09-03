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
#include "ephy-history-service.h"
#include "ephy-overview-store.h"
#include "ephy-snapshot-service.h"

/* Update thumbnails after one week. */
#define THUMBNAIL_UPDATE_THRESHOLD (60 * 60 * 24 * 7)

#define EPHY_OVERVIEW_STORE_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_OVERVIEW_STORE, EphyOverviewStorePrivate))

struct _EphyOverviewStorePrivate
{
  EphyHistoryService *history_service;
  GdkPixbuf *default_icon;
  GdkPixbuf *icon_frame;
};

enum
{
  PROP_0,
  PROP_HISTORY_SERVICE,
  PROP_DEFAULT_ICON,
  PROP_ICON_FRAME,
};

G_DEFINE_TYPE (EphyOverviewStore, ephy_overview_store, GTK_TYPE_LIST_STORE)

static void
ephy_overview_store_set_property (GObject *object,
                                  guint prop_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
  EphyOverviewStore *store = EPHY_OVERVIEW_STORE (object);

  switch (prop_id)
  {
  case PROP_HISTORY_SERVICE:
    store->priv->history_service = g_value_dup_object (value);
    g_object_notify (object, "history-service");
    break;
  case PROP_DEFAULT_ICON:
    ephy_overview_store_set_default_icon (store,
                                          g_value_get_object (value));
    break;
  case PROP_ICON_FRAME:
    ephy_overview_store_set_icon_frame (store,
                                        g_value_get_object (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
ephy_overview_store_get_property (GObject *object,
                                  guint prop_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
  EphyOverviewStore *store = EPHY_OVERVIEW_STORE (object);

  switch (prop_id)
  {
  case PROP_HISTORY_SERVICE:
    g_value_set_object (value, store->priv->history_service);
    break;
  case PROP_DEFAULT_ICON:
    g_value_set_object (value, store->priv->default_icon);
    break;
  case PROP_ICON_FRAME:
    g_value_set_object (value, store->priv->icon_frame);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
ephy_overview_store_dispose (GObject *object)
{
  EphyOverviewStorePrivate *priv = EPHY_OVERVIEW_STORE (object)->priv;

  if (priv->history_service)
    g_clear_object (&priv->history_service);
  if (priv->default_icon)
    g_clear_object (&priv->default_icon);
  if (priv->icon_frame)
    g_clear_object (&priv->icon_frame);

  G_OBJECT_CLASS (ephy_overview_store_parent_class)->dispose (object);
}

static void
ephy_overview_store_class_init (EphyOverviewStoreClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = ephy_overview_store_set_property;
  object_class->get_property = ephy_overview_store_get_property;
  object_class->dispose      = ephy_overview_store_dispose;

  g_object_class_install_property (object_class,
                                   PROP_HISTORY_SERVICE,
                                   g_param_spec_object ("history-service",
                                                        "History service",
                                                        "History Service",
                                                        EPHY_TYPE_HISTORY_SERVICE,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_object_class_install_property (object_class,
                                   PROP_DEFAULT_ICON,
                                   g_param_spec_object ("default-icon",
                                                        "Default icon",
                                                        "Default Icon",
                                                        GDK_TYPE_PIXBUF,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_object_class_install_property (object_class,
                                   PROP_ICON_FRAME,
                                   g_param_spec_object ("icon-frame",
                                                        "Icon frame",
                                                        "Frame to display around icons",
                                                        GDK_TYPE_PIXBUF,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_type_class_add_private (object_class, sizeof(EphyOverviewStorePrivate));
}

static void
ephy_overview_store_init (EphyOverviewStore *self)
{
  GType types[EPHY_OVERVIEW_STORE_NCOLS];

  types[EPHY_OVERVIEW_STORE_ID] = G_TYPE_STRING;
  types[EPHY_OVERVIEW_STORE_URI] = G_TYPE_STRING;
  types[EPHY_OVERVIEW_STORE_TITLE] = G_TYPE_STRING;
  types[EPHY_OVERVIEW_STORE_AUTHOR] = G_TYPE_STRING;
  types[EPHY_OVERVIEW_STORE_SNAPSHOT] = GDK_TYPE_PIXBUF;
  types[EPHY_OVERVIEW_STORE_LAST_VISIT] = G_TYPE_LONG;
  types[EPHY_OVERVIEW_STORE_SELECTED] = G_TYPE_BOOLEAN;
  types[EPHY_OVERVIEW_STORE_SNAPSHOT_CANCELLABLE] = G_TYPE_CANCELLABLE;
  types[EPHY_OVERVIEW_STORE_SNAPSHOT_MTIME] = G_TYPE_LONG;

  gtk_list_store_set_column_types (GTK_LIST_STORE (self),
                                   EPHY_OVERVIEW_STORE_NCOLS, types);

  self->priv = EPHY_OVERVIEW_STORE_GET_PRIVATE (self);
}

typedef struct {
  GtkTreeRowReference *ref;
  char *url;
  WebKitWebView *webview;
  GCancellable *cancellable;
  time_t timestamp;
} PeekContext;

static void
peek_context_free (PeekContext *ctx)
{
  g_free (ctx->url);
  gtk_tree_row_reference_free (ctx->ref);
  if (ctx->webview)
    g_object_unref (ctx->webview);
  if (ctx->cancellable)
    g_object_unref (ctx->cancellable);

  g_slice_free (PeekContext, ctx);
}

static GdkPixbuf *
ephy_overview_store_add_frame (EphyOverviewStore *store,
                               GdkPixbuf *snapshot)
{
  GdkPixbuf *framed;

  if (store->priv->icon_frame) {
    framed = gdk_pixbuf_copy (store->priv->icon_frame);
    gdk_pixbuf_copy_area (snapshot, 0, 0,
                          gdk_pixbuf_get_width (snapshot),
                          gdk_pixbuf_get_height (snapshot),
                          framed, 10, 9);
  } else
    framed = g_object_ref (snapshot);

  return framed;
}

static void
ephy_overview_store_set_snapshot_internal (EphyOverviewStore *store,
                                           GtkTreeIter *iter,
                                           GdkPixbuf *snapshot,
                                           int mtime)
{
  GdkPixbuf *framed;

  framed = ephy_overview_store_add_frame (store, snapshot);
  gtk_list_store_set (GTK_LIST_STORE (store), iter,
                      EPHY_OVERVIEW_STORE_SNAPSHOT, framed,
                      EPHY_OVERVIEW_STORE_SNAPSHOT_MTIME, mtime,
                      -1);
  g_object_unref (framed);
}

typedef struct {
  EphyHistoryURL *url;
  EphyHistoryService *history_service;
} ThumbnailTimeContext;

static void
on_snapshot_saved_cb (EphySnapshotService *service,
                      GAsyncResult *res,
                      ThumbnailTimeContext *ctx)
{
  ephy_history_service_set_url_thumbnail_time (ctx->history_service,
                                               ctx->url->url, ctx->url->thumbnail_time,
                                               NULL, NULL, NULL);
  ephy_history_url_free (ctx->url);
  g_slice_free (ThumbnailTimeContext, ctx);
}

void
ephy_overview_store_set_snapshot (EphyOverviewStore *store,
                                  GtkTreeIter *iter,
                                  cairo_surface_t *snapshot)
{
  GdkPixbuf *pixbuf;
  char *url;
  ThumbnailTimeContext *ctx;
  EphySnapshotService *snapshot_service;
  int mtime;

  mtime = time (NULL);
  pixbuf = ephy_snapshot_service_crop_snapshot (snapshot);
  ephy_overview_store_set_snapshot_internal (store, iter, pixbuf, mtime);
  gtk_tree_model_get (GTK_TREE_MODEL (store), iter,
                      EPHY_OVERVIEW_STORE_URI, &url,
                      -1);

  ctx = g_slice_new (ThumbnailTimeContext);
  ctx->url = ephy_history_url_new (url, NULL, 0, 0, 0);
  ctx->url->thumbnail_time = mtime;
  ctx->history_service = store->priv->history_service;
  g_free (url);

  snapshot_service = ephy_snapshot_service_get_default ();
  ephy_snapshot_service_save_snapshot_async (snapshot_service,
                                             pixbuf, ctx->url->url, ctx->url->thumbnail_time,
                                             NULL,
                                             (GAsyncReadyCallback) on_snapshot_saved_cb,
                                             ctx);
  g_object_unref (pixbuf);
}


static void
ephy_overview_store_set_default_icon_internal (EphyOverviewStore *store,
                                               GtkTreeIter *iter,
                                               GdkPixbuf *default_icon)
{
  gtk_list_store_set (GTK_LIST_STORE (store), iter,
                      EPHY_OVERVIEW_STORE_SNAPSHOT,
                      default_icon,
                      EPHY_OVERVIEW_STORE_SNAPSHOT_MTIME, 0,
                      -1);
}

static void
on_snapshot_retrieved_cb (GObject *object,
                          GAsyncResult *res,
                          PeekContext *ctx)
{
  EphyOverviewStore *store;
  GtkTreePath *path;
  GtkTreeIter iter;
  GdkPixbuf *snapshot;
  GError *error = NULL;

  snapshot = ephy_snapshot_service_get_snapshot_finish (EPHY_SNAPSHOT_SERVICE (object),
                                                        res, &error);

  if (error) {
    g_error_free (error);
    error = NULL;
  } else {
    store = EPHY_OVERVIEW_STORE (gtk_tree_row_reference_get_model (ctx->ref));
    path = gtk_tree_row_reference_get_path (ctx->ref);
    gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, path);
    gtk_tree_path_free (path);
    if (snapshot) {
      ephy_overview_store_set_snapshot_internal (store, &iter, snapshot, ctx->timestamp);
      g_object_unref (snapshot);

    } else {
      ephy_overview_store_set_default_icon_internal (store, &iter,
                                                     store->priv->default_icon);
    }
    gtk_list_store_set (GTK_LIST_STORE (store), &iter,
                        EPHY_OVERVIEW_STORE_SNAPSHOT_CANCELLABLE, NULL,
                        -1);
  }

  peek_context_free (ctx);
}

static void
history_service_url_cb (gpointer service,
                        gboolean success,
                        EphyHistoryURL *url,
                        PeekContext *ctx)
{
  EphySnapshotService *snapshot_service;

  snapshot_service = ephy_snapshot_service_get_default ();

  ctx->timestamp = url->thumbnail_time;

  ephy_snapshot_service_get_snapshot_async (snapshot_service,
                                            ctx->webview, ctx->url, ctx->timestamp, ctx->cancellable,
                                            (GAsyncReadyCallback) on_snapshot_retrieved_cb,
                                            ctx);
  ephy_history_url_free (url);
}

void
ephy_overview_store_peek_snapshot (EphyOverviewStore *self,
                                   WebKitWebView *webview,
                                   GtkTreeIter *iter)
{
  char *url;
  GtkTreePath *path;
  PeekContext *ctx;
  GCancellable *cancellable;

  gtk_tree_model_get (GTK_TREE_MODEL (self), iter,
                      EPHY_OVERVIEW_STORE_URI, &url,
                      EPHY_OVERVIEW_STORE_SNAPSHOT_CANCELLABLE, &cancellable,
                      -1);

  if (cancellable) {
    g_cancellable_cancel (cancellable);
    g_object_unref (cancellable);
  }

  if (url == NULL || g_strcmp0 (url, "about:blank") == 0) {
    gtk_list_store_set (GTK_LIST_STORE (self), iter,
                        EPHY_OVERVIEW_STORE_SNAPSHOT_CANCELLABLE,
                        NULL, -1);
    return;
  }

  cancellable = g_cancellable_new ();
  gtk_list_store_set (GTK_LIST_STORE (self), iter,
                      EPHY_OVERVIEW_STORE_SNAPSHOT_CANCELLABLE,
                      cancellable, -1);

  ctx = g_slice_new (PeekContext);
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (self), iter);
  ctx->ref = gtk_tree_row_reference_new (GTK_TREE_MODEL (self), path);
  ctx->url = url;
  ctx->webview = webview ? g_object_ref (webview) : NULL;
  ctx->cancellable = cancellable;
  ephy_history_service_get_url (self->priv->history_service,
                                url, NULL, (EphyHistoryJobCallback)history_service_url_cb,
                                ctx);
  gtk_tree_path_free (path);
}

static gboolean
set_default_icon_helper (GtkTreeModel *model,
                         GtkTreePath *path,
                         GtkTreeIter *iter,
                         GdkPixbuf *new_default_icon)
{
  EphyOverviewStorePrivate *priv;
  GdkPixbuf *current_pixbuf;

  priv = EPHY_OVERVIEW_STORE (model)->priv;

  gtk_tree_model_get (model, iter,
                      EPHY_OVERVIEW_STORE_SNAPSHOT, &current_pixbuf,
                      -1);
  if (current_pixbuf == priv->default_icon ||
      current_pixbuf == NULL)
    ephy_overview_store_set_default_icon_internal (EPHY_OVERVIEW_STORE (model), iter,
                                                   new_default_icon);
  g_object_unref (current_pixbuf);

  return FALSE;
}

void
ephy_overview_store_set_default_icon (EphyOverviewStore *store,
                                      GdkPixbuf *default_icon)
{
  GdkPixbuf *new_default_icon;

  if (store->priv->default_icon)
    g_object_unref (store->priv->default_icon);

  new_default_icon = ephy_overview_store_add_frame (store, default_icon);

  gtk_tree_model_foreach (GTK_TREE_MODEL (store),
                          (GtkTreeModelForeachFunc) set_default_icon_helper,
                          new_default_icon);

  store->priv->default_icon = new_default_icon;

  g_object_notify (G_OBJECT (store), "default-icon");
}

void
ephy_overview_store_set_icon_frame (EphyOverviewStore *store,
                                    GdkPixbuf *icon_frame)
{
  gboolean update_default = FALSE;
  GdkPixbuf *old_default_icon;

  if (store->priv->icon_frame == icon_frame)
    return;

  if (store->priv->icon_frame)
    g_object_unref (store->priv->icon_frame);
  else if (store->priv->default_icon)
    update_default = TRUE;

  store->priv->icon_frame = g_object_ref (icon_frame);

  if (update_default) {
    old_default_icon = g_object_ref (store->priv->default_icon);
    ephy_overview_store_set_default_icon (store,
                                          old_default_icon);
    g_object_unref (old_default_icon);
  }

  g_object_notify (G_OBJECT (store), "icon-frame");
}

gboolean
ephy_overview_store_needs_snapshot (EphyOverviewStore *store,
                                    GtkTreeIter *iter)
{
  GdkPixbuf *icon;
  GCancellable *cancellable;
  gboolean needs_snapshot;
  int mtime, current_mtime;

  g_return_val_if_fail (EPHY_IS_OVERVIEW_STORE (store), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  current_mtime = time (NULL);
  gtk_tree_model_get (GTK_TREE_MODEL (store), iter,
                      EPHY_OVERVIEW_STORE_SNAPSHOT, &icon,
                      EPHY_OVERVIEW_STORE_SNAPSHOT_MTIME, &mtime,
                      EPHY_OVERVIEW_STORE_SNAPSHOT_CANCELLABLE, &cancellable,
                      -1);

  /* If the thumbnail is the default icon and there is no cancellable
     in the row, then this row needs a snapshot. */
  needs_snapshot = (icon == store->priv->default_icon && cancellable == NULL) ||
    current_mtime - mtime > THUMBNAIL_UPDATE_THRESHOLD;

  if (icon)
    g_object_unref (icon);
  if (cancellable)
    g_object_unref (cancellable);

  return needs_snapshot;
}

gboolean
ephy_overview_store_remove (EphyOverviewStore *store,
                            GtkTreeIter *iter)
{
  GCancellable *cancellable;

  g_return_val_if_fail (EPHY_IS_OVERVIEW_STORE (store), FALSE);

  gtk_tree_model_get (GTK_TREE_MODEL (store), iter,
                      EPHY_OVERVIEW_STORE_SNAPSHOT_CANCELLABLE,
                      &cancellable,
                      -1);
  if (cancellable) {
    g_cancellable_cancel (cancellable);
    g_object_unref (cancellable);
  }

  return gtk_list_store_remove (GTK_LIST_STORE (store), iter);
}

typedef struct {
  GtkTreeRowReference *ref;
  EphyOverviewStoreAnimRemoveFunc callback;
  gpointer user_data;
} AnimRemoveContext;

static gboolean
animated_remove_func (AnimRemoveContext *ctx)
{
  GtkTreeRowReference *ref;
  EphyOverviewStore *store;
  GtkTreePath *path;
  GtkTreeIter iter;
  GdkPixbuf *orig_pixbuf, *new_pixbuf;
  int width, height;
  gboolean valid;

  ref = ctx->ref;
  store = EPHY_OVERVIEW_STORE (gtk_tree_row_reference_get_model (ref));
  path = gtk_tree_row_reference_get_path (ref);
  gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, path);
  gtk_tree_path_free (path);

  gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
                      EPHY_OVERVIEW_STORE_SNAPSHOT, &orig_pixbuf, -1);

  width = gdk_pixbuf_get_width (orig_pixbuf);
  height = gdk_pixbuf_get_height (orig_pixbuf);

  if (width > 10) {
    new_pixbuf = gdk_pixbuf_scale_simple (orig_pixbuf,
                                          width * 0.80,
                                          height * 0.80,
                                          GDK_INTERP_TILES);
    g_object_unref (orig_pixbuf);
    gtk_list_store_set (GTK_LIST_STORE (store), &iter,
                        EPHY_OVERVIEW_STORE_SNAPSHOT, new_pixbuf,
                        -1);
    g_object_unref (new_pixbuf);

    return TRUE;
  }

  g_object_unref (orig_pixbuf);
  valid = ephy_overview_store_remove (store, &iter);

  if (ctx->callback)
    ctx->callback (store, &iter, valid, ctx->user_data);

  gtk_tree_row_reference_free (ref);
  g_slice_free (AnimRemoveContext, ctx);

  return FALSE;
}

void
ephy_overview_store_animated_remove (EphyOverviewStore *store,
                                     GtkTreeRowReference *ref,
                                     EphyOverviewStoreAnimRemoveFunc callback,
                                     gpointer user_data)
{
  AnimRemoveContext *ctx = g_slice_new0 (AnimRemoveContext);

  ctx->ref = ref;
  ctx->callback = callback;
  ctx->user_data = user_data;

  g_timeout_add (40, (GSourceFunc) animated_remove_func, ctx);
}

gboolean
ephy_overview_store_find_url (EphyOverviewStore *store,
                              const char *url,
                              GtkTreeIter *iter)
{
  gboolean valid, found = FALSE;
  char *row_url;

  g_return_val_if_fail (EPHY_IS_OVERVIEW_STORE (store), FALSE);
  g_return_val_if_fail (url != NULL, FALSE);

  valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), iter);

  while (valid) {
    gtk_tree_model_get (GTK_TREE_MODEL (store), iter,
                        EPHY_OVERVIEW_STORE_URI, &row_url,
                        -1);

    found = g_strcmp0 (row_url, url) == 0;
    g_free (row_url);
    if (found)
      break;

    valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (store), iter);
  }

  return found;
}
