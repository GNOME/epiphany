/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2011 Igalia S.L.
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
 */

#ifndef EPHY_HISTORY_SERVICE_H
#define EPHY_HISTORY_SERVICE_H

#include <glib-object.h>
#include <gio/gio.h>
#include "ephy-history-types.h"

G_BEGIN_DECLS

/* convenience macros */
#define EPHY_TYPE_HISTORY_SERVICE             (ephy_history_service_get_type())
#define EPHY_HISTORY_SERVICE(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),EPHY_TYPE_HISTORY_SERVICE,EphyHistoryService))
#define EPHY_HISTORY_SERVICE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),EPHY_TYPE_HISTORY_SERVICE,EphyHistoryServiceClass))
#define EPHY_IS_HISTORY_SERVICE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),EPHY_TYPE_HISTORY_SERVICE))
#define EPHY_IS_HISTORY_SERVICE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass),EPHY_TYPE_HISTORY_SERVICE))
#define EPHY_HISTORY_SERVICE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj),EPHY_TYPE_HISTORY_SERVICE,EphyHistoryServiceClass))

typedef struct _EphyHistoryService                EphyHistoryService;
typedef struct _EphyHistoryServiceClass           EphyHistoryServiceClass;
typedef struct _EphyHistoryServicePrivate         EphyHistoryServicePrivate;

typedef void   (*EphyHistoryJobCallback)          (EphyHistoryService *service, gboolean success, gpointer result_data, gpointer user_data);

struct _EphyHistoryService {
     GObject parent;

    /* private */
    EphyHistoryServicePrivate *priv;
};

struct _EphyHistoryServiceClass {
  GObjectClass parent_class;

  /* Signals */
  gboolean (* visit_url)   (EphyHistoryService *self,
                            const char *url,
                            EphyHistoryPageVisitType visit_type);
};

GType                    ephy_history_service_get_type                (void);
EphyHistoryService *     ephy_history_service_new                     (const char *history_filename, gboolean read_only);

void                     ephy_history_service_add_visit               (EphyHistoryService *self, EphyHistoryPageVisit *visit, GCancellable *cancellable, EphyHistoryJobCallback callback, gpointer user_data);
void                     ephy_history_service_add_visits              (EphyHistoryService *self, GList *visits, GCancellable *cancellable, EphyHistoryJobCallback callback, gpointer user_data);
void                     ephy_history_service_find_visits_in_time     (EphyHistoryService *self, gint64 from, gint64 to, GCancellable *cancellable, EphyHistoryJobCallback callback, gpointer user_data);
void                     ephy_history_service_query_visits            (EphyHistoryService *self, EphyHistoryQuery *query, GCancellable *cancellable, EphyHistoryJobCallback callback, gpointer user_data);
void                     ephy_history_service_query_urls              (EphyHistoryService *self, EphyHistoryQuery *query, GCancellable *cancellable, EphyHistoryJobCallback callback, gpointer user_data);
void                     ephy_history_service_set_url_title           (EphyHistoryService *self, const char *url, const char *title, GCancellable *cancellable, EphyHistoryJobCallback callback, gpointer user_data);
void                     ephy_history_service_set_url_hidden          (EphyHistoryService *self, const char *url, gboolean hidden, GCancellable *cancellable, EphyHistoryJobCallback callback, gpointer user_data);
void                     ephy_history_service_set_url_thumbnail_time  (EphyHistoryService *self, const char *orig_url, int thumbnail_time, GCancellable *cancellable, EphyHistoryJobCallback callback, gpointer user_data);
void                     ephy_history_service_set_url_zoom_level      (EphyHistoryService *self, const char *url, const double zoom_level, GCancellable *cancellable, EphyHistoryJobCallback callback, gpointer user_data);
void                     ephy_history_service_get_host_for_url        (EphyHistoryService *self, const char *url, GCancellable *cancellable, EphyHistoryJobCallback callback, gpointer user_data);
void                     ephy_history_service_get_hosts               (EphyHistoryService *self, GCancellable *cancellable, EphyHistoryJobCallback callback, gpointer user_data);
void                     ephy_history_service_query_hosts             (EphyHistoryService *self, EphyHistoryQuery *query, GCancellable *cancellable, EphyHistoryJobCallback callback, gpointer user_data);
void                     ephy_history_service_delete_host             (EphyHistoryService *self, EphyHistoryHost *host, GCancellable *cancellable, EphyHistoryJobCallback callback, gpointer user_data);
void                     ephy_history_service_get_url                 (EphyHistoryService *self, const char *url, GCancellable *cancellable, EphyHistoryJobCallback callback, gpointer user_data);
void                     ephy_history_service_delete_urls             (EphyHistoryService *self, GList *urls, GCancellable *cancellable, EphyHistoryJobCallback callback, gpointer user_data);
void                     ephy_history_service_find_urls               (EphyHistoryService *self, gint64 from, gint64 to, guint limit, gint host, GList *substring_list, EphyHistorySortType sort_type, GCancellable *cancellable, EphyHistoryJobCallback callback, gpointer user_data);
void                     ephy_history_service_visit_url               (EphyHistoryService *self, const char *orig_url, EphyHistoryPageVisitType visit_type);
void                     ephy_history_service_clear                   (EphyHistoryService *self, GCancellable *cancellable, EphyHistoryJobCallback callback, gpointer user_data);
void                     ephy_history_service_find_hosts              (EphyHistoryService *self, gint64 from, gint64 to, GCancellable *cancellable, EphyHistoryJobCallback callback, gpointer user_data);

G_END_DECLS

#endif /* EPHY_HISTORY_SERVICE_H */

