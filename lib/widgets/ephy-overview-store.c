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

#define EPHY_OVERVIEW_STORE_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_OVERVIEW_STORE, EphyOverviewStorePrivate))

struct _EphyOverviewStorePrivate
{
  EphyHistoryService *history_service;
  GdkPixbuf *default_icon;
};

enum
{
  PROP_0,
  PROP_HISTORY_SERVICE,
  PROP_DEFAULT_ICON,
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
    store->priv->history_service = g_value_get_object (value);
    g_object_notify (object, "history-service");
    break;
  case PROP_DEFAULT_ICON:
    ephy_overview_store_set_default_icon (store,
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
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
ephy_overview_store_class_init (EphyOverviewStoreClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = ephy_overview_store_set_property;
  object_class->get_property = ephy_overview_store_get_property;

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

  gtk_list_store_set_column_types (GTK_LIST_STORE (self),
                                   EPHY_OVERVIEW_STORE_NCOLS, types);

  self->priv = EPHY_OVERVIEW_STORE_GET_PRIVATE (self);
}

typedef struct {
  GtkTreeRowReference *ref;
  char *url;
  WebKitWebView *webview;
  GCancellable *cancellable;
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
overview_add_frame (GdkPixbuf *pixbuf) {
  cairo_t *cr;
  cairo_surface_t *surface;
  cairo_pattern_t *pattern;
  int width, height;
  int border = 10;
  GdkPixbuf *framed;

  width = gdk_pixbuf_get_width (pixbuf) + 2*border;
  height = gdk_pixbuf_get_height (pixbuf) + 2*border;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
  cr = cairo_create (surface);

  /* FIXME: This could be done as two masks that are later rotated
     and moved, instead of repeating the same code 4 times. */

  /* Draw the left-shadow. */
  cairo_save(cr);
  pattern = cairo_pattern_create_linear (border, border, 0, border);
  cairo_pattern_add_color_stop_rgba (pattern, 0, 0, 0, 0, 0.5);
  cairo_pattern_add_color_stop_rgba (pattern, 1, 0, 0, 0, 0.0);
  cairo_rectangle (cr, 0, border, border, height - 2*border);
  cairo_clip (cr);
  cairo_set_source (cr, pattern);
  cairo_mask (cr, pattern);
  cairo_pattern_destroy (pattern);
  cairo_restore (cr);

  /* Draw the up-left quarter-circle. */
  cairo_save(cr);
  pattern = cairo_pattern_create_radial (border, border, 0, border, border, border);
  cairo_pattern_add_color_stop_rgba (pattern, 0, 0, 0, 0,  0.5);
  cairo_pattern_add_color_stop_rgba (pattern, 1, 0, 0, 0, 0.0);
  cairo_rectangle (cr, 0, 0, border, border);
  cairo_clip (cr);
  cairo_set_source (cr, pattern);
  cairo_mask (cr, pattern);
  cairo_pattern_destroy (pattern);
  cairo_restore(cr);

  cairo_save(cr);
  pattern = cairo_pattern_create_linear (border, border, border, 0);
  cairo_pattern_add_color_stop_rgba (pattern, 0, 0, 0, 0, 0.5);
  cairo_pattern_add_color_stop_rgba (pattern, 1, 0, 0, 0, 0.0);
  cairo_rectangle (cr, border, 0, width - 2*border, border);
  cairo_clip (cr);
  cairo_set_source (cr, pattern);
  cairo_mask (cr, pattern);
  cairo_pattern_destroy (pattern);
  cairo_restore (cr);

  cairo_save(cr);
  pattern = cairo_pattern_create_radial (width - border, border, 0, width - border, border, border);
  cairo_pattern_add_color_stop_rgba (pattern, 0, 0, 0, 0,  0.5);
  cairo_pattern_add_color_stop_rgba (pattern, 1, 0, 0, 0, 0.0);
  cairo_rectangle (cr, width - border, 0, border, border);
  cairo_clip (cr);
  cairo_set_source (cr, pattern);
  cairo_mask (cr, pattern);
  cairo_pattern_destroy (pattern);
  cairo_restore(cr);

  cairo_save(cr);
  pattern = cairo_pattern_create_linear (width - border, border, width, border);
  cairo_pattern_add_color_stop_rgba (pattern, 0, 0, 0, 0, 0.5);
  cairo_pattern_add_color_stop_rgba (pattern, 1, 0, 0, 0, 0.0);
  cairo_rectangle (cr, width - border, border, width, height - 2*border);
  cairo_clip (cr);
  cairo_set_source (cr, pattern);
  cairo_mask (cr, pattern);
  cairo_pattern_destroy (pattern);
  cairo_restore (cr);

  cairo_save(cr);
  pattern = cairo_pattern_create_radial (border, height - border, 0, border, height - border, border);
  cairo_pattern_add_color_stop_rgba (pattern, 0, 0, 0, 0,  0.5);
  cairo_pattern_add_color_stop_rgba (pattern, 1, 0, 0, 0, 0.0);
  cairo_rectangle (cr, 0, height - border, border, border);
  cairo_clip (cr);
  cairo_set_source (cr, pattern);
  cairo_mask (cr, pattern);
  cairo_pattern_destroy (pattern);
  cairo_restore(cr);

  cairo_save(cr);
  pattern = cairo_pattern_create_linear (border, height - border, border, height);
  cairo_pattern_add_color_stop_rgba (pattern, 0, 0, 0, 0, 0.5);
  cairo_pattern_add_color_stop_rgba (pattern, 1, 0, 0, 0, 0.0);
  cairo_rectangle (cr, border, height - border, width - 2*border, border);
  cairo_clip (cr);
  cairo_set_source (cr, pattern);
  cairo_mask (cr, pattern);
  cairo_pattern_destroy (pattern);
  cairo_restore (cr);

  cairo_save(cr);
  pattern = cairo_pattern_create_radial (width - border, height - border, 0, width - border, height - border, border);
  cairo_pattern_add_color_stop_rgba (pattern, 0, 0, 0, 0,  0.5);
  cairo_pattern_add_color_stop_rgba (pattern, 1, 0, 0, 0, 0.0);
  cairo_rectangle (cr, width - border, height - border, border, border);
  cairo_clip (cr);
  cairo_set_source (cr, pattern);
  cairo_mask (cr, pattern);
  cairo_pattern_destroy (pattern);
  cairo_restore(cr);

  gdk_cairo_set_source_pixbuf (cr, pixbuf, border, border);
  cairo_rectangle (cr, border, border, width - 2*border, height - 2*border);
  cairo_clip(cr);
  cairo_paint (cr);

  framed = gdk_pixbuf_get_from_surface (surface, 0, 0, width, height);

  cairo_destroy (cr);
  cairo_surface_destroy (surface);

  return framed;
}

static void
ephy_overview_store_set_snapshot_internal (EphyOverviewStore *store,
                                           GtkTreeIter *iter,
                                           GdkPixbuf *snapshot)
{
  GdkPixbuf *framed;

  framed = overview_add_frame (snapshot);
  gtk_list_store_set (GTK_LIST_STORE (store), iter,
                      EPHY_OVERVIEW_STORE_SNAPSHOT, framed,
                      -1);
  g_object_unref (framed);
}

static void
history_service_get_url_for_saving (EphyHistoryService *service,
                                    gboolean success,
                                    EphyHistoryURL *url,
                                    GdkPixbuf *pixbuf)
{
  EphySnapshotService *snapshot_service;
  int timestamp;

  snapshot_service = ephy_snapshot_service_get_default ();
  timestamp = success ? (url->visit_count / 5) : 0;

  ephy_snapshot_service_save_snapshot_async (snapshot_service,
                                             pixbuf, url->url, timestamp,
                                             NULL, NULL, NULL);
  g_object_unref (pixbuf);
}

void
ephy_overview_store_set_snapshot (EphyOverviewStore *store,
                                  GtkTreeIter *iter,
                                  cairo_surface_t *snapshot)
{
  GdkPixbuf *pixbuf;
  char *url;

  pixbuf = ephy_snapshot_service_crop_snapshot (snapshot);
  ephy_overview_store_set_snapshot_internal (store, iter, pixbuf);
  gtk_tree_model_get (GTK_TREE_MODEL (store), iter,
                      EPHY_OVERVIEW_STORE_URI, &url,
                      -1);
  ephy_history_service_get_url (store->priv->history_service, url,
                                NULL, (EphyHistoryJobCallback) history_service_get_url_for_saving,
                                pixbuf);
}

static void
on_snapshot_retrieved_cb (GObject *object,
                          GAsyncResult *res,
                          PeekContext *ctx)
{
  GtkTreeModel *model;
  GtkTreePath *path;
  GtkTreeIter iter;
  GdkPixbuf *snapshot;
  GError *error = NULL;

  snapshot = ephy_snapshot_service_get_snapshot_finish (EPHY_SNAPSHOT_SERVICE (object),
                                                        res, &error);

  if (error) {
    g_warning ("Error retrieving snapshot: %s\n", error->message);
    g_error_free (error);
    error = NULL;
  } else {
    model = gtk_tree_row_reference_get_model (ctx->ref);
    path = gtk_tree_row_reference_get_path (ctx->ref);
    gtk_tree_model_get_iter (model, &iter, path);
    gtk_tree_path_free (path);
    if (snapshot) {
      ephy_overview_store_set_snapshot_internal (EPHY_OVERVIEW_STORE (model),
                                                 &iter, snapshot);
      g_object_unref (snapshot);

    }
    gtk_list_store_set (GTK_LIST_STORE (model), &iter,
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
  int timestamp;

  snapshot_service = ephy_snapshot_service_get_default ();

  /* This is a bit of an abuse of the semantics of the mtime
   paramenter. Since the thumbnailing backend only takes the exact
   mtime of the thumbnailed file, we will use the visit count to
   generate a fake timestamp that will be increased every fifth
   visit. This way, we'll update the thumbnail every other fifth
   visit. */
  timestamp = success ? (url->visit_count / 5) : 0;

  ephy_snapshot_service_get_snapshot_async (snapshot_service,
                                            ctx->webview, ctx->url, timestamp, ctx->cancellable,
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

  gtk_list_store_set (GTK_LIST_STORE (self), iter,
                      EPHY_OVERVIEW_STORE_SNAPSHOT,
                      self->priv->default_icon,
                      -1);

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
    gtk_list_store_set (GTK_LIST_STORE (model), iter,
                        EPHY_OVERVIEW_STORE_SNAPSHOT, new_default_icon,
                        -1);
  g_object_unref (current_pixbuf);

  return FALSE;
}

void
ephy_overview_store_set_default_icon (EphyOverviewStore *store,
                                      GdkPixbuf *default_icon)
{
  if (store->priv->default_icon == default_icon)
    return;

  if (store->priv->default_icon)
    g_object_unref (store->priv->default_icon);

  store->priv->default_icon = g_object_ref (default_icon);

  gtk_tree_model_foreach (GTK_TREE_MODEL (store),
                          (GtkTreeModelForeachFunc) set_default_icon_helper,
                          NULL);

  g_object_notify (G_OBJECT (store), "default-icon");
}

gboolean
ephy_overview_store_needs_snapshot (EphyOverviewStore *store,
                                    GtkTreeIter *iter)
{
  GdkPixbuf *icon;
  GCancellable *cancellable;
  gboolean needs_snapshot;

  g_return_val_if_fail (EPHY_IS_OVERVIEW_STORE (store), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  gtk_tree_model_get (GTK_TREE_MODEL (store), iter,
                      EPHY_OVERVIEW_STORE_SNAPSHOT, &icon,
                      EPHY_OVERVIEW_STORE_SNAPSHOT_CANCELLABLE, &cancellable,
                      -1);

  /* If the thumbnail is the default icon and there is no cancellable
     in the row, then this row needs a snapshot. */
  needs_snapshot = (icon == store->priv->default_icon && cancellable == NULL);

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
