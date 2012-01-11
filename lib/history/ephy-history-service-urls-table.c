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

#include "config.h"

#include "ephy-history-service.h"
#include "ephy-history-service-private.h"

gboolean
ephy_history_service_initialize_urls_table (EphyHistoryService *self)
{
  EphyHistoryServicePrivate *priv = EPHY_HISTORY_SERVICE (self)->priv;
  GError *error = NULL;

  if (ephy_sqlite_connection_table_exists (priv->history_database, "visits")) {
    return TRUE;
  }
  ephy_sqlite_connection_execute (priv->history_database,
    "CREATE TABLE urls ("
    "id INTEGER PRIMARY KEY,"
    "host INTEGER NOT NULL REFERENCES hosts(id) ON DELETE CASCADE,"
    "url LONGVARCAR,"
    "title LONGVARCAR,"
    "visit_count INTEGER DEFAULT 0 NOT NULL,"
    "typed_count INTEGER DEFAULT 0 NOT NULL,"
    "last_visit_time INTEGER,"
    "zoom_level REAL DEFAULT 1.0)", &error);

  if (error) {
    g_error("Could not create urls table: %s", error->message);
    g_error_free (error);
    return FALSE;
  }
  ephy_history_service_schedule_commit (self);
  return TRUE;
}

EphyHistoryURL *
ephy_history_service_get_url_row (EphyHistoryService *self, const char *url_string, EphyHistoryURL *url)
{
  EphyHistoryServicePrivate *priv = EPHY_HISTORY_SERVICE (self)->priv;
  EphySQLiteStatement *statement = NULL;  
  GError *error = NULL;

  g_assert (priv->history_thread == g_thread_self ());
  g_assert (priv->history_database != NULL);

  if (url_string == NULL && url != NULL) {
    url_string = url->url;
  }
  g_assert (url_string || url->id != -1);

  if (url != NULL && url->id != -1) {
    statement = ephy_sqlite_connection_create_statement (priv->history_database,
      "SELECT id, url, title, visit_count, typed_count, last_visit_time, zoom_level FROM urls "
      "WHERE id=?", &error);
  } else {
    statement = ephy_sqlite_connection_create_statement (priv->history_database,
      "SELECT id, url, title, visit_count, typed_count, last_visit_time, zoom_level FROM urls "
      "WHERE url=?", &error);
  }

  if (error) {
    g_error ("Could not build urls table query statement: %s", error->message);
    g_error_free (error);
    return NULL;
  }

  if (url != NULL && url->id != -1) {
    ephy_sqlite_statement_bind_int (statement, 0, url->id, &error);
  } else {
    ephy_sqlite_statement_bind_string (statement, 0, url_string, &error);
  }
  if (error) {
    g_error ("Could not build urls table query statement: %s", error->message);
    g_error_free (error);
    g_object_unref (statement);
    return NULL;
  }

  if (ephy_sqlite_statement_step (statement, &error) == FALSE) {
    g_object_unref (statement);
    return NULL;
  }

  if (url == NULL) {
    url = ephy_history_url_new (NULL, NULL, 0, 0, 0, 1.0);
  } else {
    if (url->url)
      g_free (url->url);
    if (url->title)
      g_free (url->title);
  }

  url->id = ephy_sqlite_statement_get_column_as_int (statement, 0);
  url->url = g_strdup (ephy_sqlite_statement_get_column_as_string (statement, 1)),
  url->title = g_strdup (ephy_sqlite_statement_get_column_as_string (statement, 2)),
  url->visit_count = ephy_sqlite_statement_get_column_as_int (statement, 3),
  url->typed_count = ephy_sqlite_statement_get_column_as_int (statement, 4),
  url->last_visit_time = ephy_sqlite_statement_get_column_as_int (statement, 5);
  url->zoom_level = ephy_sqlite_statement_get_column_as_double (statement, 6);

  g_object_unref (statement);
  return url;
}

void
ephy_history_service_add_url_row (EphyHistoryService *self, EphyHistoryURL *url)
{
  EphyHistoryServicePrivate *priv = EPHY_HISTORY_SERVICE (self)->priv;
  EphySQLiteStatement *statement = NULL;  
  GError *error = NULL;

  g_assert (priv->history_thread == g_thread_self ());
  g_assert (priv->history_database != NULL);

  statement = ephy_sqlite_connection_create_statement (priv->history_database,
    "INSERT INTO urls (url, title, visit_count, typed_count, last_visit_time, zoom_level, host) "
    " VALUES (?, ?, ?, ?, ?, ?, ?)", &error);
  if (error) {
    g_error ("Could not build urls table addition statement: %s", error->message);
    g_error_free (error);
    return;
  }

  if (ephy_sqlite_statement_bind_string (statement, 0, url->url, &error) == FALSE ||
      ephy_sqlite_statement_bind_string (statement, 1, url->title, &error) == FALSE || 
      ephy_sqlite_statement_bind_int (statement, 2, url->visit_count, &error) == FALSE ||
      ephy_sqlite_statement_bind_int (statement, 3, url->typed_count, &error) == FALSE ||
      ephy_sqlite_statement_bind_int (statement, 4, url->last_visit_time, &error) == FALSE ||
      ephy_sqlite_statement_bind_double (statement, 5, url->zoom_level, &error) == FALSE ||
      ephy_sqlite_statement_bind_int (statement, 6, url->host->id, &error) == FALSE) {
    g_error ("Could not insert URL into urls table: %s", error->message);
    g_error_free (error);
    return;
  }

  ephy_sqlite_statement_step (statement, &error);
  if (error) {
    g_error ("Could not insert URL into urls table: %s", error->message);
    g_error_free (error);
  } else {
    url->id = ephy_sqlite_connection_get_last_insert_id (priv->history_database);
  }

  g_object_unref (statement);
}

void
ephy_history_service_update_url_row (EphyHistoryService *self, EphyHistoryURL *url)
{
  EphyHistoryServicePrivate *priv = EPHY_HISTORY_SERVICE (self)->priv;
  EphySQLiteStatement *statement;
  GError *error = NULL;

  g_assert (priv->history_thread == g_thread_self ());
  g_assert (priv->history_database != NULL);

  statement = ephy_sqlite_connection_create_statement (priv->history_database,
    "UPDATE urls SET title=?, visit_count=?, typed_count=?, last_visit_time=?, zoom_level=? "
    "WHERE id=?", &error);
  if (error) {
    g_error ("Could not build urls table modification statement: %s", error->message);
    g_error_free (error);
    return;
  }

  if (ephy_sqlite_statement_bind_string (statement, 0, url->title, &error) == FALSE || 
      ephy_sqlite_statement_bind_int (statement, 1, url->visit_count, &error) == FALSE ||
      ephy_sqlite_statement_bind_int (statement, 2, url->typed_count, &error) == FALSE ||
      ephy_sqlite_statement_bind_int (statement, 3, url->last_visit_time, &error) == FALSE ||
      ephy_sqlite_statement_bind_double (statement, 4, url->zoom_level, &error) == FALSE ||
      ephy_sqlite_statement_bind_int (statement, 5, url->id, &error) == FALSE) {
    g_error ("Could not modify URL in urls table: %s", error->message);
    g_error_free (error);
    return;
  }

  ephy_sqlite_statement_step (statement, &error);
  if (error) {
    g_error ("Could not modify URL in urls table: %s", error->message);
    g_error_free (error);
  }
  g_object_unref (statement);
}

static EphyHistoryURL *
create_url_from_statement (EphySQLiteStatement *statement)
{
  EphyHistoryURL *url = ephy_history_url_new (ephy_sqlite_statement_get_column_as_string (statement, 1),
                                              ephy_sqlite_statement_get_column_as_string (statement, 2),
                                              ephy_sqlite_statement_get_column_as_int (statement, 3),
                                              ephy_sqlite_statement_get_column_as_int (statement, 4),
                                              ephy_sqlite_statement_get_column_as_int (statement, 5),
                                              ephy_sqlite_statement_get_column_as_int (statement, 6));

  url->id = ephy_sqlite_statement_get_column_as_int (statement, 0);
  url->host = ephy_history_host_new (NULL, NULL, 0);
  url->host->id = ephy_sqlite_statement_get_column_as_int (statement, 7);

  return url;
}

GList *
ephy_history_service_find_url_rows (EphyHistoryService *self, EphyHistoryQuery *query)
{
  EphyHistoryServicePrivate *priv = EPHY_HISTORY_SERVICE (self)->priv;
  EphySQLiteStatement *statement = NULL;
  GList *substring;
  GString *statement_str;
  GList *urls = NULL;
  GError *error = NULL;
  const char *base_statement = ""
    "SELECT "
      "DISTINCT urls.id, "
      "urls.url, "
      "urls.title, "
      "urls.visit_count, "
      "urls.typed_count, "
      "urls.last_visit_time, "
      "urls.zoom_level, "
      "urls.host "
    "FROM "
      "urls ";

  int i = 0;

  g_assert (priv->history_thread == g_thread_self ());
  g_assert (priv->history_database != NULL);

  statement_str = g_string_new (base_statement);

  if (query->from > 0 || query->to > 0) {
    statement_str = g_string_append (statement_str, "JOIN visits ON visits.url = urls.id WHERE ");
    if (query->from > 0)
      statement_str = g_string_append (statement_str, "visits.visit_time >= ? AND ");
    if (query->to > 0)
      statement_str = g_string_append (statement_str, "visits.visit_time <= ? AND ");
  } else {
    statement_str = g_string_append (statement_str, "WHERE ");
  }

  for (substring = query->substring_list; substring != NULL; substring = substring->next)
    statement_str = g_string_append (statement_str, "(urls.url LIKE ? OR urls.title LIKE ?) AND ");

  statement_str = g_string_append (statement_str, "1 ");

  switch (query->sort_type) {
  case EPHY_HISTORY_SORT_MV:
    statement_str = g_string_append (statement_str, "ORDER BY urls.visit_count DESC ");
    break;
  case EPHY_HISTORY_SORT_LV:
    statement_str = g_string_append (statement_str, "ORDER BY urls.visit_count ");
  default:
    g_warning ("We don't support this sorting method yet.");
  }

  if (query->limit) {
    statement_str = g_string_append (statement_str, "LIMIT ? ");
  }

  statement = ephy_sqlite_connection_create_statement (priv->history_database,
						       statement_str->str, &error);
  g_string_free (statement_str, TRUE);

  if (error) {
    g_error ("Could not build urls table query statement: %s", error->message);
    g_error_free (error);
    g_object_unref (statement);
    return NULL;
  }

  if (query->from > 0) {
    if (ephy_sqlite_statement_bind_int (statement, i++, (int)query->from, &error) == FALSE) {
      g_error ("Could not build urls table query statement: %s", error->message);
      g_error_free (error);
      g_object_unref (statement);
      return NULL;
    }
  }
  if (query->to > 0) {
    if (ephy_sqlite_statement_bind_int (statement, i++, (int)query->to, &error) == FALSE) {
      g_error ("Could not build urls table query statement: %s", error->message);
      g_error_free (error);
      g_object_unref (statement);
      return NULL;
    }
  }
  for (substring = query->substring_list; substring != NULL; substring = substring->next) {
    char *string = g_strdup_printf ("%%%s%%", (char*)substring->data);
    if (ephy_sqlite_statement_bind_string (statement, i++, string, &error) == FALSE) {
      g_error ("Could not build urls table query statement: %s", error->message);
      g_error_free (error);
      g_object_unref (statement);
      g_free (string);
      return NULL;
    }
    if (ephy_sqlite_statement_bind_string (statement, i++, string, &error) == FALSE) {
      g_error ("Could not build urls table query statement: %s", error->message);
      g_error_free (error);
      g_object_unref (statement);
      g_free (string);
      return NULL;
    }
    g_free (string);
  }

  if (query->limit)
    if (ephy_sqlite_statement_bind_int (statement, i++, query->limit, &error) == FALSE) {
      g_error ("Could not build urls table query statement: %s", error->message);
      g_error_free (error);
      g_object_unref (statement);
      return NULL;
    }

  while (ephy_sqlite_statement_step (statement, &error))
    urls = g_list_prepend (urls, create_url_from_statement (statement));

  urls = g_list_reverse (urls);

  if (error) {
    g_error ("Could not execute urls table query statement: %s", error->message);
    g_error_free (error);
    g_object_unref (statement);
    g_list_free_full (urls, (GDestroyNotify)ephy_history_url_free);
    return NULL;
  }

  g_object_unref (statement);
  return urls;
}

void
ephy_history_service_delete_url (EphyHistoryService *self, EphyHistoryURL *url)
{
  EphyHistoryServicePrivate *priv = EPHY_HISTORY_SERVICE (self)->priv;
  EphySQLiteStatement *statement = NULL;
  gchar *sql_statement;
  GError *error = NULL;

  g_assert (priv->history_thread == g_thread_self ());
  g_assert (priv->history_database != NULL);

  g_assert (url->id != -1 || url->url);

  if (url->id != -1)
    sql_statement = g_strdup ("DELETE FROM urls WHERE id=?");
  else
    sql_statement = g_strdup ("DELETE FROM urls WHERE url=?");

  statement = ephy_sqlite_connection_create_statement (priv->history_database,
                                                       sql_statement, &error);
  g_free (sql_statement);

  if (error) {
    g_error ("Could not build urls table query statement: %s", error->message);
    g_error_free (error);
    g_object_unref (statement);
    return;
  }

  if (url->id != -1)
    ephy_sqlite_statement_bind_int (statement, 0, url->id, &error);
  else
    ephy_sqlite_statement_bind_string (statement, 0, url->url, &error);

  if (error) {
    g_error ("Could not build urls table query statement: %s", error->message);
    g_error_free (error);
    g_object_unref (statement);
    return;
  }

  ephy_sqlite_statement_step (statement, &error);
  if (error) {
    g_error ("Could not modify URL in urls table: %s", error->message);
    g_error_free (error);
  }
  g_object_unref (statement);
}
