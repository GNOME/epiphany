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

#include <string.h>

#define EXPIRATION_THRESHOLD (8 * 60 * 60)

/* Keep this lower than 6533 (SQLITE_MAX_VARIABLE_NUMBER / 5 slots) or else
 * you'll get "too many SQL variables" error in ephy_gsb_storage_insert_batch().
 * SQLITE_MAX_VARIABLE_NUMBER is hardcoded in sqlite3 (>= 3.22) as 32766.
 */
#define BATCH_SIZE 6553

/* Increment schema version if you:
 * 1) Modify the database table structure.
 * 2) Modify the threat lists below.
 */
#define SCHEMA_VERSION 3

/* The available Linux threat lists of Google Safe Browsing API v4.
 * The format is {THREAT_TYPE, PLATFORM_TYPE, THREAT_ENTRY_TYPE}.
 */
static const char * const gsb_linux_threat_lists[][3] = {
  {GSB_THREAT_TYPE_MALWARE, "LINUX", "URL"},
  {GSB_THREAT_TYPE_SOCIAL_ENGINEERING, "ANY_PLATFORM", "URL"},
  {GSB_THREAT_TYPE_UNWANTED_SOFTWARE, "LINUX", "URL"},
  {GSB_THREAT_TYPE_MALWARE, "LINUX", "IP_RANGE"},
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

  if (!self->is_operable)
    return;

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

  if (!self->is_operable)
    return;

  ephy_sqlite_connection_commit_transaction (self->db, &error);
  if (error) {
    g_warning ("Failed to commit transaction on GSB database: %s", error->message);
    g_error_free (error);
  }
}

static gboolean
ephy_gsb_storage_init_metadata_table (EphyGSBStorage *self)
{
  EphySQLiteStatement *statement;
  GError *error = NULL;
  const char *sql;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (EPHY_IS_SQLITE_CONNECTION (self->db));

  if (ephy_sqlite_connection_table_exists (self->db, "metadata"))
    return TRUE;

  sql = "CREATE TABLE metadata ("
        "key VARCHAR NOT NULL PRIMARY KEY,"
        "value INTEGER NOT NULL"
        ")";
  ephy_sqlite_connection_execute (self->db, sql, &error);
  if (error) {
    g_warning ("Failed to create metadata table: %s", error->message);
    g_error_free (error);
    return FALSE;
  }

  sql = "INSERT INTO metadata (key, value) VALUES"
        "('schema_version', ?),"
        "('next_list_updates_time', (CAST(strftime('%s', 'now') AS INT))),"
        "('next_full_hashes_time', (CAST(strftime('%s', 'now') AS INT))),"
        "('back_off_exit_time', 0),"
        "('back_off_num_fails', 0)";
  statement = ephy_sqlite_connection_create_statement (self->db, sql, &error);
  if (error) {
    g_warning ("Failed to create metadata insert statement: %s", error->message);
    g_error_free (error);
    return FALSE;
  }

  ephy_sqlite_statement_bind_int64 (statement, 0, SCHEMA_VERSION, &error);
  if (error) {
    g_warning ("Failed to bind int64 in metadata insert statement: %s", error->message);
    g_error_free (error);
    g_object_unref (statement);
    return FALSE;
  }

  ephy_sqlite_statement_step (statement, &error);
  g_object_unref (statement);

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
  EphySQLiteStatement *statement;
  GError *error = NULL;
  GString *string;
  const char *sql;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (EPHY_IS_SQLITE_CONNECTION (self->db));

  if (ephy_sqlite_connection_table_exists (self->db, "threats"))
    return TRUE;

  sql = "CREATE TABLE threats ("
        "threat_type VARCHAR NOT NULL,"
        "platform_type VARCHAR NOT NULL,"
        "threat_entry_type VARCHAR NOT NULL,"
        "client_state VARCHAR,"
        "PRIMARY KEY (threat_type, platform_type, threat_entry_type)"
        ")";
  ephy_sqlite_connection_execute (self->db, sql, &error);
  if (error) {
    g_warning ("Failed to create threats table: %s", error->message);
    g_error_free (error);
    return FALSE;
  }

  sql = "INSERT INTO threats (threat_type, platform_type, threat_entry_type) VALUES ";
  string = g_string_new (sql);
  for (guint i = 0; i < G_N_ELEMENTS (gsb_linux_threat_lists); i++)
    g_string_append (string, "(?, ?, ?),");
  /* Remove trailing comma character. */
  g_string_erase (string, string->len - 1, -1);

  statement = ephy_sqlite_connection_create_statement (self->db, string->str, &error);
  g_string_free (string, TRUE);

  if (error) {
    g_warning ("Failed to create threats table insert statement: %s", error->message);
    g_error_free (error);
    return FALSE;
  }

  for (guint i = 0; i < G_N_ELEMENTS (gsb_linux_threat_lists); i++) {
    EphyGSBThreatList *list = ephy_gsb_threat_list_new (gsb_linux_threat_lists[i][0],
                                                        gsb_linux_threat_lists[i][1],
                                                        gsb_linux_threat_lists[i][2],
                                                        NULL);
    bind_threat_list_params (statement, list, i * 3, i * 3 + 1, i * 3 + 2, -1);
    ephy_gsb_threat_list_free (list);
  }

  ephy_sqlite_statement_step (statement, &error);
  g_object_unref (statement);

  if (error) {
    g_warning ("Failed to insert initial data into threats table: %s", error->message);
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
        "value BLOB NOT NULL,"  /* The prefix itself, can vary from 4 to 32 bytes. */
        "threat_type VARCHAR NOT NULL,"
        "platform_type VARCHAR NOT NULL,"
        "threat_entry_type VARCHAR NOT NULL,"
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

  sql = "CREATE INDEX idx_hash_prefix_cue ON hash_prefix (cue)";
  ephy_sqlite_connection_execute (self->db, sql, &error);
  if (error) {
    g_warning ("Failed to create idx_hash_prefix_cue index: %s", error->message);
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
        "expires_at INTEGER NOT NULL DEFAULT (CAST(strftime('%s', 'now') AS INT)),"
        "PRIMARY KEY (value, threat_type, platform_type, threat_entry_type)"
        ")";
  ephy_sqlite_connection_execute (self->db, sql, &error);
  if (error) {
    g_warning ("Failed to create hash_full table: %s", error->message);
    g_error_free (error);
    return FALSE;
  }

  sql = "CREATE INDEX idx_hash_full_value ON hash_full (value)";
  ephy_sqlite_connection_execute (self->db, sql, &error);
  if (error) {
    g_warning ("Failed to create idx_hash_full_value index: %s", error->message);
    g_error_free (error);
    return FALSE;
  }

  return TRUE;
}

static gboolean
ephy_gsb_storage_open_db (EphyGSBStorage *self)
{
  GError *error = NULL;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (!self->db);

  self->db = ephy_sqlite_connection_new (EPHY_SQLITE_CONNECTION_MODE_READWRITE, self->db_path);
  ephy_sqlite_connection_open (self->db, &error);
  if (error) {
    g_warning ("Failed to open GSB database at %s: %s", self->db_path, error->message);
    g_error_free (error);
    g_clear_object (&self->db);
    return FALSE;
  }

  ephy_sqlite_connection_enable_foreign_keys (self->db);

  ephy_sqlite_connection_execute (self->db, "PRAGMA synchronous=OFF", &error);
  if (error) {
    g_warning ("Failed to disable synchronous pragma: %s", error->message);
    g_error_free (error);
  }

  return TRUE;
}

static void
ephy_gsb_storage_clear_db (EphyGSBStorage *self)
{
  g_assert (EPHY_IS_GSB_STORAGE (self));

  if (self->db) {
    ephy_sqlite_connection_close (self->db);
    ephy_sqlite_connection_delete_database (self->db);
    g_clear_object (&self->db);
  }
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

  if (!success)
    ephy_gsb_storage_clear_db (self);

  self->is_operable = success;

  return success;
}

static gboolean
ephy_gsb_storage_recreate_db (EphyGSBStorage *self)
{
  g_assert (EPHY_IS_GSB_STORAGE (self));

  ephy_gsb_storage_clear_db (self);
  return ephy_gsb_storage_init_db (self);
}

static inline gboolean
ephy_gsb_storage_check_schema_version (EphyGSBStorage *self)
{
  gint64 schema_version;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (EPHY_IS_SQLITE_CONNECTION (self->db));

  schema_version = ephy_gsb_storage_get_metadata (self, "schema_version", 0);

  return schema_version == SCHEMA_VERSION;
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
      self->db_path = g_value_dup_string (value);
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
  if (self->db) {
    ephy_sqlite_connection_close (self->db);
    g_object_unref (self->db);
  }

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
    ephy_gsb_storage_init_db (self);
  } else {
    LOG ("GSB database exists, opening...");
    success = ephy_gsb_storage_open_db (self);
    if (success) {
      if (!ephy_gsb_storage_check_schema_version (self)) {
        LOG ("GSB database schema incompatibility, recreating database...");
        ephy_gsb_storage_recreate_db (self);
      } else {
        self->is_operable = TRUE;
      }
    }
  }
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

/**
 * ephy_gsb_storage_is_operable:
 * @self: an #EphyGSBStorage
 *
 * Verify whether the local database is operable, i.e. no error occurred during
 * the opening and/or initialization of the database. No operations on @self are
 * allowed if the local database is inoperable.
 *
 * Return value: %TRUE if the local database is operable
 **/
gboolean
ephy_gsb_storage_is_operable (EphyGSBStorage *self)
{
  g_assert (EPHY_IS_GSB_STORAGE (self));

  return self->is_operable;
}

/**
 * ephy_gsb_storage_get_metadata:
 * @self: an #EphyGSBStorage
 * @key: the key whose value to retrieve
 * @default_value: the value to return in case of error or if @key is missing
 *
 * Retrieve the value of a key from the metadata table of the local database.
 *
 * Return value: The metadata value associated with @key
 **/
gint64
ephy_gsb_storage_get_metadata (EphyGSBStorage *self,
                               const char     *key,
                               gint64          default_value)
{
  EphySQLiteStatement *statement;
  GError *error = NULL;
  const char *sql;
  gint64 value;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (EPHY_IS_SQLITE_CONNECTION (self->db));
  g_assert (key);

  sql = "SELECT value FROM metadata WHERE key=?";
  statement = ephy_sqlite_connection_create_statement (self->db, sql, &error);
  if (error) {
    g_warning ("Failed to create select metadata statement: %s", error->message);
    g_error_free (error);
    return default_value;
  }

  ephy_sqlite_statement_bind_string (statement, 0, key, &error);
  if (error) {
    g_warning ("Failed to bind key as string in select metadata statement: %s", error->message);
    g_error_free (error);
    g_object_unref (statement);
    return default_value;
  }

  ephy_sqlite_statement_step (statement, &error);
  if (error) {
    g_warning ("Failed to execute select metadata statement: %s", error->message);
    g_error_free (error);
    g_object_unref (statement);
    ephy_gsb_storage_recreate_db (self);
    return default_value;
  }

  value = ephy_sqlite_statement_get_column_as_int64 (statement, 0);
  g_object_unref (statement);

  return value;
}

/**
 * ephy_gsb_storage_set_metadata:
 * @self: an #EphyGSBStorage
 * @key: the key whose value to update
 * @value: the updated value
 *
 * Update the value of a key in the metadata table of the local database.
 **/
void
ephy_gsb_storage_set_metadata (EphyGSBStorage *self,
                               const char     *key,
                               gint64          value)
{
  EphySQLiteStatement *statement;
  GError *error = NULL;
  const char *sql;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (key);

  if (!self->is_operable)
    return;

  sql = "UPDATE metadata SET value=? WHERE key=?";
  statement = ephy_sqlite_connection_create_statement (self->db, sql, &error);
  if (error) {
    g_warning ("Failed to create update metadata statement: %s", error->message);
    g_error_free (error);
    return;
  }

  ephy_sqlite_statement_bind_int64 (statement, 0, value, &error);
  if (error) {
    g_warning ("Failed to bind value as int64 in update metadata statement: %s", error->message);
    g_error_free (error);
    g_object_unref (statement);
    return;
  }

  ephy_sqlite_statement_bind_string (statement, 1, key, &error);
  if (error) {
    g_warning ("Failed to bind key as string in update metadata statement: %s", error->message);
    g_error_free (error);
    g_object_unref (statement);
    return;
  }

  ephy_sqlite_statement_step (statement, &error);
  g_object_unref (statement);

  if (error) {
    g_warning ("Failed to execute update metadata statement: %s", error->message);
    g_error_free (error);
    ephy_gsb_storage_recreate_db (self);
  }
}

/**
 * ephy_gsb_storage_get_threat_lists:
 * @self: an #EphyGSBStorage
 *
 * Retrieve the list of supported threat lists from the threats table of the
 * local database.
 *
 * Return value: (element-type #EphyGSBThreatList) (transfer full): a #GList
 *               containing the threat lists. The caller takes ownership
 *               of the list and its content. Use g_list_free_full() with
 *               ephy_gsb_threat_list_free() as free_func when done using
 *               the list.
 **/
GList *
ephy_gsb_storage_get_threat_lists (EphyGSBStorage *self)
{
  EphySQLiteStatement *statement;
  GError *error = NULL;
  GList *threat_lists = NULL;
  const char *sql;

  g_assert (EPHY_IS_GSB_STORAGE (self));

  if (!self->is_operable)
    return NULL;

  sql = "SELECT threat_type, platform_type, threat_entry_type, client_state FROM threats";
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
    EphyGSBThreatList *list = ephy_gsb_threat_list_new (threat_type, platform_type,
                                                        threat_entry_type, client_state);
    threat_lists = g_list_prepend (threat_lists, list);
  }

  if (error) {
    g_warning ("Failed to execute select threat lists statement: %s", error->message);
    g_error_free (error);
    ephy_gsb_storage_recreate_db (self);
  }

  g_object_unref (statement);

  return g_list_reverse (threat_lists);
}

/**
 * ephy_gsb_storage_compute_checksum:
 * @self: an #EphyGSBSTorage
 * @list: an #EphyGSBThreatList
 *
 * Compute the SHA256 checksum of the lexicographically sorted list of all the
 * hash prefixes belonging to @list in the local database.
 *
 * https://developers.google.com/safe-browsing/v4/local-databases#validation-checks
 *
 * Return value: (transfer full): the base64 encoded checksum or %NULL if error
 **/
char *
ephy_gsb_storage_compute_checksum (EphyGSBStorage    *self,
                                   EphyGSBThreatList *list)
{
  EphySQLiteStatement *statement;
  GError *error = NULL;
  const char *sql;
  char *retval = NULL;
  GChecksum *checksum;
  guint8 *digest;
  gsize digest_len = GSB_HASH_SIZE;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (list);

  if (!self->is_operable)
    return NULL;

  sql = "SELECT value FROM hash_prefix WHERE "
        "threat_type=? AND platform_type=? AND threat_entry_type=? "
        "ORDER BY value";
  statement = ephy_sqlite_connection_create_statement (self->db, sql, &error);
  if (error) {
    g_warning ("Failed to create select hash prefix statement: %s", error->message);
    g_error_free (error);
    return NULL;
  }

  if (!bind_threat_list_params (statement, list, 0, 1, 2, -1)) {
    g_object_unref (statement);
    return NULL;
  }

  checksum = g_checksum_new (GSB_HASH_TYPE);
  while (ephy_sqlite_statement_step (statement, &error)) {
    g_checksum_update (checksum,
                       ephy_sqlite_statement_get_column_as_blob (statement, 0),
                       ephy_sqlite_statement_get_column_size (statement, 0));
  }

  if (error) {
    g_warning ("Failed to execute select hash prefix statement: %s", error->message);
    g_error_free (error);
    ephy_gsb_storage_recreate_db (self);
    goto out;
  }

  digest = g_malloc (digest_len);
  g_checksum_get_digest (checksum, digest, &digest_len);
  retval = g_base64_encode (digest, digest_len);

  g_free (digest);
out:
  g_object_unref (statement);
  g_checksum_free (checksum);

  return retval;
}

/**
 * ephy_gsb_storage_update_client_state:
 * @self: an #EphyGSBStorage
 * @list: an #EphyGSBThreatList
 * @clear: %TRUE if the client state should be set to %NULL
 *
 * Update the client state column of @list in the threats table of the local
 * database. The new state is set according to the client_state field of @list.
 * Set @clear to %TRUE if you wish to reset the state.
 **/
void
ephy_gsb_storage_update_client_state (EphyGSBStorage    *self,
                                      EphyGSBThreatList *list,
                                      gboolean           clear)
{
  EphySQLiteStatement *statement;
  GError *error = NULL;
  const char *sql;
  gboolean success;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (list);

  if (!self->is_operable)
    return;

  if (clear) {
    sql = "UPDATE threats SET client_state=NULL "
          "WHERE threat_type=? AND platform_type=? AND threat_entry_type=?";
  } else {
    sql = "UPDATE threats SET client_state=? "
          "WHERE threat_type=? AND platform_type=? AND threat_entry_type=?";
  }

  statement = ephy_sqlite_connection_create_statement (self->db, sql, &error);
  if (error) {
    g_warning ("Failed to create update threats statement: %s", error->message);
    g_error_free (error);
    return;
  }

  if (clear)
    success = bind_threat_list_params (statement, list, 0, 1, 2, -1);
  else
    success = bind_threat_list_params (statement, list, 1, 2, 3, 0);

  if (!success) {
    g_object_unref (statement);
    return;
  }

  ephy_sqlite_statement_step (statement, &error);
  if (error) {
    g_warning ("Failed to execute update threat statement: %s", error->message);
    g_error_free (error);
    ephy_gsb_storage_recreate_db (self);
  }

  g_object_unref (statement);
}

/**
 * ephy_gsb_storage_clear_hash_prefixes:
 * @self: an #EphyGSBStorage
 * @list: an #EphyGSBThreatList
 *
 * Delete all hash prefixes belonging to @list from the local database.
 **/
void
ephy_gsb_storage_clear_hash_prefixes (EphyGSBStorage    *self,
                                      EphyGSBThreatList *list)
{
  EphySQLiteStatement *statement;
  GError *error = NULL;
  const char *sql;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (list);

  if (!self->is_operable)
    return;

  sql = "DELETE FROM hash_prefix WHERE "
        "threat_type=? AND platform_type=? AND threat_entry_type=?";
  statement = ephy_sqlite_connection_create_statement (self->db, sql, &error);
  if (error) {
    g_warning ("Failed to create delete hash prefix statement: %s", error->message);
    g_error_free (error);
    return;
  }

  if (!bind_threat_list_params (statement, list, 0, 1, 2, -1)) {
    g_object_unref (statement);
    return;
  }

  ephy_sqlite_statement_step (statement, &error);
  if (error) {
    g_warning ("Failed to execute clear hash prefix statement: %s", error->message);
    g_error_free (error);
    ephy_gsb_storage_recreate_db (self);
  }

  g_object_unref (statement);
}

static GList *
ephy_gsb_storage_get_hash_prefixes_to_delete (EphyGSBStorage    *self,
                                              EphyGSBThreatList *list,
                                              GHashTable        *indices,
                                              gsize             *num_prefixes)
{
  EphySQLiteStatement *statement;
  GError *error = NULL;
  GList *prefixes = NULL;
  const char *sql;
  guint index = 0;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (list);
  g_assert (indices);

  *num_prefixes = 0;

  if (!self->is_operable)
    return NULL;

  sql = "SELECT value FROM hash_prefix WHERE "
        "threat_type=? AND platform_type=? AND threat_entry_type=? "
        "ORDER BY value";
  statement = ephy_sqlite_connection_create_statement (self->db, sql, &error);
  if (error) {
    g_warning ("Failed to create select prefix value statement: %s", error->message);
    g_error_free (error);
    return NULL;
  }

  if (!bind_threat_list_params (statement, list, 0, 1, 2, -1)) {
    g_object_unref (statement);
    return NULL;
  }

  while (ephy_sqlite_statement_step (statement, &error)) {
    if (g_hash_table_contains (indices, GUINT_TO_POINTER (index))) {
      const guint8 *blob = ephy_sqlite_statement_get_column_as_blob (statement, 0);
      gsize size = ephy_sqlite_statement_get_column_size (statement, 0);
      prefixes = g_list_prepend (prefixes, g_bytes_new (blob, size));
      *num_prefixes += 1;
    }
    index++;
  }

  if (error) {
    g_warning ("Failed to execute select prefix value statement: %s", error->message);
    g_error_free (error);
    ephy_gsb_storage_recreate_db (self);
  }

  g_object_unref (statement);

  return prefixes;
}

static EphySQLiteStatement *
ephy_gsb_storage_make_delete_hash_prefix_statement (EphyGSBStorage *self,
                                                    gsize           num_prefixes)
{
  EphySQLiteStatement *statement;
  GError *error = NULL;
  GString *sql;

  g_assert (EPHY_IS_GSB_STORAGE (self));

  if (!self->is_operable)
    return NULL;

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
ephy_gsb_storage_delete_hash_prefixes_batch (EphyGSBStorage      *self,
                                             EphyGSBThreatList   *list,
                                             GList               *prefixes,
                                             gsize                num_prefixes,
                                             EphySQLiteStatement *stmt)
{
  EphySQLiteStatement *statement = NULL;
  GError *error = NULL;
  gboolean free_statement = TRUE;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (list);
  g_assert (prefixes);

  if (!self->is_operable)
    return NULL;

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
    ephy_gsb_storage_recreate_db (self);
  }

out:
  if (free_statement && statement)
    g_object_unref (statement);

  /* Return where we left off. */
  return prefixes;
}

static void
ephy_gsb_storage_delete_hash_prefixes_internal (EphyGSBStorage    *self,
                                                EphyGSBThreatList *list,
                                                guint32           *indices,
                                                gsize              num_indices)
{
  EphySQLiteStatement *statement = NULL;
  GList *prefixes = NULL;
  GList *head = NULL;
  GHashTable *set;
  gsize num_prefixes = 0;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (list);
  g_assert (indices);

  if (!self->is_operable)
    return;

  LOG ("Deleting %lu hash prefixes...", num_indices);

  /* Move indices from the array to a hash table set. */
  set = g_hash_table_new (g_direct_hash, g_direct_equal);
  for (gsize i = 0; i < num_indices; i++)
    g_hash_table_add (set, GUINT_TO_POINTER (indices[i]));

  prefixes = ephy_gsb_storage_get_hash_prefixes_to_delete (self, list, set, &num_prefixes);
  head = prefixes;

  ephy_gsb_storage_start_transaction (self);

  if (num_prefixes / BATCH_SIZE > 0) {
    /* Reuse statement to increase performance. */
    statement = ephy_gsb_storage_make_delete_hash_prefix_statement (self, BATCH_SIZE);

    for (gsize i = 0; i < num_prefixes / BATCH_SIZE; i++) {
      head = ephy_gsb_storage_delete_hash_prefixes_batch (self, list,
                                                          head, BATCH_SIZE,
                                                          statement);
    }
  }

  if (num_prefixes % BATCH_SIZE != 0) {
    ephy_gsb_storage_delete_hash_prefixes_batch (self, list,
                                                 head, num_prefixes % BATCH_SIZE,
                                                 NULL);
  }

  ephy_gsb_storage_end_transaction (self);

  g_hash_table_unref (set);
  g_list_free_full (prefixes, (GDestroyNotify)g_bytes_unref);
  if (statement)
    g_object_unref (statement);
}

/**
 * ephy_gsb_storage_delete_hash_prefixes:
 * @self: an #EphyGSBStorage
 * @list: an #EphyGSBThreatList
 * @tes: a ThreatEntrySet object as a #JsonObject
 *
 * Delete hash prefixes belonging to @list from the local database. Use this
 * when handling the response of a threatListUpdates:fetch request.
 **/
void
ephy_gsb_storage_delete_hash_prefixes (EphyGSBStorage    *self,
                                       EphyGSBThreatList *list,
                                       JsonObject        *tes)
{
  JsonObject *raw_indices;
  JsonObject *rice_indices;
  JsonArray *indices_arr;
  const char *compression;
  guint32 *indices;
  gsize num_indices;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (list);
  g_assert (tes);

  if (!self->is_operable)
    return;

  compression = json_object_get_string_member (tes, "compressionType");
  if (!g_strcmp0 (compression, GSB_COMPRESSION_TYPE_RICE)) {
    rice_indices = json_object_get_object_member (tes, "riceIndices");
    indices = ephy_gsb_utils_rice_delta_decode (rice_indices, &num_indices);
  } else {
    raw_indices = json_object_get_object_member (tes, "rawIndices");
    indices_arr = json_object_get_array_member (raw_indices, "indices");
    num_indices = json_array_get_length (indices_arr);

    indices = g_malloc (num_indices * sizeof (guint32));
    for (gsize i = 0; i < num_indices; i++)
      indices[i] = json_array_get_int_element (indices_arr, i);
  }

  ephy_gsb_storage_delete_hash_prefixes_internal (self, list, indices, num_indices);

  g_free (indices);
}

static EphySQLiteStatement *
ephy_gsb_storage_make_insert_hash_prefix_statement (EphyGSBStorage *self,
                                                    gsize           num_prefixes)
{
  EphySQLiteStatement *statement;
  GError *error = NULL;
  GString *sql;

  g_assert (EPHY_IS_GSB_STORAGE (self));

  if (!self->is_operable)
    return NULL;

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
ephy_gsb_storage_insert_hash_prefixes_batch (EphyGSBStorage      *self,
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
  g_assert (list);
  g_assert (prefixes);

  if (!self->is_operable)
    return;

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
    if (!ephy_sqlite_statement_bind_blob (statement, id++, prefixes + k, GSB_HASH_CUE_LEN, NULL) ||
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
    ephy_gsb_storage_recreate_db (self);
  }

out:
  if (free_statement && statement)
    g_object_unref (statement);
}

static void
ephy_gsb_storage_insert_hash_prefixes_internal (EphyGSBStorage    *self,
                                                EphyGSBThreatList *list,
                                                const guint8      *prefixes,
                                                gsize              num_prefixes,
                                                gsize              prefix_len)
{
  EphySQLiteStatement *statement = NULL;
  gsize num_batches;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (list);
  g_assert (prefixes);

  if (!self->is_operable)
    return;

  LOG ("Inserting %lu hash prefixes of size %ld...", num_prefixes, prefix_len);

  ephy_gsb_storage_start_transaction (self);

  num_batches = num_prefixes / BATCH_SIZE;
  if (num_batches > 0) {
    /* Reuse statement to increase performance. */
    statement = ephy_gsb_storage_make_insert_hash_prefix_statement (self, BATCH_SIZE);

    for (gsize i = 0; i < num_batches; i++) {
      ephy_gsb_storage_insert_hash_prefixes_batch (self, list, prefixes,
                                                   i * prefix_len * BATCH_SIZE,
                                                   (i + 1) * prefix_len * BATCH_SIZE,
                                                   prefix_len,
                                                   statement);
    }
  }

  if (num_prefixes % BATCH_SIZE != 0) {
    ephy_gsb_storage_insert_hash_prefixes_batch (self, list, prefixes,
                                                 num_batches * prefix_len * BATCH_SIZE,
                                                 num_prefixes * prefix_len - 1,
                                                 prefix_len,
                                                 NULL);
  }

  ephy_gsb_storage_end_transaction (self);

  if (statement)
    g_object_unref (statement);
}

/**
 * ephy_gsb_storage_insert_hash_prefixes:
 * @self: an #EphyGSBStorage
 * @list: an #EphyGSBThreatList
 * @tes: a ThreatEntrySet object as a #JsonObject
 *
 * Insert hash prefixes belonging to @list in the local database. Use this
 * when handling the response of a threatListUpdates:fetch request.
 **/
void
ephy_gsb_storage_insert_hash_prefixes (EphyGSBStorage    *self,
                                       EphyGSBThreatList *list,
                                       JsonObject        *tes)
{
  JsonObject *raw_hashes;
  JsonObject *rice_hashes;
  const char *compression;
  const char *prefixes_b64;
  guint32 *items = NULL;
  guint8 *prefixes;
  gsize prefixes_len;
  gsize prefix_len;
  gsize num_prefixes;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (list);
  g_assert (tes);

  if (!self->is_operable)
    return;

  compression = json_object_get_string_member (tes, "compressionType");
  if (!g_strcmp0 (compression, GSB_COMPRESSION_TYPE_RICE)) {
    rice_hashes = json_object_get_object_member (tes, "riceHashes");
    items = ephy_gsb_utils_rice_delta_decode (rice_hashes, &num_prefixes);

    prefixes = g_malloc (num_prefixes * GSB_RICE_PREFIX_LEN);
    for (gsize i = 0; i < num_prefixes; i++)
      memcpy (prefixes + i * GSB_RICE_PREFIX_LEN, &items[i], GSB_RICE_PREFIX_LEN);

    prefix_len = GSB_RICE_PREFIX_LEN;
  } else {
    raw_hashes = json_object_get_object_member (tes, "rawHashes");
    prefix_len = json_object_get_int_member (raw_hashes, "prefixSize");
    prefixes_b64 = json_object_get_string_member (raw_hashes, "rawHashes");

    prefixes = g_base64_decode (prefixes_b64, &prefixes_len);
    num_prefixes = prefixes_len / prefix_len;
  }

  ephy_gsb_storage_insert_hash_prefixes_internal (self, list, prefixes, num_prefixes, prefix_len);

  g_free (items);
  g_free (prefixes);
}

/**
 * ephy_gsb_storage_lookup_hash_prefixes:
 * @self: an #EphyGSBStorage
 * @cues: a #GList of hash cues as #GBytes
 *
 * Retrieve the hash prefixes and their negative cache expiration time from the
 * local database that begin with the hash cues in @cues. The hash cue length is
 * specified by the GSB_HASH_CUE_LEN macro.
 *
 * Return value: (element-type #EphyGSBHashPrefixLookup) (transfer-full):
 *               a #GList containing the lookup result.  The caller takes
 *               ownership of the list and its content. Use g_list_free_full()
 *               with ephy_gsb_hash_prefix_lookup_free() as free_func when done
 *               using the list.
 **/
GList *
ephy_gsb_storage_lookup_hash_prefixes (EphyGSBStorage *self,
                                       GList          *cues)
{
  EphySQLiteStatement *statement;
  GError *error = NULL;
  GList *retval = NULL;
  GString *sql;
  guint id = 0;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (cues);

  if (!self->is_operable)
    return NULL;

  sql = g_string_new ("SELECT value, negative_expires_at <= (CAST(strftime('%s', 'now') AS INT)) "
                      "FROM hash_prefix WHERE cue IN (");
  for (GList *l = cues; l && l->data; l = l->next)
    g_string_append (sql, "?,");
  /* Replace trailing comma character with close parenthesis character. */
  g_string_overwrite (sql, sql->len - 1, ")");

  statement = ephy_sqlite_connection_create_statement (self->db, sql->str, &error);
  g_string_free (sql, TRUE);

  if (error) {
    g_warning ("Failed to create select hash prefix statement: %s", error->message);
    g_error_free (error);
    return NULL;
  }

  for (GList *l = cues; l && l->data; l = l->next) {
    ephy_sqlite_statement_bind_blob (statement, id++,
                                     g_bytes_get_data (l->data, NULL), GSB_HASH_CUE_LEN,
                                     &error);
    if (error) {
      g_warning ("Failed to bind cue value as blob: %s", error->message);
      g_error_free (error);
      g_object_unref (statement);
      return NULL;
    }
  }

  while (ephy_sqlite_statement_step (statement, &error)) {
    const guint8 *blob = ephy_sqlite_statement_get_column_as_blob (statement, 0);
    gsize size = ephy_sqlite_statement_get_column_size (statement, 0);
    gboolean negative_expired = ephy_sqlite_statement_get_column_as_boolean (statement, 1);
    retval = g_list_prepend (retval, ephy_gsb_hash_prefix_lookup_new (blob, size, negative_expired));
  }

  if (error) {
    g_warning ("Failed to execute select hash prefix statement: %s", error->message);
    g_error_free (error);
    g_list_free_full (retval, (GDestroyNotify)ephy_gsb_hash_prefix_lookup_free);
    retval = NULL;
    ephy_gsb_storage_recreate_db (self);
  }

  g_object_unref (statement);

  return g_list_reverse (retval);
}

/**
 * ephy_gsb_storage_lookup_full_hashes:
 * @self: an #EphyGSBStorage
 * @hashes: a #GList of full hashes as #GBytes
 *
 * Retrieve the full hashes together with their positive cache expiration time
 * and threat parameters from the local database that match any of the hashes
 * in @hashes.
 *
 * Return value: (element-type #EphyGSBHashFullLookup) (transfer-full):
 *               a #GList containing the lookup result.  The caller takes
 *               ownership of the list and its content. Use g_list_free_full()
 *               with ephy_gsb_hash_full_lookup_free() as free_func when done
 *               using the list.
 **/
GList *
ephy_gsb_storage_lookup_full_hashes (EphyGSBStorage *self,
                                     GList          *hashes)
{
  EphySQLiteStatement *statement;
  GError *error = NULL;
  GList *retval = NULL;
  GString *sql;
  guint id = 0;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (hashes);

  if (!self->is_operable)
    return NULL;

  sql = g_string_new ("SELECT value, threat_type, platform_type, threat_entry_type, "
                      "expires_at <= (CAST(strftime('%s', 'now') AS INT)) "
                      "FROM hash_full WHERE value IN (");
  for (GList *l = hashes; l && l->data; l = l->next)
    g_string_append (sql, "?,");
  /* Replace trailing comma character with close parenthesis character. */
  g_string_overwrite (sql, sql->len - 1, ")");

  statement = ephy_sqlite_connection_create_statement (self->db, sql->str, &error);
  g_string_free (sql, TRUE);

  if (error) {
    g_warning ("Failed to create select full hash statement: %s", error->message);
    g_error_free (error);
    return NULL;
  }

  for (GList *l = hashes; l && l->data; l = l->next) {
    ephy_sqlite_statement_bind_blob (statement, id++,
                                     g_bytes_get_data (l->data, NULL), GSB_HASH_SIZE,
                                     &error);
    if (error) {
      g_warning ("Failed to bind hash value as blob: %s", error->message);
      g_error_free (error);
      g_object_unref (statement);
      return NULL;
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
    g_error_free (error);
    g_list_free_full (retval, (GDestroyNotify)ephy_gsb_hash_full_lookup_free);
    retval = NULL;
    ephy_gsb_storage_recreate_db (self);
  }

  g_object_unref (statement);

  return g_list_reverse (retval);
}

/**
 * ephy_gsb_storage_insert_full_hash:
 * @self: an #EphyGSBStorage
 * @list: an #EphyGSBThreatList
 * @hash: the full SHA256 hash
 * @duration: the positive cache duration
 *
 * Insert a full hash belonging to @list in the local database. Use this
 * when handling the response from a fullHashes:find request. If @hash
 * already exists in the database and belongs to @list, then only the
 * duration is updated. Otherwise, a new record is created.
 **/
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
  g_assert (list);
  g_assert (hash);

  if (!self->is_operable)
    return;

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
    ephy_gsb_storage_recreate_db (self);
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
  if (error) {
    g_warning ("Failed to execute insert full hash statement: %s", error->message);
    ephy_gsb_storage_recreate_db (self);
  }

out:
  if (statement)
    g_object_unref (statement);
  if (error)
    g_error_free (error);
}

/**
 * ephy_gsb_storage_delete_old_full_hashes:
 * @self: an #EphyGSBStorage
 *
 * Delete long expired full hashes from the local database. The expiration
 * threshold is specified by the EXPIRATION_THRESHOLD macro.
 **/
void
ephy_gsb_storage_delete_old_full_hashes (EphyGSBStorage *self)
{
  EphySQLiteStatement *statement;
  GError *error = NULL;
  const char *sql;

  g_assert (EPHY_IS_GSB_STORAGE (self));

  if (!self->is_operable)
    return;

  LOG ("Deleting full hashes expired for more than %d seconds", EXPIRATION_THRESHOLD);

  sql = "DELETE FROM hash_full "
        "WHERE expires_at <= (CAST(strftime('%s', 'now') AS INT)) - ?";
  statement = ephy_sqlite_connection_create_statement (self->db, sql, &error);
  if (error) {
    g_warning ("Failed to create delete full hash statement: %s", error->message);
    g_error_free (error);
    return;
  }

  ephy_sqlite_statement_bind_int64 (statement, 0, EXPIRATION_THRESHOLD, &error);
  if (error) {
    g_warning ("Failed to bind int64 in delete full hash statement: %s", error->message);
    g_error_free (error);
    g_object_unref (statement);
    return;
  }

  ephy_sqlite_statement_step (statement, &error);
  if (error) {
    g_warning ("Failed to execute delete full hash statement: %s", error->message);
    g_error_free (error);
    ephy_gsb_storage_recreate_db (self);
  }

  g_object_unref (statement);
}

/**
 * ephy_gsb_storage_update_hash_prefix_expiration:
 * @self: an #EphyGSBStorage
 * @prefix: the hash prefix
 * @duration: the negative cache duration
 *
 * Update the negative cache expiration time of a hash prefix in the local database.
 **/
void
ephy_gsb_storage_update_hash_prefix_expiration (EphyGSBStorage *self,
                                                GBytes         *prefix,
                                                gint64          duration)
{
  EphySQLiteStatement *statement;
  GError *error = NULL;
  const char *sql;

  g_assert (EPHY_IS_GSB_STORAGE (self));
  g_assert (prefix);

  if (!self->is_operable)
    return;

  sql = "UPDATE hash_prefix "
        "SET negative_expires_at=(CAST(strftime('%s', 'now') AS INT)) + ? "
        "WHERE value=?";
  statement = ephy_sqlite_connection_create_statement (self->db, sql, &error);
  if (error) {
    g_warning ("Failed to create update hash prefix statement: %s", error->message);
    g_error_free (error);
    return;
  }

  ephy_sqlite_statement_bind_int64 (statement, 0, duration, &error);
  if (error) {
    g_warning ("Failed to bind int64 in update hash prefix statement: %s", error->message);
    g_error_free (error);
    g_object_unref (statement);
    return;
  }
  ephy_sqlite_statement_bind_blob (statement, 1,
                                   g_bytes_get_data (prefix, NULL),
                                   g_bytes_get_size (prefix),
                                   &error);
  if (error) {
    g_warning ("Failed to bind blob in update hash prefix statement: %s", error->message);
    g_error_free (error);
    g_object_unref (statement);
    return;
  }

  ephy_sqlite_statement_step (statement, &error);
  if (error) {
    g_warning ("Failed to execute update hash prefix statement: %s", error->message);
    g_error_free (error);
    ephy_gsb_storage_recreate_db (self);
  }

  g_object_unref (statement);
}
