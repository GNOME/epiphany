/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2002, 2003 Marco Pesenti Gritti
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
 *
 */

#include "config.h"

#include "ephy-history-service.h"
#include "ephy-history-types.h"
#include "ephy-request-about.h"
#include "ephy-file-helpers.h"
#include "ephy-browse-history.h"

G_DEFINE_TYPE (EphyBrowseHistory, ephy_browse_history, G_TYPE_OBJECT)

#define EPHY_BROWSE_HISTORY_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), EPHY_TYPE_BROWSE_HISTORY, EphyBrowseHistoryPrivate))

struct _EphyBrowseHistoryPrivate
{
  EphyHistoryService *history_service;
};


static void
ephy_browse_history_get_property (GObject    *object,
                                  guint       property_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
ephy_browse_history_set_property (GObject      *object,
                                  guint         property_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
ephy_browse_history_dispose (GObject *object)
{
  G_OBJECT_CLASS (ephy_browse_history_parent_class)->dispose (object);
}

static void
ephy_browse_history_finalize (GObject *object)
{
  EphyBrowseHistory *history = EPHY_BROWSE_HISTORY (object);

  if (history->priv->history_service) {
    g_object_unref (history->priv->history_service);
  }

  G_OBJECT_CLASS (ephy_browse_history_parent_class)->finalize (object);
}

static void
ephy_browse_history_class_init (EphyBrowseHistoryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (EphyBrowseHistoryPrivate));

  object_class->get_property = ephy_browse_history_get_property;
  object_class->set_property = ephy_browse_history_set_property;
  object_class->dispose = ephy_browse_history_dispose;
  object_class->finalize = ephy_browse_history_finalize;
}

static void
ephy_browse_history_init (EphyBrowseHistory *history)
{
  gchar *filename;

  history->priv = EPHY_BROWSE_HISTORY_PRIVATE (history);

  filename = g_build_filename (ephy_dot_dir (), "ephy-history.db", NULL);
  history->priv->history_service = ephy_history_service_new (filename);
  g_free (filename);
}

EphyBrowseHistory *
ephy_browse_history_new (void)
{
  return g_object_new (EPHY_TYPE_BROWSE_HISTORY, NULL);
}

void
ephy_browse_history_add_page (EphyBrowseHistory *history,
                              const char *orig_url)
{
  EphyHistoryPageVisit *visit;
  char *url;

  if (g_str_has_prefix (orig_url, EPHY_ABOUT_SCHEME))
    url = g_strdup_printf ("about:%s", orig_url + EPHY_ABOUT_SCHEME_LEN + 1);
  else
    url = g_strdup (orig_url);

  visit = ephy_history_page_visit_new (url,
                                       time(NULL),
                                       EPHY_PAGE_VISIT_TYPED);
  ephy_history_service_add_visit (history->priv->history_service,
                                  visit, NULL, NULL);
  ephy_history_page_visit_free (visit);
}

void
ephy_browse_history_set_page_title (EphyBrowseHistory *history,
                                    const char *url,
                                    const char *title)
{
  g_return_if_fail (EPHY_IS_BROWSE_HISTORY (history));
  g_return_if_fail (url != NULL);

  ephy_history_service_set_url_title (history->priv->history_service,
                                      url,
                                      title,
                                      NULL, NULL);
}

void
ephy_browse_history_set_page_zoom_level (EphyBrowseHistory *history,
                                         const char *url,
                                         const double zoom_level)
{
  g_return_if_fail (EPHY_IS_BROWSE_HISTORY (history));
  g_return_if_fail (url != NULL);

  ephy_history_service_set_url_zoom_level (history->priv->history_service,
                                           url,
                                           zoom_level,
                                           NULL, NULL);
}

void
ephy_browse_history_get_url (EphyBrowseHistory *history,
                             const char *url,
                             EphyHistoryJobCallback callback,
                             gpointer user_data)
{
  g_return_if_fail (EPHY_IS_BROWSE_HISTORY (history));
  g_return_if_fail (url != NULL);

  ephy_history_service_get_url (history->priv->history_service,
                                url, callback, user_data);
}

void
ephy_browse_history_find_urls (EphyBrowseHistory *history,
                               gint64 from, gint64 to,
                               guint limit,
                               GList *substring_list,
                               EphyHistoryJobCallback callback,
                               gpointer user_data)
{
  EphyHistoryQuery *query;

  g_return_if_fail (EPHY_IS_BROWSE_HISTORY (history));

  query = ephy_history_query_new ();
  query->from = from;
  query->to = to;
  query->substring_list = substring_list;
  query->sort_type = EPHY_HISTORY_SORT_MV;

  if (limit != 0)
    query->limit = limit;

  ephy_history_service_query_urls (history->priv->history_service,
                                   query, callback, user_data);
}

void
ephy_browse_history_delete_urls (EphyBrowseHistory *history,
                                 GList *urls,
                                 EphyHistoryJobCallback callback,
                                 gpointer user_data)
{
  g_return_if_fail (EPHY_IS_BROWSE_HISTORY (history));

  ephy_history_service_delete_urls (history->priv->history_service,
                                    urls, callback, user_data);
}

void
ephy_browse_history_get_host_for_url (EphyBrowseHistory *history,
                                      const char *url,
                                      EphyHistoryJobCallback callback,
                                      gpointer user_data)
{
  g_return_if_fail (EPHY_IS_BROWSE_HISTORY (history));

  ephy_history_service_get_host_for_url (history->priv->history_service,
                                         url, callback, user_data);
}
