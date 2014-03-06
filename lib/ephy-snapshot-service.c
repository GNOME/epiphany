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

/* Update snapshots after one week. */
#define SNAPSHOT_UPDATE_THRESHOLD (60 * 60 * 24 * 7)

struct _EphySnapshotServicePrivate
{
  GnomeDesktopThumbnailFactory *factory;
  GHashTable *cache;
};

G_DEFINE_TYPE (EphySnapshotService, ephy_snapshot_service, G_TYPE_OBJECT)

/* GObject boilerplate methods. */

static void
ephy_snapshot_service_class_init (EphySnapshotServiceClass *klass)
{
  g_type_class_add_private (klass, sizeof (EphySnapshotServicePrivate));
}


static void
ephy_snapshot_service_init (EphySnapshotService *self)
{
  self->priv = EPHY_SNAPSHOT_SERVICE_GET_PRIVATE (self);
  self->priv->factory = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE);
  self->priv->cache = g_hash_table_new_full (g_str_hash, g_str_equal,
                                             (GDestroyNotify)g_free,
                                             (GDestroyNotify)g_free);
}

typedef struct {
  char *url;
  time_t mtime;

  GdkPixbuf *snapshot;
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
  g_clear_object (&data->snapshot);
  g_free (data->path);

  g_slice_free (SnapshotForURLAsyncData, data);
}

static void
get_snapshot_for_url_thread (GSimpleAsyncResult *result,
                             EphySnapshotService *service,
                             GCancellable *cancellable)
{
  SnapshotForURLAsyncData *data;
  GError *error = NULL;

  data = (SnapshotForURLAsyncData *)g_simple_async_result_get_op_res_gpointer (result);

  data->path = gnome_desktop_thumbnail_factory_lookup (service->priv->factory, data->url, data->mtime);
  if (data->path == NULL) {
    g_simple_async_result_set_error (result,
                                     EPHY_SNAPSHOT_SERVICE_ERROR,
                                     EPHY_SNAPSHOT_SERVICE_ERROR_NOT_FOUND,
                                     "Snapshot for url \"%s\" not found in cache", data->url);
    return;
  }

  g_hash_table_insert (service->priv->cache, g_strdup (data->url), g_strdup (data->path));

  data->snapshot = gdk_pixbuf_new_from_file (data->path, &error);
  if (data->snapshot == NULL) {
    g_simple_async_result_set_error (result,
                                     EPHY_SNAPSHOT_SERVICE_ERROR,
                                     EPHY_SNAPSHOT_SERVICE_ERROR_INVALID,
                                     "Error creating pixbuf for snapshot file \"%s\": %s",
                                     data->path, error->message);
    g_error_free (error);
  }
}

typedef struct {
  WebKitWebView *web_view;
  time_t mtime;
  GCancellable *cancellable;

  GdkPixbuf *snapshot;
  char *path;
} SnapshotAsyncData;

static SnapshotAsyncData *
snapshot_async_data_new (WebKitWebView *web_view,
                         time_t mtime,
                         GCancellable *cancellable)
{
  SnapshotAsyncData *data;

  data = g_slice_new0 (SnapshotAsyncData);
  data->web_view = g_object_ref (web_view);
  data->mtime = mtime;
  data->cancellable = cancellable ? g_object_ref (cancellable) : NULL;

  return data;
}

static void
snapshot_async_data_free (SnapshotAsyncData *data)
{
  g_object_unref (data->web_view);
  g_clear_object (&data->cancellable);
  g_clear_object (&data->snapshot);
  g_free (data->path);

  g_slice_free (SnapshotAsyncData, data);
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

static void
snapshot_saved (EphySnapshotService *service,
                GAsyncResult *result,
                GSimpleAsyncResult *simple)
{
  SnapshotAsyncData *data;

  data = (SnapshotAsyncData *)g_simple_async_result_get_op_res_gpointer (simple);
  data->path = ephy_snapshot_service_save_snapshot_finish (service, result, NULL);
  g_simple_async_result_complete (simple);
  g_object_unref (simple);
}

static void
save_snapshot (cairo_surface_t *surface,
               GSimpleAsyncResult *result)
{
  SnapshotAsyncData *data;
  EphySnapshotService *service;

  data = (SnapshotAsyncData *)g_simple_async_result_get_op_res_gpointer (result);
  data->snapshot = ephy_snapshot_service_prepare_snapshot (surface,
                                                           webkit_web_view_get_favicon (data->web_view));

  service = (EphySnapshotService *)g_async_result_get_source_object (G_ASYNC_RESULT (result));
  ephy_snapshot_service_save_snapshot_async (service, data->snapshot,
                                             webkit_web_view_get_uri (data->web_view),
                                             data->mtime, data->cancellable,
                                             (GAsyncReadyCallback)snapshot_saved, result);
}

static void
on_snapshot_ready (WebKitWebView *webview,
                   GAsyncResult *result,
                   GSimpleAsyncResult *simple)
{
  cairo_surface_t *surface;
  GError *error = NULL;

  surface = webkit_web_view_get_snapshot_finish (webview, result, &error);
  if (error) {
    g_simple_async_result_take_error (simple, error);
    g_simple_async_result_complete (simple);
    g_object_unref (simple);
    return;
  }

  save_snapshot (surface, simple);
  cairo_surface_destroy (surface);
}

static gboolean
retrieve_snapshot_from_web_view (GSimpleAsyncResult *result)
{
  SnapshotAsyncData *data;

  data = (SnapshotAsyncData *)g_simple_async_result_get_op_res_gpointer (result);

  webkit_web_view_get_snapshot (data->web_view,
                                WEBKIT_SNAPSHOT_REGION_VISIBLE,
                                WEBKIT_SNAPSHOT_OPTIONS_NONE,
                                NULL, (GAsyncReadyCallback)on_snapshot_ready,
                                result);
  return FALSE;
}

static void
webview_load_changed_cb (WebKitWebView *webview,
                         WebKitLoadEvent load_event,
                         GSimpleAsyncResult *result)
{
  if (load_event != WEBKIT_LOAD_FINISHED)
    return;

  /* Load finished doesn't ensure that we actually have visible content yet,
     so hold a bit before retrieving the snapshot. */
  g_idle_add ((GSourceFunc) retrieve_snapshot_from_web_view, result);

  /* Some pages might end up causing this condition to happen twice, so remove
     the handler in order to avoid calling the above idle function twice. */
  g_signal_handlers_disconnect_by_func (webview, webview_load_changed_cb, result);
}

static gboolean
webview_load_failed_cb (WebKitWebView *webview,
                        WebKitLoadEvent load_event,
                        const char failing_uri,
                        GError *error,
                        GSimpleAsyncResult *result)
{
  g_signal_handlers_disconnect_by_func (webview, webview_load_changed_cb, result);
  g_signal_handlers_disconnect_by_func (webview, webview_load_failed_cb, result);
  g_simple_async_result_set_error (result,
                                   EPHY_SNAPSHOT_SERVICE_ERROR,
                                   EPHY_SNAPSHOT_SERVICE_ERROR_WEB_VIEW,
                                   "Error getting snapshot, web view failed to load: %s",
                                   error->message);
  g_simple_async_result_complete_in_idle (result);
  g_object_unref (result);

  return FALSE;
}

static gboolean
ephy_snapshot_service_take_from_webview (GSimpleAsyncResult *result)
{
  SnapshotAsyncData *data;

  data = (SnapshotAsyncData *)g_simple_async_result_get_op_res_gpointer (result);

  if (webkit_web_view_get_estimated_load_progress (WEBKIT_WEB_VIEW (data->web_view)) == 1.0)
    retrieve_snapshot_from_web_view (result);
  else {
    g_signal_connect (data->web_view, "load-changed",
                      G_CALLBACK (webview_load_changed_cb), result);
    g_signal_connect (data->web_view, "load-failed",
                      G_CALLBACK (webview_load_failed_cb), result);
  }

  return FALSE;
}

typedef struct {
  GdkPixbuf *snapshot;
  char *url;
  time_t mtime;
  char *path;
} SaveSnapshotAsyncData;

static SaveSnapshotAsyncData *
save_snapshot_async_data_new (GdkPixbuf *snapshot,
                              const char *url,
                              time_t mtime)
{
  SaveSnapshotAsyncData *data;

  data = g_slice_new0 (SaveSnapshotAsyncData);
  data->snapshot = g_object_ref (snapshot);
  data->url = g_strdup (url);
  data->mtime = mtime;

  return data;
}

static void
save_snapshot_async_data_free (SaveSnapshotAsyncData *data)
{
  g_object_unref (data->snapshot);
  g_free (data->url);
  g_free (data->path);

  g_slice_free (SaveSnapshotAsyncData, data);
}

static void
save_snapshot_thread (GSimpleAsyncResult *result,
                      EphySnapshotService *service,
                      GCancellable *cancellable)
{
  SaveSnapshotAsyncData *data;

  data = (SaveSnapshotAsyncData *)g_simple_async_result_get_op_res_gpointer (result);
  gnome_desktop_thumbnail_factory_save_thumbnail (service->priv->factory,
                                                  data->snapshot,
                                                  data->url,
                                                  data->mtime);
  data->path = gnome_desktop_thumbnail_path_for_uri (data->url, GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE);
  g_hash_table_insert (service->priv->cache, g_strdup (data->url), g_strdup (data->path));
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
  GSimpleAsyncResult *result;

  g_return_if_fail (EPHY_IS_SNAPSHOT_SERVICE (service));
  g_return_if_fail (url != NULL);

  result = g_simple_async_result_new (G_OBJECT (service), callback, user_data,
                                      ephy_snapshot_service_get_snapshot_for_url_async);

  g_simple_async_result_set_op_res_gpointer (result,
                                             snapshot_for_url_async_data_new (url, mtime),
                                             (GDestroyNotify)snapshot_for_url_async_data_free);
  g_simple_async_result_run_in_thread (result,
                                       (GSimpleAsyncThreadFunc)get_snapshot_for_url_thread,
                                       G_PRIORITY_LOW, cancellable);
  g_object_unref (result);
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
  GSimpleAsyncResult *simple;
  SnapshotForURLAsyncData *data;

  g_return_val_if_fail (EPHY_IS_SNAPSHOT_SERVICE (service), NULL);
  g_return_val_if_fail (g_simple_async_result_is_valid (result,
                                                        G_OBJECT (service),
                                                        ephy_snapshot_service_get_snapshot_for_url_async),
                        NULL);

  simple = (GSimpleAsyncResult *)result;

  if (g_simple_async_result_propagate_error (simple, error))
    return NULL;

  data = (SnapshotForURLAsyncData *)g_simple_async_result_get_op_res_gpointer (simple);

  if (path) {
    *path = data->path;
    data->path = NULL;
  }

  return data->snapshot ? g_object_ref (data->snapshot) : NULL;
}

static void
got_snapshot_for_url (EphySnapshotService *service,
                      GAsyncResult *result,
                      GSimpleAsyncResult *simple)
{
  SnapshotAsyncData *data;

  data = (SnapshotAsyncData *)g_simple_async_result_get_op_res_gpointer (simple);
  data->snapshot = ephy_snapshot_service_get_snapshot_for_url_finish (service, result, &data->path, NULL);
  if (data->snapshot) {
    g_simple_async_result_complete (simple);
    g_object_unref (simple);

    return;
  }

  ephy_snapshot_service_take_from_webview (simple);
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
  GSimpleAsyncResult *result;
  const char *uri;
  time_t current_time = time (NULL);

  g_return_if_fail (EPHY_IS_SNAPSHOT_SERVICE (service));
  g_return_if_fail (WEBKIT_IS_WEB_VIEW (web_view));

  result = g_simple_async_result_new (G_OBJECT (service), callback, user_data,
                                      ephy_snapshot_service_get_snapshot_async);

  g_simple_async_result_set_op_res_gpointer (result,
                                             snapshot_async_data_new (web_view, mtime, cancellable),
                                             (GDestroyNotify)snapshot_async_data_free);

  /* Try to get the snapshot from the cache first if we have a URL */
  uri = webkit_web_view_get_uri (web_view);
  if (uri && current_time - mtime <= SNAPSHOT_UPDATE_THRESHOLD)
    ephy_snapshot_service_get_snapshot_for_url_async (service,
                                                      uri, mtime, cancellable,
                                                      (GAsyncReadyCallback)got_snapshot_for_url,
                                                      result);
  else
    g_idle_add ((GSourceFunc)ephy_snapshot_service_take_from_webview, result);
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
  GSimpleAsyncResult *simple;
  SnapshotAsyncData *data;

  g_return_val_if_fail (EPHY_IS_SNAPSHOT_SERVICE (service), NULL);
  g_return_val_if_fail (g_simple_async_result_is_valid (result,
                                                        G_OBJECT (service),
                                                        ephy_snapshot_service_get_snapshot_async),
                        NULL);

  simple = (GSimpleAsyncResult *)result;

  if (g_simple_async_result_propagate_error (simple, error))
    return NULL;

  data = (SnapshotAsyncData *)g_simple_async_result_get_op_res_gpointer (simple);
  if (path) {
    *path = data->path;
    data->path = NULL;
  }

  return data->snapshot ? g_object_ref (data->snapshot) : NULL;
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
  GSimpleAsyncResult *result;

  g_return_if_fail (EPHY_IS_SNAPSHOT_SERVICE (service));
  g_return_if_fail (GDK_IS_PIXBUF (snapshot));
  g_return_if_fail (url != NULL);

  result = g_simple_async_result_new (G_OBJECT (service), callback, user_data,
                                      ephy_snapshot_service_save_snapshot_async);

  g_simple_async_result_set_op_res_gpointer (result,
                                             save_snapshot_async_data_new (snapshot, url, mtime),
                                             (GDestroyNotify)save_snapshot_async_data_free);
  g_simple_async_result_run_in_thread (result,
                                       (GSimpleAsyncThreadFunc)save_snapshot_thread,
                                       G_PRIORITY_LOW, cancellable);
  g_object_unref (result);
}

char *
ephy_snapshot_service_save_snapshot_finish (EphySnapshotService *service,
                                            GAsyncResult *result,
                                            GError **error)
{
  SaveSnapshotAsyncData *data;
  char *retval;

  g_return_val_if_fail (EPHY_IS_SNAPSHOT_SERVICE (service), FALSE);
  g_return_val_if_fail (g_simple_async_result_is_valid (result,
                                                        G_OBJECT (service),
                                                        ephy_snapshot_service_save_snapshot_async),
                        FALSE);

  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
    return NULL;

  data = (SaveSnapshotAsyncData *)g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (result));
  retval = data->path;
  data->path = NULL;

  return retval;
}

const char *
ephy_snapshot_service_lookup_snapshot_path (EphySnapshotService *service,
                                            const char *url)
{
  g_return_val_if_fail (EPHY_IS_SNAPSHOT_SERVICE (service), NULL);

  return g_hash_table_lookup (service->priv->cache, url);
}

static void
get_snapshot_path_for_url_thread (GSimpleAsyncResult *result,
                                  EphySnapshotService *service,
                                  GCancellable *cancellable)
{
  SnapshotForURLAsyncData *data;

  data = (SnapshotForURLAsyncData *)g_simple_async_result_get_op_res_gpointer (result);
  data->path = gnome_desktop_thumbnail_factory_lookup (service->priv->factory, data->url, data->mtime);
  if (!data->path) {
    g_simple_async_result_set_error (result,
                                     EPHY_SNAPSHOT_SERVICE_ERROR,
                                     EPHY_SNAPSHOT_SERVICE_ERROR_NOT_FOUND,
                                     "Snapshot for url \"%s\" not found in cache", data->url);
  } else {
    g_hash_table_insert (service->priv->cache, g_strdup (data->url), g_strdup (data->path));
  }
}

void
ephy_snapshot_service_get_snapshot_path_for_url_async (EphySnapshotService *service,
                                                       const char *url,
                                                       const time_t mtime,
                                                       GCancellable *cancellable,
                                                       GAsyncReadyCallback callback,
                                                       gpointer user_data)
{
  GSimpleAsyncResult *result;
  SnapshotForURLAsyncData *data;
  const char *path;

  g_return_if_fail (EPHY_IS_SNAPSHOT_SERVICE (service));
  g_return_if_fail (url != NULL);

  result = g_simple_async_result_new (G_OBJECT (service), callback, user_data,
                                      ephy_snapshot_service_get_snapshot_path_for_url_async);

  data = snapshot_for_url_async_data_new (url, mtime);
  g_simple_async_result_set_op_res_gpointer (result, data, (GDestroyNotify)snapshot_for_url_async_data_free);

  path = g_hash_table_lookup (service->priv->cache, url);
  if (path) {
    data->path = g_strdup (path);
    g_simple_async_result_complete_in_idle (result);
  } else {
    g_simple_async_result_run_in_thread (result,
                                         (GSimpleAsyncThreadFunc)get_snapshot_path_for_url_thread,
                                         G_PRIORITY_LOW, cancellable);
  }
  g_object_unref (result);
}

char *
ephy_snapshot_service_get_snapshot_path_for_url_finish (EphySnapshotService *service,
                                                        GAsyncResult *result,
                                                        GError **error)
{
  SnapshotForURLAsyncData *data;
  char *retval;

  g_return_val_if_fail (EPHY_IS_SNAPSHOT_SERVICE (service), NULL);
  g_return_val_if_fail (g_simple_async_result_is_valid (result,
                                                        G_OBJECT (service),
                                                        ephy_snapshot_service_get_snapshot_path_for_url_async),
                        NULL);

  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
    return NULL;

  data = (SnapshotForURLAsyncData *)g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (result));

  retval = data->path;
  data->path = NULL;

  return retval;
}

static void
got_snapshot_path_for_url (EphySnapshotService *service,
                           GAsyncResult *result,
                           GSimpleAsyncResult *simple)
{
  SnapshotAsyncData *data;

  data = (SnapshotAsyncData *)g_simple_async_result_get_op_res_gpointer (simple);
  data->path = ephy_snapshot_service_get_snapshot_path_for_url_finish (service, result, NULL);
  if (data->path) {
    g_simple_async_result_complete (simple);
    g_object_unref (simple);
  } else {
    ephy_snapshot_service_take_from_webview (simple);
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
  GSimpleAsyncResult *result;
  SnapshotAsyncData *data;
  const char *uri;
  time_t current_time = time (NULL);

  g_return_if_fail (EPHY_IS_SNAPSHOT_SERVICE (service));
  g_return_if_fail (WEBKIT_IS_WEB_VIEW (web_view));

  result = g_simple_async_result_new (G_OBJECT (service), callback, user_data,
                                      ephy_snapshot_service_get_snapshot_path_async);

  data = snapshot_async_data_new (web_view, mtime, cancellable);
  g_simple_async_result_set_op_res_gpointer (result, data, (GDestroyNotify)snapshot_async_data_free);

  uri = webkit_web_view_get_uri (web_view);
  if (uri && current_time - mtime <= SNAPSHOT_UPDATE_THRESHOLD) {
    const char *path = g_hash_table_lookup (service->priv->cache, uri);

    if (path) {
      data->path = g_strdup (path);
      g_simple_async_result_complete_in_idle (result);
    } else {
      ephy_snapshot_service_get_snapshot_path_for_url_async (service,
                                                             uri, mtime, cancellable,
                                                             (GAsyncReadyCallback)got_snapshot_path_for_url,
                                                             g_object_ref (result));
    }
  } else {
    g_idle_add ((GSourceFunc)ephy_snapshot_service_take_from_webview, g_object_ref (result));
  }

  g_object_unref (result);
}

char *
ephy_snapshot_service_get_snapshot_path_finish (EphySnapshotService *service,
                                                GAsyncResult *result,
                                                GError **error)
{
  SnapshotAsyncData *data;
  char *retval;

  g_return_val_if_fail (EPHY_IS_SNAPSHOT_SERVICE (service), FALSE);
  g_return_val_if_fail (g_simple_async_result_is_valid (result,
                                                        G_OBJECT (service),
                                                        ephy_snapshot_service_get_snapshot_path_async),
                        FALSE);

  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
    return NULL;

  data = (SnapshotAsyncData *)g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (result));

  retval = data->path;
  data->path = NULL;

  return retval;
}
