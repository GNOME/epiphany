/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*
 *  Copyright © 2009 Xan López
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

#include "ephy-bookmarks-manager.h"
#include "ephy-debug.h"
#include "ephy-file-helpers.h"
#include "ephy-history-service.h"
#include "ephy-password-manager.h"
#include "ephy-prefs.h"
#include "ephy-profile-utils.h"
#include "ephy-search-engine-manager.h"
#include "ephy-settings.h"
#include "ephy-sqlite-connection.h"
#include "ephy-string.h"
#include "ephy-sync-debug.h"
#include "ephy-sync-utils.h"
#include "ephy-uri-tester-shared.h"
#include "ephy-web-app-utils.h"
#include "gvdb-builder.h"
#include "gvdb-reader.h"

#include <errno.h>
#include <fcntl.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <inttypes.h>
#include <libsecret/secret.h>
#include <libsoup/soup.h>
#include <libxml/HTMLtree.h>
#include <libxml/xmlreader.h>
#include <locale.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <webkit2/webkit2.h>

static int do_step_n = -1;
static int migration_version = -1;
static char *profile_dir = NULL;

/* The legacy dir is used by everything before version 30, which migrates
 * to the new directory.
 */
static const char *
legacy_default_profile_dir (void)
{
  static char *dir = NULL;
  if (dir == NULL)
    dir = g_build_filename (g_get_user_config_dir (), "epiphany", NULL);
  return dir;
}

static const char *
legacy_profile_dir (void)
{
  static char *dir = NULL;
  if (dir == NULL)
    {
      if (profile_dir != NULL)
        {
          dir = profile_dir;
        }
      else
        {
          dir = (char *)legacy_default_profile_dir ();
        }
    }
  return dir;
}

static gboolean
legacy_dir_is_default (void)
{
  return !strcmp (legacy_profile_dir (), legacy_default_profile_dir ());
}

/*
 * What to do to add new migration steps:
 *  - Bump EPHY_PROFILE_MIGRATION_VERSION in lib/ephy-profile-utils.h
 *  - Add your function at the end of the 'migrators' array
 */

typedef void (*EphyProfileMigrator) (void);

static gboolean
profile_dir_exists (void)
{
  if (g_file_test (ephy_profile_dir (), G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
    return TRUE;

  if (g_file_test (legacy_profile_dir (), G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
    return TRUE;

  return FALSE;
}

/* https://bugzilla.gnome.org/show_bug.cgi?id=752738 */
static void
migrate_insecure_password (SecretItem *item)
{
  GHashTable *attributes;
  WebKitSecurityOrigin *original_origin;
  const char *original_uri;

  attributes = secret_item_get_attributes (item);
  original_uri = g_hash_table_lookup (attributes, ORIGIN_KEY);
  original_origin = webkit_security_origin_new_for_uri (original_uri);
  if (original_origin == NULL) {
    g_warning ("Failed to convert URI %s to a security origin, insecure password will not be migrated", original_uri);
    g_hash_table_unref (attributes);
    return;
  }

  if (g_strcmp0 (webkit_security_origin_get_protocol (original_origin), "http") == 0) {
    WebKitSecurityOrigin *new_origin;
    char *new_uri;
    GError *error = NULL;

    new_origin = webkit_security_origin_new ("https",
                                             webkit_security_origin_get_host (original_origin),
                                             webkit_security_origin_get_port (original_origin));
    new_uri = webkit_security_origin_to_string (new_origin);
    webkit_security_origin_unref (new_origin);

    g_hash_table_replace (attributes, g_strdup (ORIGIN_KEY), new_uri);
    secret_item_set_attributes_sync (item, EPHY_FORM_PASSWORD_SCHEMA, attributes, NULL, &error);
    if (error != NULL) {
      g_warning ("Failed to convert URI %s to https://, insecure password will not be migrated: %s", original_uri, error->message);
      g_error_free (error);
    }
  }

  g_hash_table_unref (attributes);
  webkit_security_origin_unref (original_origin);
}

static void
migrate_insecure_passwords (void)
{
  SecretService *service;
  GHashTable *attributes;
  GList *items;
  int default_profile_migration_version;
  GError *error = NULL;

  /* This is ephy *profile* migrator. It runs on a per-profile basis. i.e.
   * each web app runs migrators separately. So this migration step could run
   * once for a profile dir, then again far in the future when an old web app
   * is opened. But passwords are global state, not stored in the profile dir,
   * and we want to run this migration only once. This is tricky to fix, but
   * it's easier if we relax the constraint to "never run this migrator if it
   * has been run already for the default profile dir." That's because we don't
   * really care if a couple web app passwords get converted from insecure to
   * secure, which is not a big problem and indicates the user probably never
   * uses Epiphany except for web apps anyway. We just don't want all the user's
   * passwords to get converted mysteriously because he happens to open a web
   * app. So check the migration version for the default profile dir and abort
   * if this migrator has already run there. This way we avoid adding a new flag
   * file to clutter the profile dir just to check if this migrator has run.
   */
  default_profile_migration_version = ephy_profile_utils_get_migration_version_for_profile_dir (legacy_default_profile_dir ());
  if (default_profile_migration_version >= EPHY_INSECURE_PASSWORDS_MIGRATION_VERSION) {
    LOG ("Skipping insecure password migration because default profile has already migrated");
    return;
  }

  service = secret_service_get_sync (SECRET_SERVICE_LOAD_COLLECTIONS, NULL, &error);
  if (error != NULL) {
    g_warning ("Failed to get secret service proxy, insecure passwords will not be migrated: %s", error->message);
    g_error_free (error);
    return;
  }

  attributes = secret_attributes_build (EPHY_FORM_PASSWORD_SCHEMA, NULL);

  items = secret_service_search_sync (service,
                                      EPHY_FORM_PASSWORD_SCHEMA,
                                      attributes,
                                      SECRET_SEARCH_ALL | SECRET_SEARCH_UNLOCK | SECRET_SEARCH_LOAD_SECRETS,
                                      NULL,
                                      &error);
  if (error != NULL) {
    g_warning ("Failed to search secret service, insecure passwords will not be migrated: %s", error->message);
    g_error_free (error);
    goto out;
  }

  for (GList *l = items; l != NULL; l = l->next)
    migrate_insecure_password ((SecretItem *)l->data);

  g_list_free_full (items, g_object_unref);

out:
  g_object_unref (service);
  g_hash_table_unref (attributes);
}

static void
parse_rdf_lang_tag (xmlNode  *child,
                    xmlChar **value,
                    int      *best_match)
{
  const char * const *locales;
  const char *this_language;
  xmlChar *lang;
  xmlChar *content;
  int i;

  if (*best_match == 0)
    /* there's no way we can do better */
    return;

  content = xmlNodeGetContent (child);
  if (!content)
    return;

  lang = xmlNodeGetLang (child);
  if (lang == NULL) {
    const char *translated;

    translated = _((char *)content);
    if ((char *)content != translated) {
      /* if we have a translation for the content of the
       * node, then we just use this */
      if (*value)
        xmlFree (*value);
      *value = (xmlChar *)g_strdup (translated);
      *best_match = 0;

      xmlFree (content);
      return;
    }

    this_language = "C";
  } else
    this_language = (char *)lang;

  locales = g_get_language_names ();

  for (i = 0; i < *best_match && locales[i]; i++) {
    if (!strcmp (locales[i], this_language)) {
      /* if we've already encountered a less accurate
       * translation, then free it */
      if (*value)
        xmlFree (*value);

      *value = content;
      *best_match = i;

      break;
    }
  }

  if (lang)
    xmlFree (lang);
  if (*value != content)
    xmlFree (content);
}

static void
parse_rdf_item (EphyBookmarksManager *manager,
                xmlNodePtr            node)
{
  xmlChar *title = NULL;
  int best_match_title = INT_MAX;
  xmlChar *link = NULL;
  int best_match_link = INT_MAX;
  /* we consider that it's better to use a non-localized smart link than
   * a localized link */
  gboolean use_smartlink = FALSE;
  xmlChar *subject = NULL;
  GSequence *tags;
  xmlNode *child;

  child = node->children;

  link = xmlGetProp (node, (xmlChar *)"about");

  tags = g_sequence_new (g_free);
  while (child != NULL) {
    if (xmlStrEqual (child->name, (xmlChar *)"title")) {
      parse_rdf_lang_tag (child, &title, &best_match_title);
    } else if (xmlStrEqual (child->name, (xmlChar *)"link") &&
               !use_smartlink) {
      parse_rdf_lang_tag (child, &link, &best_match_link);
    } else if (child->ns &&
               xmlStrEqual (child->ns->prefix, (xmlChar *)"ephy") &&
               xmlStrEqual (child->name, (xmlChar *)"smartlink")) {
      if (!use_smartlink) {
        use_smartlink = TRUE;
        best_match_link = INT_MAX;
      }

      parse_rdf_lang_tag (child, &link, &best_match_link);
    } else if (child->ns &&
               xmlStrEqual (child->ns->prefix, (xmlChar *)"dc") &&
               xmlStrEqual (child->name, (xmlChar *)"subject")) {
      subject = xmlNodeGetContent (child);
      if (subject) {
        g_sequence_prepend (tags, subject);
        ephy_bookmarks_manager_create_tag (manager, (const char *)subject);
      }
    }

    child = child->next;
  }

  if (link) {
    EphyBookmark *bookmark;
    char *id;

    g_sequence_sort (tags, (GCompareDataFunc)ephy_bookmark_tags_compare, NULL);
    id = ephy_sync_utils_get_random_sync_id ();
    bookmark = ephy_bookmark_new ((const char *)link, (const char *)title, tags, id);
    ephy_bookmarks_manager_add_bookmark (manager, bookmark);

    g_object_unref (bookmark);
    g_free (id);
  } else {
    g_sequence_free (tags);
  }

  xmlFree (title);
  xmlFree (link);
}

static void
migrate_bookmarks (void)
{
  EphyBookmarksManager *manager;
  char *filename;
  xmlDocPtr doc;
  xmlNodePtr child;
  xmlNodePtr root;

  filename = g_build_filename (legacy_profile_dir (),
                               EPHY_BOOKMARKS_FILE,
                               NULL);
  if (g_file_test (filename, G_FILE_TEST_EXISTS))
    goto out;
  g_free (filename);

  filename = g_build_filename (legacy_profile_dir (),
                               "bookmarks.rdf",
                               NULL);
  if (!g_file_test (filename, G_FILE_TEST_EXISTS))
    goto out;

  doc = xmlParseFile (filename);
  if (doc == NULL) {
    g_warning ("Failed to re-import the bookmarks. All bookmarks lost!");
    goto out;
  }

  manager = ephy_bookmarks_manager_new ();
  root = xmlDocGetRootElement (doc);
  child = root->children;

  while (child != NULL) {
    if (xmlStrEqual (child->name, (xmlChar *)"item")) {
      parse_rdf_item (manager, child);
    }

    child = child->next;
  }

  /* FIXME: https://bugzilla.gnome.org/show_bug.cgi?id=772668 */
  ephy_bookmarks_manager_save_to_file_async (manager, NULL,
                                             ephy_bookmarks_manager_save_to_file_warn_on_error_cb,
                                             NULL);

  xmlFreeDoc (doc);
  g_object_unref (manager);

  /* Remove old bookmarks files */
  if (g_unlink (filename) != 0)
    g_warning ("Failed to delete %s: %s", filename, g_strerror (errno));
  g_free (filename);

  filename = g_build_filename (legacy_profile_dir (),
                               "ephy-bookmarks.xml",
                               NULL);
  if (g_unlink (filename) != 0)
    g_warning ("Failed to delete %s: %s", filename, g_strerror (errno));
out:
  g_free (filename);
}

static void
migrate_adblock_filters (void)
{
  char *adblock_dir;
  char *filters_filename;
  char *contents;
  gsize content_size;
  GPtrArray *filters_array = NULL;
  GError *error = NULL;

  adblock_dir = g_build_filename (legacy_profile_dir (), "adblock", NULL);
  if (!g_file_test (adblock_dir, G_FILE_TEST_IS_DIR)) {
    g_free (adblock_dir);
    return;
  }

  if (!legacy_dir_is_default ()) {
    /* Adblock filters rules are now shared to save space */
    ephy_file_delete_dir_recursively (adblock_dir, NULL);
    g_free (adblock_dir);

    adblock_dir = g_build_filename (legacy_default_profile_dir (), "adblock", NULL);
  }

  filters_filename = g_build_filename (adblock_dir, "filters.list", NULL);
  g_free (adblock_dir);

  if (!g_file_get_contents (filters_filename, &contents, &content_size, &error)) {
    if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
      g_warning ("Failed to read filters file: %s: %s", filters_filename, error->message);
    g_free (filters_filename);
    g_error_free (error);
    return;
  }

  if (content_size > 0) {
    char **filter_list;
    guint  filter_list_length;
    guint  i;

    filter_list = g_strsplit (contents, ";", -1);
    filter_list_length = g_strv_length (filter_list);
    if (filter_list_length > 0) {
      filters_array = g_ptr_array_sized_new (MAX (2, filter_list_length) + 1);
      g_ptr_array_set_free_func (filters_array, g_free);
      g_ptr_array_add (filters_array, g_strdup (ADBLOCK_DEFAULT_FILTER_URL));

      for (i = 0; filter_list[i]; i++) {
        char *url;

        url = g_strstrip (filter_list[i]);
        if (url[0] != '\0' && strcmp (url, ADBLOCK_DEFAULT_FILTER_URL))
          g_ptr_array_add (filters_array, g_strdup (url));
      }

      if (filters_array->len == 1) {
        /* No additional filters, so do nothing. */
        g_ptr_array_free (filters_array, TRUE);
        filters_array = NULL;
      } else {
        g_ptr_array_add (filters_array, NULL);
      }
    }
    g_strfreev (filter_list);
  }

  if (filters_array) {
    g_settings_set_strv (EPHY_SETTINGS_MAIN,
                         EPHY_PREFS_ADBLOCK_FILTERS,
                         (const gchar * const *)filters_array->pdata);
    g_settings_sync ();
    g_ptr_array_free (filters_array, TRUE);
  }

  g_unlink (filters_filename);
}

static void
migrate_initial_state (void)
{
  char *filename;
  GFile *file;
  GError *error = NULL;

  /* EphyInitialState no longer exists. It's just window sizes, so "migrate" it
   * by simply removing the file to avoid cluttering the profile dir. */
  filename = g_build_filename (legacy_profile_dir (), "states.xml", NULL);
  file = g_file_new_for_path (filename);

  g_file_delete (file, NULL, &error);
  if (error != NULL) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
      g_warning ("Failed to delete %s: %s", filename, error->message);
    g_error_free (error);
  }

  g_free (filename);
  g_object_unref (file);
}

static void
migrate_permissions (void)
{
  char *filename;
  GFile *file;

  filename = g_build_filename (legacy_profile_dir (), "hosts.ini", NULL);
  file = g_file_new_for_path (filename);
  g_free (filename);

  g_file_delete (file, NULL, NULL);
  g_object_unref (file);
}

static void
migrate_icon_database (void)
{
  char *path;
  GError *error = NULL;

  /* Favicons used to be saved here by mistake. Delete them! */
  path = g_build_filename (g_get_user_cache_dir (), "icondatabase", NULL);
  ephy_file_delete_dir_recursively (path, &error);

  if (error) {
    if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
      g_warning ("Failed to delete %s: %s", path, error->message);
    g_error_free (error);
  }

  g_free (path);
}

static void
migrate_passwords_to_firefox_sync_passwords (void)
{
  GHashTable *attributes;
  GList *passwords = NULL;
  GError *error = NULL;
  int default_profile_migration_version;

  /* Similar to the insecure passwords migration, we want to migrate passwords
   * to Firefox Sync passwords only once since saved passwords are stored
   * globally and not per profile. This won't affect password lookup for web
   * apps because this migration only adds a couple of new fields to the
   * password schema, fields that are not taken into consideration when
   * querying passwords.
   */
  default_profile_migration_version = ephy_profile_utils_get_migration_version_for_profile_dir (legacy_default_profile_dir ());
  if (default_profile_migration_version >= EPHY_FIREFOX_SYNC_PASSWORDS_MIGRATION_VERSION)
    return;

  attributes = secret_attributes_build (EPHY_FORM_PASSWORD_SCHEMA, NULL);
  passwords = secret_service_search_sync (NULL, EPHY_FORM_PASSWORD_SCHEMA, attributes,
                                          SECRET_SEARCH_ALL | SECRET_SEARCH_UNLOCK | SECRET_SEARCH_LOAD_SECRETS,
                                          NULL, &error);
  if (error) {
    g_warning ("Failed to search the password schema: %s", error->message);
    goto out;
  }

  secret_service_clear_sync (NULL, EPHY_FORM_PASSWORD_SCHEMA,
                             attributes, NULL, &error);
  if (error) {
    g_warning ("Failed to clear the password schema: %s", error->message);
    goto out;
  }

  for (GList *l = passwords; l && l->data; l = l->next) {
    SecretItem *item = (SecretItem *)l->data;
    SecretValue *value = secret_item_get_secret (item);
    GHashTable *attrs = secret_item_get_attributes (item);
    const char *origin = g_hash_table_lookup (attrs, ORIGIN_KEY);
    const char *username = g_hash_table_lookup (attrs, USERNAME_KEY);
    char *uuid = g_uuid_string_random ();
    char *label;

    g_hash_table_insert (attrs, g_strdup (ID_KEY), g_strdup_printf ("{%s}", uuid));
    g_hash_table_insert (attrs, g_strdup (SERVER_TIME_MODIFIED_KEY), g_strdup ("0"));

    if (username)
      label = g_strdup_printf ("Password for %s in a form in %s", username, origin);
    else
      label = g_strdup_printf ("Password in a form in %s", origin);
    secret_service_store_sync (NULL, EPHY_FORM_PASSWORD_SCHEMA,
                               attrs, NULL, label,
                               value, NULL, &error);
    if (error) {
      g_warning ("Failed to store password: %s", error->message);
      g_clear_pointer (&error, g_error_free);
    }

    g_free (label);
    g_free (uuid);
    secret_value_unref (value);
    g_hash_table_unref (attrs);
  }

out:
  if (error)
    g_error_free (error);
  g_hash_table_unref (attributes);
  g_list_free_full (passwords, g_object_unref);
}

static void
migrate_history_to_firefox_sync_history (void)
{
  EphySQLiteConnection *history_db = NULL;
  EphySQLiteStatement *statement = NULL;
  GError *error = NULL;
  char *history_filename;
  const char *sql_query;

  history_filename = g_build_filename (legacy_profile_dir (), EPHY_HISTORY_FILE, NULL);
  if (!g_file_test (history_filename, G_FILE_TEST_EXISTS)) {
    LOG ("There is no history to migrate...");
    goto out;
  }

  history_db = ephy_sqlite_connection_new (EPHY_SQLITE_CONNECTION_MODE_READWRITE,
                                           history_filename);
  ephy_sqlite_connection_open (history_db, &error);
  if (error) {
    g_warning ("Failed to open history database: %s", error->message);
    g_clear_object (&history_db);
    goto out;
  }

  /* Add new sync_id column. All sync ids will default to NULL. */
  sql_query = "ALTER TABLE urls ADD COLUMN sync_id LONGVARCAR";
  ephy_sqlite_connection_execute (history_db, sql_query, &error);
  if (error) {
    /* SQLite gives only a generic error code if the column already exists, so
     * assume this migrator has been run already and don't print a warning. */
    goto out;
  }

  /* Update visit timestamps to microseconds. */
  sql_query = "UPDATE urls SET last_visit_time = last_visit_time * 1000000";
  ephy_sqlite_connection_execute (history_db, sql_query, &error);
  if (error) {
    g_warning ("Failed to update last visit time to microseconds: %s", error->message);
    goto out;
  }

  sql_query = "UPDATE visits SET visit_time = visit_time * 1000000";
  ephy_sqlite_connection_execute (history_db, sql_query, &error);
  if (error)
    g_warning ("Failed to update visit time to microseconds: %s", error->message);

out:
  g_free (history_filename);
  if (history_db) {
    ephy_sqlite_connection_close (history_db);
    g_object_unref (history_db);
  }
  if (statement)
    g_object_unref (statement);
  if (error)
    g_error_free (error);
}

static void
migrate_passwords_add_target_origin (void)
{
  GHashTable *attributes;
  GList *passwords = NULL;
  GError *error = NULL;
  int default_profile_migration_version;

  /* Similar to Firefox Sync and the insecure passwords migrations.
   * This is also a migration that runs once, and not for each profile
   * Adds target_origin field to all existing records,
   * with the same value as origin.
   */
  default_profile_migration_version = ephy_profile_utils_get_migration_version_for_profile_dir (legacy_default_profile_dir ());
  if (default_profile_migration_version >= EPHY_TARGET_ORIGIN_MIGRATION_VERSION)
    return;

  attributes = secret_attributes_build (EPHY_FORM_PASSWORD_SCHEMA, NULL);
  passwords = secret_service_search_sync (NULL, EPHY_FORM_PASSWORD_SCHEMA, attributes,
                                          SECRET_SEARCH_ALL | SECRET_SEARCH_UNLOCK | SECRET_SEARCH_LOAD_SECRETS,
                                          NULL, &error);
  if (error) {
    g_warning ("Failed to search the password schema: %s", error->message);
    goto out;
  }

  secret_service_clear_sync (NULL, EPHY_FORM_PASSWORD_SCHEMA,
                             attributes, NULL, &error);
  if (error) {
    g_warning ("Failed to clear the password schema: %s", error->message);
    goto out;
  }

  for (GList *l = passwords; l && l->data; l = l->next) {
    SecretItem *item = (SecretItem *)l->data;
    SecretValue *value = secret_item_get_secret (item);
    GHashTable *attrs = secret_item_get_attributes (item);
    const char *origin = g_hash_table_lookup (attrs, ORIGIN_KEY);
    const char *username = g_hash_table_lookup (attrs, USERNAME_KEY);
    const char *target_origin = g_hash_table_lookup (attrs, TARGET_ORIGIN_KEY);
    char *label;

    /* In most cases target_origin has the same value as origin
     * We don't have a way of figuring out the correct value retroactively,
     * so just use the origin value.
    */
    if (target_origin == NULL)
      g_hash_table_insert (attrs, g_strdup (TARGET_ORIGIN_KEY), g_strdup (origin));

    if (username)
      label = g_strdup_printf ("Password for %s in a form in %s", username, origin);
    else
      label = g_strdup_printf ("Password in a form in %s", origin);
    secret_service_store_sync (NULL, EPHY_FORM_PASSWORD_SCHEMA,
                               attrs, NULL, label,
                               value, NULL, &error);
    if (error) {
      g_warning ("Failed to store password: %s", error->message);
      g_clear_pointer (&error, g_error_free);
    }

    g_free (label);
    secret_value_unref (value);
    g_hash_table_unref (attrs);
  }

out:
  if (error)
    g_error_free (error);
  g_hash_table_unref (attributes);
  g_list_free_full (passwords, g_object_unref);
}

static void
migrate_sync_device_info (void)
{
  JsonObject *device;
  const char *device_id;
  const char *device_name;
  char *prev_device_id;
  char *device_bso_id;
  char *record;
  int default_profile_migration_version;

  default_profile_migration_version = ephy_profile_utils_get_migration_version_for_profile_dir (legacy_default_profile_dir ());
  if (default_profile_migration_version >= EPHY_SYNC_DEVICE_ID_MIGRATION_VERSION)
    return;

  if (!ephy_sync_utils_user_is_signed_in ())
    return;

  /* Fetch the device info from the Firefox Accounts Server. */
  device = ephy_sync_debug_get_current_device ();
  if (!device) {
    g_warning ("Failed to migrate sync device info. Sign in again to Sync "
               "to have your device re-registered and continue syncing.");
    return;
  }

  /* Erase previous records from the Sync Storage Server. */
  prev_device_id = ephy_sync_utils_get_device_id ();
  ephy_sync_debug_erase_record ("clients", prev_device_id);
  ephy_sync_debug_erase_record ("tabs", prev_device_id);

  /* Use the device id and name assigned by the Firefox Accounts Server at sign in.
   * The user can change later the device name in the Preferences dialog. */
  device_id = json_object_get_string_member (device, "id");
  ephy_sync_utils_set_device_id (device_id);
  device_name = json_object_get_string_member (device, "name");
  ephy_sync_utils_set_device_name (device_name);

  device_bso_id = ephy_sync_utils_get_device_bso_id ();
  record = ephy_sync_utils_make_client_record (device_bso_id, device_id, device_name);
  ephy_sync_debug_upload_record ("clients", device_bso_id, record);

  g_free (record);
  g_free (device_bso_id);
  g_free (prev_device_id);
  json_object_unref (device);
}

static GVariant *
convert_bookmark_timestamp (GVariant *value)
{
  GVariantBuilder builder;
  GVariantIter *iter;
  gboolean is_uploaded;
  gint64 time_added;
  gint64 timestamp_i;
  double timestamp_d;
  const char *title;
  const char *id;
  char *tag;

  if (!g_variant_check_format_string (value, "(xssdbas)", FALSE))
    return NULL;

  g_variant_get (value, "(x&s&sdbas)",
                 &time_added, &title, &id,
                 &timestamp_d, &is_uploaded, &iter);
  timestamp_i = ceil (timestamp_d);

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("(xssxbas)"));
  g_variant_builder_add (&builder, "x", time_added);
  g_variant_builder_add (&builder, "s", title);
  g_variant_builder_add (&builder, "s", id);
  g_variant_builder_add (&builder, "x", timestamp_i);
  g_variant_builder_add (&builder, "b", is_uploaded);

  g_variant_builder_open (&builder, G_VARIANT_TYPE ("as"));
  while (g_variant_iter_next (iter, "s", &tag)) {
    g_variant_builder_add (&builder, "s", tag);
    g_free (tag);
  }
  g_variant_builder_close (&builder);

  g_variant_iter_free (iter);

  return g_variant_builder_end (&builder);
}

static void
migrate_bookmarks_timestamp (void)
{
  GvdbTable *root_table_in = NULL;
  GvdbTable *bookmarks_table_in = NULL;
  GvdbTable *tags_table_in = NULL;
  GHashTable *root_table_out = NULL;
  GHashTable *bookmarks_table_out = NULL;
  GHashTable *tags_table_out = NULL;
  GError *error = NULL;
  char **tags = NULL;
  char **urls = NULL;
  char *filename;
  int length;

  filename = g_build_filename (legacy_profile_dir (), EPHY_BOOKMARKS_FILE, NULL);
  root_table_in = gvdb_table_new (filename, TRUE, &error);
  if (error) {
    g_warning ("Failed to create Gvdb table: %s", error->message);
    goto out;
  }

  bookmarks_table_in = gvdb_table_get_table (root_table_in, "bookmarks");
  if (!bookmarks_table_in) {
    g_warning ("Failed to find bookmarks inner table");
    goto out;
  }

  tags_table_in = gvdb_table_get_table (root_table_in, "tags");
  if (!tags_table_in) {
    g_warning ("Failed to find tags inner table");
    goto out;
  }

  root_table_out = gvdb_hash_table_new (NULL, NULL);
  bookmarks_table_out = gvdb_hash_table_new (root_table_out, "bookmarks");
  tags_table_out = gvdb_hash_table_new (root_table_out, "tags");

  tags = gvdb_table_get_names (tags_table_in, &length);
  for (int i = 0; i < length; i++)
    gvdb_hash_table_insert (tags_table_out, tags[i]);

  urls = gvdb_table_get_names (bookmarks_table_in, &length);
  for (int i = 0; i < length; i++) {
    GVariant *value = gvdb_table_get_value (bookmarks_table_in, urls[i]);
    GVariant *new_value = convert_bookmark_timestamp (value);
    if (new_value != NULL) {
      GvdbItem *item = gvdb_hash_table_insert (bookmarks_table_out, urls[i]);
      gvdb_item_set_value (item, new_value);
    }
    g_variant_unref (value);

    if (new_value == NULL)
      goto out;
  }

  gvdb_table_write_contents (root_table_out, filename, FALSE, NULL);

out:
  if (error)
    g_error_free (error);
  if (root_table_out)
    g_hash_table_unref (root_table_out);
  if (bookmarks_table_out)
    g_hash_table_unref (bookmarks_table_out);
  if (tags_table_out)
    g_hash_table_unref (tags_table_out);
  if (root_table_in)
    gvdb_table_free (root_table_in);
  if (bookmarks_table_in)
    gvdb_table_free (bookmarks_table_in);
  if (tags_table_in)
    gvdb_table_free (tags_table_in);
  g_strfreev (urls);
  g_strfreev (tags);
  g_free (filename);
}

static void
migrate_passwords_timestamp (void)
{
  GHashTable *attributes;
  GHashTable *attrs;
  SecretItem *item;
  SecretValue *value;
  GList *passwords = NULL;
  GError *error = NULL;
  const char *origin;
  const char *username;
  const char *timestamp;
  char *label;
  double timestamp_d;
  gint64 timestamp_i;
  int default_profile_migration_version;

  /* We want this migration to run only once. */
  default_profile_migration_version = ephy_profile_utils_get_migration_version_for_profile_dir (legacy_default_profile_dir ());
  if (default_profile_migration_version >= EPHY_PASSWORDS_TIMESTAMP_MIGRATION_VERSION)
    return;

  attributes = secret_attributes_build (EPHY_FORM_PASSWORD_SCHEMA, NULL);
  passwords = secret_service_search_sync (NULL, EPHY_FORM_PASSWORD_SCHEMA, attributes,
                                          SECRET_SEARCH_ALL | SECRET_SEARCH_UNLOCK | SECRET_SEARCH_LOAD_SECRETS,
                                          NULL, &error);
  if (error) {
    g_warning ("Failed to search the password schema: %s", error->message);
    goto out;
  }

  secret_service_clear_sync (NULL, EPHY_FORM_PASSWORD_SCHEMA,
                             attributes, NULL, &error);
  if (error) {
    g_warning ("Failed to clear the password schema: %s", error->message);
    goto out;
  }

  for (GList *p = passwords; p && p->data; p = p->next) {
    item = (SecretItem *)p->data;
    value = secret_item_get_secret (item);

    if (!value)
      continue;

    attrs = secret_item_get_attributes (item);
    origin = g_hash_table_lookup (attrs, ORIGIN_KEY);
    username = g_hash_table_lookup (attrs, USERNAME_KEY);
    timestamp = g_hash_table_lookup (attrs, SERVER_TIME_MODIFIED_KEY);

    if (!origin || !timestamp)
      goto next;

    timestamp_d = g_ascii_strtod (timestamp, NULL);
    timestamp_i = (gint64)ceil (timestamp_d);
    g_hash_table_insert (attrs, g_strdup (SERVER_TIME_MODIFIED_KEY), g_strdup_printf ("%"PRId64, timestamp_i));

    if (username)
      label = g_strdup_printf ("Password for %s in a form in %s", username, origin);
    else
      label = g_strdup_printf ("Password in a form in %s", origin);

    secret_service_store_sync (NULL, EPHY_FORM_PASSWORD_SCHEMA,
                               attrs, NULL, label,
                               value, NULL, &error);
    if (error) {
      g_warning ("Failed to store password: %s", error->message);
      g_clear_pointer (&error, g_error_free);
    }

    g_free (label);
next:
    g_hash_table_unref (attrs);
    secret_value_unref (value);
  }

out:
  if (error)
    g_error_free (error);
  g_hash_table_unref (attributes);
  g_list_free_full (passwords, g_object_unref);
}

static void
migrate_annoyance_list (void)
{
  GVariant *user_value;
  const char **filters;
  char **modified_filters;

  /* Has the filters setting been modified? If not, we're done. */
  user_value = g_settings_get_user_value (EPHY_SETTINGS_MAIN, EPHY_PREFS_ADBLOCK_FILTERS);
  if (!user_value)
    return;

  /* The annoyance list was causing a bunch of problems. Forcibly remove it. */
  filters = g_variant_get_strv (user_value, NULL);
  modified_filters = ephy_strv_remove (filters, "https://easylist.to/easylist/fanboy-annoyance.txt");
  g_settings_set_strv (EPHY_SETTINGS_MAIN, EPHY_PREFS_ADBLOCK_FILTERS, (const char * const *)modified_filters);

  g_variant_unref (user_value);
  g_free (filters);
  g_strfreev (modified_filters);
}

static gboolean
move_directory_contents (const char *source_path,
                         const char *dest_path)
{
  g_autoptr(GFile) source = g_file_new_for_path (source_path);
  g_autoptr(GFile) dest = g_file_new_for_path (dest_path);
  g_autoptr(GFileEnumerator) direnum = NULL;
  g_autoptr(GError) error = NULL;

  /* Just a sanity check as it should already exist */
  g_file_make_directory (dest, NULL, NULL);

  direnum = g_file_enumerate_children (source, G_FILE_ATTRIBUTE_STANDARD_NAME,
                                       G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                       NULL, &error);
  if (error) {
    g_warning ("Failed to enumerate files: %s", error->message);
    return FALSE;
  }

  while (TRUE) {
    GFileInfo *info;
    g_autoptr(GError) error = NULL;
    if (!g_file_enumerator_iterate (direnum, &info, NULL, NULL, &error)) {
      g_warning ("Failed to enumerate dir: %s", error->message);
      return FALSE;
    }
    if (!info)
      break;

    g_autoptr(GFile) source_f = g_file_get_child (source, g_file_info_get_name (info));
    g_autoptr(GFile) dest_f = g_file_get_child (dest, g_file_info_get_name (info));
    if (!g_file_move (source_f, dest_f, G_FILE_COPY_NONE, NULL, NULL, NULL, &error)) {
      g_warning ("Failed to move %s: %s", g_file_info_get_name (info), error->message);
      return FALSE;
    }
  }

  if (!g_file_delete (source, NULL, &error))
    g_warning ("Failed to delete left-over source: %s", error->message);

  return TRUE;
}

static void
migrate_profile_directories (void)
{
  GList *web_apps, *l;

  /* Web app profiles moved from config to data dirs. If this migration has
   * already been run for a separate profile, then the legacy application list
   * should be empty, so it's harmless to run this multiple times.
   */
  web_apps = ephy_web_application_get_legacy_application_list ();
  for (l = web_apps; l; l = l->next) {
    EphyWebApplication *app = (EphyWebApplication *)l->data;
    g_autoptr(GError) error = NULL;

    g_autofree char *old_name = g_strconcat ("app-epiphany-", app->id, NULL);
    g_autofree char *old_path = g_build_filename (legacy_default_profile_dir (), old_name, NULL);
    g_autofree char *app_path = ephy_web_application_get_profile_directory (app->id);

    if (!move_directory_contents (old_path, app_path))
      continue;

    // Create an empty file to indicate it's an app
    g_autofree char *app_file = g_build_filename (app_path, ".app", NULL);
    int fd = g_open (app_file, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if (fd < 0)
      g_warning ("Failed to create .app file: %s", g_strerror (errno));
    else
      close (fd);

    // Update Exec and Icon to point to the new profile dir
    g_autofree char *old_profile_prefix = g_build_filename (legacy_default_profile_dir (), "app-epiphany-", NULL);
    g_autofree char *new_profile_prefix = g_build_filename (g_get_user_data_dir (), "epiphany-", NULL);
    g_autoptr(GKeyFile) file = g_key_file_new ();
    g_autofree char *desktop_file_path = g_build_filename (app_path, app->desktop_file, NULL);
    g_key_file_load_from_file (file, desktop_file_path, G_KEY_FILE_NONE, NULL);

    g_autofree char *exec = g_key_file_get_string (file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_EXEC, &error);
    if (exec == NULL) {
      g_warning ("Failed to get Exec key from %s: %s", desktop_file_path, error->message);
      continue;
    }
    g_autofree char *new_exec = ephy_string_find_and_replace (exec, old_profile_prefix, new_profile_prefix);
    LOG ("migrate_profile_directories: setting Exec to %s", new_exec);
    g_key_file_set_string (file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_EXEC, new_exec);

    g_autofree char *icon = g_key_file_get_string (file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_ICON, &error);
    if (exec == NULL) {
      g_warning ("Failed to get Icon key from %s: %s", desktop_file_path, error->message);
      continue;
    }
    g_autofree char *new_icon = ephy_string_find_and_replace (icon, old_profile_prefix, new_profile_prefix);
    LOG ("migrate_profile_directories: setting Icon to %s", new_icon);
    g_key_file_set_string (file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_ICON, new_icon);

    if (!g_key_file_save_to_file (file, desktop_file_path, &error))
      g_warning ("Failed to save desktop file %s", error->message);

    g_autofree char *desktop_symlink_path = g_build_filename (g_get_user_data_dir (),
                                                              "applications",
                                                              app->desktop_file,
                                                              NULL);
    g_autoptr(GFile) desktop_symlink = g_file_new_for_path (desktop_symlink_path);
    LOG ("Symlinking %s to %s", desktop_symlink_path, desktop_file_path);

    // Try removing old symlink, failure is ok assuming it doesn't exist.
    if (!g_file_delete (desktop_symlink, NULL, &error)) {
      g_warning ("Failed to remove old symbolic link: %s", error->message);
      g_clear_error (&error);
    }

    if (!g_file_make_symbolic_link (desktop_symlink, desktop_file_path, NULL, &error))
      g_warning ("Failed to make symbolic link: %s", error->message);
  }

  ephy_web_application_free_application_list (web_apps);

  /* The default profile also changed directories. This needs to run only once,
   * for the main browser instance.
   */
  if (!ephy_profile_dir_is_default ())
    return;

  if (!move_directory_contents (legacy_default_profile_dir (), ephy_default_profile_dir ()))
    return;

  /* We are also moving some cache directories so just remove the old ones */
  g_autoptr(GFile) new_directory = g_file_new_for_path (ephy_default_profile_dir ());
  g_autoptr(GFile) adblock_directory = g_file_get_child (new_directory, "adblock");
  g_file_delete (adblock_directory, NULL, NULL);
  g_autoptr(GFile) gsb_file = g_file_get_child (new_directory, "gsb-threats.db");
  g_file_delete (gsb_file, NULL, NULL);
  g_autoptr(GFile) gsb_journal_file = g_file_get_child (new_directory, "gsb-threats.db-journal");
  g_file_delete (gsb_journal_file, NULL, NULL);
}

static void
migrate_web_extension_config_dir (void)
{
  /* Epiphany 3.32.0 web process created its config dir in the wrong place by
   * mistake for default profile dirs. It only contains read-only data copied
   * from dconf, so we can just delete it.
   */
  g_autofree char *path = g_build_filename (ephy_default_profile_dir (), "config", NULL);
  ephy_file_delete_dir_recursively (path, NULL);
}

static void
migrate_webapps_harder (void)
{
  /* We created some webapps in the default profile directory by mistake.
   * Whoopsie!
   */
  g_autoptr(GFileEnumerator) children = NULL;
  g_autoptr(GFileInfo) info;
  g_autofree char *parent_directory_path = NULL;
  g_autoptr(GFile) parent_directory = NULL;

  parent_directory_path = g_build_filename (g_get_user_data_dir (), "epiphany", NULL);

  parent_directory = g_file_new_for_path (parent_directory_path);
  children = g_file_enumerate_children (parent_directory,
                                        "standard::name",
                                        0, NULL, NULL);
  if (!children)
    return;

  info = g_file_enumerator_next_file (children, NULL, NULL);
  while (info) {
    const char *name = g_file_info_get_name (info);

    if (g_str_has_prefix (name, "epiphany-")) {
      g_autofree char *incorrect_profile_dir = g_build_filename (parent_directory_path, name, NULL);
      g_autofree char *correct_profile_dir = g_build_filename (g_get_user_data_dir (), name, NULL);
      g_autofree char *app_file = g_build_filename (incorrect_profile_dir, ".app", NULL);

      if (g_file_test (app_file, G_FILE_TEST_EXISTS)) {
        g_autoptr(GKeyFile) file = g_key_file_new ();
        g_autofree char *desktop_file_name = g_strconcat (name, ".desktop", NULL);
        g_autofree char *desktop_file_path = g_build_filename (correct_profile_dir, desktop_file_name, NULL);
        g_autoptr(GError) error = NULL;

        move_directory_contents (incorrect_profile_dir, correct_profile_dir);

        // Update Exec and Icon to point to the new profile dir
        g_key_file_load_from_file (file, desktop_file_path, G_KEY_FILE_NONE, NULL);

        g_autofree char *exec = g_key_file_get_string (file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_EXEC, &error);
        if (exec == NULL) {
          g_warning ("Failed to get Exec key from %s: %s", desktop_file_path, error->message);
          continue;
        }
        g_autofree char *new_exec = ephy_string_find_and_replace (exec, incorrect_profile_dir, correct_profile_dir);
        LOG ("migrate_profile_directories: setting Exec to %s", new_exec);
        g_key_file_set_string (file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_EXEC, new_exec);

        g_autofree char *icon = g_key_file_get_string (file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_ICON, &error);
        if (exec == NULL) {
          g_warning ("Failed to get Icon key from %s: %s", desktop_file_path, error->message);
          continue;
        }
        g_autofree char *new_icon = ephy_string_find_and_replace (icon, incorrect_profile_dir, correct_profile_dir);
        LOG ("migrate_profile_directories: setting Icon to %s", new_icon);
        g_key_file_set_string (file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_ICON, new_icon);

        if (!g_key_file_save_to_file (file, desktop_file_path, &error)) {
          g_warning ("Failed to save desktop file %s", error->message);
          g_clear_error (&error);
        }

        g_autofree char *desktop_symlink_path = g_build_filename (g_get_user_data_dir (), "applications", desktop_file_name, NULL);
        g_autoptr(GFile) desktop_symlink = g_file_new_for_path (desktop_symlink_path);
        LOG ("Symlinking %s to %s", desktop_symlink_path, desktop_file_path);

        // Try removing old symlink, failure is ok assuming it doesn't exist.
        if (!g_file_delete (desktop_symlink, NULL, &error)) {
          g_warning ("Failed to remove old symbolic link: %s", error->message);
          g_clear_error (&error);
        }

        if (!g_file_make_symbolic_link (desktop_symlink, desktop_file_path, NULL, &error))
          g_warning ("Failed to make symbolic link: %s", error->message);
      }
    }

    g_clear_object (&info);
    info = g_file_enumerator_next_file (children, NULL, NULL);
  }
}

static void
migrate_nothing (void)
{
  /* Used to replace migrators that have been removed. Only remove migrators
   * that support particularly ancient versions of Epiphany.
   *
   * Note that you cannot simply remove a migrator from the migrators struct,
   * as that would mess up tracking of which migrators have been run.
   */
}

static void
migrate_zoom_level (void)
{
  EphySQLiteConnection *history_db = NULL;
  EphySQLiteStatement *statement = NULL;
  GError *error = NULL;
  char *history_filename;
  const char *sql_query;

  history_filename = g_build_filename (ephy_profile_dir (), EPHY_HISTORY_FILE, NULL);
  if (!g_file_test (history_filename, G_FILE_TEST_EXISTS)) {
    LOG ("There is no history to migrate...");
    goto out;
  }

  history_db = ephy_sqlite_connection_new (EPHY_SQLITE_CONNECTION_MODE_READWRITE,
                                           history_filename);
  ephy_sqlite_connection_open (history_db, &error);
  if (error) {
    g_warning ("Failed to open history database: %s", error->message);
    g_clear_object (&history_db);
    goto out;
  }

  /* Update zoom level values. */
  sql_query = "UPDATE hosts SET zoom_level = 0.0 WHERE zoom_level = 1.0";
  ephy_sqlite_connection_execute (history_db, sql_query, &error);
  if (error) {
    g_warning ("Failed to update zoom level: %s", error->message);
    goto out;
  }

  sql_query = "CREATE TABLE hosts_backup ("
              "id INTEGER PRIMARY KEY,"
              "url LONGVARCAR,"
              "title LONGVARCAR,"
              "visit_count INTEGER DEFAULT 0 NOT NULL,"
              "zoom_level REAL DEFAULT 0.0)";

  ephy_sqlite_connection_execute (history_db, sql_query, &error);
  if (error) {
    g_warning ("Failed to create host backup table: %s", error->message);
    goto out;
  }

  sql_query = "INSERT INTO hosts_backup SELECT id,url,title,visit_count,zoom_level FROM hosts";
  ephy_sqlite_connection_execute (history_db, sql_query, &error);
  if (error) {
    g_warning ("Failed to copy data from hosts to hosts_backup: %s", error->message);
    goto out;
  }

  sql_query = "DROP TABLE hosts";
  ephy_sqlite_connection_execute (history_db, sql_query, &error);
  if (error) {
    g_warning ("Failed to remove table hosts: %s", error->message);
    goto out;
  }

  sql_query = "ALTER TABLE hosts_backup RENAME TO hosts";
  ephy_sqlite_connection_execute (history_db, sql_query, &error);
  if (error) {
    g_warning ("Failed to rename hosts_backup to hosts: %s", error->message);
    goto out;
  }

out:
  g_free (history_filename);
  if (history_db) {
    ephy_sqlite_connection_close (history_db);
    g_object_unref (history_db);
  }
  if (statement)
    g_object_unref (statement);
  if (error)
    g_error_free (error);
}

/* If adding anything here, you need to edit EPHY_PROFILE_MIGRATION_VERSION
 * in ephy-profile-utils.h. */
const EphyProfileMigrator migrators[] = {
  /*  1 */ migrate_nothing,
  /*  2 */ migrate_nothing,
  /*  3 */ migrate_nothing,
  /*  4 */ migrate_nothing,
  /*  5 */ migrate_nothing,
  /*  6 */ migrate_nothing,
  /*  7 */ migrate_nothing,
  /*  8 */ migrate_nothing,
  /*  9 */ migrate_nothing,
  /* 10 */ migrate_nothing,
  /* 11 */ migrate_insecure_passwords,
  /* 12 */ migrate_bookmarks,
  /* 13 */ migrate_adblock_filters,
  /* 14 */ migrate_initial_state,
  /* 15 */ migrate_permissions,
  /* 16 */ migrate_nothing,
  /* 17 */ migrate_nothing,
  /* 18 */ migrate_icon_database,
  /* 19 */ migrate_passwords_to_firefox_sync_passwords,
  /* 20 */ migrate_history_to_firefox_sync_history,
  /* 21 */ migrate_passwords_add_target_origin,
  /* 22 */ migrate_nothing,
  /* 23 */ migrate_sync_device_info,
  /* 24 */ migrate_bookmarks_timestamp,
  /* 25 */ migrate_passwords_timestamp,
  /* 26 */ migrate_nothing,
  /* 27 */ migrate_nothing,
  /* 28 */ migrate_annoyance_list,
  /* 29 */ migrate_zoom_level,
  /* 30 */ migrate_profile_directories,
  /* 31 */ migrate_web_extension_config_dir,
  /* 32 */ migrate_webapps_harder,
};

static gboolean
ephy_migrator (void)
{
  int latest, i;
  EphyProfileMigrator m;
  g_autofree char *legacy_migration_file = NULL;

  /* If after this point there's no profile dir, there's no point in
   * running anything because Epiphany has never run in this system, so
   * exit here. */
  if (!profile_dir_exists ())
    return TRUE;

  if (do_step_n != -1) {
    if (do_step_n >= EPHY_PROFILE_MIGRATION_VERSION)
      return FALSE;

    if (do_step_n < 1) {
      g_printf ("Invalid migration step %d\n", do_step_n);
      return FALSE;
    }

    LOG ("Running only migrator: %d", do_step_n);
    m = migrators[do_step_n - 1];
    m ();

    return TRUE;
  }

  legacy_migration_file = g_build_filename (legacy_profile_dir (), ".migrated", NULL);
  if (g_file_test (legacy_migration_file, G_FILE_TEST_EXISTS))
    latest = ephy_profile_utils_get_migration_version_for_profile_dir (legacy_profile_dir ());
  else
    latest = ephy_profile_utils_get_migration_version ();

  LOG ("Running migrators up to version %d, current migration version is %d.",
       EPHY_PROFILE_MIGRATION_VERSION, latest);

  for (i = latest; i < EPHY_PROFILE_MIGRATION_VERSION; i++) {
    LOG ("Running migrator: %d of %d", i + 1, EPHY_PROFILE_MIGRATION_VERSION);

    m = migrators[i];
    m ();
  }

  if (ephy_profile_utils_set_migration_version (EPHY_PROFILE_MIGRATION_VERSION) != TRUE) {
    LOG ("Failed to store the current migration version");
    return FALSE;
  }

  return TRUE;
}

static const GOptionEntry option_entries[] =
{
  { "do-step", 'd', 0, G_OPTION_ARG_INT, &do_step_n,
    N_("Executes only the n-th migration step"), NULL },
  { "version", 'v', 0, G_OPTION_ARG_INT, &migration_version,
    N_("Specifies the required version for the migrator"), NULL },
  { "profile-dir", 'p', 0, G_OPTION_ARG_FILENAME, &profile_dir,
    N_("Specifies the profile where the migrator should run"), NULL },
  { NULL }
};

int
main (int argc, char *argv[])
{
  GOptionContext *option_context;
  GOptionGroup *option_group;
  GError *error = NULL;
  EphyFileHelpersFlags file_helpers_flags = EPHY_FILE_HELPERS_NONE;

  setlocale (LC_ALL, "");
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  g_assert (EPHY_PROFILE_MIGRATION_VERSION == G_N_ELEMENTS (migrators));

  option_group = g_option_group_new ("ephy-profile-migrator",
                                     N_("Web profile migrator"),
                                     N_("Web profile migrator options"),
                                     NULL, NULL);

  g_option_group_set_translation_domain (option_group, GETTEXT_PACKAGE);
  g_option_group_add_entries (option_group, option_entries);

  option_context = g_option_context_new ("");
  g_option_context_set_main_group (option_context, option_group);

  if (!g_option_context_parse (option_context, &argc, &argv, &error)) {
    g_print ("Failed to parse arguments: %s\n", error->message);
    g_error_free (error);
    g_option_context_free (option_context);

    return 1;
  }

  g_option_context_free (option_context);

  if (migration_version != -1 && migration_version != EPHY_PROFILE_MIGRATION_VERSION) {
    g_print ("Version mismatch, version %d requested but our version is %d\n",
             migration_version, EPHY_PROFILE_MIGRATION_VERSION);

    return 1;
  }

  ephy_debug_init ();

  if (profile_dir != NULL)
    file_helpers_flags = EPHY_FILE_HELPERS_PRIVATE_PROFILE |
                         EPHY_FILE_HELPERS_KEEP_DIR;

  if (!ephy_file_helpers_init (profile_dir, file_helpers_flags, NULL)) {
    LOG ("Something wrong happened with ephy_file_helpers_init()");
    return -1;
  }

  return ephy_migrator () ? 0 : 1;
}
