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

#ifndef EPHY_HISTORY_TYPES_H
#define EPHY_HISTORY_TYPES_H

#include <glib.h>

G_BEGIN_DECLS

/*
 * Page transition types heavily inspired by those used in Chromium. See:
 * src/chrome/common/page_transition_types.h in the Chromium source code.
 */
typedef enum {
  EPHY_PAGE_VISIT_NONE,
  EPHY_PAGE_VISIT_LINK,
  EPHY_PAGE_VISIT_TYPED,
  EPHY_PAGE_VISIT_MANUAL_SUBFRAME,
  EPHY_PAGE_VISIT_AUTO_SUBFRAME,
  EPHY_PAGE_VISIT_STARTUP,
  EPHY_PAGE_VISIT_FORM_SUBMISSION,
  EPHY_PAGE_VISIT_FORM_RELOAD,
  EPHY_PAGE_VISIT_BOOKMARK,
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
  int visit_count;
  int typed_count;
  int last_visit_time;
  int thumbnail_time;
  gboolean hidden;
  EphyHistoryHost *host;
} EphyHistoryURL;

typedef struct _EphyHistoryPageVisit
{
  EphyHistoryURL* url;
  int id;
  gint64 visit_time;
  EphyHistoryPageVisitType visit_type;
} EphyHistoryPageVisit;

typedef struct _EphyHistoryQuery
{
  gint64 from;
  gint64 to;
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

EphyHistoryURL *                ephy_history_url_new (const char *url, const char* title, int visit_count, int typed_count, int last_visit_time);
EphyHistoryURL *                ephy_history_url_copy (EphyHistoryURL *url);
void                            ephy_history_url_free (EphyHistoryURL *url);

GList *                         ephy_history_url_list_copy (GList *original);
void                            ephy_history_url_list_free (GList *list);

EphyHistoryQuery *              ephy_history_query_new (void);
void                            ephy_history_query_free (EphyHistoryQuery *query);
EphyHistoryQuery *              ephy_history_query_copy (EphyHistoryQuery *query);

G_END_DECLS

#endif /* EPHY_HISTORY_TYPES_H */
