/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include "config.h"
#include "ephy-sqlite-connection.h"

#include "ephy-lib-type-builtins.h"

#include <errno.h>
#include <glib/gstdio.h>
#include <sqlite3.h>

struct _EphySQLiteConnection {
  GObject parent_instance;

  char *database_path;
  sqlite3 *database;
  EphySQLiteConnectionMode mode;
};

G_DEFINE_FINAL_TYPE (EphySQLiteConnection, ephy_sqlite_connection, G_TYPE_OBJECT);

enum {
  PROP_0,
  PROP_MODE,
  PROP_DATABASE_PATH,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

static void
ephy_sqlite_connection_finalize (GObject *self)
{
  g_free (EPHY_SQLITE_CONNECTION (self)->database_path);
  ephy_sqlite_connection_close (EPHY_SQLITE_CONNECTION (self));
  G_OBJECT_CLASS (ephy_sqlite_connection_parent_class)->finalize (self);
}

static void
ephy_sqlite_connection_set_property (GObject      *object,
                                     guint         property_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  EphySQLiteConnection *self = EPHY_SQLITE_CONNECTION (object);

  switch (property_id) {
    case PROP_MODE:
      self->mode = g_value_get_enum (value);
      break;
    case PROP_DATABASE_PATH:
      self->database_path = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (self, property_id, pspec);
      break;
  }
}

static void
ephy_sqlite_connection_class_init (EphySQLiteConnectionClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = ephy_sqlite_connection_finalize;
  gobject_class->set_property = ephy_sqlite_connection_set_property;

  obj_properties[PROP_MODE] =
    g_param_spec_enum ("mode",
                       NULL, NULL,
                       EPHY_TYPE_SQ_LITE_CONNECTION_MODE,
                       EPHY_SQLITE_CONNECTION_MODE_READWRITE,
                       G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_DATABASE_PATH] =
    g_param_spec_string ("database-path",
                         NULL, NULL,
                         NULL,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, LAST_PROP, obj_properties);
}

static void
ephy_sqlite_connection_init (EphySQLiteConnection *self)
{
  self->database = NULL;
}

GQuark
ephy_sqlite_error_quark (void)
{
  return g_quark_from_static_string ("ephy-sqlite");
}

static void
set_error_from_string (const char  *string,
                       GError     **error)
{
  if (error)
    *error = g_error_new_literal (ephy_sqlite_error_quark (), 0, string);
}

EphySQLiteConnection *
ephy_sqlite_connection_new (EphySQLiteConnectionMode  mode,
                            const char               *database_path)
{
  return EPHY_SQLITE_CONNECTION (g_object_new (EPHY_TYPE_SQLITE_CONNECTION,
                                               "mode", mode,
                                               "database-path", database_path,
                                               NULL));
}

gboolean
ephy_sqlite_connection_open (EphySQLiteConnection  *self,
                             GError               **error)
{
  if (self->database) {
    set_error_from_string ("Connection already open.", error);
    return FALSE;
  }

  if (sqlite3_open_v2 (self->database_path,
                       &self->database,
                       self->mode == EPHY_SQLITE_CONNECTION_MODE_MEMORY ? SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_MEMORY
                                                                        : SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                       NULL) != SQLITE_OK) {
    ephy_sqlite_connection_get_error (self, error);
    ephy_sqlite_connection_close (self);
    return FALSE;
  }

  /* Create a local copy of current database for in memory usage */
  if (self->mode == EPHY_SQLITE_CONNECTION_MODE_MEMORY) {
    sqlite3 *init_db;
    int rc;

    rc = sqlite3_open_v2 (self->database_path, &init_db, SQLITE_OPEN_READONLY, 0);
    if (rc == SQLITE_OK) {
      sqlite3_backup *backup;

      backup = sqlite3_backup_init (self->database, "main", init_db, "main");
      rc = sqlite3_backup_step (backup, -1);

      if (rc != SQLITE_DONE)
        g_warning ("Failed to copy history to in-memory database: %s", sqlite3_errstr (rc));

      sqlite3_backup_finish (backup);
    }

    sqlite3_close (init_db);
  } else {
    ephy_sqlite_connection_execute (self, "PRAGMA main.journal_mode=WAL", error);
    ephy_sqlite_connection_execute (self, "PRAGMA main.synchronous=NORMAL", error);
    ephy_sqlite_connection_execute (self, "PRAGMA main.cache_size=10000", error);
  }

  return TRUE;
}

void
ephy_sqlite_connection_close (EphySQLiteConnection *self)
{
  if (self->database) {
    sqlite3_close (self->database);
    self->database = NULL;
  }
}

void
ephy_sqlite_connection_delete_database (EphySQLiteConnection *self)
{
  g_autofree char *journal = NULL;
  g_autofree char *shm = NULL;

  g_assert (EPHY_IS_SQLITE_CONNECTION (self));

  if (g_file_test (self->database_path, G_FILE_TEST_EXISTS) && g_unlink (self->database_path) == -1)
    g_warning ("Failed to delete database at %s: %s", self->database_path, g_strerror (errno));

  journal = g_strdup_printf ("%s-wal", self->database_path);
  if (g_file_test (journal, G_FILE_TEST_EXISTS) && g_unlink (journal) == -1)
    g_warning ("Failed to delete database journal at %s: %s", journal, g_strerror (errno));

  shm = g_strdup_printf ("%s-shm", self->database_path);
  if (g_file_test (shm, G_FILE_TEST_EXISTS) && g_unlink (shm) == -1)
    g_warning ("Failed to delete database shm at %s: %s", shm, g_strerror (errno));
}

void
ephy_sqlite_connection_get_error (EphySQLiteConnection  *self,
                                  GError               **error)
{
  if (error)
    *error = g_error_new_literal (ephy_sqlite_error_quark (), sqlite3_errcode (self->database), sqlite3_errmsg (self->database));
}

gboolean
ephy_sqlite_connection_execute (EphySQLiteConnection  *self,
                                const char            *sql,
                                GError               **error)
{
  if (!self->database) {
    set_error_from_string ("Connection not open.", error);
    return FALSE;
  }

  if (sqlite3_exec (self->database, sql, NULL, NULL, NULL) != SQLITE_OK) {
    ephy_sqlite_connection_get_error (self, error);
    return FALSE;
  }
  return TRUE;
}

EphySQLiteStatement *
ephy_sqlite_connection_create_statement (EphySQLiteConnection  *self,
                                         const char            *sql,
                                         GError               **error)
{
  sqlite3_stmt *prepared_statement;

  if (!self->database) {
    set_error_from_string ("Connection not open.", error);
    return NULL;
  }

  if (sqlite3_prepare_v2 (self->database, sql, -1, &prepared_statement, NULL) != SQLITE_OK) {
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
  return sqlite3_last_insert_rowid (self->database);
}

void
ephy_sqlite_connection_enable_foreign_keys (EphySQLiteConnection *self)
{
  GError *error = NULL;

  g_assert (EPHY_IS_SQLITE_CONNECTION (self));

  ephy_sqlite_connection_execute (self, "PRAGMA foreign_keys=ON", &error);
  if (error) {
    g_warning ("Failed to enable foreign keys pragma: %s", error->message);
    g_error_free (error);
  }
}

gboolean
ephy_sqlite_connection_begin_transaction (EphySQLiteConnection  *self,
                                          GError               **error)
{
  return ephy_sqlite_connection_execute (self, "BEGIN TRANSACTION", error);
}

gboolean
ephy_sqlite_connection_commit_transaction (EphySQLiteConnection  *self,
                                           GError               **error)
{
  return ephy_sqlite_connection_execute (self, "COMMIT", error);
}

gboolean
ephy_sqlite_connection_table_exists (EphySQLiteConnection *self,
                                     const char           *table_name)
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
