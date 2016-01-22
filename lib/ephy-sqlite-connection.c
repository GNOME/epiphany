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
#include "ephy-sqlite-connection.h"

#include <sqlite3.h>

struct _EphySQLiteConnectionPrivate {
  sqlite3 *database;
};

#define EPHY_SQLITE_CONNECTION_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE((o), EPHY_TYPE_SQLITE_CONNECTION, EphySQLiteConnectionPrivate))

G_DEFINE_TYPE (EphySQLiteConnection, ephy_sqlite_connection, G_TYPE_OBJECT);

static void
ephy_sqlite_connection_finalize (GObject *self)
{
  ephy_sqlite_connection_close (EPHY_SQLITE_CONNECTION (self));
  G_OBJECT_CLASS (ephy_sqlite_connection_parent_class)->finalize (self);
}

static void
ephy_sqlite_connection_class_init (EphySQLiteConnectionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = ephy_sqlite_connection_finalize;
  g_type_class_add_private (object_class, sizeof (EphySQLiteConnectionPrivate));
}

static void
ephy_sqlite_connection_init (EphySQLiteConnection *self)
{
  self->priv = EPHY_SQLITE_CONNECTION_GET_PRIVATE (self);
  self->priv->database = NULL;
}

static GQuark get_ephy_sqlite_quark (void)
{
  return g_quark_from_static_string ("ephy-sqlite");
}

static void
set_error_from_string (const char* string, GError **error)
{
  if (error)
    *error = g_error_new (get_ephy_sqlite_quark (), 0, string, 0);
}

EphySQLiteConnection *
ephy_sqlite_connection_new ()
{
  return EPHY_SQLITE_CONNECTION (g_object_new (EPHY_TYPE_SQLITE_CONNECTION, NULL));
}

gboolean
ephy_sqlite_connection_open (EphySQLiteConnection *self, const gchar *filename, GError **error)
{
  EphySQLiteConnectionPrivate *priv = self->priv;

  if (priv->database) {
    set_error_from_string ("Connection already open.", error);
    return FALSE;
  }
  
  if (sqlite3_open (filename, &priv->database) != SQLITE_OK) {
    ephy_sqlite_connection_get_error (self, error);
    priv->database = NULL;
    return FALSE;
  }

  return TRUE;
}

void
ephy_sqlite_connection_close (EphySQLiteConnection *self)
{
  EphySQLiteConnectionPrivate *priv = self->priv;
  if (priv->database) {
    sqlite3_close (priv->database);
    priv->database = NULL;
  }
}

void
ephy_sqlite_connection_get_error (EphySQLiteConnection *self, GError **error)
{
  if (error)
    *error = g_error_new (get_ephy_sqlite_quark (), 0, sqlite3_errmsg (self->priv->database), 0);
}

gboolean
ephy_sqlite_connection_execute (EphySQLiteConnection *self, const char *sql, GError **error)
{
  EphySQLiteConnectionPrivate *priv = self->priv;
  
  if (priv->database == NULL) {
    set_error_from_string ("Connection not open.", error);
    return FALSE;
  }
  
  return sqlite3_exec (priv->database, sql, NULL, NULL, NULL) == SQLITE_OK;
}

EphySQLiteStatement *
ephy_sqlite_connection_create_statement (EphySQLiteConnection *self, const char *sql, GError **error)
{
  EphySQLiteConnectionPrivate *priv = self->priv;
  sqlite3_stmt *prepared_statement;

  if (priv->database == NULL) {
    set_error_from_string ("Connection not open.", error);
    return NULL;
  }

  if (sqlite3_prepare_v2 (priv->database, sql, -1, &prepared_statement, NULL) != SQLITE_OK) {
    ephy_sqlite_connection_get_error (self, error);
    return NULL;
  }

  return EPHY_SQLITE_STATEMENT (g_object_new (EPHY_TYPE_SQLITE_STATEMENT,
                                              "prepared-statement", prepared_statement,
                                              "connection", self,
                                              NULL));
}

gint64
ephy_sqlite_connection_get_last_insert_id (EphySQLiteConnection *self)
{
  return sqlite3_last_insert_rowid (self->priv->database);
}

gboolean
ephy_sqlite_connection_begin_transaction (EphySQLiteConnection *self, GError **error)
{
  return ephy_sqlite_connection_execute (self, "BEGIN TRANSACTION", error);
}

gboolean
ephy_sqlite_connection_rollback_transaction (EphySQLiteConnection *self, GError **error)
{
  return ephy_sqlite_connection_execute (self, "ROLLBACK", error);
}

gboolean
ephy_sqlite_connection_commit_transaction (EphySQLiteConnection *self, GError **error)
{
  return ephy_sqlite_connection_execute (self, "COMMIT", error);
}

gboolean
ephy_sqlite_connection_table_exists (EphySQLiteConnection *self, const char *table_name)
{
  GError *error = NULL;
  gboolean table_exists = FALSE;

  EphySQLiteStatement *statement = ephy_sqlite_connection_create_statement (self,
    "SELECT COUNT(type) FROM sqlite_master WHERE type='table' and name=?", &error);
  if (error) {
    g_warning ("Could not detect table existence: %s", error->message);
    g_error_free (error);
    return FALSE;
  }

  ephy_sqlite_statement_bind_string (statement, 0, table_name, &error);
  if (error) {
    g_object_unref (statement);
    g_warning ("Could not detect table existence: %s", error->message);
    g_error_free (error);
    return FALSE;
  }

  ephy_sqlite_statement_step (statement, &error);
  if (error) {
    g_object_unref (statement);
    g_warning ("Could not detect table existence: %s", error->message);
    g_error_free (error);
    return FALSE;
  }

  table_exists = ephy_sqlite_statement_get_column_as_int (statement, 0);
  g_object_unref (statement);
  return table_exists;
}
