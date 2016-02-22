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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _EPHY_SNAPSHOT_SERVICE_H
#define _EPHY_SNAPSHOT_SERVICE_H

#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

#include <glib-object.h>

G_BEGIN_DECLS

#define EPHY_TYPE_SNAPSHOT_SERVICE (ephy_snapshot_service_get_type ())

G_DECLARE_FINAL_TYPE (EphySnapshotService, ephy_snapshot_service, EPHY, SNAPSHOT_SERVICE, GObject)

#define EPHY_SNAPSHOT_SERVICE_ERROR           (ephy_snapshot_service_error_quark())

typedef enum {
  EPHY_SNAPSHOT_SERVICE_ERROR_NOT_FOUND,
  EPHY_SNAPSHOT_SERVICE_ERROR_WEB_VIEW,
  EPHY_SNAPSHOT_SERVICE_ERROR_INVALID
} EphySnapshotServiceError;

/* Values taken from the Web mockups. */
#define EPHY_THUMBNAIL_WIDTH 180
#define EPHY_THUMBNAIL_HEIGHT 135

GQuark               ephy_snapshot_service_error_quark                 (void);

EphySnapshotService *ephy_snapshot_service_get_default                 (void);

void                 ephy_snapshot_service_get_snapshot_for_url_async  (EphySnapshotService *service,
                                                                        const char *url,
                                                                        const time_t mtime,
                                                                        GCancellable *cancellable,
                                                                        GAsyncReadyCallback callback,
                                                                        gpointer user_data);

GdkPixbuf           *ephy_snapshot_service_get_snapshot_for_url_finish (EphySnapshotService *service,
                                                                        GAsyncResult *result,
                                                                        gchar **path,
                                                                        GError **error);

void                 ephy_snapshot_service_get_snapshot_async          (EphySnapshotService *service,
                                                                        WebKitWebView *web_view,
                                                                        const time_t mtime,
                                                                        GCancellable *cancellable,
                                                                        GAsyncReadyCallback callback,
                                                                        gpointer user_data);

GdkPixbuf           *ephy_snapshot_service_get_snapshot_finish         (EphySnapshotService *service,
                                                                        GAsyncResult *result,
                                                                        gchar **path,
                                                                        GError **error);

void                 ephy_snapshot_service_save_snapshot_async         (EphySnapshotService *service,
                                                                        GdkPixbuf *snapshot,
                                                                        const char *url,
                                                                        time_t mtime,
                                                                        GCancellable *cancellable,
                                                                        GAsyncReadyCallback callback,
                                                                        gpointer user_data);

char                *ephy_snapshot_service_save_snapshot_finish        (EphySnapshotService *service,
                                                                        GAsyncResult *result,
                                                                        GError **error);

const char          *ephy_snapshot_service_lookup_snapshot_path        (EphySnapshotService *service,
                                                                        const char *url);

void             ephy_snapshot_service_get_snapshot_path_for_url_async (EphySnapshotService *service,
                                                                        const char *url,
                                                                        const time_t mtime,
                                                                        GCancellable *cancellable,
                                                                        GAsyncReadyCallback callback,
                                                                        gpointer user_data);
char           *ephy_snapshot_service_get_snapshot_path_for_url_finish (EphySnapshotService *service,
                                                                        GAsyncResult *result,
                                                                        GError **error);
void           ephy_snapshot_service_get_snapshot_path_async           (EphySnapshotService *service,
                                                                        WebKitWebView *web_view,
                                                                        const time_t mtime,
                                                                        GCancellable *cancellable,
                                                                        GAsyncReadyCallback callback,
                                                                        gpointer user_data);
char                *ephy_snapshot_service_get_snapshot_path_finish    (EphySnapshotService *service,
                                                                        GAsyncResult *result,
                                                                        GError **error);

G_END_DECLS

#endif /* _EPHY_SNAPSHOT_SERVICE_H */
