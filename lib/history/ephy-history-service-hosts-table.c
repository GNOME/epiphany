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
#include "ephy-string.h"
#include <glib/gi18n.h>

gboolean
ephy_history_service_initialize_hosts_table (EphyHistoryService *self)
{
  EphyHistoryServicePrivate *priv = EPHY_HISTORY_SERVICE (self)->priv;
  GError *error = NULL;

  if (ephy_sqlite_connection_table_exists (priv->history_database, "hosts")) {
    return TRUE;
  }
  ephy_sqlite_connection_execute (priv->history_database,
    "CREATE TABLE hosts ("
    "id INTEGER PRIMARY KEY,"
    "url LONGVARCAR,"
    "title LONGVARCAR,"
    "visit_count INTEGER DEFAULT 0 NOT NULL,"
    "zoom_level REAL DEFAULT 1.0)", &error);

  if (error) {
    g_warning ("Could not create hosts table: %s", error->message);
    g_error_free (error);
    return FALSE;
  }
  ephy_history_service_schedule_commit (self);
  return TRUE;
}

void
ephy_history_service_add_host_row (EphyHistoryService *self, EphyHistoryHost *host)
{
  EphyHistoryServicePrivate *priv = EPHY_HISTORY_SERVICE (self)->priv;
  EphySQLiteStatement *statement = NULL;
  GError *error = NULL;

  g_assert (priv->history_thread == g_thread_self ());
  g_assert (priv->history_database != NULL);

  statement = ephy_sqlite_connection_create_statement (priv->history_database,
    "INSERT INTO hosts (url, title, visit_count, zoom_level) "
    "VALUES (?, ?, ?, ?)", &error);

  if (error) {
    g_warning ("Could not build hosts table addition statement: %s", error->message);
    g_error_free (error);
    return;
  }

  if (ephy_sqlite_statement_bind_string (statement, 0, host->url, &error) == FALSE ||
      ephy_sqlite_statement_bind_string (statement, 1, host->title, &error) == FALSE ||
      ephy_sqlite_statement_bind_int (statement, 2, host->visit_count, &error) == FALSE ||
      ephy_sqlite_statement_bind_double (statement, 3, host->zoom_level, &error) == FALSE) {
    g_warning ("Could not insert host into hosts table: %s", error->message);
    g_error_free (error);
    g_object_unref (statement);
    return;
  }

  ephy_sqlite_statement_step (statement, &error);
  if (error) {
    g_warning ("Could not insert host into hosts table: %s", error->message);
    g_error_free (error);
  } else {
    host->id = ephy_sqlite_connection_get_last_insert_id (priv->history_database);
  }

  g_object_unref (statement);
}

void
ephy_history_service_update_host_row (EphyHistoryService *self, EphyHistoryHost *host)
{
  EphyHistoryServicePrivate *priv = EPHY_HISTORY_SERVICE (self)->priv;
  EphySQLiteStatement *statement;
  GError *error = NULL;

  g_assert (priv->history_thread == g_thread_self ());
  g_assert (priv->history_database != NULL);

  statement = ephy_sqlite_connection_create_statement (priv->history_database,
    "UPDATE hosts SET url=?, title=?, visit_count=?, zoom_level=?"
    "WHERE id=?", &error);
  if (error) {
    g_warning ("Could not build hosts table modification statement: %s", error->message);
    g_error_free (error);
    return;
  }

  if (ephy_sqlite_statement_bind_string (statement, 0, host->url, &error) == FALSE ||
      ephy_sqlite_statement_bind_string (statement, 1, host->title, &error) == FALSE ||
      ephy_sqlite_statement_bind_int (statement, 2, host->visit_count, &error) == FALSE ||
      ephy_sqlite_statement_bind_double (statement, 3, host->zoom_level, &error) == FALSE ||
      ephy_sqlite_statement_bind_int (statement, 4, host->id, &error) == FALSE) {
    g_warning ("Could not modify host in hosts table: %s", error->message);
    g_error_free (error);
    g_object_unref (statement);
    return;
  }

  ephy_sqlite_statement_step (statement, &error);
  if (error) {
    g_warning ("Could not modify URL in urls table: %s", error->message);
    g_error_free (error);
  }
  g_object_unref (statement);
}

EphyHistoryHost*
ephy_history_service_get_host_row (EphyHistoryService *self, const gchar *host_string, EphyHistoryHost *host)
{
  EphyHistoryServicePrivate *priv = EPHY_HISTORY_SERVICE (self)->priv;
  EphySQLiteStatement *statement = NULL;
  GError *error = NULL;

  g_assert (priv->history_thread == g_thread_self ());
  g_assert (priv->history_database != NULL);

  if (host_string == NULL && host != NULL)
    host_string = host->url;

  g_assert (host_string || host->id !=-1);

  if (host != NULL && host->id != -1) {
    statement = ephy_sqlite_connection_create_statement (priv->history_database,
        "SELECT id, url, title, visit_count, zoom_level FROM hosts "
        "WHERE id=?", &error);
  } else {
    statement = ephy_sqlite_connection_create_statement (priv->history_database,
        "SELECT id, url, title, visit_count, zoom_level FROM hosts "
        "WHERE url=?", &error);
  }

  if (error) {
    g_warning ("Could not build hosts query statement: %s", error->message);
    g_error_free (error);
    return NULL;
  }

  if (host != NULL && host->id != -1)
    ephy_sqlite_statement_bind_int (statement, 0, host->id, &error);
  else
    ephy_sqlite_statement_bind_string (statement, 0, host_string, &error);

  if (error) {
    g_warning ("Could not build hosts table query statement: %s", error->message);
    g_error_free (error);
    g_object_unref (statement);
    return NULL;
  }

  if (ephy_sqlite_statement_step (statement, &error) == FALSE) {
    if (error)
      g_error_free (error);
    g_object_unref (statement);
    return NULL;
  }

  if (host == NULL) {
    host = ephy_history_host_new (NULL, NULL, 0, 1.0);
  } else {
    if (host->url)
      g_free (host->url);
    if (host->title)
      g_free (host->title);
  }

  host->id = ephy_sqlite_statement_get_column_as_int (statement, 0);
  host->url = g_strdup (ephy_sqlite_statement_get_column_as_string (statement, 1));
  host->title = g_strdup (ephy_sqlite_statement_get_column_as_string (statement, 2));
  host->visit_count = ephy_sqlite_statement_get_column_as_int (statement, 3);
  host->zoom_level = ephy_sqlite_statement_get_column_as_double (statement, 4);

  g_object_unref (statement);
  return host;
}

static EphyHistoryHost*
create_host_from_statement (EphySQLiteStatement *statement)
{
  EphyHistoryHost *host =
    ephy_history_host_new (ephy_sqlite_statement_get_column_as_string (statement, 1),
                           ephy_sqlite_statement_get_column_as_string (statement, 2),
                           ephy_sqlite_statement_get_column_as_int (statement, 3),
                           ephy_sqlite_statement_get_column_as_double (statement, 4));
  host->id = ephy_sqlite_statement_get_column_as_int (statement, 0);

  return host;
}

GList*
ephy_history_service_get_all_hosts (EphyHistoryService *self)
{
  EphyHistoryServicePrivate *priv = EPHY_HISTORY_SERVICE (self)->priv;
  EphySQLiteStatement *statement = NULL;
  GList *hosts = NULL;
  GError *error = NULL;

  g_assert (priv->history_thread == g_thread_self ());
  g_assert (priv->history_database != NULL);

  statement = ephy_sqlite_connection_create_statement (priv->history_database,
      "SELECT id, url, title, visit_count, zoom_level FROM hosts", &error);

  if (error) {
    g_warning ("Could not build hosts query statement: %s", error->message);
    g_error_free (error);
    return NULL;
  }

  while (ephy_sqlite_statement_step (statement, &error))
    hosts = g_list_prepend (hosts, create_host_from_statement (statement));

  hosts = g_list_reverse (hosts);

  if (error) {
    g_warning ("Could not execute hosts table query statement: %s", error->message);
    g_error_free (error);
  }
  g_object_unref (statement);

  return hosts;
}

GList*
ephy_history_service_find_host_rows (EphyHistoryService *self, EphyHistoryQuery *query)
{
  EphyHistoryServicePrivate *priv = EPHY_HISTORY_SERVICE (self)->priv;
  EphySQLiteStatement *statement = NULL;
  GList *substring;
  GString *statement_str;
  GList *hosts = NULL;
  GError *error = NULL;
  const char *base_statement = ""
    "SELECT "
      "DISTINCT hosts.id, "
      "hosts.url, "
      "hosts.title, "
      "hosts.visit_count, "
      "hosts.zoom_level "
    "FROM "
      "hosts ";

  int i = 0;

  g_assert (priv->history_thread == g_thread_self ());
  g_assert (priv->history_database != NULL);

  statement_str = g_string_new (base_statement);

  /* In either of these cases we need to at least join with the urls table. */
  if (query->substring_list || query->from > 0 || query->to > 0)
    statement_str = g_string_append (statement_str,  "JOIN urls on hosts.id = urls.host ");

  /* In these cases, we additionally need to join with the visits table. */
  if (query->from > 0 || query->to > 0) {
    statement_str = g_string_append (statement_str, "JOIN visits on urls.id = visits.url WHERE ");
    if (query->from > 0)
      statement_str = g_string_append (statement_str, "visits.visit_time >= ? AND ");
    if (query->to > 0)
      statement_str = g_string_append (statement_str, "visits.visit_time <= ? AND ");
  } else {
    statement_str = g_string_append (statement_str, "WHERE ");
  }

  for (substring = query->substring_list; substring != NULL; substring = substring->next)
    statement_str = g_string_append (statement_str, "(hosts.url LIKE ? OR hosts.title LIKE ? OR "
                                     "urls.url LIKE ? OR urls.title LIKE ?) AND ");

  statement_str = g_string_append (statement_str, "1 ");

  statement = ephy_sqlite_connection_create_statement (priv->history_database,
						       statement_str->str, &error);
  g_string_free (statement_str, TRUE);

  if (error) {
    g_warning ("Could not build hosts table query statement: %s", error->message);
    g_error_free (error);
    return NULL;
  }
  if (query->from > 0) {
    if (ephy_sqlite_statement_bind_int (statement, i++, (int)query->from, &error) == FALSE) {
      g_warning ("Could not build hosts table query statement: %s", error->message);
      g_error_free (error);
      g_object_unref (statement);
      return NULL;
    }
  }
  if (query->to > 0) {
    if (ephy_sqlite_statement_bind_int (statement, i++, (int)query->to, &error) == FALSE) {
      g_warning ("Could not build hosts table query statement: %s", error->message);
      g_error_free (error);
      g_object_unref (statement);
      return NULL;
    }
  }
  for (substring = query->substring_list; substring != NULL; substring = substring->next) {
    int j = 4;
    char *string = ephy_sqlite_create_match_pattern (substring->data);
    while (j--)
      /* The bitwise operation ensures we only skip two characters for titles. */
      if (ephy_sqlite_statement_bind_string (statement, i++, string + 2*((j+1) & 1), &error) == FALSE) {
        g_warning ("Could not build hosts table query statement: %s", error->message);
        g_error_free (error);
        g_object_unref (statement);
        g_free (string);
        return NULL;
      }
    g_free (string);
  }

  while (ephy_sqlite_statement_step (statement, &error))
    hosts = g_list_prepend (hosts, create_host_from_statement (statement));

  hosts = g_list_reverse (hosts);

  if (error) {
    g_warning ("Could not execute hosts table query statement: %s", error->message);
    g_error_free (error);
  }
  g_object_unref (statement);

  return hosts;
}

/* Inspired from ephy-history.c */
static GList *
get_hostname_and_locations (const gchar *url, gchar **hostname)
{
	GList *host_locations = NULL;
	char *scheme = NULL;

  if (url) {
		scheme = g_uri_parse_scheme (url);
		*hostname = ephy_string_get_host_name (url);
	}
  /* Build an host name */
  if (scheme == NULL || *hostname == NULL) {
    *hostname = g_strdup (_("Others"));
    host_locations = g_list_append (host_locations,
                                    g_strdup ("about:blank"));
  }  else if (strcmp (scheme, "file") == 0) {
    *hostname = g_strdup (_("Local files"));
    host_locations = g_list_append (host_locations,
                                    g_strdup ("file:///"));
  } else {
    char *location;
    char *tmp;

    if (g_str_equal (scheme, "https")) {
      /* If scheme is https, we still fake http. */
      location = g_strconcat ("http://", *hostname, "/", NULL);
      host_locations = g_list_append (host_locations, location);
    }

    /* We append the real address */
    location = g_strconcat (scheme,
                            "://", *hostname, "/", NULL);
    host_locations = g_list_append (host_locations, location);

    /* and also a fake www-modified address if it's http or https. */
    if (g_str_has_prefix (scheme, "http")) {
      if (g_str_has_prefix (*hostname, "www."))
        tmp = g_strdup (*hostname + 4);
      else
        tmp = g_strconcat ("www.", *hostname, NULL);
      location = g_strconcat ("http://", tmp, "/", NULL);
      g_free (tmp);
      host_locations = g_list_append (host_locations, location);
    }
  }
  g_free (scheme);

  return host_locations;
}

EphyHistoryHost*
ephy_history_service_get_host_row_from_url (EphyHistoryService *self,
                                            const gchar *url)
{
  GList *host_locations, *l;
  char *hostname;
  EphyHistoryHost *host = NULL;

  host_locations = get_hostname_and_locations (url, &hostname);

  for (l = host_locations; l != NULL; l = l->next) {
    host = ephy_history_service_get_host_row (self, l->data, NULL);
    if (host != NULL)
      break;
  }

  if (host == NULL) {
    host = ephy_history_host_new (host_locations->data, hostname, 0, 1.0);
    ephy_history_service_add_host_row (self, host);
  }

  g_free (hostname);
  g_list_free_full (host_locations, (GDestroyNotify) g_free);

  return host;
}

void
ephy_history_service_delete_host_row (EphyHistoryService *self,
                                      EphyHistoryHost *host)
{
  EphyHistoryServicePrivate *priv = EPHY_HISTORY_SERVICE (self)->priv;
  EphySQLiteStatement *statement = NULL;
  gchar *sql_statement;
  GError *error = NULL;

  g_assert (priv->history_thread == g_thread_self ());
  g_assert (priv->history_database != NULL);

  g_assert (host->id != -1 || host->url);

  if (host->id != -1)
    sql_statement = g_strdup ("DELETE FROM hosts WHERE id=?");
  else
    sql_statement = g_strdup ("DELETE FROM hosts WHERE url=?");

  statement = ephy_sqlite_connection_create_statement (priv->history_database,
                                                       sql_statement, &error);
  g_free (sql_statement);

  if (error) {
    g_warning ("Could not build urls table query statement: %s", error->message);
    g_error_free (error);
    return;
  }

  if (host->id != -1)
    ephy_sqlite_statement_bind_int (statement, 0, host->id, &error);
  else
    ephy_sqlite_statement_bind_string (statement, 0, host->url, &error);

  if (error) {
    g_warning ("Could not build hosts table query statement: %s", error->message);
    g_error_free (error);
    g_object_unref (statement);
    return;
  }

  ephy_sqlite_statement_step (statement, &error);
  if (error) {
    g_warning ("Could not modify host in hosts table: %s", error->message);
    g_error_free (error);
  }
  g_object_unref (statement);
}

void
ephy_history_service_delete_orphan_hosts (EphyHistoryService *self)
{
  EphyHistoryServicePrivate *priv = EPHY_HISTORY_SERVICE (self)->priv;
  GError *error = NULL;

  g_assert (priv->history_thread == g_thread_self ());
  g_assert (priv->history_database != NULL);

  /* Where a JOIN would give us all hosts with urls associated, a LEFT
     JOIN also gives us those hosts for which there are no urls.  By
     means of urls.host == NULL we filter out anything else and
     retrieve only the ids of the hosts without associated urls. Then,
     we delete all these rows from the hosts table. */
  ephy_sqlite_connection_execute (priv->history_database,
                                  "DELETE FROM hosts WHERE hosts.id IN "
                                  "  (SELECT hosts.id FROM hosts LEFT JOIN urls "
                                  "    ON hosts.id = urls.host WHERE urls.host is NULL);",
                                  &error);
  if (error) {
    g_warning ("Couldn't remove orphan hosts from database: %s", error->message);
    g_error_free (error);
  }
}
