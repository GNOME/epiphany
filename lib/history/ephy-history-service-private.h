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

#include "ephy-sqlite-connection.h"

G_BEGIN_DECLS

struct _EphyHistoryService {
  GObject parent_instance;
  char *history_filename;
  EphySQLiteConnection *history_database;
  GMutex history_thread_mutex;
  gboolean history_thread_initialized;
  GCond history_thread_initialized_condition;
  GThread *history_thread;
  GAsyncQueue *queue;
  gboolean scheduled_to_quit;
  gboolean in_memory;
  int queue_urls_visited_id;
};

gboolean                 ephy_history_service_initialize_urls_table   (EphyHistoryService *self);
EphyHistoryURL *         ephy_history_service_get_url_row             (EphyHistoryService *self, const char *url_string, EphyHistoryURL *url);
void                     ephy_history_service_add_url_row             (EphyHistoryService *self, EphyHistoryURL *url);
void                     ephy_history_service_update_url_row          (EphyHistoryService *self, EphyHistoryURL *url);
GList*                   ephy_history_service_find_url_rows           (EphyHistoryService *self, EphyHistoryQuery *query);
void                     ephy_history_service_delete_url              (EphyHistoryService *self, EphyHistoryURL *url);

gboolean                 ephy_history_service_initialize_visits_table (EphyHistoryService *self);
void                     ephy_history_service_add_visit_row           (EphyHistoryService *self, EphyHistoryPageVisit *visit);
GList *                  ephy_history_service_find_visit_rows         (EphyHistoryService *self, EphyHistoryQuery *query);

gboolean                 ephy_history_service_initialize_hosts_table  (EphyHistoryService *self);
void                     ephy_history_service_add_host_row            (EphyHistoryService *self, EphyHistoryHost *host);
void                     ephy_history_service_update_host_row         (EphyHistoryService *self, EphyHistoryHost *host);
EphyHistoryHost *        ephy_history_service_get_host_row            (EphyHistoryService *self, const gchar *url_string, EphyHistoryHost *host);
GList *                  ephy_history_service_get_all_hosts           (EphyHistoryService *self);
GList*                   ephy_history_service_find_host_rows          (EphyHistoryService *self, EphyHistoryQuery *query);
EphyHistoryHost *        ephy_history_service_get_host_row_from_url   (EphyHistoryService *self, const gchar *url);
void                     ephy_history_service_delete_host_row         (EphyHistoryService *self, EphyHistoryHost *host);
void                     ephy_history_service_delete_orphan_hosts     (EphyHistoryService *self);

G_END_DECLS
