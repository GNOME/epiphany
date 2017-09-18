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
#include <string.h>

/* Keep this lower than 200 or else you'll get "too many SQL variables" error
 * in ephy_gsb_storage_insert_batch(). SQLITE_MAX_VARIABLE_NUMBER is hardcoded
 * in sqlite3 as 999.
 */
#define BATCH_SIZE 199

/* Update schema version if you:
 * 1) Modify the database table structure.
 * 2) Add new threat lists below.
 */
#define SCHEMA_VERSION "1.0"

/* The available Linux threat lists of Google Safe Browsing API v4.
 * The format is {THREAT_TYPE, PLATFORM_TYPE, THREAT_ENTRY_TYPE}.
 */
static const char * const gsb_linux_threat_lists[][3] = {
  {"MALWARE",            "LINUX", "URL"},
  {"SOCIAL_ENGINEERING", "LINUX", "URL"},
  {"UNWANTED_SOFTWARE",  "LINUX", "URL"},
  {"MALWARE",            "LINUX", "IP_RANGE"},
};

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
bind_threat_list_params (EphySQLiteStatement *statement,
                         EphyGSBThreatList   *list,
                         int                  threat_type_col,
                         int                  platform_type_col,
                         int                  threat_entry_type_col,
                         int                  client_state_col)
{
  GError *error = NULL;

  g_assert (statement);
  g_assert (list);

  if (list->threat_type && threat_type_col >= 0) {
    ephy_sqlite_statement_bind_string (statement, threat_type_col, list->threat_type, &error);
    if (error) {
      g_warning ("Failed to bind threat type: %s", error->message);
      g_error_free (error);
      return FALSE;
    }
  }
  if (list->platform_type && platform_type_col >= 0) {
    ephy_sqlite_statement_bind_string (statement, platform_type_col, list->platform_type, &error);
    if (error) {
      g_warning ("Failed to bind platform type: %s", error->message);
      g_error_free (error);
      return FALSE;
    }
  }
  if (list->threat_entry_type && threat_entry_type_col >= 0) {
    ephy_sqlite_statement_bind_string (statement, threat_entry_type_col, list->threat_entry_type, &error);
    if (error) {
      g_warning ("Failed to bind threat entry type: %s", error->message);
      g_error_free (error);
      return FALSE;
    }
  }
  if (list->client_state && client_state_col >= 0) {
    ephy_sqlite_statement_bind_string (statement, client_state_col, list->client_state, &error);
    if (error) {
      g_warning ("Failed to bind client state: %s", error->message);
      g_error_free (error);
      return FALSE;
    }
  }

  return TRUE;
}

static void
ephy_gsb_storage_start_transaction (EphyGSBStorage *self)
{
  GError *error = NULL;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (self->is_operable);

  ephy_sqlite_connection_begin_transaction (self->db, &error);
  if (error) {
    g_warning ("Failed to begin transaction on GSB database: %s", error->message);
    g_error_free (error);
  }
}

static void
ephy_gsb_storage_end_transaction (EphyGSBStorage *self)
{
  GError *error = NULL;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (self->is_operable);

  ephy_sqlite_connection_commit_transaction (self->db, &error);
  if (error) {
    g_warning ("Failed to commit transaction on GSB database: %s", error->message);
    g_error_free (error);
  }
}

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
        "name VARCHAR NOT NULL PRIMARY KEY,"
        "value VARCHAR NOT NULL"
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
  EphySQLiteStatement *statement = NULL;
  GError *error = NULL;
  GString *string = NULL;
  const char *sql;
  gboolean retval = FALSE;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (EPHY_IS_SQLITE_CONNECTION (self->db));

  if (ephy_sqlite_connection_table_exists (self->db, "threats"))
    return TRUE;

  sql = "CREATE TABLE threats ("
        "threat_type VARCHAR NOT NULL,"
        "platform_type VARCHAR NOT NULL,"
        "threat_entry_type VARCHAR NOT NULL,"
        "client_state VARCHAR,"
        "timestamp INTEGER NOT NULL DEFAULT (CAST(strftime('%s', 'now') AS INT)),"
        "PRIMARY KEY (threat_type, platform_type, threat_entry_type)"
        ")";
  ephy_sqlite_connection_execute (self->db, sql, &error);
  if (error) {
    g_warning ("Failed to create threats table: %s", error->message);
    goto out;
  }

  sql = "INSERT INTO threats (threat_type, platform_type, threat_entry_type) VALUES ";
  string = g_string_new (sql);
  for (guint i = 0; i < G_N_ELEMENTS (gsb_linux_threat_lists); i++)
    g_string_append (string, "(?, ?, ?),");
  /* Remove trailing comma character. */
  g_string_erase (string, string->len - 1, -1);

  statement = ephy_sqlite_connection_create_statement (self->db, string->str, &error);
  if (error) {
    g_warning ("Failed to create threats table insert statement: %s", error->message);
    goto out;
  }

  for (guint i = 0; i < G_N_ELEMENTS (gsb_linux_threat_lists); i++) {
    EphyGSBThreatList *list = ephy_gsb_threat_list_new (gsb_linux_threat_lists[i][0],
                                                        gsb_linux_threat_lists[i][1],
                                                        gsb_linux_threat_lists[i][2],
                                                        NULL, 0);
    bind_threat_list_params (statement, list, i * 3, i * 3 + 1, i * 3 + 2, -1);
    ephy_gsb_threat_list_free (list);
  }

  ephy_sqlite_statement_step (statement, &error);
  if (error) {
    g_warning ("Failed to insert initial data into threats table: %s", error->message);
    goto out;
  }

  retval = TRUE;

out:
  if (string)
    g_string_free (string, TRUE);
  if (statement)
    g_object_unref (statement);
  if (error)
    g_error_free (error);

  return retval;
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
        "value BLOB NOT NULL,"  /* The prefix itself, can vary from 4 to 32 bytes. */
        "threat_type VARCHAR NOT NULL,"
        "platform_type VARCHAR NOT NULL,"
        "threat_entry_type VARCHAR NOT NULL,"
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
        "threat_type VARCHAR NOT NULL,"
        "platform_type VARCHAR NOT NULL,"
        "threat_entry_type VARCHAR NOT NULL,"
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
  ephy_sqlite_connection_execute (self->db, "PRAGMA foreign_keys=ON", &error);
  if (error) {
    g_warning ("Failed to enable foreign keys pragma: %s", error->message);
    goto out_err;
  }

  ephy_sqlite_connection_execute (self->db, "PRAGMA synchronous=OFF", &error);
  if (error) {
    g_warning ("Failed to disable synchronous pragma: %s", error->message);
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
    g_warning ("Failed to create select schema version statement: %s", error->message);
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

gint64
ephy_gsb_storage_get_next_update_time (EphyGSBStorage *self)
{
  EphySQLiteStatement *statement = NULL;
  GError *error = NULL;
  const char *next_update_at;
  const char *sql;
  gint64 next_update_time;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (self->is_operable);

  sql = "SELECT value FROM metadata WHERE name='next_update_at'";
  statement = ephy_sqlite_connection_create_statement (self->db, sql, &error);
  if (error) {
    g_warning ("Failed to create select next update statement: %s", error->message);
    g_error_free (error);
    return G_MAXINT64;
  }

  ephy_sqlite_statement_step (statement, &error);
  if (error) {
    g_warning ("Failed to retrieve next update time: %s", error->message);
    g_error_free (error);
    g_object_unref (statement);
    return G_MAXINT64;
  }

  next_update_at = ephy_sqlite_statement_get_column_as_string (statement, 0);
  sscanf (next_update_at, "%ld", &next_update_time);

  g_object_unref (statement);

  return next_update_time;
}

void
ephy_gsb_storage_set_next_update_time (EphyGSBStorage *self,
                                       gint64          next_update_time)
{
  EphySQLiteStatement *statement = NULL;
  GError *error = NULL;
  char *value = NULL;
  const char *sql;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (self->is_operable);

  sql = "UPDATE metadata SET value=? WHERE name='next_update_at'";
  statement = ephy_sqlite_connection_create_statement (self->db, sql, &error);
  if (error) {
    g_warning ("Failed to create update next update time statement: %s", error->message);
    goto out;
  }

  value = g_strdup_printf ("%ld", next_update_time);;
  ephy_sqlite_statement_bind_string (statement, 0, value, &error);
  if (error) {
    g_warning ("Failed to bind string in next update time statement: %s", error->message);
    goto out;
  }

  ephy_sqlite_statement_step (statement, &error);
  if (error)
    g_warning ("Failed to update next update time: %s", error->message);

out:
  g_free (value);
  if (statement)
    g_object_unref (statement);
  if (error)
    g_error_free (error);
}

GList *
ephy_gsb_storage_get_threat_lists (EphyGSBStorage *self)
{
  EphySQLiteStatement *statement = NULL;
  GError *error = NULL;
  GList *threat_lists = NULL;
  const char *sql;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (self->is_operable);

  sql = "SELECT threat_type, platform_type, threat_entry_type, client_state, timestamp FROM threats";
  statement = ephy_sqlite_connection_create_statement (self->db, sql, &error);
  if (error) {
    g_warning ("Failed to create select threat lists statement: %s", error->message);
    g_error_free (error);
    return NULL;
  }

  while (ephy_sqlite_statement_step (statement, &error)) {
    const char *threat_type = ephy_sqlite_statement_get_column_as_string (statement, 0);
    const char *platform_type = ephy_sqlite_statement_get_column_as_string (statement, 1);
    const char *threat_entry_type = ephy_sqlite_statement_get_column_as_string (statement, 2);
    const char *client_state = ephy_sqlite_statement_get_column_as_string (statement, 3);
    gint64 timestamp = ephy_sqlite_statement_get_column_as_int64 (statement, 4);
    EphyGSBThreatList *list = ephy_gsb_threat_list_new (threat_type, platform_type,
                                                        threat_entry_type, client_state,
                                                        timestamp);
    threat_lists = g_list_prepend (threat_lists, list);
  }

  if (error) {
    g_warning ("Failed to execute select threat lists statement: %s", error->message);
    g_error_free (error);
  }

  g_object_unref (statement);

  return g_list_reverse (threat_lists);
}

char *
ephy_gsb_storage_compute_checksum (EphyGSBStorage    *self,
                                   EphyGSBThreatList *list)
{
  EphySQLiteStatement *statement = NULL;
  GError *error = NULL;
  const char *sql;
  char *retval = NULL;
  GChecksum *checksum = NULL;
  guint8 *digest = NULL;
  gsize digest_len = GSB_HASH_SIZE;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (self->is_operable);
  g_assert (list);

  sql = "SELECT value FROM hash_prefix WHERE "
        "threat_type=? AND platform_type=? AND threat_entry_type=? "
        "ORDER BY value";
  statement = ephy_sqlite_connection_create_statement (self->db, sql, &error);
  if (error) {
    g_warning ("Failed to create select hash prefix statement: %s", error->message);
    goto out;
  }

  if (!bind_threat_list_params (statement, list, 0, 1, 2, -1))
    goto out;

  checksum = g_checksum_new (GSB_HASH_TYPE);
  while (ephy_sqlite_statement_step (statement, &error)) {
    g_checksum_update (checksum,
                       ephy_sqlite_statement_get_column_as_blob (statement, 0),
                       ephy_sqlite_statement_get_column_size (statement, 0));
  }

  if (error) {
    g_warning ("Failed to execute select hash prefix statement: %s", error->message);
    goto out;
  }

  digest = g_malloc (digest_len);
  g_checksum_get_digest (checksum, digest, &digest_len);
  retval = g_base64_encode (digest, digest_len);

out:
  g_free (digest);
  if (statement)
    g_object_unref (statement);
  if (checksum)
    g_checksum_free (checksum);
  if (error)
    g_error_free (error);

  return retval;
}

void
ephy_gsb_storage_update_client_state (EphyGSBStorage    *self,
                                      EphyGSBThreatList *list,
                                      gboolean           clear)
{
  EphySQLiteStatement *statement = NULL;
  GError *error = NULL;
  const char *sql;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (self->is_operable);
  g_assert (list);

  if (clear) {
    sql = "UPDATE threats SET "
          "timestamp=(CAST(strftime('%s', 'now') AS INT)), client_state=NULL "
          "WHERE threat_type=? AND platform_type=? AND threat_entry_type=?";
  } else {
    sql = "UPDATE threats SET "
          "timestamp=(CAST(strftime('%s', 'now') AS INT)), client_state=? "
          "WHERE threat_type=? AND platform_type=? AND threat_entry_type=?";
  }

  statement = ephy_sqlite_connection_create_statement (self->db, sql, &error);
  if (error) {
    g_warning ("Failed to create update threats statement: %s", error->message);
    goto out;
  }

  if (!bind_threat_list_params (statement, list, 1, 2, 3, clear ? -1 : 0))
    goto out;

  ephy_sqlite_statement_step (statement, &error);
  if (error)
    g_warning ("Failed to execute update threat statement: %s", error->message);

out:
  if (statement)
    g_object_unref (statement);
  if (error)
    g_error_free (error);
}

void
ephy_gsb_storage_clear_hash_prefixes (EphyGSBStorage    *self,
                                      EphyGSBThreatList *list)
{
  EphySQLiteStatement *statement = NULL;
  GError *error = NULL;
  const char *sql;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (self->is_operable);
  g_assert (list);

  sql = "DELETE FROM hash_prefix WHERE "
        "threat_type=? AND platform_type=? AND threat_entry_type=?";
  statement = ephy_sqlite_connection_create_statement (self->db, sql, &error);
  if (error) {
    g_warning ("Failed to create delete hash prefix statement: %s", error->message);
    goto out;
  }

  if (!bind_threat_list_params (statement, list, 0, 1, 2, -1))
    goto out;

  ephy_sqlite_statement_step (statement, &error);
  if (error)
    g_warning ("Failed to execute clear hash prefix statement: %s", error->message);

out:
  if (statement)
    g_object_unref (statement);
  if (error)
    g_error_free (error);
}

static GList *
ephy_gsb_storage_get_hash_prefixes_to_delete (EphyGSBStorage    *self,
                                              EphyGSBThreatList *list,
                                              GHashTable        *indices,
                                              gsize             *num_prefixes)
{
  EphySQLiteStatement *statement = NULL;
  GError *error = NULL;
  GList *prefixes = NULL;
  const char *sql;
  guint index = 0;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (self->is_operable);
  g_assert (list);
  g_assert (indices);

  *num_prefixes = 0;

  sql = "SELECT value FROM hash_prefix WHERE "
        "threat_type=? AND platform_type=? AND threat_entry_type=? "
        "ORDER BY value";
  statement = ephy_sqlite_connection_create_statement (self->db, sql, &error);
  if (error) {
    g_warning ("Failed to create select prefix value statement: %s", error->message);
    goto out;
  }

  if (!bind_threat_list_params (statement, list, 0, 1, 2, -1))
    goto out;

  while (ephy_sqlite_statement_step (statement, &error)) {
    if (g_hash_table_contains (indices, GINT_TO_POINTER (index))) {
      const guint8 *blob = ephy_sqlite_statement_get_column_as_blob (statement, 0);
      gsize size = ephy_sqlite_statement_get_column_size (statement, 0);
      prefixes = g_list_prepend (prefixes, g_bytes_new (blob, size));
      *num_prefixes += 1;
    }
    index++;
  }

  if (error)
    g_warning ("Failed to execute select prefix value statement: %s", error->message);

out:
  if (statement)
    g_object_unref (statement);
  if (error)
    g_error_free (error);

  return prefixes;
}

static EphySQLiteStatement *
ephy_gsb_storage_make_delete_hash_prefix_statement (EphyGSBStorage *self,
                                                    gsize           num_prefixes)
{
  EphySQLiteStatement *statement = NULL;
  GError *error = NULL;
  GString *sql;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (self->is_operable);

  sql = g_string_new ("DELETE FROM hash_prefix WHERE "
                      "threat_type=? AND platform_type=? and threat_entry_type=? "
                      "AND value IN (");
  for (gsize i = 0; i < num_prefixes; i++)
    g_string_append (sql, "?,");
  /* Replace trailing comma character with close parenthesis character. */
  g_string_overwrite (sql, sql->len - 1, ")");

  statement = ephy_sqlite_connection_create_statement (self->db, sql->str, &error);
  if (error) {
    g_warning ("Failed to create delete hash prefix statement: %s", error->message);
    g_error_free (error);
  }

  g_string_free (sql, TRUE);

  return statement;
}

static GList *
ephy_gsb_storage_delete_hash_prefix_batch (EphyGSBStorage      *self,
                                           EphyGSBThreatList   *list,
                                           GList               *prefixes,
                                           gsize                num_prefixes,
                                           EphySQLiteStatement *stmt)
{
  EphySQLiteStatement *statement = NULL;
  GError *error = NULL;
  gboolean free_statement = TRUE;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (self->is_operable);
  g_assert (list);
  g_assert (prefixes);

  if (stmt) {
    statement = stmt;
    ephy_sqlite_statement_reset (statement);
    free_statement = FALSE;
  } else {
    statement = ephy_gsb_storage_make_delete_hash_prefix_statement (self, num_prefixes);
    if (!statement)
      return prefixes;
  }

  if (!bind_threat_list_params (statement, list, 0, 1, 2, -1))
    goto out;

  for (gsize i = 0; i < num_prefixes; i++) {
    GBytes *prefix = (GBytes *)prefixes->data;
    if (!ephy_sqlite_statement_bind_blob (statement, i + 3,
                                          g_bytes_get_data (prefix, NULL),
                                          g_bytes_get_size (prefix),
                                          NULL)) {
      g_warning ("Failed to bind values in delete hash prefix statement");
      goto out;
    }
    prefixes = prefixes->next;
  }

  ephy_sqlite_statement_step (statement, &error);
  if (error) {
    g_warning ("Failed to execute delete hash prefix statement: %s", error->message);
    g_error_free (error);
  }

out:
  if (free_statement && statement)
    g_object_unref (statement);

  /* Return where we left off. */
  return prefixes;
}

void
ephy_gsb_storage_delete_hash_prefixes (EphyGSBStorage    *self,
                                       EphyGSBThreatList *list,
                                       JsonArray         *indices)
{
  EphySQLiteStatement *statement = NULL;
  GList *prefixes = NULL;
  GList *head = NULL;
  GHashTable *set;
  gsize num_prefixes;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (self->is_operable);
  g_assert (list);
  g_assert (indices);

  LOG ("Deleting %u hash prefixes...", json_array_get_length (indices));

  /* Move indices from the JSON array to a hash table set. */
  set = g_hash_table_new (g_direct_hash, g_direct_equal);
  for (guint i = 0; i < json_array_get_length (indices); i++)
    g_hash_table_add (set, GINT_TO_POINTER (json_array_get_int_element (indices, i)));

  prefixes = ephy_gsb_storage_get_hash_prefixes_to_delete (self, list, set, &num_prefixes);
  head = prefixes;

  ephy_gsb_storage_start_transaction (self);

  if (num_prefixes / BATCH_SIZE > 0) {
    /* Reuse statement to increase performance. */
    statement = ephy_gsb_storage_make_delete_hash_prefix_statement (self, BATCH_SIZE);

    for (gsize i = 0; i < num_prefixes / BATCH_SIZE; i++) {
      head = ephy_gsb_storage_delete_hash_prefix_batch (self, list,
                                                        head, BATCH_SIZE,
                                                        statement);
    }
  }

  if (num_prefixes % BATCH_SIZE != 0) {
    ephy_gsb_storage_delete_hash_prefix_batch (self, list,
                                               head, num_prefixes % BATCH_SIZE,
                                               NULL);
  }

  ephy_gsb_storage_end_transaction (self);

  g_hash_table_unref (set);
  g_list_free_full (prefixes, (GDestroyNotify)g_bytes_unref);
  if (statement)
    g_object_unref (statement);
}

static EphySQLiteStatement *
ephy_gsb_storage_make_insert_hash_prefix_statement (EphyGSBStorage *self,
                                                    gsize           num_prefixes)
{
  EphySQLiteStatement *statement = NULL;
  GError *error = NULL;
  GString *sql;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (self->is_operable);

  sql = g_string_new ("INSERT INTO hash_prefix "
                      "(cue, value, threat_type, platform_type, threat_entry_type) VALUES ");
  for (gsize i = 0; i < num_prefixes; i++)
    g_string_append (sql, "(?, ?, ?, ?, ?),");
  /* Remove trailing comma character. */
  g_string_erase (sql, sql->len - 1, -1);

  statement = ephy_sqlite_connection_create_statement (self->db, sql->str, &error);
  if (error) {
    g_warning ("Failed to create insert hash prefix statement: %s", error->message);
    g_error_free (error);
  }

  g_string_free (sql, TRUE);

  return statement;
}

static void
ephy_gsb_storage_insert_hash_prefix_batch (EphyGSBStorage      *self,
                                           EphyGSBThreatList   *list,
                                           const guint8        *prefixes,
                                           gsize                start,
                                           gsize                end,
                                           gsize                len,
                                           EphySQLiteStatement *stmt)
{
  EphySQLiteStatement *statement = NULL;
  GError *error = NULL;
  gsize id = 0;
  gboolean free_statement = TRUE;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (self->is_operable);
  g_assert (list);
  g_assert (prefixes);

  if (stmt) {
    statement = stmt;
    ephy_sqlite_statement_reset (statement);
    free_statement = FALSE;
  } else {
    statement = ephy_gsb_storage_make_insert_hash_prefix_statement (self, (end - start + 1) / len);
    if (!statement)
      return;
  }

  for (gsize k = start; k < end; k += len) {
    if (!ephy_sqlite_statement_bind_blob (statement, id++, prefixes + k, GSB_CUE_LEN, NULL) ||
        !ephy_sqlite_statement_bind_blob (statement, id++, prefixes + k, len, NULL) ||
        !bind_threat_list_params (statement, list, id, id + 1, id + 2, -1)) {
      g_warning ("Failed to bind values in hash prefix statement");
      goto out;
    }
    id += 3;
  }

  ephy_sqlite_statement_step (statement, &error);
  if (error) {
    g_warning ("Failed to execute insert hash prefix statement: %s", error->message);
    g_error_free (error);
  }

out:
  if (free_statement && statement)
    g_object_unref (statement);
}

void
ephy_gsb_storage_insert_hash_prefixes (EphyGSBStorage    *self,
                                       EphyGSBThreatList *list,
                                       gsize              prefix_len,
                                       const char        *prefixes_b64)
{
  EphySQLiteStatement *statement = NULL;
  guint8 *prefixes;
  gsize prefixes_len;
  gsize num_prefixes;
  gsize num_batches;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (self->is_operable);
  g_assert (list);
  g_assert (prefix_len > 0);
  g_assert (prefixes_b64);

  prefixes = g_base64_decode (prefixes_b64, &prefixes_len);
  num_prefixes = prefixes_len / prefix_len;
  num_batches = num_prefixes / BATCH_SIZE;

  LOG ("Inserting %lu hash prefixes of size %ld...", num_prefixes, prefix_len);

  ephy_gsb_storage_start_transaction (self);

  if (num_batches > 0) {
    /* Reuse statement to increase performance. */
    statement = ephy_gsb_storage_make_insert_hash_prefix_statement (self, BATCH_SIZE);

    for (gsize i = 0; i < num_batches; i++) {
      ephy_gsb_storage_insert_hash_prefix_batch (self, list, prefixes,
                                                 i * prefix_len * BATCH_SIZE,
                                                 (i + 1) * prefix_len * BATCH_SIZE,
                                                 prefix_len,
                                                 statement);
    }
  }

  if (num_prefixes % BATCH_SIZE != 0) {
    ephy_gsb_storage_insert_hash_prefix_batch (self, list, prefixes,
                                               num_batches * prefix_len * BATCH_SIZE,
                                               prefixes_len - 1,
                                               prefix_len,
                                               NULL);
  }

  ephy_gsb_storage_end_transaction (self);

  g_free (prefixes);
  if (statement)
    g_object_unref (statement);
}

GList *
ephy_gsb_storage_lookup_hash_prefixes (EphyGSBStorage *self,
                                       GList          *cues)
{
  EphySQLiteStatement *statement = NULL;
  GError *error = NULL;
  GList *retval = NULL;
  GString *sql;
  guint id = 0;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (self->is_operable);
  g_assert (cues);

  sql = g_string_new ("SELECT value, threat_type, platform_type, threat_entry_type, "
                      "negative_expires_at <= (CAST(strftime('%s', 'now') AS INT)) "
                      "FROM hash_prefix WHERE cue IN (");
  for (GList *l = cues; l && l->data; l = l->next)
    g_string_append (sql, "?,");
  /* Replace trailing comma character with close parenthesis character. */
  g_string_overwrite (sql, sql->len - 1, ")");

  statement = ephy_sqlite_connection_create_statement (self->db, sql->str, &error);
  if (error) {
    g_warning ("Failed to create select hash prefix statement: %s", error->message);
    goto out;
  }

  for (GList *l = cues; l && l->data; l = l->next) {
    ephy_sqlite_statement_bind_blob (statement, id++,
                                     g_bytes_get_data (l->data, NULL), GSB_CUE_LEN,
                                     &error);
    if (error) {
      g_warning ("Failed to bind cue value as blob: %s", error->message);
      goto out;
    }
  }

  while (ephy_sqlite_statement_step (statement, &error)) {
    const guint8 *blob = ephy_sqlite_statement_get_column_as_blob (statement, 0);
    gsize size = ephy_sqlite_statement_get_column_size (statement, 0);
    const char *threat_type = ephy_sqlite_statement_get_column_as_string (statement, 1);
    const char *platform_type = ephy_sqlite_statement_get_column_as_string (statement, 2);
    const char *threat_entry_type = ephy_sqlite_statement_get_column_as_string (statement, 3);
    gboolean negative_expired = ephy_sqlite_statement_get_column_as_boolean (statement, 4);
    EphyGSBHashPrefixLookup *lookup = ephy_gsb_hash_prefix_lookup_new (blob, size,
                                                                       threat_type,
                                                                       platform_type,
                                                                       threat_entry_type,
                                                                       negative_expired);
    retval = g_list_prepend (retval, lookup);
  }

  if (error) {
    g_warning ("Failed to execute select hash prefix statement: %s", error->message);
    g_list_free_full (retval, (GDestroyNotify)ephy_gsb_hash_prefix_lookup_free);
    retval = NULL;
  }

out:
  g_string_free (sql, TRUE);
  if (statement)
    g_object_unref (statement);
  if (error)
    g_error_free (error);

  return g_list_reverse (retval);
}

GList *
ephy_gsb_storage_lookup_full_hashes (EphyGSBStorage *self,
                                     GList          *hashes)
{
  EphySQLiteStatement *statement = NULL;
  GError *error = NULL;
  GList *retval = NULL;
  GString *sql;
  guint id = 0;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (self->is_operable);
  g_assert (hashes);

  sql = g_string_new ("SELECT value, threat_type, platform_type, threat_entry_type, "
                      "expires_at <= (CAST(strftime('%s', 'now') AS INT)) "
                      "FROM hash_full WHERE value IN (");
  for (GList *l = hashes; l && l->data; l = l->next)
    g_string_append (sql, "?,");
  /* Replace trailing comma character with close parenthesis character. */
  g_string_overwrite (sql, sql->len - 1, ")");

  statement = ephy_sqlite_connection_create_statement (self->db, sql->str, &error);
  if (error) {
    g_warning ("Failed to create select full hash statement: %s", error->message);
    goto out;
  }

  for (GList *l = hashes; l && l->data; l = l->next) {
    ephy_sqlite_statement_bind_blob (statement, id++,
                                     g_bytes_get_data (l->data, NULL), GSB_HASH_SIZE,
                                     &error);
    if (error) {
      g_warning ("Failed to bind hash value as blob: %s", error->message);
      goto out;
    }
  }

  while (ephy_sqlite_statement_step (statement, &error)) {
    const guint8 *blob = ephy_sqlite_statement_get_column_as_blob (statement, 0);
    const char *threat_type = ephy_sqlite_statement_get_column_as_string (statement, 1);
    const char *platform_type = ephy_sqlite_statement_get_column_as_string (statement, 2);
    const char *threat_entry_type = ephy_sqlite_statement_get_column_as_string (statement, 3);
    gboolean expired = ephy_sqlite_statement_get_column_as_boolean (statement, 4);
    EphyGSBHashFullLookup *lookup = ephy_gsb_hash_full_lookup_new (blob,
                                                                   threat_type,
                                                                   platform_type,
                                                                   threat_entry_type,
                                                                   expired);
    retval = g_list_prepend (retval, lookup);
  }

  if (error) {
    g_warning ("Failed to execute select full hash statement: %s", error->message);
    g_list_free_full (retval, (GDestroyNotify)ephy_gsb_hash_full_lookup_free);
    retval = NULL;
  }

out:
  g_string_free (sql, TRUE);
  if (statement)
    g_object_unref (statement);
  if (error)
    g_error_free (error);

  return g_list_reverse (retval);
}

void
ephy_gsb_storage_insert_full_hash (EphyGSBStorage    *self,
                                   EphyGSBThreatList *list,
                                   const guint8      *hash,
                                   gint64             duration)
{
  EphySQLiteStatement *statement = NULL;
  GError *error = NULL;
  const char *sql;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (self->is_operable);
  g_assert (list);
  g_assert (hash);

  LOG ("Inserting full hash with duration %ld for list %s/%s/%s",
       duration, list->threat_type, list->platform_type, list->threat_entry_type);

  sql = "INSERT OR IGNORE INTO hash_full "
        "(value, threat_type, platform_type, threat_entry_type) "
        "VALUES (?, ?, ?, ?)";
  statement = ephy_sqlite_connection_create_statement (self->db, sql, &error);
  if (error) {
    g_warning ("Failed to create insert full hash statement: %s", error->message);
    goto out;
  }

  if (!bind_threat_list_params (statement, list, 1, 2, 3, -1))
    goto out;
  ephy_sqlite_statement_bind_blob (statement, 0, hash, GSB_HASH_SIZE, &error);
  if (error) {
    g_warning ("Failed to bind blob in insert full hash statement: %s", error->message);
    goto out;
  }

  ephy_sqlite_statement_step (statement, &error);
  if (error) {
    g_warning ("Failed to execute insert full hash statement: %s", error->message);
    goto out;
  }

  /* Update expiration time. */
  g_clear_object (&statement);
  sql = "UPDATE hash_full SET expires_at=(CAST(strftime('%s', 'now') AS INT)) + ? "
        "WHERE value=? AND threat_type=? AND platform_type=? AND threat_entry_type=?";
  statement = ephy_sqlite_connection_create_statement (self->db, sql, &error);
  if (error) {
    g_warning ("Failed to create update full hash statement: %s", error->message);
    goto out;
  }

  ephy_sqlite_statement_bind_int64 (statement, 0, duration, &error);
  if (error) {
    g_warning ("Failed to bind int64 in update full hash statement: %s", error->message);
    goto out;
  }
  ephy_sqlite_statement_bind_blob (statement, 1, hash, GSB_HASH_SIZE, &error);
  if (error) {
    g_warning ("Failed to bind blob in update full hash statement: %s", error->message);
    goto out;
  }
  if (!bind_threat_list_params (statement, list, 2, 3, 4, -1))
    goto out;

  ephy_sqlite_statement_step (statement, &error);
  if (error)
    g_warning ("Failed to execute insert full hash statement: %s", error->message);

out:
  if (statement)
    g_object_unref (statement);
  if (error)
    g_error_free (error);
}

void
ephy_gsb_storage_update_hash_prefix_expiration (EphyGSBStorage *self,
                                                GBytes         *prefix,
                                                gint64          duration)
{
  EphySQLiteStatement *statement = NULL;
  GError *error = NULL;
  const char *sql;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (self->is_operable);
  g_assert (prefix);

  sql = "UPDATE hash_prefix "
        "SET negative_expires_at=(CAST(strftime('%s', 'now') AS INT)) + ? "
        "WHERE value=?";
  statement = ephy_sqlite_connection_create_statement (self->db, sql, &error);
  if (error) {
    g_warning ("Failed to create update hash prefix statement: %s", error->message);
    goto out;
  }

  ephy_sqlite_statement_bind_int64 (statement, 0, duration, &error);
  if (error) {
    g_warning ("Failed to bind int64 in update hash prefix statement: %s", error->message);
    goto out;
  }
  ephy_sqlite_statement_bind_blob (statement, 1,
                                   g_bytes_get_data (prefix, NULL),
                                   g_bytes_get_size (prefix),
                                   &error);
  if (error) {
    g_warning ("Failed to bind blob in update hash prefix statement: %s", error->message);
    goto out;
  }

  ephy_sqlite_statement_step (statement, &error);
  if (error)
    g_warning ("Failed to execute update hash prefix statement: %s", error->message);

out:
  if (statement)
    g_object_unref (statement);
  if (error)
    g_error_free (error);
}
