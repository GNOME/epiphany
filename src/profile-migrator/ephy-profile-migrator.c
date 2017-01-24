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

/* Portions of this file based on Chromium code.
 * License block as follows:
 *
 * Copyright (c) 2009 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * The LICENSE file from Chromium can be found in the LICENSE.chromium
 * file.
 */

#include "config.h"

#include "ephy-bookmarks-manager.h"
#include "ephy-debug.h"
#include "ephy-file-helpers.h"
#include "ephy-form-auth-data.h"
#include "ephy-history-service.h"
#include "ephy-profile-utils.h"
#include "ephy-settings.h"
#include "ephy-sqlite-connection.h"
#include "ephy-uri-tester-shared.h"
#include "ephy-web-app-utils.h"

#include <fcntl.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <libsecret/secret.h>
#include <libsoup/soup.h>
#include <libxml/HTMLtree.h>
#include <libxml/xmlreader.h>
#include <locale.h>
#include <sys/stat.h>
#include <sys/types.h>

static int do_step_n = -1;
static int migration_version = -1;
static char *profile_dir = NULL;
static GMainLoop *loop = NULL;

/*
 * What to do to add new migration steps:
 *  - Bump EPHY_PROFILE_MIGRATION_VERSION in lib/ephy-profile-utils.h
 *  - Add your function at the end of the 'migrators' array
 */

typedef void (*EphyProfileMigrator) (void);

static gboolean
profile_dir_exists (void)
{
  return g_file_test (ephy_dot_dir (), G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR);
}

static void
migrate_profile (const char *old_dir,
                 const char *new_dir)
{
  char *parent_dir;
  char *updated;
  const char *message;

  if (g_file_test (new_dir, G_FILE_TEST_EXISTS) ||
      !g_file_test (old_dir, G_FILE_TEST_IS_DIR))
    return;

  /* Test if we already attempted to migrate first. */
  updated = g_build_filename (old_dir, "DEPRECATED-DIRECTORY", NULL);
  message = _("Web 3.6 deprecated this directory and tried migrating "
              "this configuration to ~/.config/epiphany");

  parent_dir = g_path_get_dirname (new_dir);
  if (g_mkdir_with_parents (parent_dir, 0700) == 0) {
    int fd, res;

    /* rename() works fine if the destination directory is empty. */
    res = g_rename (old_dir, new_dir);
    if (res == -1 && !g_file_test (updated, G_FILE_TEST_EXISTS)) {
      fd = g_creat (updated, 0600);
      if (fd != -1) {
        res = write (fd, message, strlen (message));
        close (fd);
      }
    }
  }

  g_free (parent_dir);
  g_free (updated);
}

static void
migrate_profile_gnome2_to_xdg (void)
{
  char *old_dir;
  char *new_dir;

  old_dir = g_build_filename (g_get_home_dir (),
                              ".gnome2",
                              "epiphany",
                              NULL);
  new_dir = g_build_filename (g_get_user_config_dir (),
                              "epiphany",
                              NULL);

  migrate_profile (old_dir, new_dir);

  g_free (new_dir);
  g_free (old_dir);
}

static char *
fix_desktop_file_and_return_new_location (const char *dir)
{
  GRegex *regex;
  char *result, *old_profile_dir, *replacement, *contents, *new_contents;
  gsize length;

  old_profile_dir = g_build_filename (g_get_home_dir (),
                                      ".gnome2",
                                      NULL);
  replacement = g_build_filename (g_get_user_config_dir (),
                                  NULL);
  regex = g_regex_new (old_profile_dir, 0, 0, NULL);

  /* We want to modify both the link destination and the contents of
   * the .desktop file itself. */
  result = g_regex_replace (regex, dir, -1,
                            0, replacement, 0, NULL);
  g_file_get_contents (result, &contents, &length, NULL);
  new_contents = g_regex_replace (regex, contents, -1, 0,
                                  replacement, 0, NULL);
  g_file_set_contents (result, new_contents, length, NULL);

  g_free (contents);
  g_free (new_contents);
  g_free (old_profile_dir);
  g_free (replacement);

  g_regex_unref (regex);

  return result;
}

static void
migrate_web_app_links (void)
{
  GList *apps, *p;

  apps = ephy_web_application_get_application_list ();
  for (p = apps; p; p = p->next) {
    char *desktop_file, *app_link;
    EphyWebApplication *app = (EphyWebApplication *)p->data;

    desktop_file = app->desktop_file;

    /* Update the link in applications. */
    app_link = g_build_filename (g_get_user_data_dir (), "applications", desktop_file, NULL);
    if (g_file_test (app_link, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_SYMLINK)) {
      /* Change the link to point to the new profile dir. */
      GFileInfo *info;
      const char *target;
      GFile *file = g_file_new_for_path (app_link);

      info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET,
                                0, NULL, NULL);
      if (info) {
        char *new_target;

        target = g_file_info_get_symlink_target (info);
        new_target = fix_desktop_file_and_return_new_location (target);

        /* FIXME: Updating the file info and setting it again should
         * work, but it does not? Just delete and create the link
         * again. */
        g_file_delete (file, 0, 0);
        g_object_unref (file);

        file = g_file_new_for_path (app_link);
        g_file_make_symbolic_link (file, new_target, NULL, NULL);

        g_object_unref (info);
        g_free (new_target);
      }

      g_object_unref (file);
    }

    g_free (app_link);
  }

  ephy_web_application_free_application_list (apps);
}

static void
migrate_new_urls_table (void)
{
  EphySQLiteConnection *history_database;
  char *filename;
  GError *error = NULL;

  filename = g_build_filename (ephy_dot_dir (), EPHY_HISTORY_FILE, NULL);
  history_database = ephy_sqlite_connection_new ();
  ephy_sqlite_connection_open (history_database, filename, &error);

  if (error) {
    g_warning ("Failed to open history database: %s\n", error->message);
    g_error_free (error);
    g_free (filename);
    return;
  }

  ephy_sqlite_connection_execute (history_database,
                                  "ALTER TABLE urls "
                                  "ADD COLUMN thumbnail_update_time INTEGER DEFAULT 0",
                                  &error);
  if (error) {
    g_warning ("Failed to add new column to table in history backend: %s\n",
               error->message);
    g_error_free (error);
    error = NULL;
  }
  ephy_sqlite_connection_execute (history_database,
                                  "ALTER TABLE urls "
                                  "ADD COLUMN hidden_from_overview INTEGER DEFAULT 0",
                                  &error);
  if (error) {
    g_warning ("Failed to add new column to table in history backend: %s\n",
               error->message);
    g_error_free (error);
    error = NULL;
  }

  g_object_unref (history_database);
  g_free (filename);
}

/* Migrating form password data. */

static int form_passwords_migrating = 0;


static void
password_cleared_cb (SecretService *service,
                     GAsyncResult  *res,
                     gpointer       userdata)
{
  secret_service_clear_finish (service, res, NULL);

  if (g_atomic_int_dec_and_test (&form_passwords_migrating))
    g_main_loop_quit (loop);
}

static void
store_form_auth_data_cb (GObject      *object,
                         GAsyncResult *res,
                         GHashTable   *attributes)
{
  GError *error = NULL;

  if (ephy_form_auth_data_store_finish (res, &error) == FALSE) {
    g_warning ("Couldn't store a form password: %s", error->message);
    g_error_free (error);
    goto out;
  }

  g_atomic_int_inc (&form_passwords_migrating);
  secret_service_clear (NULL, NULL,
                        attributes, NULL, (GAsyncReadyCallback)password_cleared_cb,
                        NULL);

 out:
  if (g_atomic_int_dec_and_test (&form_passwords_migrating))
    g_main_loop_quit (loop);

  g_hash_table_unref (attributes);
}

static void
load_collection_items_cb (SecretCollection *collection,
                          GAsyncResult     *res,
                          gpointer          data)
{
  SecretItem *item;
  SecretValue *secret;
  GList *l;
  GHashTable *attributes, *t;
  const char *server, *username, *form_username, *form_password, *password;
  char *actual_server;
  SoupURI *uri;
  GError *error = NULL;
  GList *items;

  secret_collection_load_items_finish (collection, res, &error);

  if (error) {
    g_warning ("Couldn't retrieve form data: %s", error->message);
    g_error_free (error);
    return;
  }
  items = secret_collection_get_items (collection);

  for (l = items; l; l = l->next) {
    item = (SecretItem *)l->data;

    attributes = secret_item_get_attributes (item);
    server = g_hash_table_lookup (attributes, "server");
    if (server &&
        g_strstr_len (server, -1, "form%5Fusername") &&
        g_strstr_len (server, -1, "form%5Fpassword")) {
      g_atomic_int_inc (&form_passwords_migrating);
      /* This is one of the hackish ones that need to be migrated.
         Fetch the rest of the data and take care of it. */
      username = g_hash_table_lookup (attributes, "user");
      uri = soup_uri_new (server);
      t = soup_form_decode (uri->query);
      form_username = g_hash_table_lookup (t, FORM_USERNAME_KEY);
      form_password = g_hash_table_lookup (t, FORM_PASSWORD_KEY);
      soup_uri_set_query (uri, NULL);
      actual_server = soup_uri_to_string (uri, FALSE);
      secret_item_load_secret_sync (item, NULL, NULL);
      secret = secret_item_get_secret (item);
      password = secret_value_get (secret, NULL);
      ephy_form_auth_data_store (actual_server,
                                 form_username,
                                 form_password,
                                 username,
                                 password,
                                 (GAsyncReadyCallback)store_form_auth_data_cb,
                                 g_hash_table_ref (attributes));
      g_free (actual_server);
      secret_value_unref (secret);
      g_hash_table_unref (t);
      soup_uri_free (uri);
    }
    g_hash_table_unref (attributes);
  }

  /* And decrease here so that we finish eventually. */
  if (g_atomic_int_dec_and_test (&form_passwords_migrating))
    g_main_loop_quit (loop);

  g_list_free_full (items, (GDestroyNotify)g_object_unref);
}

static void
migrate_form_passwords_to_libsecret (void)
{
  SecretService *service;
  GList *collections, *c;
  GError *error = NULL;

  service = secret_service_get_sync (SECRET_SERVICE_OPEN_SESSION | SECRET_SERVICE_LOAD_COLLECTIONS, NULL, &error);
  if (error) {
    g_warning ("Could not get the secret service: %s", error->message);
    g_error_free (error);
    return;
  }

  collections = secret_service_get_collections (service);

  for (c = collections; c; c = c->next) {
    g_atomic_int_inc (&form_passwords_migrating);
    secret_collection_load_items ((SecretCollection *)c->data, NULL, (GAsyncReadyCallback)load_collection_items_cb,
                                  NULL);
  }

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  g_list_free_full (collections, (GDestroyNotify)g_object_unref);
  g_object_unref (service);
}

static void
migrate_app_desktop_file_categories (void)
{
  GList *web_apps, *l;

  web_apps = ephy_web_application_get_application_list ();

  for (l = web_apps; l; l = l->next) {
    EphyWebApplication *app = (EphyWebApplication *)l->data;
    GKeyFile *file;
    char *data = NULL;
    char *app_path;
    char *desktop_file_path;

    file = g_key_file_new ();

    app_path = ephy_web_application_get_profile_directory (app->name);
    desktop_file_path = g_build_filename (app_path, app->desktop_file, NULL);
    g_key_file_load_from_file (file, desktop_file_path, G_KEY_FILE_NONE, NULL);

    LOG ("migrate_app_desktop_file_categories: adding Categories to %s", app->name);
    g_key_file_set_value (file, "Desktop Entry", "Categories", "Network;GNOME;GTK;");

    data = g_key_file_to_data (file, NULL, NULL);
    g_file_set_contents (desktop_file_path, data, -1, NULL);

    g_free (app_path);
    g_free (desktop_file_path);
    g_free (data);
    g_key_file_free (file);
  }

  ephy_web_application_free_application_list (web_apps);
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

  for (i = 0; locales[i] && i < *best_match; i++) {
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

  if (lang) xmlFree (lang);
  if (*value != content) xmlFree (content);
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

    g_sequence_sort (tags, (GCompareDataFunc)ephy_bookmark_tags_compare, NULL);
    bookmark = ephy_bookmark_new ((const char *)link, (const char *)title, tags);
    ephy_bookmarks_manager_add_bookmark (manager, bookmark);
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

  filename = g_build_filename (ephy_dot_dir (),
                               EPHY_BOOKMARKS_FILE,
                               NULL);

  if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
    g_free (filename);
    return;
  }

  g_free (filename);
  filename = g_build_filename (ephy_dot_dir (),
                               "bookmarks.rdf",
                               NULL);

  if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
    g_free (filename);
    return;
  }

  doc = xmlParseFile (filename);
  g_free (filename);
  if (doc == NULL) {
    g_warning ("Failed to re-import the bookmarks. All bookmarks lost!\n");
    return;
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

  adblock_dir = g_build_filename (ephy_dot_dir (), "adblock", NULL);
  if (!g_file_test (adblock_dir, G_FILE_TEST_IS_DIR)) {
    g_free (adblock_dir);
    return;
  }

  if (!ephy_dot_dir_is_default ()) {
    char *default_dot_dir;

    /* Adblock filters rules are now shared to save space */
    ephy_file_delete_dir_recursively (adblock_dir, NULL);
    g_free (adblock_dir);

    default_dot_dir = ephy_default_dot_dir ();
    adblock_dir = g_build_filename (default_dot_dir, "adblock", NULL);
    g_free (default_dot_dir);
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
        if (url[0] != '\0' && !g_str_equal (url, ADBLOCK_DEFAULT_FILTER_URL))
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
    g_settings_set_strv (EPHY_SETTINGS_WEB,
                         EPHY_PREFS_WEB_ADBLOCK_FILTERS,
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
  filename = g_build_filename (ephy_dot_dir (), "states.xml", NULL);
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
migrate_nothing (void)
{
  /* Used to replace migrators that have been removed. Only remove migrators
   * that support particularly ancient versions of Epiphany.
   *
   * Note that you cannot simply remove a migrator from the migrators struct,
   * as that would mess up tracking of which migrators have been run.
   */
}

/* If adding anything here, you need to edit EPHY_PROFILE_MIGRATION_VERSION
 * in ephy-profile-utils.h. */
const EphyProfileMigrator migrators[] = {
  migrate_nothing,
  migrate_nothing,
  migrate_nothing,
  migrate_nothing,
  migrate_nothing,
  migrate_nothing,
  migrate_web_app_links,
  migrate_new_urls_table,
  migrate_form_passwords_to_libsecret,
  migrate_app_desktop_file_categories,
  migrate_bookmarks,
  migrate_adblock_filters,
  migrate_initial_state,
};

static gboolean
ephy_migrator (void)
{
  int latest, i;
  EphyProfileMigrator m;

  /* Always try to migrate the data from the old profile dir at the
   * very beginning. */
  migrate_profile_gnome2_to_xdg ();

  /* If after this point there's no profile dir, there's no point in
   * running anything because Epiphany has never run in this sytem, so
   * exit here. */
  if (!profile_dir_exists ())
    return TRUE;

  if (do_step_n != -1) {
    if (do_step_n >= EPHY_PROFILE_MIGRATION_VERSION)
      return FALSE;

    LOG ("Running only migrator: %d", do_step_n);
    m = migrators[do_step_n];
    m ();

    return TRUE;
  }

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
