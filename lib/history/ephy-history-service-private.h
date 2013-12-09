/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
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

#ifndef EPHY_HISTORY_SERVICE_PRIVATE_H
#define EPHY_HISTORY_SERVICE_PRIVATE_H

#include "ephy-sqlite-connection.h"

struct _EphyHistoryServicePrivate {
  char *history_filename;
  EphySQLiteConnection *history_database;
  GThread *history_thread;
  GAsyncQueue *queue;
  gboolean scheduled_to_quit;
  gboolean scheduled_to_commit;
  gboolean read_only;
  int queue_urls_visited_id;
};

void                     ephy_history_service_schedule_commit         (EphyHistoryService *self); 
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

#endif /* EPHY_HISTORY_SERVICE_PRIVATE_H */
