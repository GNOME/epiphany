/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2026 Epiphany Developers
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

#include <math.h>

#include <glib.h>
#include <json-glib/json-glib.h>

#include "ephy-debug.h"
#include "ephy-file-helpers.h"
#include "ephy-password-manager.h"
#include "ephy-password-record.h"
#include "ephy-settings.h"
#include "ephy-sync-crypto.h"
#include "ephy-sync-utils.h"
#include "ephy-synchronizable-manager.h"
#include "ephy-synchronizable.h"

static void
test_password_record_properties (void)
{
  g_autoptr (EphyPasswordRecord) record = NULL;
  g_autoptr (EphyPasswordRecord) copy = NULL;
  const char *id = "{12345678-1234-1234-1234-123456789abc}";
  const char *origin = "https://example.com";
  const char *target_origin = "https://example.com/login";
  const char *username = "testuser";
  const char *password = "secretpass";
  const char *username_field = "user_input";
  const char *password_field = "pass_input";
  guint64 time_created = 1000000000;
  guint64 time_password_changed = 2000000000;

  record = ephy_password_record_new (id, origin, target_origin,
                                     username, password,
                                     username_field, password_field,
                                     time_created, time_password_changed);
  g_assert_nonnull (record);
  g_assert_true (EPHY_IS_PASSWORD_RECORD (record));
  g_assert_true (EPHY_IS_SYNCHRONIZABLE (record));

  g_assert_cmpstr (ephy_password_record_get_id (record), ==, id);
  g_assert_cmpstr (ephy_synchronizable_get_id (EPHY_SYNCHRONIZABLE (record)), ==, id);
  g_assert_cmpstr (ephy_password_record_get_origin (record), ==, origin);
  g_assert_cmpstr (ephy_password_record_get_target_origin (record), ==, target_origin);
  g_assert_cmpstr (ephy_password_record_get_username (record), ==, username);
  g_assert_cmpstr (ephy_password_record_get_password (record), ==, password);
  g_assert_cmpstr (ephy_password_record_get_username_field (record), ==, username_field);
  g_assert_cmpstr (ephy_password_record_get_password_field (record), ==, password_field);
  g_assert_cmpuint (ephy_password_record_get_time_password_changed (record), ==, time_password_changed);

  /* Setters */
  ephy_password_record_set_username (record, "newuser");
  g_assert_cmpstr (ephy_password_record_get_username (record), ==, "newuser");

  ephy_password_record_set_password (record, "newpass");
  g_assert_cmpstr (ephy_password_record_get_password (record), ==, "newpass");

  /* Server time modified */
  ephy_synchronizable_set_server_time_modified (EPHY_SYNCHRONIZABLE (record), 55555);
  g_assert_cmpint (ephy_synchronizable_get_server_time_modified (EPHY_SYNCHRONIZABLE (record)), ==, 55555);

  /* Copy */
  copy = ephy_password_record_copy (record);
  g_assert_nonnull (copy);
  g_assert_true (copy != record);
  g_assert_cmpstr (ephy_password_record_get_id (copy), ==, id);
  g_assert_cmpstr (ephy_password_record_get_origin (copy), ==, origin);
  g_assert_cmpstr (ephy_password_record_get_target_origin (copy), ==, target_origin);
  g_assert_cmpstr (ephy_password_record_get_username (copy), ==, "newuser");
  g_assert_cmpstr (ephy_password_record_get_password (copy), ==, "newpass");
  g_assert_cmpint (ephy_synchronizable_get_server_time_modified (EPHY_SYNCHRONIZABLE (copy)), ==, 55555);
}

static void
test_password_record_json_serialization_standard (void)
{
  g_autoptr (EphyPasswordRecord) record = NULL;
  g_autoptr (EphyPasswordRecord) deserialized = NULL;
  g_autofree char *json_str = NULL;
  g_autoptr (GError) error = NULL;
  const char *id = "{abcd-1234}";
  const char *origin = "https://accounts.example.org";
  const char *target_origin = "https://accounts.example.org/auth";
  const char *username = "alice";
  const char *password = "password123";
  const char *username_field = "login";
  const char *password_field = "pwd";

  record = ephy_password_record_new (id, origin, target_origin,
                                     username, password,
                                     username_field, password_field,
                                     1000, 2000);

  json_str = json_gobject_to_data (G_OBJECT (record), NULL);
  g_assert_nonnull (json_str);

  /* Verify deserialization */
  deserialized = EPHY_PASSWORD_RECORD (json_gobject_from_data (EPHY_TYPE_PASSWORD_RECORD, json_str, -1, &error));
  g_assert_no_error (error);
  g_assert_nonnull (deserialized);

  g_assert_cmpstr (ephy_password_record_get_id (deserialized), ==, id);
  g_assert_cmpstr (ephy_password_record_get_origin (deserialized), ==, origin);
  g_assert_cmpstr (ephy_password_record_get_target_origin (deserialized), ==, target_origin);
  g_assert_cmpstr (ephy_password_record_get_username (deserialized), ==, username);
  g_assert_cmpstr (ephy_password_record_get_password (deserialized), ==, password);
  g_assert_cmpstr (ephy_password_record_get_username_field (deserialized), ==, username_field);
  g_assert_cmpstr (ephy_password_record_get_password_field (deserialized), ==, password_field);
}

static void
test_password_record_json_null_and_empty_fields (void)
{
  g_autoptr (EphyPasswordRecord) record = NULL;
  g_autoptr (EphyPasswordRecord) deserialized = NULL;
  g_autofree char *json_str = NULL;
  g_autoptr (GError) error = NULL;
  const char *firefox_json = "{"
                             "\"id\":\"{firefox-uuid}\","
                             "\"hostname\":\"https://example.com\","
                             "\"formSubmitURL\":\"\","
                             "\"username\":\"user1\","
                             "\"password\":\"secret\","
                             "\"usernameField\":\"\","
                             "\"passwordField\":\"\","
                             "\"timeCreated\":100,"
                             "\"timePasswordChanged\":200"
                             "}";

  /* 1. Test deserialization of empty string fields from Firefox */
  deserialized = EPHY_PASSWORD_RECORD (json_gobject_from_data (EPHY_TYPE_PASSWORD_RECORD, firefox_json, -1, &error));
  g_assert_no_error (error);
  g_assert_nonnull (deserialized);

  g_assert_cmpstr (ephy_password_record_get_id (deserialized), ==, "{firefox-uuid}");
  g_assert_cmpstr (ephy_password_record_get_origin (deserialized), ==, "https://example.com");
  /* Empty formSubmitURL/usernameField/passwordField should deserialize to NULL */
  g_assert_null (ephy_password_record_get_target_origin (deserialized));
  g_assert_null (ephy_password_record_get_username_field (deserialized));
  g_assert_null (ephy_password_record_get_password_field (deserialized));

  /* 2. Test serialization with NULL fields -> must produce empty strings for Firefox Sync */
  record = ephy_password_record_new ("{local-uuid}", "https://example.com", NULL,
                                     "user1", "secret", NULL, NULL,
                                     100, 200);
  json_str = json_gobject_to_data (G_OBJECT (record), NULL);
  g_assert_nonnull (json_str);

  {
    g_autoptr (JsonNode) node = json_from_string (json_str, &error);
    JsonObject *obj;

    g_assert_no_error (error);
    g_assert_nonnull (node);
    obj = json_node_get_object (node);
    g_assert_nonnull (obj);

    /* Should serialize NULL string fields as "" empty strings */
    g_assert_cmpstr (json_object_get_string_member (obj, "formSubmitURL"), ==, "");
    g_assert_cmpstr (json_object_get_string_member (obj, "usernameField"), ==, "");
    g_assert_cmpstr (json_object_get_string_member (obj, "passwordField"), ==, "");
  }
}

static void
test_password_record_bso_roundtrip (void)
{
  SyncCryptoKeyBundle *bundle;
  g_autoptr (EphyPasswordRecord) record = NULL;
  g_autoptr (EphyPasswordRecord) remote = NULL;
  JsonNode *bso_node = NULL;
  gboolean is_deleted = TRUE;
  const char *id = "{test-bso-uuid}";
  const char *origin = "https://mysite.org";
  const char *target_origin = "https://mysite.org/login";
  const char *username = "bob";
  const char *password = "supersecret";

  /* 32 zero bytes in base64 */
  bundle = ephy_sync_crypto_key_bundle_new ("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",
                                            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=");
  g_assert_nonnull (bundle);

  record = ephy_password_record_new (id, origin, target_origin,
                                     username, password,
                                     "user_f", "pass_f",
                                     1234567, 7654321);
  ephy_synchronizable_set_server_time_modified (EPHY_SYNCHRONIZABLE (record), 1600000);

  /* Convert to BSO */
  bso_node = ephy_synchronizable_to_bso (EPHY_SYNCHRONIZABLE (record), bundle);
  g_assert_nonnull (bso_node);
  g_assert_true (JSON_NODE_HOLDS_OBJECT (bso_node));

  /* Server adds modified timestamp when returning BSOs */
  json_object_set_double_member (json_node_get_object (bso_node), "modified", 1600000.0);

  /* Reconstruct from BSO */
  remote = EPHY_PASSWORD_RECORD (ephy_synchronizable_from_bso (bso_node,
                                                               EPHY_TYPE_PASSWORD_RECORD,
                                                               bundle,
                                                               &is_deleted));
  g_assert_nonnull (remote);
  g_assert_false (is_deleted);
  g_assert_cmpstr (ephy_password_record_get_id (remote), ==, id);
  g_assert_cmpstr (ephy_password_record_get_origin (remote), ==, origin);
  g_assert_cmpstr (ephy_password_record_get_target_origin (remote), ==, target_origin);
  g_assert_cmpstr (ephy_password_record_get_username (remote), ==, username);
  g_assert_cmpstr (ephy_password_record_get_password (remote), ==, password);
  g_assert_cmpstr (ephy_password_record_get_username_field (remote), ==, "user_f");
  g_assert_cmpstr (ephy_password_record_get_password_field (remote), ==, "pass_f");
  g_assert_cmpint (ephy_synchronizable_get_server_time_modified (EPHY_SYNCHRONIZABLE (remote)), ==, 1600000);

  json_node_unref (bso_node);
  ephy_sync_crypto_key_bundle_free (bundle);
}

static void
test_password_record_bso_deleted (void)
{
  SyncCryptoKeyBundle *bundle;
  g_autoptr (EphyPasswordRecord) record = NULL;
  JsonNode *bso_node = NULL;
  g_autoptr (JsonBuilder) builder = NULL;
  g_autofree char *encrypted_payload = NULL;
  gboolean is_deleted = FALSE;
  const char *id = "{deleted-record-uuid}";

  bundle = ephy_sync_crypto_key_bundle_new ("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",
                                            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=");
  g_assert_nonnull (bundle);

  /* Build deleted payload JSON: {"id": "{deleted-record-uuid}", "deleted": true} */
  encrypted_payload = ephy_sync_crypto_encrypt_record ("{\"id\":\"{deleted-record-uuid}\",\"deleted\":true}", bundle);
  g_assert_nonnull (encrypted_payload);

  builder = json_builder_new ();
  json_builder_begin_object (builder);
  json_builder_set_member_name (builder, "id");
  json_builder_add_string_value (builder, id);
  json_builder_set_member_name (builder, "modified");
  json_builder_add_double_value (builder, 12345.67);
  json_builder_set_member_name (builder, "payload");
  json_builder_add_string_value (builder, encrypted_payload);
  json_builder_end_object (builder);

  bso_node = json_builder_get_root (builder);
  g_assert_nonnull (bso_node);

  record = EPHY_PASSWORD_RECORD (ephy_synchronizable_from_bso (bso_node,
                                                               EPHY_TYPE_PASSWORD_RECORD,
                                                               bundle,
                                                               &is_deleted));
  g_assert_nonnull (record);
  g_assert_true (is_deleted);
  g_assert_cmpstr (ephy_password_record_get_id (record), ==, id);

  json_node_unref (bso_node);
  ephy_sync_crypto_key_bundle_free (bundle);
}

static void
test_password_manager_secret_schema (void)
{
  const SecretSchema *schema = ephy_password_manager_get_password_schema ();

  g_assert_nonnull (schema);
  g_assert_cmpstr (schema->name, ==, "org.epiphany.FormPassword");
  g_assert_cmpint (schema->flags, ==, SECRET_SCHEMA_NONE);

  /* Ensure the expected attribute keys are present in schema */
  g_assert_cmpstr (schema->attributes[0].name, ==, ID_KEY);
  g_assert_cmpstr (schema->attributes[1].name, ==, ORIGIN_KEY);
  g_assert_cmpstr (schema->attributes[2].name, ==, TARGET_ORIGIN_KEY);
  g_assert_cmpstr (schema->attributes[3].name, ==, USERNAME_FIELD_KEY);
  g_assert_cmpstr (schema->attributes[4].name, ==, PASSWORD_FIELD_KEY);
  g_assert_cmpstr (schema->attributes[5].name, ==, USERNAME_KEY);
  g_assert_cmpstr (schema->attributes[6].name, ==, SERVER_TIME_MODIFIED_KEY);
}

static void
test_password_synchronizable_manager_interface (void)
{
  g_autoptr (EphyPasswordManager) manager = ephy_password_manager_new ();
  EphySynchronizableManager *iface;

  g_assert_nonnull (manager);
  g_assert_true (EPHY_IS_PASSWORD_MANAGER (manager));
  g_assert_true (EPHY_IS_SYNCHRONIZABLE_MANAGER (manager));

  iface = EPHY_SYNCHRONIZABLE_MANAGER (manager);

  g_assert_cmpstr (ephy_synchronizable_manager_get_collection_name (iface), ==, "passwords");
  g_assert_true (ephy_synchronizable_manager_get_synchronizable_type (iface) == EPHY_TYPE_PASSWORD_RECORD);
}

static void
test_password_manager_initial_merge_identical (void)
{
  g_autoptr (EphyPasswordManager) manager = ephy_password_manager_new ();
  g_autoptr (GPtrArray) to_upload = NULL;
  GList *local_records = NULL;
  GList *remote_records = NULL;
  EphyPasswordRecord *local;
  EphyPasswordRecord *remote;

  local = ephy_password_record_new ("{uuid-1}", "https://example.com", "https://example.com/login",
                                    "alice", "password", "user", "pass",
                                    1000, 1000);
  remote = ephy_password_record_new ("{uuid-1}", "https://example.com", "https://example.com/login",
                                     "alice", "password", "user", "pass",
                                     1000, 1000);

  local_records = g_list_append (local_records, local);
  remote_records = g_list_append (remote_records, remote);

  to_upload = ephy_password_manager_handle_initial_merge (manager, local_records, remote_records);
  g_assert_nonnull (to_upload);
  /* Same ID and same password -> nothing needs to be uploaded */
  g_assert_cmpuint (to_upload->len, ==, 0);

  g_list_free_full (local_records, g_object_unref);
  g_list_free_full (remote_records, g_object_unref);
}

static void
test_password_manager_initial_merge_local_newer (void)
{
  g_autoptr (EphyPasswordManager) manager = ephy_password_manager_new ();
  g_autoptr (GPtrArray) to_upload = NULL;
  GList *local_records = NULL;
  GList *remote_records = NULL;
  EphyPasswordRecord *local;
  EphyPasswordRecord *remote;

  /* Local is newer (time=2000 vs 1000) */
  local = ephy_password_record_new ("{uuid-1}", "https://example.com", "https://example.com/login",
                                    "alice", "newpassword", "user", "pass",
                                    1000, 2000);
  remote = ephy_password_record_new ("{uuid-1}", "https://example.com", "https://example.com/login",
                                     "alice", "oldpassword", "user", "pass",
                                     1000, 1000);

  local_records = g_list_append (local_records, local);
  remote_records = g_list_append (remote_records, remote);

  to_upload = ephy_password_manager_handle_initial_merge (manager, local_records, remote_records);
  g_assert_nonnull (to_upload);
  /* Local is newer -> must upload to server */
  g_assert_cmpuint (to_upload->len, ==, 1);
  g_assert_cmpstr (ephy_password_record_get_id (g_ptr_array_index (to_upload, 0)), ==, "{uuid-1}");
  g_assert_cmpstr (ephy_password_record_get_password (g_ptr_array_index (to_upload, 0)), ==, "newpassword");

  g_list_free_full (local_records, g_object_unref);
  g_list_free_full (remote_records, g_object_unref);
}

static void
test_password_manager_initial_merge_tuple_match_local_newer (void)
{
  g_autoptr (EphyPasswordManager) manager = ephy_password_manager_new ();
  g_autoptr (GPtrArray) to_upload = NULL;
  GList *local_records = NULL;
  GList *remote_records = NULL;
  EphyPasswordRecord *local;
  EphyPasswordRecord *remote;

  /* Different IDs, same tuple (origin, target, username, fields). Local is newer (time=5000 vs 2000) */
  local = ephy_password_record_new ("{local-uuid}", "https://example.com", "https://example.com/login",
                                    "alice", "localsecret", "user", "pass",
                                    1000, 5000);
  remote = ephy_password_record_new ("{remote-uuid}", "https://example.com", "https://example.com/login",
                                     "alice", "remotesecret", "user", "pass",
                                     1000, 2000);

  local_records = g_list_append (local_records, local);
  remote_records = g_list_append (remote_records, remote);

  to_upload = ephy_password_manager_handle_initial_merge (manager, local_records, remote_records);
  g_assert_nonnull (to_upload);
  /* Local is newer -> local record must be uploaded */
  g_assert_cmpuint (to_upload->len, ==, 1);
  g_assert_cmpstr (ephy_password_record_get_id (g_ptr_array_index (to_upload, 0)), ==, "{local-uuid}");

  g_list_free_full (local_records, g_object_unref);
  g_list_free_full (remote_records, g_object_unref);
}

static void
test_password_manager_initial_merge_new_local_and_remote (void)
{
  g_autoptr (EphyPasswordManager) manager = ephy_password_manager_new ();
  g_autoptr (GPtrArray) to_upload = NULL;
  GList *local_records = NULL;
  GList *remote_records = NULL;
  EphyPasswordRecord *local;
  EphyPasswordRecord *remote;

  /* Different sites: local-only and remote-only */
  local = ephy_password_record_new ("{local-only}", "https://siteA.com", "https://siteA.com/login",
                                    "alice", "passA", "user", "pass",
                                    1000, 1000);
  remote = ephy_password_record_new ("{remote-only}", "https://siteB.com", "https://siteB.com/login",
                                     "bob", "passB", "user", "pass",
                                     1000, 1000);

  local_records = g_list_append (local_records, local);
  remote_records = g_list_append (remote_records, remote);

  to_upload = ephy_password_manager_handle_initial_merge (manager, local_records, remote_records);
  g_assert_nonnull (to_upload);
  /* Local-only record must be scheduled for upload */
  g_assert_cmpuint (to_upload->len, ==, 1);
  g_assert_cmpstr (ephy_password_record_get_id (g_ptr_array_index (to_upload, 0)), ==, "{local-only}");

  g_list_free_full (local_records, g_object_unref);
  g_list_free_full (remote_records, g_object_unref);
}

static void
test_password_manager_regular_merge_deleted (void)
{
  g_autoptr (EphyPasswordManager) manager = ephy_password_manager_new ();
  g_autoptr (GPtrArray) to_upload = NULL;
  GList *local_records = NULL;
  GList *deleted_records = NULL;
  EphyPasswordRecord *keep;
  EphyPasswordRecord *del;
  EphyPasswordRecord *remote_del;

  keep = ephy_password_record_new ("{uuid-keep}", "https://siteA.com", "https://siteA.com",
                                   "alice", "passA", "u", "p", 100, 100);
  del = ephy_password_record_new ("{uuid-del}", "https://siteB.com", "https://siteB.com",
                                  "bob", "passB", "u", "p", 100, 100);
  remote_del = ephy_password_record_new ("{uuid-del}", "https://siteB.com", "https://siteB.com",
                                         "bob", "passB", "u", "p", 100, 100);

  local_records = g_list_append (local_records, keep);
  local_records = g_list_append (local_records, del);
  deleted_records = g_list_append (deleted_records, remote_del);

  to_upload = ephy_password_manager_handle_regular_merge (manager, &local_records, deleted_records, NULL);
  g_assert_nonnull (to_upload);
  /* Only {uuid-del} should have been removed from local_records; {uuid-keep} must remain */
  g_assert_cmpuint (g_list_length (local_records), ==, 1);
  g_assert_cmpstr (ephy_password_record_get_id (local_records->data), ==, "{uuid-keep}");

  g_list_free_full (local_records, g_object_unref);
  g_list_free_full (deleted_records, g_object_unref);
}

static void
test_password_manager_deduplicate_records (void)
{
  g_autoptr (EphyPasswordManager) manager = ephy_password_manager_new ();
  GList *records = NULL;
  GList *newest_list = NULL;
  EphyPasswordRecord *oldest;
  EphyPasswordRecord *middle;
  EphyPasswordRecord *newest;

  oldest = ephy_password_record_new ("{uuid-old}", "https://site.com", "https://site.com",
                                     "user", "oldpass", "u", "p", 100, 1000);
  middle = ephy_password_record_new ("{uuid-mid}", "https://site.com", "https://site.com",
                                     "user", "midpass", "u", "p", 100, 2000);
  newest = ephy_password_record_new ("{uuid-new}", "https://site.com", "https://site.com",
                                     "user", "newpass", "u", "p", 100, 3000);

  records = g_list_append (records, oldest);
  records = g_list_append (records, newest);
  records = g_list_append (records, middle);

  newest_list = ephy_password_manager_deduplicate_records (manager, records);
  g_assert_nonnull (newest_list);
  g_assert_cmpuint (g_list_length (newest_list), ==, 1);
  g_assert_cmpstr (ephy_password_record_get_id (newest_list->data), ==, "{uuid-new}");
  g_assert_cmpuint (ephy_password_record_get_time_password_changed (newest_list->data), ==, 3000);

  g_list_free_full (newest_list, g_object_unref);
}

static void
test_sync_utils_log_sync_change (void)
{
  g_autofree char *log_path = NULL;
  g_autofree char *contents = NULL;
  g_autoptr (GError) error = NULL;

  log_path = g_build_filename (ephy_profile_dir (), "sync-changes.log", NULL);
  g_assert_nonnull (log_path);

  /* By default, sync debug logging is disabled */
  g_assert_false (ephy_sync_utils_debug_log_is_enabled ());
  ephy_sync_utils_log_sync_change ("Disabled test entry");
  g_assert_false (g_file_test (log_path, G_FILE_TEST_EXISTS));

  /* Enable sync debug logging */
  g_settings_set_boolean (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_DEBUG_LOG_ENABLED, TRUE);
  g_assert_true (ephy_sync_utils_debug_log_is_enabled ());

  ephy_sync_utils_log_sync_change ("Unit test log entry for record %s", "{test-log-uuid}");

  g_assert_true (g_file_test (log_path, G_FILE_TEST_EXISTS));

  g_file_get_contents (log_path, &contents, NULL, &error);
  g_assert_no_error (error);
  g_assert_nonnull (contents);
  g_assert_nonnull (strstr (contents, "Unit test log entry for record {test-log-uuid}"));

  /* Cleanup */
  g_settings_reset (EPHY_SETTINGS_SYNC, EPHY_PREFS_SYNC_DEBUG_LOG_ENABLED);
}

static void
test_synchronizable_to_debug_string (void)
{
  g_autoptr (EphyPasswordRecord) record = NULL;
  g_autofree char *debug_str = NULL;

  record = ephy_password_record_new ("{test-id}",
                                     "https://example.com",
                                     "https://example.com",
                                     "testuser",
                                     "supersecretpassword",
                                     "user_field",
                                     "pass_field",
                                     1000,
                                     2000);
  debug_str = ephy_synchronizable_to_debug_string (EPHY_SYNCHRONIZABLE (record));
  g_assert_nonnull (debug_str);
  g_assert_nonnull (strstr (debug_str, "\"id\":\"{test-id}\""));
  g_assert_nonnull (strstr (debug_str, "\"hostname\":\"https://example.com\""));
  g_assert_nonnull (strstr (debug_str, "\"username\":\"testuser\""));
  g_assert_null (strstr (debug_str, "supersecretpassword"));
  g_assert_null (strstr (debug_str, "\"password\""));
}

int
main (int   argc,
      char *argv[])
{
  int ret;

  g_setenv ("GSETTINGS_BACKEND", "memory", TRUE);
  ephy_debug_init ();

  g_test_init (&argc, &argv, NULL);

  if (!ephy_file_helpers_init (NULL,
                               EPHY_FILE_HELPERS_TESTING_MODE | EPHY_FILE_HELPERS_ENSURE_EXISTS,
                               NULL)) {
    g_debug ("Something wrong happened with ephy_file_helpers_init()");
    return -1;
  }

  g_test_add_func ("/lib/sync/password-record/properties", test_password_record_properties);
  g_test_add_func ("/lib/sync/password-record/json-standard", test_password_record_json_serialization_standard);
  g_test_add_func ("/lib/sync/password-record/json-null-and-empty", test_password_record_json_null_and_empty_fields);
  g_test_add_func ("/lib/sync/password-record/bso-roundtrip", test_password_record_bso_roundtrip);
  g_test_add_func ("/lib/sync/password-record/bso-deleted", test_password_record_bso_deleted);
  g_test_add_func ("/lib/sync/password-manager/secret-schema", test_password_manager_secret_schema);
  g_test_add_func ("/lib/sync/password-manager/synchronizable-interface", test_password_synchronizable_manager_interface);
  g_test_add_func ("/lib/sync/password-manager/initial-merge-identical", test_password_manager_initial_merge_identical);
  g_test_add_func ("/lib/sync/password-manager/initial-merge-local-newer", test_password_manager_initial_merge_local_newer);
  g_test_add_func ("/lib/sync/password-manager/initial-merge-tuple-match", test_password_manager_initial_merge_tuple_match_local_newer);
  g_test_add_func ("/lib/sync/password-manager/initial-merge-new-records", test_password_manager_initial_merge_new_local_and_remote);
  g_test_add_func ("/lib/sync/password-manager/regular-merge-deleted", test_password_manager_regular_merge_deleted);
  g_test_add_func ("/lib/sync/password-manager/deduplicate-records", test_password_manager_deduplicate_records);
  g_test_add_func ("/lib/sync/utils/log-sync-change", test_sync_utils_log_sync_change);
  g_test_add_func ("/lib/sync/synchronizable/debug-string", test_synchronizable_to_debug_string);

  ret = g_test_run ();

  ephy_file_helpers_shutdown ();

  return ret;
}
