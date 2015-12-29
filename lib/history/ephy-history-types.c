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

#include <glib.h>

#include "ephy-history-types.h"

EphyHistoryPageVisit *
ephy_history_page_visit_new_with_url (EphyHistoryURL *url, gint64 visit_time, EphyHistoryPageVisitType visit_type)
{
  EphyHistoryPageVisit *visit = g_slice_alloc0 (sizeof (EphyHistoryPageVisit));
  visit->id = -1;
  visit->url = url;
  visit->visit_time = visit_time;
  visit->visit_type = visit_type;
  return visit;
}

EphyHistoryPageVisit *
ephy_history_page_visit_new (const char *url, gint64 visit_time, EphyHistoryPageVisitType visit_type)
{
  return ephy_history_page_visit_new_with_url (ephy_history_url_new (url, url, 0, 0, 0),
                                               visit_time, visit_type);
}

void
ephy_history_page_visit_free (EphyHistoryPageVisit *visit)
{
  if (visit == NULL)
    return;

  ephy_history_url_free (visit->url);
  g_slice_free1 (sizeof (EphyHistoryPageVisit), visit);
}

EphyHistoryPageVisit *
ephy_history_page_visit_copy (EphyHistoryPageVisit *visit)
{
  EphyHistoryPageVisit *copy = ephy_history_page_visit_new_with_url (0, visit->visit_time, visit->visit_type);
  copy->id = visit->id;
  copy->url = ephy_history_url_copy (visit->url);
  return copy;
}

GList *
ephy_history_page_visit_list_copy (GList *original)
{
  GList *new = g_list_copy (original);
  GList *current = new;
  while (current) {
    current->data = ephy_history_page_visit_copy ((EphyHistoryPageVisit *) current->data);
    current = current->next;
  }
  return new;
}

void
ephy_history_page_visit_list_free (GList *list)
{
  g_list_free_full (list, (GDestroyNotify) ephy_history_page_visit_free);
}

EphyHistoryHost *
ephy_history_host_new (const char *url, const char *title, int visit_count, double zoom_level)
{
  EphyHistoryHost *host = g_slice_alloc0 (sizeof (EphyHistoryHost));

  host->id = -1;
  host->url = g_strdup (url);
  host->title = g_strdup (title);
  host->visit_count = visit_count;
  host->zoom_level = zoom_level;

  return host;
}

EphyHistoryHost *
ephy_history_host_copy (EphyHistoryHost *original)
{
  EphyHistoryHost *host;

  if (original == NULL)
    return NULL;

  host = ephy_history_host_new (original->url,
                                original->title,
                                original->visit_count,
                                original->zoom_level);
  host->id = original->id;

  return host;
}

void
ephy_history_host_free (EphyHistoryHost *host)
{
  if (host == NULL)
    return;

  g_free (host->url);
  g_free (host->title);

  g_slice_free1 (sizeof (EphyHistoryHost), host);
}

EphyHistoryURL *
ephy_history_url_new (const char *url, const char *title, int visit_count, int typed_count, int last_visit_time)
{
  EphyHistoryURL *history_url = g_slice_alloc0 (sizeof (EphyHistoryURL));
  history_url->id = -1;
  history_url->url = g_strdup (url);
  history_url->title = g_strdup (title);
  history_url->visit_count = visit_count;
  history_url->typed_count = typed_count;
  history_url->last_visit_time = last_visit_time;
  history_url->host = NULL;
  return history_url;
}

EphyHistoryURL *
ephy_history_url_copy (EphyHistoryURL *url)
{
  EphyHistoryURL *copy;
  if (url == NULL)
    return NULL;

  copy = ephy_history_url_new (url->url,
                               url->title,
                               url->visit_count,
                               url->typed_count,
                               url->last_visit_time);
  copy->id = url->id;
  copy->hidden = url->hidden;
  copy->host = ephy_history_host_copy (url->host);
  copy->thumbnail_time = url->thumbnail_time;

  return copy;
}

void
ephy_history_url_free (EphyHistoryURL *url)
{
  if (url == NULL)
    return;

  g_free (url->url);
  g_free (url->title);
  ephy_history_host_free (url->host);
  g_slice_free1 (sizeof (EphyHistoryURL), url);
}

GList *
ephy_history_url_list_copy (GList *original)
{
  GList *new = NULL, *last;

  if (original) {
    new = last = g_list_append (NULL, ephy_history_url_copy (original->data));
    original = original->next;

    while (original) {
      last = g_list_append (last, ephy_history_url_copy (original->data));
      last = last->next;
      original = original->next;
    }
  }

  return new;
}

void
ephy_history_url_list_free (GList *list)
{
  g_list_free_full (list, (GDestroyNotify) ephy_history_url_free);
}

EphyHistoryQuery *
ephy_history_query_new ()
{
  return (EphyHistoryQuery*) g_slice_alloc0 (sizeof (EphyHistoryQuery));
}

void
ephy_history_query_free (EphyHistoryQuery *query)
{
  g_list_free_full (query->substring_list, g_free);
  g_slice_free1 (sizeof (EphyHistoryQuery), query);
}

EphyHistoryQuery *
ephy_history_query_copy (EphyHistoryQuery *query)
{
  GList *iter;
  EphyHistoryQuery *copy = ephy_history_query_new ();
  copy->from = query->from;
  copy->to = query->to;
  copy->limit = query->limit;
  copy->sort_type = query->sort_type;
  copy->ignore_hidden = query->ignore_hidden;
  copy->ignore_local = query->ignore_local;
  copy->host = query->host;

  for (iter = query->substring_list; iter != NULL; iter = iter->next) {
    copy->substring_list = g_list_prepend (copy->substring_list, g_strdup (iter->data));
  }
  copy->substring_list = g_list_reverse (copy->substring_list);

  return copy;
}
