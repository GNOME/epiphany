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
ephy_history_service_initialize_visits_table (EphyHistoryService *self)
{
  EphyHistoryServicePrivate *priv = EPHY_HISTORY_SERVICE (self)->priv;
  GError *error = NULL;

  if (ephy_sqlite_connection_table_exists (priv->history_database, "visits"))
    return TRUE;

  ephy_sqlite_connection_execute (priv->history_database,
    "CREATE TABLE visits ("
    "id INTEGER PRIMARY KEY,"
    "url INTEGER NOT NULL REFERENCES urls(id) ON DELETE CASCADE,"
    "visit_time INTEGER NOT NULL,"
    "visit_type INTEGER NOT NULL,"
    "referring_visit INTEGER)", &error);

  if (error) {
    g_warning ("Could not create visits table: %s", error->message);
    g_error_free (error);
    return FALSE;
  }
  ephy_history_service_schedule_commit (self);
  return TRUE;
}

void
ephy_history_service_add_visit_row (EphyHistoryService *self, EphyHistoryPageVisit *visit)
{
  EphyHistoryServicePrivate *priv = EPHY_HISTORY_SERVICE (self)->priv;
  EphySQLiteStatement *statement;
  GError *error = NULL;

  g_assert (priv->history_thread == g_thread_self ());
  g_assert (priv->history_database != NULL);

  statement = ephy_sqlite_connection_create_statement (
    priv->history_database,
    "INSERT INTO visits (url, visit_time, visit_type) "
    " VALUES (?, ?, ?) ", &error);
  if (error) {
    g_warning ("Could not build visits table addition statement: %s", error->message);
    g_error_free (error);
    return;
  }

  if (ephy_sqlite_statement_bind_int (statement, 0, visit->url->id, &error) == FALSE ||
      ephy_sqlite_statement_bind_int (statement, 1, visit->visit_time, &error) == FALSE ||
      ephy_sqlite_statement_bind_int (statement, 2, visit->visit_type, &error) == FALSE ) {
    g_warning ("Could not build visits table addition statement: %s", error->message);
    g_error_free (error);
    g_object_unref (statement);
    return;
  }

  ephy_sqlite_statement_step (statement, &error);
  if (error) {
    g_warning ("Could not insert URL into visits table: %s", error->message);
    g_error_free (error);
  } else {
    visit->id = ephy_sqlite_connection_get_last_insert_id (priv->history_database);
  }

  ephy_history_service_schedule_commit (self);
  g_object_unref (statement);
}

static EphyHistoryPageVisit *
create_page_visit_from_statement (EphySQLiteStatement *statement)
{
  EphyHistoryPageVisit *visit = 
    ephy_history_page_visit_new (NULL,
                                 ephy_sqlite_statement_get_column_as_int (statement, 1),
                                 ephy_sqlite_statement_get_column_as_int (statement, 2));
  visit->url->id = ephy_sqlite_statement_get_column_as_int (statement, 0);
  return visit;
}

GList *
ephy_history_service_find_visit_rows (EphyHistoryService *self, EphyHistoryQuery *query)
{
  EphyHistoryServicePrivate *priv = EPHY_HISTORY_SERVICE (self)->priv;
  EphySQLiteStatement *statement = NULL;
  GList *substring;
  GString *statement_str;
  GList *visits = NULL;
  GError *error = NULL;
  const char *base_statement = ""
    "SELECT "
      "visits.url, "
      "visits.visit_time, "
      "visits.visit_type ";
  const char *from_join_statement = ""
    "FROM "
      "visits JOIN urls ON visits.url = urls.id ";
  const char *from_visits_statement = ""
    "FROM "
      "visits ";

  int i = 0;

  g_assert (priv->history_thread == g_thread_self ());
  g_assert (priv->history_database != NULL);

  statement_str = g_string_new (base_statement);

  if (query->substring_list)
    statement_str = g_string_append (statement_str, from_join_statement);
  else
    statement_str = g_string_append (statement_str, from_visits_statement);

  statement_str = g_string_append (statement_str, "WHERE ");

  if (query->from >= 0)
    statement_str = g_string_append (statement_str, "visits.visit_time >= ? AND ");
  if (query->to >= 0)
    statement_str = g_string_append (statement_str, "visits.visit_time <= ? AND ");

  if (query->host > 0)
    statement_str = g_string_append (statement_str, "urls.host = ? AND ");

  for (substring = query->substring_list; substring != NULL; substring = substring->next) {
    statement_str = g_string_append (statement_str, "(urls.url LIKE ? OR urls.title LIKE ?) AND ");
  }

  statement_str = g_string_append (statement_str, "1");

  statement = ephy_sqlite_connection_create_statement (priv->history_database,
						       statement_str->str, &error);
  g_string_free (statement_str, TRUE);

  if (error) {
    g_warning ("Could not build visits table query statement: %s", error->message);
    g_error_free (error);
    return NULL;
  }

  if (query->from >= 0) {
    if (ephy_sqlite_statement_bind_int (statement, i++, (int)query->from, &error) == FALSE) {
      g_warning ("Could not build urls table query statement: %s", error->message);
      g_error_free (error);
      g_object_unref (statement);
      return NULL;
    }
  }
  if (query->to >= 0) {
    if (ephy_sqlite_statement_bind_int (statement, i++, (int)query->to, &error) == FALSE) {
      g_warning ("Could not build urls table query statement: %s", error->message);
      g_error_free (error);
      g_object_unref (statement);
      return NULL;
    }
  }
  if (query->host > 0) {
    if (ephy_sqlite_statement_bind_int (statement, i++, (int)query->host, &error) == FALSE) {
      g_warning ("Could not build urls table query statement: %s", error->message);
      g_error_free (error);
      g_object_unref (statement);
      return NULL;
    }
  }
  for (substring = query->substring_list; substring != NULL; substring = substring->next) {
    char *string = ephy_sqlite_create_match_pattern (substring->data);
    if (ephy_sqlite_statement_bind_string (statement, i++, string, &error) == FALSE) {
      g_warning ("Could not build urls table query statement: %s", error->message);
      g_error_free (error);
      g_object_unref (statement);
      g_free (string);
      return NULL;
    }
    if (ephy_sqlite_statement_bind_string (statement, i++, string + 2, &error) == FALSE) {
      g_warning ("Could not build urls table query statement: %s", error->message);
      g_error_free (error);
      g_object_unref (statement);
      g_free (string);
      return NULL;
    }
    g_free (string);
  }

  while (ephy_sqlite_statement_step (statement, &error))
    visits = g_list_prepend (visits, create_page_visit_from_statement (statement));

  visits = g_list_reverse (visits);

  if (error) {
    g_warning ("Could not execute visits table query statement: %s", error->message);
    g_error_free (error);
    g_object_unref (statement);
    ephy_history_page_visit_list_free (visits);
    return NULL;
  }

  g_object_unref (statement);
  return visits;
}
