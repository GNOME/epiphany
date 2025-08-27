/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2012 Igalia S.L.
 *
 *  This file is part of Epiphany.
 *
 *  Epiphany is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Epiphany is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Epiphany.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "ephy-snapshot-service.h"

#include "ephy-file-helpers.h"
#include "ephy-pixbuf-utils.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <webkit/webkit.h>

struct _EphySnapshotService {
  GObject parent_instance;

  GHashTable *cache;
};

G_DEFINE_FINAL_TYPE (EphySnapshotService, ephy_snapshot_service, G_TYPE_OBJECT)

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
}

static void
ephy_snapshot_service_init (EphySnapshotService *self)
{
  self->cache = g_hash_table_new_full (g_str_hash, g_str_equal,
                                       (GDestroyNotify)g_free,
                                       (GDestroyNotify)snapshot_path_cached_data_free);
}

static char *
thumbnail_filename (const char *uri)
{
  GChecksum *checksum;
  guint8 digest[16];
  gsize digest_len = sizeof (digest);
  char *file;

  checksum = g_checksum_new (G_CHECKSUM_MD5);
  g_checksum_update (checksum, (const guchar *)uri, strlen (uri));

  g_checksum_get_digest (checksum, digest, &digest_len);
  g_assert (digest_len == 16);

  file = g_strconcat (g_checksum_get_string (checksum), ".jpg", NULL);

  g_checksum_free (checksum);

  return file;
}

static char *
thumbnail_directory (void)
{
  return g_build_filename (ephy_cache_dir (),
                           "thumbnails",
                           NULL);
}

static char *
thumbnail_path (const char *uri)
{
  char *path, *file, *dir;

  dir = thumbnail_directory ();
  file = thumbnail_filename (uri);
  path = g_build_filename (dir, file, NULL);

  g_free (dir);
  g_free (file);

  return path;
}

static gboolean
save_thumbnail (GdkPixbuf  *pixbuf,
                const char *uri)
{
  char *path;
  char *dirname;
  char *tmp_path = NULL;
  int tmp_fd;
  gboolean ret = FALSE;
  GError *error = NULL;

  if (!pixbuf)
    return FALSE;

  path = thumbnail_path (uri);
  dirname = g_path_get_dirname (path);

  if (g_mkdir_with_parents (dirname, 0700) != 0)
    goto out;

  tmp_path = g_strconcat (path, ".XXXXXX", NULL);
  tmp_fd = g_mkstemp (tmp_path);

  if (tmp_fd == -1)
    goto out;
  close (tmp_fd);

  error = NULL;
  ret = gdk_pixbuf_save (pixbuf,
                         tmp_path,
                         "jpeg", &error,
                         NULL);

  if (!ret)
    goto out;

  chmod (tmp_path, 0600);
  rename (tmp_path, path);

out:
  if (error) {
    g_warning ("Failed to create thumbnail %s: %s", tmp_path, error->message);
    g_error_free (error);
  }

  if (tmp_path)
    unlink (tmp_path);

  g_free (path);
  g_free (tmp_path);
  g_free (dirname);
  return ret;
}

static GdkPixbuf *
ephy_snapshot_service_prepare_snapshot (GdkTexture *texture)
{
  GdkPixbuf *snapshot, *scaled;
  int orig_width, orig_height;

  orig_width = gdk_texture_get_width (texture);
  orig_height = gdk_texture_get_height (texture);

  if (!orig_width || !orig_height)
    return NULL;

  snapshot = ephy_texture_to_pixbuf (texture, FALSE);

  if (orig_width < EPHY_THUMBNAIL_WIDTH ||
      orig_height < EPHY_THUMBNAIL_HEIGHT) {
    scaled = gdk_pixbuf_scale_simple (snapshot,
                                      EPHY_THUMBNAIL_WIDTH,
                                      EPHY_THUMBNAIL_HEIGHT,
                                      GDK_INTERP_TILES);
  } else {
    gfloat width_ratio = (gfloat)EPHY_THUMBNAIL_WIDTH / (gfloat)orig_width;
    gfloat new_height = orig_height * width_ratio;

    scaled = gdk_pixbuf_scale_simple (snapshot,
                                      EPHY_THUMBNAIL_WIDTH,
                                      new_height,
                                      GDK_INTERP_BILINEAR);

    /* Crop image in case it exceeds our thumbnail height limit */
    if (new_height > EPHY_THUMBNAIL_HEIGHT) {
      GdkPixbuf *tmp = gdk_pixbuf_new_subpixbuf (scaled, 0, 0, EPHY_THUMBNAIL_WIDTH, EPHY_THUMBNAIL_HEIGHT);
      g_clear_object (&scaled);
      scaled = tmp;
    }
  }

  g_object_unref (snapshot);

  return scaled;
}

typedef struct {
  EphySnapshotService *service;
  GdkPixbuf *snapshot;
  WebKitWebView *web_view;
  char *url;
} SnapshotAsyncData;

static SnapshotAsyncData *
snapshot_async_data_new (EphySnapshotService *service,
                         GdkPixbuf           *snapshot,
                         WebKitWebView       *web_view,
                         const char          *url)
{
  SnapshotAsyncData *data;

  data = g_new0 (SnapshotAsyncData, 1);
  data->service = g_object_ref (service);
  data->snapshot = snapshot ? g_object_ref (snapshot) : NULL;
  data->web_view = web_view;
  data->url = g_strdup (url);

  if (web_view)
    g_object_add_weak_pointer (G_OBJECT (web_view), (gpointer *)&data->web_view);

  return data;
}

static SnapshotAsyncData *
snapshot_async_data_copy (SnapshotAsyncData *data)
{
  SnapshotAsyncData *copy = snapshot_async_data_new (data->service,
                                                     data->snapshot,
                                                     data->web_view,
                                                     data->url);
  return copy;
}

static void
snapshot_async_data_free (SnapshotAsyncData *data)
{
  g_clear_object (&data->service);
  g_clear_object (&data->snapshot);

  if (data->web_view)
    g_object_remove_weak_pointer (G_OBJECT (data->web_view), (gpointer *)&data->web_view);

  g_free (data->url);
  g_free (data);
}

typedef struct {
  GHashTable *cache;
  char *url;
  SnapshotPathCachedData *data;
} CacheData;

static void
idle_cache_snapshot_path (gpointer user_data)
{
  CacheData *data = (CacheData *)user_data;
  g_hash_table_insert (data->cache, data->url, data->data);
  g_hash_table_unref (data->cache);
  g_free (data);
}

static void
cache_snapshot_data_in_idle (EphySnapshotService   *service,
                             const char            *url,
                             const char            *path,
                             EphySnapshotFreshness  freshness)
{
  CacheData *data;
  data = g_new (CacheData, 1);
  data->cache = g_hash_table_ref (service->cache);
  data->url = g_strdup (url);
  data->data = g_new (SnapshotPathCachedData, 1);
  data->data->path = g_strdup (path);
  data->data->freshness = freshness;
  g_idle_add_once (idle_cache_snapshot_path, data);
}

static void
save_snapshot_thread (GTask               *task,
                      EphySnapshotService *service,
                      SnapshotAsyncData   *data,
                      GCancellable        *cancellable)
{
  char *path;

  save_thumbnail (data->snapshot, data->url);
  path = thumbnail_path (data->url);
  cache_snapshot_data_in_idle (service, data->url, path, SNAPSHOT_FRESH);

  g_task_return_pointer (task, path, g_free);
}

static void
ephy_snapshot_service_save_snapshot_async (EphySnapshotService *service,
                                           GdkPixbuf           *snapshot,
                                           const char          *url,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data)
{
  GTask *task;

  g_assert (EPHY_IS_SNAPSHOT_SERVICE (service));
  g_assert (GDK_IS_PIXBUF (snapshot));
  g_assert (url);

  task = g_task_new (service, cancellable, callback, user_data);
  g_task_set_priority (task, G_PRIORITY_LOW);
  g_task_set_task_data (task,
                        snapshot_async_data_new (service, snapshot, NULL, url),
                        (GDestroyNotify)snapshot_async_data_free);
  g_task_run_in_thread (task, (GTaskThreadFunc)save_snapshot_thread);
  g_object_unref (task);
}

static char *
ephy_snapshot_service_save_snapshot_finish (EphySnapshotService  *service,
                                            GAsyncResult         *result,
                                            GError              **error)
{
  g_assert (g_task_is_valid (result, service));

  return g_task_propagate_pointer (G_TASK (result), error);
}


static void
snapshot_saved (EphySnapshotService *service,
                GAsyncResult        *result,
                GTask               *task)
{
  char *path;

  path = ephy_snapshot_service_save_snapshot_finish (service, result, NULL);
  g_task_return_pointer (task, path, g_free);
  g_object_unref (task);
}

static void
save_snapshot (GdkTexture *texture,
               GTask      *task)
{
  SnapshotAsyncData *data = g_task_get_task_data (task);

  data->snapshot = ephy_snapshot_service_prepare_snapshot (texture);
  if (!data->snapshot) {
    g_task_return_new_error (task,
                             EPHY_SNAPSHOT_SERVICE_ERROR,
                             EPHY_SNAPSHOT_SERVICE_ERROR_WEB_VIEW,
                             "WebView returned invalid snapshot for \"%s\"", data->url);
    g_object_unref (task);
    return;
  }

  ephy_snapshot_service_save_snapshot_async (g_task_get_source_object (task),
                                             data->snapshot,
                                             webkit_web_view_get_uri (data->web_view),
                                             g_task_get_cancellable (task),
                                             (GAsyncReadyCallback)snapshot_saved,
                                             task);
}

static void
on_snapshot_ready (WebKitWebView *web_view,
                   GAsyncResult  *result,
                   GTask         *task)
{
  g_autoptr (GdkTexture) texture = NULL;
  GError *error = NULL;

  texture = webkit_web_view_get_snapshot_finish (web_view, result, &error);
  if (error) {
    g_task_return_error (task, error);
    g_object_unref (task);
    return;
  }

  save_snapshot (texture, task);
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
                                WEBKIT_SNAPSHOT_REGION_FULL_DOCUMENT,
                                WEBKIT_SNAPSHOT_OPTIONS_NONE,
                                g_task_get_cancellable (task),
                                (GAsyncReadyCallback)on_snapshot_ready,
                                task);
  return FALSE;
}

static void
webview_destroyed_cb (GtkWidget *web_view,
                      GTask     *task)
{
  g_task_return_new_error (task,
                           EPHY_SNAPSHOT_SERVICE_ERROR,
                           EPHY_SNAPSHOT_SERVICE_ERROR_WEB_VIEW,
                           "%s", "Error getting snapshot, web view was destroyed");
  g_object_unref (task);
}

static void
webview_load_changed_cb (WebKitWebView   *web_view,
                         WebKitLoadEvent  load_event,
                         GTask           *task)
{
  if (load_event != WEBKIT_LOAD_FINISHED)
    return;

  /* Load finished doesn't ensure that we actually have visible content yet,
   *  so hold a bit before retrieving the snapshot. */
  g_idle_add ((GSourceFunc)retrieve_snapshot_from_web_view, task);

  /* Some pages might end up causing this condition to happen twice, so remove
   *  the handler in order to avoid calling the above idle function twice. */
  g_signal_handlers_disconnect_by_func (web_view, webview_load_changed_cb, task);
  g_signal_handlers_disconnect_by_func (web_view, webview_destroyed_cb, task);
}

static gboolean
webview_load_failed_cb (WebKitWebView   *web_view,
                        WebKitLoadEvent  load_event,
                        const char       failing_uri,
                        GError          *error,
                        GTask           *task)
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

  if (!service)
    service = g_object_new (EPHY_TYPE_SNAPSHOT_SERVICE, NULL);

  return service;
}

const char *
ephy_snapshot_service_lookup_cached_snapshot_path (EphySnapshotService *service,
                                                   const char          *url)
{
  SnapshotPathCachedData *data;

  g_assert (EPHY_IS_SNAPSHOT_SERVICE (service));

  data = g_hash_table_lookup (service->cache, url);

  return !data ? NULL : data->path;
}

static EphySnapshotFreshness
ephy_snapshot_service_lookup_snapshot_freshness (EphySnapshotService *service,
                                                 const char          *url)
{
  SnapshotPathCachedData *data;

  data = g_hash_table_lookup (service->cache, url);

  return !data ? SNAPSHOT_STALE : data->freshness;
}

static void
get_snapshot_path_for_url_thread (GTask               *task,
                                  EphySnapshotService *service,
                                  SnapshotAsyncData   *data,
                                  GCancellable        *cancellable)
{
  g_autoptr (GdkPixbuf) pixbuf = NULL;
  g_autoptr (GError) error = NULL;
  char *path;

  path = thumbnail_path (data->url);

  pixbuf = gdk_pixbuf_new_from_file (path, &error);
  if (!pixbuf) {
    g_task_return_new_error (task,
                             EPHY_SNAPSHOT_SERVICE_ERROR,
                             EPHY_SNAPSHOT_SERVICE_ERROR_NOT_FOUND,
                             "Snapshot for url \"%s\" not found in disk cache: %s",
                             data->url, error->message);
    g_free (path);
    return;
  }

  cache_snapshot_data_in_idle (service, data->url, path, SNAPSHOT_STALE);

  g_task_return_pointer (task, path, g_free);
}

void
ephy_snapshot_service_get_snapshot_path_for_url_async (EphySnapshotService *service,
                                                       const char          *url,
                                                       GCancellable        *cancellable,
                                                       GAsyncReadyCallback  callback,
                                                       gpointer             user_data)
{
  GTask *task;
  const char *path;

  g_assert (EPHY_IS_SNAPSHOT_SERVICE (service));
  g_assert (url);

  task = g_task_new (service, cancellable, callback, user_data);

  path = ephy_snapshot_service_lookup_cached_snapshot_path (service, url);
  if (path) {
    g_task_return_pointer (task, g_strdup (path), g_free);
    g_object_unref (task);
    return;
  }

  g_task_set_priority (task, G_PRIORITY_LOW);
  g_task_set_task_data (task,
                        snapshot_async_data_new (service, NULL, NULL, url),
                        (GDestroyNotify)snapshot_async_data_free);
  g_task_run_in_thread (task, (GTaskThreadFunc)get_snapshot_path_for_url_thread);
  g_object_unref (task);
}

static void
take_fresh_snapshot_in_background_if_stale (EphySnapshotService *service,
                                            SnapshotAsyncData   *data)
{
  GTask *task;

  /* We schedule a new snapshot now, which will complete eventually. It won't be
   * used now. This is just to ensure we get a newer snapshot in the future. */
  if (ephy_snapshot_service_lookup_snapshot_freshness (service, data->url) == SNAPSHOT_STALE) {
    task = g_task_new (service, NULL, NULL, NULL);
    g_task_set_task_data (task,
                          data,
                          (GDestroyNotify)snapshot_async_data_free);
    ephy_snapshot_service_take_from_webview (task);
  } else {
    snapshot_async_data_free (data);
  }
}

char *
ephy_snapshot_service_get_snapshot_path_for_url_finish (EphySnapshotService  *service,
                                                        GAsyncResult         *result,
                                                        GError              **error)
{
  g_assert (g_task_is_valid (result, service));

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
got_snapshot_path_for_url (EphySnapshotService *service,
                           GAsyncResult        *result,
                           GTask               *task)
{
  SnapshotAsyncData *data = g_task_get_task_data (task);
  char *path;

  path = ephy_snapshot_service_get_snapshot_path_for_url_finish (service, result, NULL);
  if (path) {
    take_fresh_snapshot_in_background_if_stale (service, snapshot_async_data_copy (data));
    g_task_return_pointer (task, path, g_free);
    g_object_unref (task);
  } else {
    ephy_snapshot_service_take_from_webview (task);
  }
}

void
ephy_snapshot_service_get_snapshot_path_async (EphySnapshotService *service,
                                               WebKitWebView       *web_view,
                                               GCancellable        *cancellable,
                                               GAsyncReadyCallback  callback,
                                               gpointer             user_data)
{
  GTask *task;
  const char *uri;
  const char *path;

  g_assert (EPHY_IS_SNAPSHOT_SERVICE (service));
  g_assert (WEBKIT_IS_WEB_VIEW (web_view));
  g_assert (webkit_web_view_get_uri (web_view));

  task = g_task_new (service, cancellable, callback, user_data);

  uri = webkit_web_view_get_uri (web_view);
  path = ephy_snapshot_service_lookup_cached_snapshot_path (service, uri);

  if (path) {
    take_fresh_snapshot_in_background_if_stale (service,
                                                snapshot_async_data_new (service, NULL, web_view, uri));
    g_task_return_pointer (task, g_strdup (path), g_free);
    g_object_unref (task);
  } else {
    g_task_set_task_data (task,
                          snapshot_async_data_new (service, NULL, web_view, uri),
                          (GDestroyNotify)snapshot_async_data_free);
    ephy_snapshot_service_get_snapshot_path_for_url_async (service,
                                                           uri,
                                                           cancellable,
                                                           (GAsyncReadyCallback)got_snapshot_path_for_url,
                                                           task);
  }
}

char *
ephy_snapshot_service_get_snapshot_path_finish (EphySnapshotService  *service,
                                                GAsyncResult         *result,
                                                GError              **error)
{
  g_assert (g_task_is_valid (result, service));

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
got_snapshot_path_to_delete_cb (EphySnapshotService *service,
                                GAsyncResult        *result,
                                gpointer             user_data)
{
  char *path;

  path = ephy_snapshot_service_get_snapshot_path_for_url_finish (service, result, NULL);
  if (path)
    unlink (path);
  g_free (path);

  g_object_unref (service);
}

void
ephy_snapshot_service_delete_snapshot_for_url (EphySnapshotService *service,
                                               const char          *url)
{
  ephy_snapshot_service_get_snapshot_path_for_url_async (g_object_ref (service),
                                                         url,
                                                         NULL,
                                                         (GAsyncReadyCallback)got_snapshot_path_to_delete_cb,
                                                         NULL);
}

void
ephy_snapshot_service_delete_all_snapshots (EphySnapshotService *service)
{
  GError *error = NULL;
  char *dir;

  dir = thumbnail_directory ();

  ephy_file_delete_dir_recursively (dir, &error);
  if (error) {
    g_warning ("Failed to delete thumbnail directory: %s", error->message);
    g_error_free (error);
  }

  g_free (dir);
}
