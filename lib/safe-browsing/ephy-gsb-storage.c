/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2017 Gabriel Ivascu <gabrielivascu@gnome.org>
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
#include "ephy-gsb-storage.h"

#include "ephy-debug.h"
#include "ephy-sqlite-connection.h"

#include <errno.h>
#include <glib/gstdio.h>

/* Update this if you modify the database table structure. */
#define SCHEMA_VERSION "1.0"

struct _EphyGSBStorage {
  GObject parent_instance;

  char *db_path;
  EphySQLiteConnection *db;

  gboolean is_operable;
};

G_DEFINE_TYPE (EphyGSBStorage, ephy_gsb_storage, G_TYPE_OBJECT);

enum {
  PROP_0,
  PROP_DB_PATH,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

static gboolean
ephy_gsb_storage_init_metadata_table (EphyGSBStorage *self)
{
  GError *error = NULL;
  const char *sql;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (EPHY_IS_SQLITE_CONNECTION (self->db));

  if (ephy_sqlite_connection_table_exists (self->db, "metadata"))
    return TRUE;

  sql = "CREATE TABLE metadata ("
        "name LONGVARCHAR NOT NULL PRIMARY KEY,"
        "value LONGVARCHAR NOT NULL"
        ")";
  ephy_sqlite_connection_execute (self->db, sql, &error);
  if (error) {
    g_warning ("Failed to create metadata table: %s", error->message);
    g_error_free (error);
    return FALSE;
  }

  sql = "INSERT INTO metadata (name, value) VALUES"
        "('schema_version', '"SCHEMA_VERSION"'),"
        "('next_update_at', strftime('%s', 'now'))";
  ephy_sqlite_connection_execute (self->db, sql, &error);
  if (error) {
    g_warning ("Failed to insert initial data into metadata table: %s", error->message);
    g_error_free (error);
    return FALSE;
  }

  return TRUE;
}

static gboolean
ephy_gsb_storage_init_threats_table (EphyGSBStorage *self)
{
  GError *error = NULL;
  const char *sql;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (EPHY_IS_SQLITE_CONNECTION (self->db));

  if (ephy_sqlite_connection_table_exists (self->db, "threats"))
    return TRUE;

  sql = "CREATE TABLE threats ("
        "threat_type LONGVARCHAR NOT NULL,"
        "platform_type LONGVARCHAR NOT NULL,"
        "threat_entry_type LONGVARCHAR NOT NULL,"
        "client_state LONGVARCHAR,"
        "timestamp INTEGER NOT NULL DEFAULT (CAST(strftime('%s', 'now') AS INT)),"
        "PRIMARY KEY (threat_type, platform_type, threat_entry_type)"
        ")";
  ephy_sqlite_connection_execute (self->db, sql, &error);
  if (error) {
    g_warning ("Failed to create threats table: %s", error->message);
    g_error_free (error);
    return FALSE;
  }

  return TRUE;
}

static gboolean
ephy_gsb_storage_init_hash_prefix_table (EphyGSBStorage *self)
{
  GError *error = NULL;
  const char *sql;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (EPHY_IS_SQLITE_CONNECTION (self->db));

  if (ephy_sqlite_connection_table_exists (self->db, "hash_prefix"))
    return TRUE;

  sql = "CREATE TABLE hash_prefix ("
        "cue BLOB NOT NULL,"    /* The first 4 bytes. */
        "value BLOB NOT NULL,"  /* The prefix itself, can vary from 4 bytes to 32 bytes. */
        "threat_type LONGVARCHAR NOT NULL,"
        "platform_type LONGVARCHAR NOT NULL,"
        "threat_entry_type LONGVARCHAR NOT NULL,"
        "timestamp INTEGER NOT NULL DEFAULT (CAST(strftime('%s', 'now') AS INT)),"
        "negative_expires_at INTEGER NOT NULL DEFAULT (CAST(strftime('%s', 'now') AS INT)),"
        "PRIMARY KEY (value, threat_type, platform_type, threat_entry_type),"
        "FOREIGN KEY(threat_type, platform_type, threat_entry_type)"
        "   REFERENCES threats(threat_type, platform_type, threat_entry_type)"
        "   ON DELETE CASCADE"
        ")";
  ephy_sqlite_connection_execute (self->db, sql, &error);
  if (error) {
    g_warning ("Failed to create hash_prefix table: %s", error->message);
    g_error_free (error);
    return FALSE;
  }

  return TRUE;
}

static gboolean
ephy_gsb_storage_init_hash_full_table (EphyGSBStorage *self)
{
  GError *error = NULL;
  const char *sql;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (EPHY_IS_SQLITE_CONNECTION (self->db));

  if (ephy_sqlite_connection_table_exists (self->db, "hash_full"))
    return TRUE;

  sql = "CREATE TABLE hash_full ("
        "value BLOB NOT NULL,"  /* The 32 bytes full hash. */
        "threat_type LONGVARCHAR NOT NULL,"
        "platform_type LONGVARCHAR NOT NULL,"
        "threat_entry_type LONGVARCHAR NOT NULL,"
        "timestamp INTEGER NOT NULL DEFAULT (CAST(strftime('%s', 'now') AS INT)),"
        "expires_at INTEGER NOT NULL DEFAULT (CAST(strftime('%s', 'now') AS INT)),"
        "PRIMARY KEY (value, threat_type, platform_type, threat_entry_type)"
        ")";
  ephy_sqlite_connection_execute (self->db, sql, &error);
  if (error) {
    g_warning ("Failed to create hash_full table: %s", error->message);
    g_error_free (error);
    return FALSE;
  }

  return TRUE;
}

static void
ephy_gsb_storage_close_db (EphyGSBStorage *self)
{
  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (EPHY_IS_SQLITE_CONNECTION (self->db));

  ephy_sqlite_connection_close (self->db);
  g_clear_object (&self->db);
}

static gboolean
ephy_gsb_storage_open_db (EphyGSBStorage *self)
{
  GError *error = NULL;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (!self->db);

  self->db = ephy_sqlite_connection_new (EPHY_SQLITE_CONNECTION_MODE_READWRITE);
  ephy_sqlite_connection_open (self->db, self->db_path, &error);
  if (error) {
    g_warning ("Failed to open GSB database at %s: %s", self->db_path, error->message);
    goto out_err;
  }

  /* Enable foreign keys. */
  ephy_sqlite_connection_execute (self->db, "PRAGMA foreign_keys = ON", &error);
  if (error) {
    g_warning ("Failed to enable foreign keys pragma: %s", error->message);
    goto out_err;
  }

  return TRUE;

out_err:
  g_clear_object (&self->db);
  g_error_free (error);
  return FALSE;
}

static void
ephy_gsb_storage_delete_db (EphyGSBStorage *self)
{
  char *journal;

  g_assert (EPHY_IS_GSB_STORAGE (self));

  if (g_unlink (self->db_path) == -1 && errno != ENOENT)
    g_warning ("Failed to delete GSB database at %s: %s", self->db_path, g_strerror (errno));

  journal = g_strdup_printf ("%s-journal", self->db_path);
  if (g_unlink (journal) == -1 && errno != ENOENT)
    g_warning ("Failed to delete GSB database journal at %s: %s", journal, g_strerror (errno));

  g_free (journal);
}

static gboolean
ephy_gsb_storage_init_db (EphyGSBStorage *self)
{
  gboolean success;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (!self->db);

  if (!ephy_gsb_storage_open_db (self))
    return FALSE;

  success = ephy_gsb_storage_init_metadata_table (self) &&
            ephy_gsb_storage_init_threats_table (self) &&
            ephy_gsb_storage_init_hash_prefix_table (self) &&
            ephy_gsb_storage_init_hash_full_table (self);

  if (!success) {
    ephy_gsb_storage_close_db (self);
    ephy_gsb_storage_delete_db (self);
  }

  return success;
}

static gboolean
ephy_gsb_storage_check_schema_version (EphyGSBStorage *self)
{
  EphySQLiteStatement *statement = NULL;
  GError *error = NULL;
  gboolean success;
  const char *schema_version;
  const char *sql;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (EPHY_IS_SQLITE_CONNECTION (self->db));

  sql = "SELECT value FROM metadata WHERE name='schema_version'";
  statement = ephy_sqlite_connection_create_statement (self->db, sql, &error);
  if (error) {
    g_warning ("Failed to build select schema version statement: %s", error->message);
    g_error_free (error);
    return FALSE;
  }

  ephy_sqlite_statement_step (statement, &error);
  if (error) {
    g_warning ("Failed to retrieve schema version: %s", error->message);
    g_error_free (error);
    g_object_unref (statement);
    return FALSE;
  }

  schema_version = ephy_sqlite_statement_get_column_as_string (statement, 0);
  success = g_strcmp0 (schema_version, SCHEMA_VERSION) == 0;

  g_object_unref (statement);

  return success;
}

static void
ephy_gsb_storage_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  EphyGSBStorage *self = EPHY_GSB_STORAGE (object);

  switch (prop_id) {
    case PROP_DB_PATH:
      g_free (self->db_path);
      self->db_path = g_strdup (g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_gsb_storage_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  EphyGSBStorage *self = EPHY_GSB_STORAGE (object);

  switch (prop_id) {
    case PROP_DB_PATH:
      g_value_set_string (value, self->db_path);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_gsb_storage_finalize (GObject *object)
{
  EphyGSBStorage *self = EPHY_GSB_STORAGE (object);

  g_free (self->db_path);
  if (self->db)
    ephy_gsb_storage_close_db (self);

  G_OBJECT_CLASS (ephy_gsb_storage_parent_class)->finalize (object);
}

static void
ephy_gsb_storage_constructed (GObject *object)
{
  EphyGSBStorage *self = EPHY_GSB_STORAGE (object);
  gboolean success;

  G_OBJECT_CLASS (ephy_gsb_storage_parent_class)->constructed (object);

  if (!g_file_test (self->db_path, G_FILE_TEST_EXISTS)) {
    LOG ("GSB database does not exist, initializing...");
    success = ephy_gsb_storage_init_db (self);
  } else {
    LOG ("GSB database exists, opening...");
    success = ephy_gsb_storage_open_db (self);
    if (success && !ephy_gsb_storage_check_schema_version (self)) {
      LOG ("GSB database schema incompatibility, recreating database...");
      ephy_gsb_storage_close_db (self);
      ephy_gsb_storage_delete_db (self);
      success = ephy_gsb_storage_init_db (self);
    }
  }

  self->is_operable = success;
}

static void
ephy_gsb_storage_init (EphyGSBStorage *self)
{
}

static void
ephy_gsb_storage_class_init (EphyGSBStorageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = ephy_gsb_storage_set_property;
  object_class->get_property = ephy_gsb_storage_get_property;
  object_class->constructed = ephy_gsb_storage_constructed;
  object_class->finalize = ephy_gsb_storage_finalize;

  obj_properties[PROP_DB_PATH] =
    g_param_spec_string ("db-path",
                         "Database path",
                         "The path of the SQLite file holding the lists of unsafe web resources",
                         NULL,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);
}

EphyGSBStorage *
ephy_gsb_storage_new (const char *db_path)
{
  return g_object_new (EPHY_TYPE_GSB_STORAGE, "db-path", db_path, NULL);
}

gboolean
ephy_gsb_storage_is_operable (EphyGSBStorage *self)
{
  g_assert (EPHY_IS_GSB_STORAGE (self));

  return self->is_operable;
}
