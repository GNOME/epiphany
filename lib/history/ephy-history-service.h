/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*
 *  Copyright © 2011 Igalia S.L.
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

#pragma once

#include <glib-object.h>
#include <gio/gio.h>

#include "ephy-history-types.h"
#include "ephy-sqlite-connection.h"

G_BEGIN_DECLS

#define EPHY_TYPE_HISTORY_SERVICE (ephy_history_service_get_type())

G_DECLARE_FINAL_TYPE (EphyHistoryService, ephy_history_service, EPHY, HISTORY_SERVICE, GObject)

typedef void   (*EphyHistoryJobCallback)          (EphyHistoryService *service, gboolean success, gpointer result_data, gpointer user_data);

EphyHistoryService *     ephy_history_service_new                     (const char *history_filename, EphySQLiteConnectionMode mode);

void                     ephy_history_service_add_visit               (EphyHistoryService *self, EphyHistoryPageVisit *visit, GCancellable *cancellable, EphyHistoryJobCallback callback, gpointer user_data);
void                     ephy_history_service_add_visits              (EphyHistoryService *self, GList *visits, GCancellable *cancellable, EphyHistoryJobCallback callback, gpointer user_data);
void                     ephy_history_service_find_visits_in_time     (EphyHistoryService *self, gint64 from, gint64 to, GCancellable *cancellable, EphyHistoryJobCallback callback, gpointer user_data);
void                     ephy_history_service_query_visits            (EphyHistoryService *self, EphyHistoryQuery *query, GCancellable *cancellable, EphyHistoryJobCallback callback, gpointer user_data);
void                     ephy_history_service_query_urls              (EphyHistoryService *self, EphyHistoryQuery *query, GCancellable *cancellable, EphyHistoryJobCallback callback, gpointer user_data);
void                     ephy_history_service_set_url_title           (EphyHistoryService *self, const char *url, const char *title, GCancellable *cancellable, EphyHistoryJobCallback callback, gpointer user_data);
void                     ephy_history_service_set_url_hidden          (EphyHistoryService *self, const char *url, gboolean hidden, GCancellable *cancellable, EphyHistoryJobCallback callback, gpointer user_data);
void                     ephy_history_service_set_url_zoom_level      (EphyHistoryService *self, const char *url, double zoom_level, GCancellable *cancellable, EphyHistoryJobCallback callback, gpointer user_data);
void                     ephy_history_service_get_host_for_url        (EphyHistoryService *self, const char *url, GCancellable *cancellable, EphyHistoryJobCallback callback, gpointer user_data);
void                     ephy_history_service_get_hosts               (EphyHistoryService *self, GCancellable *cancellable, EphyHistoryJobCallback callback, gpointer user_data);
void                     ephy_history_service_query_hosts             (EphyHistoryService *self, EphyHistoryQuery *query, GCancellable *cancellable, EphyHistoryJobCallback callback, gpointer user_data);
void                     ephy_history_service_delete_host             (EphyHistoryService *self, EphyHistoryHost *host, GCancellable *cancellable, EphyHistoryJobCallback callback, gpointer user_data);
void                     ephy_history_service_get_url                 (EphyHistoryService *self, const char *url, GCancellable *cancellable, EphyHistoryJobCallback callback, gpointer user_data);
void                     ephy_history_service_delete_urls             (EphyHistoryService *self, GList *urls, GCancellable *cancellable, EphyHistoryJobCallback callback, gpointer user_data);
void                     ephy_history_service_find_urls               (EphyHistoryService *self, gint64 from, gint64 to, guint limit, gint host, GList *substring_list, EphyHistorySortType sort_type, GCancellable *cancellable, EphyHistoryJobCallback callback, gpointer user_data);
void                     ephy_history_service_visit_url               (EphyHistoryService *self, const char *url, const char *sync_id, gint64 visit_time, EphyHistoryPageVisitType visit_type, gboolean should_notify);
void                     ephy_history_service_clear                   (EphyHistoryService *self, GCancellable *cancellable, EphyHistoryJobCallback callback, gpointer user_data);
void                     ephy_history_service_find_hosts              (EphyHistoryService *self, gint64 from, gint64 to, GCancellable *cancellable, EphyHistoryJobCallback callback, gpointer user_data);

G_END_DECLS
