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
#include "ephy-snapshot-service.h"

#include "ephy-favicon-helpers.h"

#ifndef GNOME_DESKTOP_USE_UNSTABLE_API
#define GNOME_DESKTOP_USE_UNSTABLE_API
#endif
#include <libgnome-desktop/gnome-desktop-thumbnail.h>
#include <webkit2/webkit2.h>

#define EPHY_SNAPSHOT_SERVICE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), EPHY_TYPE_SNAPSHOT_SERVICE, EphySnapshotServicePrivate))

struct _EphySnapshotServicePrivate
{
  /* Disk cache */
  GnomeDesktopThumbnailFactory *factory;

  /* Memory cache */
  GHashTable *cache;
};

G_DEFINE_TYPE (EphySnapshotService, ephy_snapshot_service, G_TYPE_OBJECT)

enum {
  SNAPSHOT_SAVED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

typedef enum {
  SNAPSHOT_STALE,
  SNAPSHOT_FRESH
} EphySnapshotFreshness;

typedef struct {
  char *path;
  EphySnapshotFreshness freshness;
} SnapshotPathCachedData;

static void
snapshot_path_cached_data_free (SnapshotPathCachedData *data)
{
  g_free (data->path);
  g_free (data);
}

static void
ephy_snapshot_service_class_init (EphySnapshotServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (EphySnapshotServicePrivate));

  /**
   * EphySnapshotService::snapshot-saved:
   * @url: the URL the snapshot was saved for
   * @mtime: the mtime embedded in the snapshot, needed to retrieve it
   *
   * The ::snapshot-saved signal is emitted when a new snapshot is saved.
   **/
  signals[SNAPSHOT_SAVED] = g_signal_new ("snapshot-saved",
                                          G_OBJECT_CLASS_TYPE (object_class),
                                          G_SIGNAL_RUN_LAST,
                                          0,
                                          NULL, NULL, NULL,
                                          G_TYPE_NONE,
                                          2,
                                          G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                                          G_TYPE_INT64);
}

static void
ephy_snapshot_service_init (EphySnapshotService *self)
{
  self->priv = EPHY_SNAPSHOT_SERVICE_GET_PRIVATE (self);
  self->priv->factory = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE);
  self->priv->cache = g_hash_table_new_full (g_str_hash, g_str_equal,
                                             (GDestroyNotify)g_free,
                                             (GDestroyNotify)snapshot_path_cached_data_free);
}

static GdkPixbuf *
ephy_snapshot_service_prepare_snapshot (cairo_surface_t *surface,
                                        cairo_surface_t *favicon)
{
  GdkPixbuf *snapshot, *scaled;
  int orig_width, orig_height;
  float orig_aspect_ratio, dest_aspect_ratio;
  int x_offset, new_width = 0, new_height;

  orig_width = cairo_image_surface_get_width (surface);
  orig_height = cairo_image_surface_get_height (surface);

  if (orig_width < EPHY_THUMBNAIL_WIDTH ||
      orig_height < EPHY_THUMBNAIL_HEIGHT) {
    snapshot = gdk_pixbuf_get_from_surface (surface,
                                            0, 0,
                                            orig_width, orig_height);
    scaled = gdk_pixbuf_scale_simple (snapshot,
                                      EPHY_THUMBNAIL_WIDTH,
                                      EPHY_THUMBNAIL_HEIGHT,
                                      GDK_INTERP_TILES);
  } else {
    orig_aspect_ratio = orig_width / (float)orig_height;
    dest_aspect_ratio = EPHY_THUMBNAIL_WIDTH / (float)EPHY_THUMBNAIL_HEIGHT;

    if (orig_aspect_ratio > dest_aspect_ratio) {
      /* Wider than taller, crop the sides. */
      new_width = orig_height * dest_aspect_ratio;
      new_height = orig_height;
      x_offset = (orig_width - new_width) / 2;
    } else {
      /* Crop the bottom otherwise. */
      new_width = orig_width;
      new_height = orig_width / (float)dest_aspect_ratio;
      x_offset = 0;
    }

    snapshot = gdk_pixbuf_get_from_surface (surface, x_offset, 0, new_width, new_height);
    scaled = gnome_desktop_thumbnail_scale_down_pixbuf (snapshot,
                                                        EPHY_THUMBNAIL_WIDTH,
                                                        EPHY_THUMBNAIL_HEIGHT);
  }

  g_object_unref (snapshot);

  if (favicon) {
    GdkPixbuf* fav_pixbuf;
    int favicon_size = 16;
    int x_offset = 6;
    int y_offset = gdk_pixbuf_get_height (scaled) - favicon_size - x_offset;

    fav_pixbuf = ephy_pixbuf_get_from_surface_scaled (favicon, favicon_size, favicon_size);
    gdk_pixbuf_composite (fav_pixbuf, scaled,
                          x_offset, y_offset, favicon_size, favicon_size,
                          x_offset, y_offset, 1, 1,
                          GDK_INTERP_NEAREST, 255);
    g_object_unref (fav_pixbuf);
  }

  return scaled;
}

typedef struct {
  WebKitWebView *web_view;
  time_t mtime;
  gboolean for_snapshot;

  GdkPixbuf *snapshot;
  char *path;
} SnapshotAsyncData;

static SnapshotAsyncData *
snapshot_async_data_new (WebKitWebView *web_view,
                         time_t mtime)
{
  SnapshotAsyncData *data;

  data = g_slice_new0 (SnapshotAsyncData);
  data->web_view = web_view;
  data->mtime = mtime;

  g_object_add_weak_pointer (G_OBJECT (web_view), (gpointer *)&data->web_view);

  return data;
}

static SnapshotAsyncData *
snapshot_async_data_new_for_snapshot (WebKitWebView *web_view,
                                      time_t mtime)
{
  SnapshotAsyncData *data = snapshot_async_data_new (web_view, mtime);

  data->for_snapshot = TRUE;

  return data;
}

static void
snapshot_async_data_free (SnapshotAsyncData *data)
{
  if (data->web_view)
    g_object_remove_weak_pointer (G_OBJECT (data->web_view), (gpointer *)&data->web_view);
  g_clear_object(&data->snapshot);
  g_free (data->path);

  g_slice_free (SnapshotAsyncData, data);
}

static void
snapshot_saved (EphySnapshotService *service,
                GAsyncResult *result,
                GTask *task)
{
  SnapshotAsyncData *data = g_task_get_task_data (task);
  char *path;

  path = ephy_snapshot_service_save_snapshot_finish (service, result, NULL);
  if (data->for_snapshot) {
    data->path = path;
    g_task_return_pointer (task, g_object_ref (data->snapshot), g_object_unref);
  } else {
    g_task_return_pointer (task, path, g_free);
  }
  g_object_unref (task);
}

static void
save_snapshot (cairo_surface_t *surface,
               GTask *task)
{
  SnapshotAsyncData *data = g_task_get_task_data (task);

  data->snapshot = ephy_snapshot_service_prepare_snapshot (surface,
                                                           webkit_web_view_get_favicon (data->web_view));

  ephy_snapshot_service_save_snapshot_async (g_task_get_source_object (task),
                                             data->snapshot,
                                             webkit_web_view_get_uri (data->web_view),
                                             data->mtime,
                                             g_task_get_cancellable (task),
                                             (GAsyncReadyCallback)snapshot_saved,
                                             task);
}

static void
on_snapshot_ready (WebKitWebView *web_view,
                   GAsyncResult *result,
                   GTask *task)
{
  cairo_surface_t *surface;
  GError *error = NULL;

  surface = webkit_web_view_get_snapshot_finish (web_view, result, &error);
  if (error) {
    g_task_return_error (task, error);
    g_object_unref (task);
    return;
  }

  save_snapshot (surface, task);
  cairo_surface_destroy (surface);
}

static gboolean
retrieve_snapshot_from_web_view (GTask *task)
{
  SnapshotAsyncData *data;

  data = g_task_get_task_data (task);
  if (!data->web_view) {
    g_task_return_new_error (task,
                             EPHY_SNAPSHOT_SERVICE_ERROR,
                             EPHY_SNAPSHOT_SERVICE_ERROR_WEB_VIEW,
                             "%s", "Error getting snapshot, web view was destroyed");
    g_object_unref (task);
    return FALSE;
  }

  webkit_web_view_get_snapshot (data->web_view,
                                WEBKIT_SNAPSHOT_REGION_VISIBLE,
                                WEBKIT_SNAPSHOT_OPTIONS_NONE,
                                NULL, (GAsyncReadyCallback)on_snapshot_ready,
                                task);
  return FALSE;
}

static void
webview_destroyed_cb (GtkWidget *web_view,
                      GTask *task)
{
  g_task_return_new_error (task,
                           EPHY_SNAPSHOT_SERVICE_ERROR,
                           EPHY_SNAPSHOT_SERVICE_ERROR_WEB_VIEW,
                           "%s", "Error getting snapshot, web view was destroyed");
  g_object_unref (task);
}

static void
webview_load_changed_cb (WebKitWebView *web_view,
                         WebKitLoadEvent load_event,
                         GTask *task)
{
  if (load_event != WEBKIT_LOAD_FINISHED)
    return;

  /* Load finished doesn't ensure that we actually have visible content yet,
     so hold a bit before retrieving the snapshot. */
  g_idle_add ((GSourceFunc) retrieve_snapshot_from_web_view, task);

  /* Some pages might end up causing this condition to happen twice, so remove
     the handler in order to avoid calling the above idle function twice. */
  g_signal_handlers_disconnect_by_func (web_view, webview_load_changed_cb, task);
  g_signal_handlers_disconnect_by_func (web_view, webview_destroyed_cb, task);
}

static gboolean
webview_load_failed_cb (WebKitWebView *web_view,
                        WebKitLoadEvent load_event,
                        const char failing_uri,
                        GError *error,
                        GTask *task)
{
  g_signal_handlers_disconnect_by_func (web_view, webview_load_changed_cb, task);
  g_signal_handlers_disconnect_by_func (web_view, webview_load_failed_cb, task);
  g_signal_handlers_disconnect_by_func (web_view, webview_destroyed_cb, task);
  g_task_return_new_error (task,
                           EPHY_SNAPSHOT_SERVICE_ERROR,
                           EPHY_SNAPSHOT_SERVICE_ERROR_WEB_VIEW,
                           "Error getting snapshot, web view failed to load: %s",
                           error->message);
  g_object_unref (task);

  return TRUE;
}

static gboolean
ephy_snapshot_service_take_from_webview (GTask *task)
{
  SnapshotAsyncData *data;

  data = g_task_get_task_data (task);
  if (!data->web_view) {
    g_task_return_new_error (task,
                             EPHY_SNAPSHOT_SERVICE_ERROR,
                             EPHY_SNAPSHOT_SERVICE_ERROR_WEB_VIEW,
                             "%s", "Error getting snapshot, web view was destroyed");
    g_object_unref (task);
    return FALSE;
  }

  if (webkit_web_view_get_estimated_load_progress (WEBKIT_WEB_VIEW (data->web_view)) == 1.0)
    retrieve_snapshot_from_web_view (task);
  else {
    g_signal_connect_object (data->web_view, "destroy",
                             G_CALLBACK (webview_destroyed_cb),
                             task, 0);
    g_signal_connect_object (data->web_view, "load-changed",
                             G_CALLBACK (webview_load_changed_cb),
                             task, 0);
    g_signal_connect_object (data->web_view, "load-failed",
                             G_CALLBACK (webview_load_failed_cb),
                             task, 0);
  }

  return FALSE;
}

GQuark
ephy_snapshot_service_error_quark (void)
{
  return g_quark_from_static_string ("ephy-snapshot-service-error-quark");
}

/**
 * ephy_snapshot_service_get_default:
 *
 * Gets the default instance of #EphySnapshotService.
 *
 * Returns: a #EphySnapshotService
 **/
EphySnapshotService *
ephy_snapshot_service_get_default (void)
{
  static EphySnapshotService *service = NULL;

  if (service == NULL)
    service = g_object_new (EPHY_TYPE_SNAPSHOT_SERVICE, NULL);

  return service;
}

const char *
ephy_snapshot_service_lookup_snapshot_path (EphySnapshotService *service,
                                            const char          *url)
{
  SnapshotPathCachedData *data;

  g_return_val_if_fail (EPHY_IS_SNAPSHOT_SERVICE (service), NULL);

  data = g_hash_table_lookup (service->priv->cache, url);

  return data == NULL ? NULL : data->path;
}

static EphySnapshotFreshness
ephy_snapshot_service_lookup_snapshot_freshness (EphySnapshotService *service,
                                                 const char          *url)
{
  SnapshotPathCachedData *data;

  data = g_hash_table_lookup (service->priv->cache, url);

  return data == NULL ? SNAPSHOT_STALE : data->freshness;
}

typedef struct {
  char *url;
  time_t mtime;

  char *path;
} SnapshotForURLAsyncData;

static SnapshotForURLAsyncData *
snapshot_for_url_async_data_new (const char *url,
                                 time_t mtime)
{
  SnapshotForURLAsyncData *data;

  data = g_slice_new0 (SnapshotForURLAsyncData);
  data->url = g_strdup (url);
  data->mtime = mtime;

  return data;
}

static void
snapshot_for_url_async_data_free (SnapshotForURLAsyncData *data)
{
  g_free (data->url);
  g_free (data->path);

  g_slice_free (SnapshotForURLAsyncData, data);
}

typedef struct {
  GHashTable *cache;
  char *url;
  SnapshotPathCachedData *data;
} CacheData;

static gboolean
idle_cache_snapshot_path (gpointer user_data)
{
  CacheData *data = (CacheData *)user_data;
  g_hash_table_insert (data->cache, data->url, data->data);
  g_hash_table_unref (data->cache);
  g_free (data);

  return G_SOURCE_REMOVE;
}

static void
cache_snapshot_data_in_idle (EphySnapshotService  *service,
                             const char           *url,
                             const char           *path,
                             EphySnapshotFreshness freshness)
{
  CacheData *data;
  data = g_new (CacheData, 1);
  data->cache = g_hash_table_ref (service->priv->cache);
  data->url = g_strdup (url);
  data->data = g_new (SnapshotPathCachedData, 1);
  data->data->path = g_strdup (path);
  data->data->freshness = freshness;
  g_idle_add (idle_cache_snapshot_path, data);
}

static void
get_snapshot_for_url_thread (GTask *task,
                             EphySnapshotService *service,
                             SnapshotForURLAsyncData *data,
                             GCancellable *cancellable)
{
  GdkPixbuf *snapshot;
  GError *error = NULL;

  data->path = gnome_desktop_thumbnail_factory_lookup (service->priv->factory, data->url, data->mtime);
  if (data->path == NULL) {
    g_task_return_new_error (task,
                             EPHY_SNAPSHOT_SERVICE_ERROR,
                             EPHY_SNAPSHOT_SERVICE_ERROR_NOT_FOUND,
                             "Snapshot for url \"%s\" not found in cache", data->url);
    return;
  }

  cache_snapshot_data_in_idle (service, data->url, data->path, SNAPSHOT_STALE);

  snapshot = gdk_pixbuf_new_from_file (data->path, &error);
  if (snapshot == NULL) {
    g_task_return_new_error (task,
                             EPHY_SNAPSHOT_SERVICE_ERROR,
                             EPHY_SNAPSHOT_SERVICE_ERROR_INVALID,
                             "Error creating pixbuf for snapshot file \"%s\": %s",
                             data->path, error->message);
    g_error_free (error);
  }

  g_task_return_pointer (task, snapshot, g_object_unref);
}

/**
 * ephy_snapshot_service_get_snapshot_for_url:
 * @service: a #EphySnapshotService
 * @url: the URL for which a snapshot is needed
 * @mtime: @the last
 * @callback: a #EphySnapshotServiceCallback
 * @user_data: user data to pass to @callback
 *
 * Schedules a query for a snapshot of @url. If there is an up-to-date
 * snapshot in the cache, this will be retrieved.
 *
 **/
void
ephy_snapshot_service_get_snapshot_for_url_async (EphySnapshotService *service,
                                                  const char *url,
                                                  const time_t mtime,
                                                  GCancellable *cancellable,
                                                  GAsyncReadyCallback callback,
                                                  gpointer user_data)
{
  GTask *task;

  g_return_if_fail (EPHY_IS_SNAPSHOT_SERVICE (service));
  g_return_if_fail (url != NULL);

  task = g_task_new (service, cancellable, callback, user_data);
  g_task_set_priority (task, G_PRIORITY_LOW);
  g_task_set_task_data (task,
                        snapshot_for_url_async_data_new (url, mtime),
                        (GDestroyNotify)snapshot_for_url_async_data_free);
  g_task_run_in_thread (task, (GTaskThreadFunc)get_snapshot_for_url_thread);
  g_object_unref (task);
}

/**
 * ephy_snapshot_service_get_snapshot_for_url_finish:
 * @service: a #EphySnapshotService
 * @result: a #GAsyncResult
 * @error: a location to store a #GError or %NULL
 *
 * Finishes the retrieval of a snapshot. Call from the
 * #GAsyncReadyCallback passed to
 * ephy_snapshot_service_get_snapshot_for_url_async().
 *
 * Returns: (transfer full): the snapshot.
 **/
GdkPixbuf *
ephy_snapshot_service_get_snapshot_for_url_finish (EphySnapshotService *service,
                                                   GAsyncResult *result,
                                                   gchar **path,
                                                   GError **error)
{
  GTask *task = G_TASK (result);
  GdkPixbuf *snapshot;

  g_return_val_if_fail (g_task_is_valid (result, service), NULL);

  snapshot = g_task_propagate_pointer (task, error);
  if (!snapshot)
    return NULL;

  if (path) {
    SnapshotForURLAsyncData *data;

    data = g_task_get_task_data (task);
    *path = data->path;
    data->path = NULL;
  }

  return snapshot;
}

static void
got_snapshot_for_url (EphySnapshotService *service,
                      GAsyncResult *result,
                      GTask *task)
{
  GdkPixbuf *snapshot;
  SnapshotAsyncData *data;

  data = g_task_get_task_data (task);
  snapshot = ephy_snapshot_service_get_snapshot_for_url_finish (service, result, &data->path, NULL);
  if (snapshot) {
    g_task_return_pointer (task, snapshot, g_object_unref);
    g_object_unref (task);
    return;
  }

  ephy_snapshot_service_take_from_webview (task);
}

/* We want to return an existing snapshot immediately, even if it is stale,
 * because snapshot creation is best-effort and often fails (e.g. if the user
 * navigates away from the page too soon), and we must be sure to return an old
 * result if a new one does not yet exist.
 */
static void
ensure_snapshot_freshness_for_web_view (EphySnapshotService *service,
                                        WebKitWebView       *web_view)
{
  GTask *task;
  const char *uri;

  uri = webkit_web_view_get_uri (web_view);
  if (ephy_snapshot_service_lookup_snapshot_freshness (service, uri) != SNAPSHOT_FRESH) {
    task = g_task_new (service, NULL, NULL, NULL);
    g_task_set_task_data (task,
                          snapshot_async_data_new (web_view, time (NULL)),
                          (GDestroyNotify)snapshot_async_data_free);
    g_idle_add ((GSourceFunc)ephy_snapshot_service_take_from_webview, task);
  }
}

/**
 * ephy_snapshot_service_get_snapshot_async:
 * @service: a #EphySnapshotService
 * @web_view: the #WebKitWebView for which a snapshot is needed
 * @mtime: @the last
 * @callback: a #EphySnapshotServiceCallback
 * @user_data: user data to pass to @callback
 *
 * Schedules a query for a snapshot of @url. If there is an up-to-date
 * snapshot in the cache, this will be retrieved. Otherwise, this
 * the snapshot will be taken, cached, and retrieved.
 *
 **/
void
ephy_snapshot_service_get_snapshot_async (EphySnapshotService *service,
                                          WebKitWebView *web_view,
                                          const time_t mtime,
                                          GCancellable *cancellable,
                                          GAsyncReadyCallback callback,
                                          gpointer user_data)
{
  GTask *task;
  const char *uri;

  g_return_if_fail (EPHY_IS_SNAPSHOT_SERVICE (service));
  g_return_if_fail (WEBKIT_IS_WEB_VIEW (web_view));

  task = g_task_new (service, cancellable, callback, user_data);
  g_task_set_task_data (task,
                        snapshot_async_data_new_for_snapshot (web_view, mtime),
                        (GDestroyNotify)snapshot_async_data_free);

  /* Try to get the snapshot from the cache first if we have a URL, but only if
   * the snapshot path is in memory cache; this is an indication that the
   * snapshot is fresh. */
  uri = webkit_web_view_get_uri (web_view);
  if (uri) {
    ephy_snapshot_service_get_snapshot_for_url_async (service,
                                                      uri, mtime, cancellable,
                                                      (GAsyncReadyCallback)got_snapshot_for_url,
                                                      task);
    ensure_snapshot_freshness_for_web_view (service, web_view);
  } else {
    g_idle_add ((GSourceFunc)ephy_snapshot_service_take_from_webview, task);
  }
}

/**
 * ephy_snapshot_service_get_snapshot_finish:
 * @service: a #EphySnapshotService
 * @result: a #GAsyncResult
 * @error: a location to store a #GError or %NULL
 *
 * Finishes the retrieval of a snapshot. Call from the
 * #GAsyncReadyCallback passed to
 * ephy_snapshot_service_get_snapshot_async().
 *
 * Returns: (transfer full): the snapshot.
 **/
GdkPixbuf *
ephy_snapshot_service_get_snapshot_finish (EphySnapshotService *service,
                                           GAsyncResult *result,
                                           gchar **path,
                                           GError **error)
{
  GTask *task = G_TASK (result);
  GdkPixbuf *snapshot;

  g_return_val_if_fail (g_task_is_valid (result, service), NULL);

  snapshot = g_task_propagate_pointer (task, error);
  if (!snapshot)
    return NULL;

  if (path) {
    SnapshotAsyncData *data;

    data = g_task_get_task_data (task);
    *path = data->path;
    data->path = NULL;
  }

  return snapshot;
}

typedef struct {
  EphySnapshotService *service;
  GdkPixbuf *snapshot;
  char *url;
  time_t mtime;
  gint refcount;
} SaveSnapshotAsyncData;

static SaveSnapshotAsyncData *
save_snapshot_async_data_new (EphySnapshotService *service,
                              GdkPixbuf           *snapshot,
                              const char          *url,
                              time_t               mtime)
{
  SaveSnapshotAsyncData *data;

  data = g_slice_new0 (SaveSnapshotAsyncData);
  data->service = g_object_ref (service);
  data->snapshot = g_object_ref (snapshot);
  data->url = g_strdup (url);
  data->mtime = mtime;
  data->refcount = 1;

  return data;
}

static SaveSnapshotAsyncData *
save_snapshot_async_data_ref (SaveSnapshotAsyncData *data)
{
  g_atomic_int_add (&data->refcount, 1);
  return data;
}

static void
save_snapshot_async_data_unref (SaveSnapshotAsyncData *data)
{
  if (g_atomic_int_dec_and_test (&data->refcount)) {
    g_object_unref (data->service);
    g_object_unref (data->snapshot);
    g_free (data->url);
    g_slice_free (SaveSnapshotAsyncData, data);
  }
}

static gboolean
idle_emit_snapshot_saved (gpointer user_data)
{
  SaveSnapshotAsyncData *data = (SaveSnapshotAsyncData *)user_data;

  g_signal_emit (data->service, signals[SNAPSHOT_SAVED], 0, data->url, data->mtime);

  save_snapshot_async_data_unref (data);
  return G_SOURCE_REMOVE;
}

static void
save_snapshot_thread (GTask *task,
                      EphySnapshotService *service,
                      SaveSnapshotAsyncData *data,
                      GCancellable *cancellable)
{
  char *path;

  gnome_desktop_thumbnail_factory_save_thumbnail (service->priv->factory,
                                                  data->snapshot,
                                                  data->url,
                                                  data->mtime);
  g_idle_add (idle_emit_snapshot_saved, save_snapshot_async_data_ref (data));

  path = gnome_desktop_thumbnail_path_for_uri (data->url, GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE);
  cache_snapshot_data_in_idle (service, data->url, path, SNAPSHOT_FRESH);

  g_task_return_pointer (task, path, g_free);
}

void
ephy_snapshot_service_save_snapshot_async (EphySnapshotService *service,
                                           GdkPixbuf *snapshot,
                                           const char *url,
                                           time_t mtime,
                                           GCancellable *cancellable,
                                           GAsyncReadyCallback callback,
                                           gpointer user_data)
{
  GTask *task;

  g_return_if_fail (EPHY_IS_SNAPSHOT_SERVICE (service));
  g_return_if_fail (GDK_IS_PIXBUF (snapshot));
  g_return_if_fail (url != NULL);

  task = g_task_new (service, cancellable, callback, user_data);
  g_task_set_priority (task, G_PRIORITY_LOW);
  g_task_set_task_data (task,
                        save_snapshot_async_data_new (service, snapshot, url, mtime),
                        (GDestroyNotify)save_snapshot_async_data_unref);
  g_task_run_in_thread (task, (GTaskThreadFunc)save_snapshot_thread);
  g_object_unref (task);
}

char *
ephy_snapshot_service_save_snapshot_finish (EphySnapshotService *service,
                                            GAsyncResult *result,
                                            GError **error)
{
  g_return_val_if_fail (g_task_is_valid (result, service), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
get_snapshot_path_for_url_thread (GTask *task,
                                  EphySnapshotService *service,
                                  SnapshotForURLAsyncData *data,
                                  GCancellable *cancellable)
{
  char *path;

  path = gnome_desktop_thumbnail_factory_lookup (service->priv->factory, data->url, data->mtime);
  if (!path) {
    g_task_return_new_error (task,
                             EPHY_SNAPSHOT_SERVICE_ERROR,
                             EPHY_SNAPSHOT_SERVICE_ERROR_NOT_FOUND,
                             "Snapshot for url \"%s\" not found in cache", data->url);
    return;
  }

  cache_snapshot_data_in_idle (service, data->url, path, SNAPSHOT_STALE);

  g_task_return_pointer (task, path, g_free);
}

void
ephy_snapshot_service_get_snapshot_path_for_url_async (EphySnapshotService *service,
                                                       const char *url,
                                                       const time_t mtime,
                                                       GCancellable *cancellable,
                                                       GAsyncReadyCallback callback,
                                                       gpointer user_data)
{
  GTask *task;
  const char *path;

  g_return_if_fail (EPHY_IS_SNAPSHOT_SERVICE (service));
  g_return_if_fail (url != NULL);

  task = g_task_new (service, cancellable, callback, user_data);

  path = ephy_snapshot_service_lookup_snapshot_path (service, url);

  if (path) {
    g_task_return_pointer (task, g_strdup (path), g_free);
    g_object_unref (task);
    return;
  }

  g_task_set_priority (task, G_PRIORITY_LOW);
  g_task_set_task_data (task,
                        snapshot_for_url_async_data_new (url, mtime),
                        (GDestroyNotify)snapshot_for_url_async_data_free);
  g_task_run_in_thread (task, (GTaskThreadFunc)get_snapshot_path_for_url_thread);
  g_object_unref (task);
}

char *
ephy_snapshot_service_get_snapshot_path_for_url_finish (EphySnapshotService *service,
                                                        GAsyncResult *result,
                                                        GError **error)
{
  g_return_val_if_fail (g_task_is_valid (result, service), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
got_snapshot_path_for_url (EphySnapshotService *service,
                           GAsyncResult *result,
                           GTask *task)
{
  char *path;

  path = ephy_snapshot_service_get_snapshot_path_for_url_finish (service, result, NULL);
  if (path) {
    g_task_return_pointer (task, path, g_free);
    g_object_unref (task);
  } else {
    ephy_snapshot_service_take_from_webview (task);
  }
}

void
ephy_snapshot_service_get_snapshot_path_async (EphySnapshotService *service,
                                               WebKitWebView *web_view,
                                               const time_t mtime,
                                               GCancellable *cancellable,
                                               GAsyncReadyCallback callback,
                                               gpointer user_data)
{
  GTask *task;
  const char *uri;

  g_return_if_fail (EPHY_IS_SNAPSHOT_SERVICE (service));
  g_return_if_fail (WEBKIT_IS_WEB_VIEW (web_view));

  task = g_task_new (service, cancellable, callback, user_data);

  uri = webkit_web_view_get_uri (web_view);
  if (uri) {
    const char *path = ephy_snapshot_service_lookup_snapshot_path (service, uri);
    if (path) {
      g_task_return_pointer (task, g_strdup (path), g_free);
      g_object_unref (task);
    } else {
      g_task_set_task_data (task,
                            snapshot_async_data_new (web_view, mtime),
                            (GDestroyNotify)snapshot_async_data_free);
      ephy_snapshot_service_get_snapshot_path_for_url_async (service,
                                                             uri, mtime, cancellable,
                                                             (GAsyncReadyCallback)got_snapshot_path_for_url,
                                                             task);
    }

    ensure_snapshot_freshness_for_web_view (service, web_view);
  } else {
    g_task_set_task_data (task,
                          snapshot_async_data_new (web_view, mtime),
                          (GDestroyNotify)snapshot_async_data_free);
    g_idle_add ((GSourceFunc)ephy_snapshot_service_take_from_webview, task);
  }
}

char *
ephy_snapshot_service_get_snapshot_path_finish (EphySnapshotService *service,
                                                GAsyncResult *result,
                                                GError **error)
{
  g_return_val_if_fail (g_task_is_valid (result, service), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}
