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
#include "ephy-sqlite-statement.h"

#include "ephy-sqlite-connection.h"
#include <sqlite3.h>

enum {
  PROP_0,
  PROP_PREPARED_STATEMENT,
  PROP_CONNECTION,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

struct _EphySQLiteStatement {
  GObject parent_instance;
  sqlite3_stmt *prepared_statement;
  EphySQLiteConnection *connection;
};

G_DEFINE_TYPE (EphySQLiteStatement, ephy_sqlite_statement, G_TYPE_OBJECT);

static void
ephy_sqlite_statement_set_property (GObject      *object,
                                    guint         property_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  EphySQLiteStatement *self = EPHY_SQLITE_STATEMENT (object);

  switch (property_id) {
    case PROP_PREPARED_STATEMENT:
      self->prepared_statement = g_value_get_pointer (value);
      break;
    case PROP_CONNECTION:
      self->connection = EPHY_SQLITE_CONNECTION (g_object_ref (g_value_get_object (value)));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (self, property_id, pspec);
      break;
  }
}

static void
ephy_sqlite_statement_finalize (GObject *object)
{
  EphySQLiteStatement *self = EPHY_SQLITE_STATEMENT (object);

  if (self->prepared_statement) {
    sqlite3_finalize (self->prepared_statement);
    self->prepared_statement = NULL;
  }

  if (self->connection) {
    g_object_unref (self->connection);
    self->connection = NULL;
  }

  G_OBJECT_CLASS (ephy_sqlite_statement_parent_class)->finalize (object);
}

static void
ephy_sqlite_statement_class_init (EphySQLiteStatementClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = ephy_sqlite_statement_finalize;
  gobject_class->set_property = ephy_sqlite_statement_set_property;

  obj_properties[PROP_PREPARED_STATEMENT] =
    g_param_spec_pointer ("prepared-statement",
                          "Prepared statement",
                          "The statement's backing SQLite prepared statement",
                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_CONNECTION] =
    g_param_spec_object ("connection",
                         "Connection",
                         "The statement's backing SQLite connection",
                         EPHY_TYPE_SQLITE_CONNECTION,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, LAST_PROP, obj_properties);
}

static void
ephy_sqlite_statement_init (EphySQLiteStatement *self)
{
  self->prepared_statement = NULL;
  self->connection = NULL;
}

gboolean
ephy_sqlite_statement_bind_null (EphySQLiteStatement  *self,
                                 int                   column,
                                 GError              **error)
{
  if (sqlite3_bind_null (self->prepared_statement, column) != SQLITE_OK) {
    ephy_sqlite_connection_get_error (self->connection, error);
    return FALSE;
  }

  return TRUE;
}

gboolean
ephy_sqlite_statement_bind_boolean (EphySQLiteStatement  *self,
                                    int                   column,
                                    gboolean              value,
                                    GError              **error)
{
  if (sqlite3_bind_int (self->prepared_statement, column + 1, value ? 1 : 0) != SQLITE_OK) {
    ephy_sqlite_connection_get_error (self->connection, error);
    return FALSE;
  }

  return TRUE;
}

gboolean
ephy_sqlite_statement_bind_int (EphySQLiteStatement  *self,
                                int                   column,
                                int                   value,
                                GError              **error)
{
  if (sqlite3_bind_int (self->prepared_statement, column + 1, value) != SQLITE_OK) {
    ephy_sqlite_connection_get_error (self->connection, error);
    return FALSE;
  }

  return TRUE;
}

gboolean
ephy_sqlite_statement_bind_int64 (EphySQLiteStatement  *self,
                                  int                   column,
                                  gint64                value,
                                  GError              **error)
{
  if (sqlite3_bind_int64 (self->prepared_statement, column + 1, value) != SQLITE_OK) {
    ephy_sqlite_connection_get_error (self->connection, error);
    return FALSE;
  }

  return TRUE;
}

gboolean
ephy_sqlite_statement_bind_double (EphySQLiteStatement  *self,
                                   int                   column,
                                   double                value,
                                   GError              **error)
{
  if (sqlite3_bind_double (self->prepared_statement, column + 1, value) != SQLITE_OK) {
    ephy_sqlite_connection_get_error (self->connection, error);
    return FALSE;
  }

  return TRUE;
}

gboolean
ephy_sqlite_statement_bind_string (EphySQLiteStatement  *self,
                                   int                   column,
                                   const char           *value,
                                   GError              **error)
{
  if (sqlite3_bind_text (self->prepared_statement, column + 1, value, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
    ephy_sqlite_connection_get_error (self->connection, error);
    return FALSE;
  }

  return TRUE;
}

gboolean
ephy_sqlite_statement_bind_blob (EphySQLiteStatement  *self,
                                 int                   column,
                                 const void           *value,
                                 int                   length,
                                 GError              **error)
{
  if (sqlite3_bind_blob (self->prepared_statement, column + 1, value, length, SQLITE_TRANSIENT) != SQLITE_OK) {
    ephy_sqlite_connection_get_error (self->connection, error);
    return FALSE;
  }
  return TRUE;
}

gboolean
ephy_sqlite_statement_step (EphySQLiteStatement  *self,
                            GError              **error)
{
  int error_code = sqlite3_step (self->prepared_statement);
  if (error_code != SQLITE_OK && error_code != SQLITE_ROW && error_code != SQLITE_DONE) {
    ephy_sqlite_connection_get_error (self->connection, error);
  }

  return error_code == SQLITE_ROW;
}

void
ephy_sqlite_statement_reset (EphySQLiteStatement *self)
{
  sqlite3_reset (self->prepared_statement);
}

int
ephy_sqlite_statement_get_column_count (EphySQLiteStatement *self)
{
  return sqlite3_column_count (self->prepared_statement);
}

EphySQLiteColumnType
ephy_sqlite_statement_get_column_type (EphySQLiteStatement *self,
                                       int                  column)
{
  int column_type = sqlite3_column_type (self->prepared_statement, column);
  switch (column_type) {
    case SQLITE_INTEGER:
      return EPHY_SQLITE_COLUMN_TYPE_INT;
    case SQLITE_FLOAT:
      return EPHY_SQLITE_COLUMN_TYPE_FLOAT;
    case SQLITE_TEXT:
      return EPHY_SQLITE_COLUMN_TYPE_STRING;
    case SQLITE_BLOB:
      return EPHY_SQLITE_COLUMN_TYPE_BLOB;
    case SQLITE_NULL:
    default:
      return EPHY_SQLITE_COLUMN_TYPE_NULL;
  }
}

int
ephy_sqlite_statement_get_column_size (EphySQLiteStatement *self,
                                       int                  column)
{
  return sqlite3_column_bytes (self->prepared_statement, column);
}

int
ephy_sqlite_statement_get_column_as_boolean (EphySQLiteStatement *self,
                                             int                  column)
{
  return ephy_sqlite_statement_get_column_as_int (self, column);
}

int
ephy_sqlite_statement_get_column_as_int (EphySQLiteStatement *self,
                                         int                  column)
{
  return sqlite3_column_int (self->prepared_statement, column);
}

gint64
ephy_sqlite_statement_get_column_as_int64 (EphySQLiteStatement *self,
                                           int                  column)
{
  return sqlite3_column_int64 (self->prepared_statement, column);
}

double
ephy_sqlite_statement_get_column_as_double (EphySQLiteStatement *self,
                                            int                  column)
{
  return sqlite3_column_double (self->prepared_statement, column);
}

const char *
ephy_sqlite_statement_get_column_as_string (EphySQLiteStatement *self,
                                            int                  column)
{
  return (const char *)sqlite3_column_text (self->prepared_statement, column);
}

const void *
ephy_sqlite_statement_get_column_as_blob (EphySQLiteStatement *self,
                                          int                  column)
{
  return sqlite3_column_blob (self->prepared_statement, column);
}

char *
ephy_sqlite_create_match_pattern (const char *match_string)
{
  char *string, *pattern;

  string = g_strndup (match_string, EPHY_SQLITE_LIMIT_LIKE_PATTERN_LENGTH - 2);
  pattern = g_strdup_printf ("%%:%%%s%%", string);
  g_free (string);

  return pattern;
}
