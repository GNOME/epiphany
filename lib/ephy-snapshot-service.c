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

#ifndef GNOME_DESKTOP_USE_UNSTABLE_API
#define GNOME_DESKTOP_USE_UNSTABLE_API
#endif
#include <libgnome-desktop/gnome-desktop-thumbnail.h>
#ifdef HAVE_WEBKIT2
#include <webkit2/webkit2.h>
#else
#include <webkit/webkit.h>
#endif

#define EPHY_SNAPSHOT_SERVICE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), EPHY_TYPE_SNAPSHOT_SERVICE, EphySnapshotServicePrivate))

struct _EphySnapshotServicePrivate
{
  GnomeDesktopThumbnailFactory *factory;
};

G_DEFINE_TYPE (EphySnapshotService, ephy_snapshot_service, G_TYPE_OBJECT)

typedef struct {
  WebKitWebView *webview;
  char *url;
  time_t mtime;
  GdkPixbuf *snapshot;
  GCancellable *cancellable;
  GAsyncReadyCallback callback;
  gpointer user_data;
} SnapshotOp;

static gboolean ephy_snapshot_service_complete_async (SnapshotOp *op);
static gboolean ephy_snapshot_service_take_from_webview (SnapshotOp *op);

static void
snapshot_op_free (SnapshotOp *op)
{
  g_free (op->url);

  if (op->cancellable)
    g_object_unref (op->cancellable);
  if (op->webview)
    g_object_unref (op->webview);
  if (op->snapshot)
    g_object_unref (op->snapshot);

  g_slice_free (SnapshotOp, op);
}

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
}

/* IO scheduler methods, used for IO. */

static GdkPixbuf *
io_scheduler_get_cached_snapshot (EphySnapshotService *service,
                                  char *url, time_t mtime)
{
  GdkPixbuf *snapshot;
  char *uri;

  uri =  gnome_desktop_thumbnail_factory_lookup (service->priv->factory,
                                                 url, mtime);
  if (uri == NULL)
    return NULL;

  snapshot = gdk_pixbuf_new_from_file (uri, NULL);
  g_free (uri);

  return snapshot;
}

static gboolean
io_scheduler_try_cache_query (GIOSchedulerJob *job,
                              GCancellable *cancellable,
                              gpointer user_data)
{
  SnapshotOp *op;
  EphySnapshotService *service = ephy_snapshot_service_get_default ();
  GSourceFunc func;

  op = (SnapshotOp*) user_data;

  if (g_cancellable_is_cancelled (op->cancellable)) {
    func = (GSourceFunc)ephy_snapshot_service_complete_async;
    goto out;
  }

  op->snapshot = io_scheduler_get_cached_snapshot (service, op->url, op->mtime);

  /* If we do have a cached snapshot or we don't have a webview
     already, just complete early. */
  if (op->snapshot || !op->webview)
    func = (GSourceFunc)ephy_snapshot_service_complete_async;
  else
    func = (GSourceFunc)ephy_snapshot_service_take_from_webview;

out:
  g_io_scheduler_job_send_to_mainloop (job, func, op, NULL);
  return FALSE;
}

static gboolean
io_scheduler_save_thumbnail (GIOSchedulerJob *job,
                             GCancellable *cancellable,
                             gpointer user_data)
{
  SnapshotOp *op;
  EphySnapshotService *service;

  op = (SnapshotOp*) user_data;
  service = ephy_snapshot_service_get_default ();
  gnome_desktop_thumbnail_factory_save_thumbnail (service->priv->factory,
                                                  op->snapshot,
                                                  op->url,
                                                  op->mtime);

  g_io_scheduler_job_send_to_mainloop_async (job,
                                             (GSourceFunc)ephy_snapshot_service_complete_async,
                                             op, NULL);

  return FALSE;
}

/* Methods that run in the mainloop. */

static gboolean
ephy_snapshot_service_complete_async (SnapshotOp *op)
{
  GSimpleAsyncResult *res;

  res = g_simple_async_result_new (G_OBJECT (ephy_snapshot_service_get_default ()),
                                   op->callback,
                                   op->user_data,
                                   ephy_snapshot_service_complete_async);
  g_simple_async_result_set_check_cancellable (res, op->cancellable);
  g_simple_async_result_set_op_res_gpointer (res, op, (GDestroyNotify)snapshot_op_free);
  g_simple_async_result_complete (res);
  g_object_unref (res);

  return FALSE;
}

static gboolean
webview_retrieve_snapshot (SnapshotOp *op)
{
  cairo_surface_t *surface;

#ifdef HAVE_WEBKIT2
  /* FIXME: We need to add this API to WebKit2. */
  surface = NULL;
#else
  surface = webkit_web_view_get_snapshot (WEBKIT_WEB_VIEW (op->webview));
#endif

  if (surface == NULL) {
    ephy_snapshot_service_complete_async (op);
    return FALSE;
  }

  op->snapshot = ephy_snapshot_service_crop_snapshot (surface);

  g_io_scheduler_push_job ((GIOSchedulerJobFunc)io_scheduler_save_thumbnail,
                           op, NULL, G_PRIORITY_LOW, NULL);
  return FALSE;
}

#ifdef HAVE_WEBKIT2
static void
webview_load_changed_cb (WebKitWebView *webview,
                         WebKitLoadEvent load_event,
                         SnapshotOp *op)
{
  switch (load_event) {
  case WEBKIT_LOAD_FINISHED:
    /* Load finished doesn't ensure that we actually have visible content yet,
       so hold a bit before retrieving the snapshot. */
    g_idle_add ((GSourceFunc) webview_retrieve_snapshot, op);
    /* Some pages might end up causing this condition to happen twice, so remove
       the handler in order to avoid calling the above idle function twice. */
    g_signal_handlers_disconnect_by_func (webview, webview_load_changed_cb, op);
    break;
  }
}

static gboolean
webview_load_failed_cb (WebKitWebView *webview,
                        WebKitLoadEvent load_event,
                        const char failing_uri,
                        GError *error,
                        SnapshotOp *op)
{
  ephy_snapshot_service_complete_async (op);

  return FALSE;
}
#else
static void
webview_load_status_changed_cb (WebKitWebView *webview,
                                GParamSpec *pspec,
                                SnapshotOp *op)
{
  WebKitLoadStatus status;

  status = webkit_web_view_get_load_status (webview);

  switch (status) {
  case WEBKIT_LOAD_FINISHED:
    /* Load finished doesn't ensure that we actually have visible
       content yet, so hold a bit before retrieving the snapshot. */
    g_idle_add ((GSourceFunc) webview_retrieve_snapshot, op);
    g_signal_handlers_disconnect_by_func (webview, webview_load_status_changed_cb, op);
    break;
  case WEBKIT_LOAD_FAILED:
    g_signal_handlers_disconnect_by_func (webview, webview_load_status_changed_cb, op);
    ephy_snapshot_service_complete_async (op);
    break;
  default:
    break;
  }
}
#endif

static gboolean
ephy_snapshot_service_take_from_webview (SnapshotOp *op)
{
  WebKitWebView *webview = op->webview;

  if (g_cancellable_is_cancelled (op->cancellable)) {
    ephy_snapshot_service_complete_async (op);
    return FALSE;
  }

#ifdef HAVE_WEBKIT2
  if (webkit_web_view_get_estimated_load_progress (WEBKIT_WEB_VIEW (webview))== 1.0)
    webview_retrieve_snapshot (op);
  else {
    g_signal_connect (webview, "load-changed",
                      G_CALLBACK (webview_load_changed_cb), op);
    g_signal_connect (webview, "load-failed",
                      G_CALLBACK (webview_load_failed_cb), op);
  }
#else
  if (webkit_web_view_get_load_status (webview) == WEBKIT_LOAD_FINISHED)
    webview_retrieve_snapshot (op);
  else
    g_signal_connect (webview, "notify::load-status",
                      G_CALLBACK (webview_load_status_changed_cb),
                      op);
#endif

  return FALSE;
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
 * ephy_snapshot_service_get_snapshot:
 * @service: a #EphySnapshotService
 * @url: the URL for which a snapshot is needed
 * @mtime: @the last
 * @callback: a #EphySnapshotServiceCallback
 * @userdata: user data to pass to @callback
 *
 * Schedules a query for a snapshot of @url. If there is an up-to-date
 * snapshot in the cache, this will be retrieved. Otherwise, this
 * the snapshot will be taken, cached, and retrieved.
 *
 **/
void
ephy_snapshot_service_get_snapshot_async (EphySnapshotService *service,
                                          WebKitWebView *webview,
                                          const char *url,
                                          const time_t mtime,
                                          GCancellable *cancellable,
                                          GAsyncReadyCallback callback,
                                          gpointer user_data)
{
  SnapshotOp *op;

  g_return_if_fail (EPHY_IS_SNAPSHOT_SERVICE (service));
  g_return_if_fail (url != NULL);

  op = g_slice_alloc0 (sizeof(SnapshotOp));
  op->url = g_strdup (url);
  op->mtime = mtime;
  op->webview = webview ? g_object_ref (webview) : NULL;
  op->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
  op->callback = callback;
  op->user_data = user_data;

  /* Query for the snapshot from the cache using the IO scheduler, so
     that there is no UI blocking during the cache query. */
  g_io_scheduler_push_job (io_scheduler_try_cache_query,
                           op, NULL, G_PRIORITY_LOW, NULL);
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
                                           GError **error)
{
  GSimpleAsyncResult *simple;
  SnapshotOp *op;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
                                                        G_OBJECT (service),
                                                        ephy_snapshot_service_complete_async),
                        NULL);

  simple  = (GSimpleAsyncResult *) result;

  if (g_simple_async_result_propagate_error (simple, error))
    return NULL;

  op = g_simple_async_result_get_op_res_gpointer (simple);

  return op->snapshot ? g_object_ref (op->snapshot) : NULL;
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
  SnapshotOp *op;

  g_return_if_fail (EPHY_IS_SNAPSHOT_SERVICE (service));
  g_return_if_fail (GDK_IS_PIXBUF (snapshot));
  g_return_if_fail (url != NULL);

  op = g_slice_alloc0 (sizeof(SnapshotOp));
  op->snapshot = g_object_ref (snapshot);
  op->url = g_strdup (url);
  op->mtime = mtime;
  op->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
  op->callback = callback;
  op->user_data = user_data;

  g_io_scheduler_push_job (io_scheduler_save_thumbnail,
                           op, NULL, G_PRIORITY_LOW, NULL);
}

GdkPixbuf *
ephy_snapshot_service_crop_snapshot (cairo_surface_t *surface)
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

  return scaled;
}
