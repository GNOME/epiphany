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

#include <glib.h>

G_BEGIN_DECLS


typedef enum {
  EPHY_PAGE_VISIT_NONE = 0,
  EPHY_PAGE_VISIT_LINK,
  EPHY_PAGE_VISIT_TYPED,
  /* We jump to 8 to avoid changing the visits table format. */
  EPHY_PAGE_VISIT_BOOKMARK = 8,
  EPHY_PAGE_VISIT_HOMEPAGE
} EphyHistoryPageVisitType;

typedef enum {
  EPHY_HISTORY_URL_TITLE,
} EphyHistoryURLProperty;

typedef enum {
  EPHY_HISTORY_SORT_NONE = 0,
  EPHY_HISTORY_SORT_MOST_RECENTLY_VISITED,
  EPHY_HISTORY_SORT_LEAST_RECENTLY_VISITED,
  EPHY_HISTORY_SORT_MOST_VISITED,
  EPHY_HISTORY_SORT_LEAST_VISITED,
  EPHY_HISTORY_SORT_TITLE_ASCENDING,
  EPHY_HISTORY_SORT_TITLE_DESCENDING,
  EPHY_HISTORY_SORT_URL_ASCENDING,
  EPHY_HISTORY_SORT_URL_DESCENDING
} EphyHistorySortType;

typedef struct
{
  int id;
  char* url;
  char* title;
  int visit_count;
  double zoom_level;
} EphyHistoryHost;

typedef struct _EphyHistoryURL
{
  int id;
  char* url;
  char* title;
  char *sync_id;
  int visit_count;
  int typed_count;
  gint64 last_visit_time; /* Microseconds */
  gboolean hidden;
  EphyHistoryHost *host;
  gboolean notify_visit;
  gboolean notify_delete;
} EphyHistoryURL;

typedef struct _EphyHistoryPageVisit
{
  EphyHistoryURL* url;
  int id;
  gint64 visit_time; /* Microseconds */
  EphyHistoryPageVisitType visit_type;
} EphyHistoryPageVisit;

typedef struct _EphyHistoryQuery
{
  gint64 from;  /* Microseconds */
  gint64 to;    /* Microseconds */
  guint limit;
  GList* substring_list;
  gboolean ignore_hidden;
  gboolean ignore_local;
  gint host;
  EphyHistorySortType sort_type;
} EphyHistoryQuery;

EphyHistoryPageVisit *          ephy_history_page_visit_new (const char *url, gint64 visit_time, EphyHistoryPageVisitType visit_type);
EphyHistoryPageVisit *          ephy_history_page_visit_new_with_url (EphyHistoryURL *url, gint64 visit_time, EphyHistoryPageVisitType visit_type);
EphyHistoryPageVisit *          ephy_history_page_visit_copy (EphyHistoryPageVisit *visit);
void                            ephy_history_page_visit_free (EphyHistoryPageVisit *visit);

GList *                         ephy_history_page_visit_list_copy (GList* original);
void                            ephy_history_page_visit_list_free (GList* list);

EphyHistoryHost *               ephy_history_host_new (const char *url, const char *title, int visit_count, double zoom_level);
EphyHistoryHost *               ephy_history_host_copy (EphyHistoryHost *original);
void                            ephy_history_host_free (EphyHistoryHost *host);
void                            ephy_history_host_list_free (GList *list);

EphyHistoryURL *                ephy_history_url_new (const char *url, const char *title, int visit_count, int typed_count, gint64 last_visit_time);
EphyHistoryURL *                ephy_history_url_copy (EphyHistoryURL *url);
void                            ephy_history_url_free (EphyHistoryURL *url);

GList *                         ephy_history_url_list_copy (GList *original);
void                            ephy_history_url_list_free (GList *list);

EphyHistoryQuery *              ephy_history_query_new (void);
void                            ephy_history_query_free (EphyHistoryQuery *query);
EphyHistoryQuery *              ephy_history_query_copy (EphyHistoryQuery *query);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(EphyHistoryHost, ephy_history_host_free)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(EphyHistoryURL, ephy_history_url_free)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(EphyHistoryPageVisit, ephy_history_page_visit_free)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(EphyHistoryQuery, ephy_history_query_free)

G_END_DECLS
