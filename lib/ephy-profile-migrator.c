/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2009 Xan López
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
 *
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

#include "ephy-debug.h"
#include "ephy-file-helpers.h"
#include "ephy-form-auth-data.h"
#include "ephy-history-service.h"
#include "ephy-profile-utils.h"
#include "ephy-settings.h"
#include "ephy-sqlite-connection.h"
#include "ephy-web-app-utils.h"
#ifdef ENABLE_NSS
#include "ephy-nss-glue.h"
#endif

#include <fcntl.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <libsecret/secret.h>
#include <libsoup/soup.h>
#include <sys/stat.h>
#include <sys/types.h>

static int do_step_n = -1;
static int version = -1;
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
migrate_cookies (void)
{
  const char *cookies_file_sqlite = "cookies.sqlite";
  const char *cookies_file_txt = "cookies.txt";
  char *src_sqlite = NULL, *src_txt = NULL, *dest = NULL;

  dest = g_build_filename (ephy_dot_dir (), cookies_file_sqlite, NULL);
  /* If we already have a cookies.sqlite file, do nothing */
  if (g_file_test (dest, G_FILE_TEST_EXISTS))
    goto out;

  src_sqlite = g_build_filename (ephy_dot_dir (), "mozilla",
                                 "epiphany", cookies_file_sqlite, NULL);
  src_txt = g_build_filename (ephy_dot_dir (), "mozilla",
                              "epiphany", cookies_file_txt, NULL);

  /* First check if we have a cookies.sqlite file in Mozilla */
  if (g_file_test (src_sqlite, G_FILE_TEST_EXISTS)) {
    GFile *gsrc, *gdest;

    /* Copy the file */
    gsrc = g_file_new_for_path (src_sqlite);
    gdest = g_file_new_for_path (dest);

    if (!g_file_copy (gsrc, gdest, 0, NULL, NULL, NULL, NULL))
      g_warning (_("Failed to copy cookies file from Mozilla."));

    g_object_unref (gsrc);
    g_object_unref (gdest);
  } else if (g_file_test (src_txt, G_FILE_TEST_EXISTS)) {
    /* Create a SoupCookieJarSQLite with the contents of the txt file */
    GSList *cookies, *p;
    SoupCookieJar *txt, *sqlite;

    txt = soup_cookie_jar_text_new (src_txt, TRUE);
    sqlite = soup_cookie_jar_db_new (dest, FALSE);
    cookies = soup_cookie_jar_all_cookies (txt);

    for (p = cookies; p; p = p->next) {
      SoupCookie *cookie = (SoupCookie*)p->data;
      /* Cookie is stolen, so we won't free it */
      soup_cookie_jar_add_cookie (sqlite, cookie);
    }

    g_slist_free (cookies);
    g_object_unref (txt);
    g_object_unref (sqlite);
  }

 out:
  g_free (src_sqlite);
  g_free (src_txt);
  g_free (dest);
}

#ifdef ENABLE_NSS
static char*
decrypt (const char *data)
{
  unsigned char *plain;
  gsize out_len;

  /* The old style password is encoded in base64. They are identified
   * by a leading '~'. Otherwise, we should decrypt the text.
   */
  plain = g_base64_decode (data, &out_len);
  if (data[0] != '~') {
    char *decrypted;

    decrypted = ephy_nss_glue_decrypt (plain, out_len);
    g_free (plain);
    plain = (unsigned char*)decrypted;
  }

  return (char*)plain;
}

static void
parse_and_decrypt_signons (const char *signons,
                           gboolean handle_forms)
{
  int version;
  gchar **lines;
  int i;
  guint length;

  lines = g_strsplit (signons, "\r\n", -1);
  if (!g_ascii_strncasecmp (lines[0], "#2c", 3))
    version = 1;
  else if (!g_ascii_strncasecmp (lines[0], "#2d", 3))
    version = 2;
  else if (!g_ascii_strncasecmp (lines[0], "#2e", 3))
    version = 3;
  else
    goto out;

  /* Skip the never-saved list */
  for (i = 1; lines[i] && !g_str_equal (lines[i], "."); i++) {
    ;
  }

  i++;

  /*
   * Read saved passwords. The information is stored in blocks
   * separated by lines that only contain a dot. We find a block by
   * the separator and parse them one by one.
   */
  length = g_strv_length (lines);

  while (i < length) {
    size_t begin = i;
    size_t end = i + 1;
    const char *realmBracketBegin = " (";
    const char *realmBracketEnd = ")";
    SoupURI *uri = NULL;
    char *realm = NULL;

    while (lines[end] && !g_str_equal (lines[end], "."))
      end++;

    i = end + 1;

    /* A block has at least five lines */
    if (end - begin < 5)
      continue;

    /* The first line is the site URL.
     * For HTTP authentication logins, the URL may contain http realm,
     * which will be in bracket:
     *   sitename:8080 (realm)
     */
    if (g_strstr_len (lines[begin], -1, realmBracketBegin)) {
      char *start_ptr, *end_ptr;
      char *full_url, *url;
      glong start, end;

      /* In this case the scheme may not exist. We assume that the
       * scheme is HTTP.
       */
      if (!g_strstr_len (lines[begin], -1, "://"))
        full_url = g_strconcat ("http://", lines[begin], NULL);
      else
        full_url = g_strdup (lines[begin]);

      start_ptr = g_strstr_len (full_url, -1, realmBracketBegin);
      start = g_utf8_pointer_to_offset (full_url, start_ptr);
      url = g_utf8_substring (full_url, 0, start);
      url = g_strstrip (url);
      uri = soup_uri_new (url);
      g_free (url);

      start += strlen (realmBracketBegin);
      end_ptr = g_strstr_len (full_url, -1, realmBracketEnd) -1;
      end = g_utf8_pointer_to_offset (full_url, end_ptr);
      realm = g_utf8_substring (full_url, start, end);

      g_free (full_url);
    } else {
      /* Don't have HTTP realm. It is the URL that the following
       * password belongs to.
       */
      uri = soup_uri_new (lines[begin]);
    }

    if (!SOUP_URI_VALID_FOR_HTTP (uri)) {
      soup_uri_free (uri);
      g_free (realm);
      continue;
    }

    ++begin;

    /* There may be multiple username/password pairs for this site.
     * In this case, they are saved in one block without a separated
     * line (contains a dot).
     */
    while (begin + 4 < end) {
      char *username = NULL;
      char *password = NULL;
      char *form_username = NULL;
      char *form_password = NULL;

      /* The username */
      if (handle_forms) {
        form_username = g_strdup (lines[begin++]);
      } else {
        begin++; /* Skip username element */
      }
      username = decrypt (lines[begin++]);

      /* The password */
      /* The element name has a leading '*' */
      if (lines[begin][0] == '*') {
        if (handle_forms) {
          form_password = g_strdup (lines[begin++]);
        } else {
          begin++; /* Skip password element */
        }
        password = decrypt (lines[begin++]);
      } else {
        /* Maybe the file is broken, skip to the next block */
        g_free (username);
        break;
      }

      /* The action attribute for from the form element. This line
       * exists in version 2 or above
       */
      if (version >= 2) {
        if (begin < end)
          /* Skip it */ ;
        begin++;

        /* Version 3 has an extra line for further use */
        if (version == 3)
          begin++;
      }

      if (handle_forms && !realm &&
          username && password &&
          !g_str_equal (username, "") &&
          !g_str_equal (form_username, "") &&
          !g_str_equal (form_password, "*")) {
        char *u = soup_uri_to_string (uri, FALSE);
        /* We skip the '*' at the beginning of form_password. */
        ephy_form_auth_data_store (u,
                                   form_username,
                                   form_password+1,
                                   username,
                                   password,
                                   NULL, NULL);
        g_free (u);
      } else if (!handle_forms && realm &&
                 username && password &&
                 !g_str_equal (username, "") &&
                 form_username == NULL && form_password == NULL) {
        char *u = soup_uri_to_string (uri, FALSE);
        secret_password_store_sync (SECRET_SCHEMA_COMPAT_NETWORK,
                                    SECRET_COLLECTION_DEFAULT,
                                    u, password, NULL, NULL,
                                    "user", username,
                                    "domain", realm,
                                    "server", uri->host,
                                    "protocol", uri->scheme,
                                    "port", (gint)uri->port,
                                    NULL);
      }

      g_free (username);
      g_free (password);
      g_free (form_username);
      g_free (form_password);
    }

    soup_uri_free (uri);
    g_free (realm);
  }

 out:
  g_strfreev (lines);
}
#endif

static void
migrate_passwords (void)
{
#ifdef ENABLE_NSS
  char *dest, *contents, *gecko_passwords_backup;
  gsize length;
  GError *error = NULL;

  dest = g_build_filename (ephy_dot_dir (),
                           "mozilla", "epiphany", "signons3.txt",
                           NULL);
  if (!g_file_test (dest, G_FILE_TEST_EXISTS)) {
    g_free (dest);
    dest = g_build_filename (ephy_dot_dir (),
                             "mozilla", "epiphany", "signons2.txt",
                             NULL);
    if (!g_file_test (dest, G_FILE_TEST_EXISTS)) {
      g_free (dest);
      return;
    }
  }

  if (!ephy_nss_glue_init ())
    return;

  if (!g_file_get_contents (dest, &contents, &length, &error)) {
    g_free (dest);
  }

  parse_and_decrypt_signons (contents, FALSE);

  /* Save the contents in a backup directory for future data
     extraction when we support more features */
  gecko_passwords_backup = g_build_filename (ephy_dot_dir (),
                                             "gecko-passwords.txt", NULL);

  if (!g_file_set_contents (gecko_passwords_backup, contents,
                            -1, &error)) {
    g_error_free (error);
  }

  g_free (gecko_passwords_backup);
  g_free (contents);

  ephy_nss_glue_close ();
#endif
}

static void
migrate_passwords2 (void)
{
#ifdef ENABLE_NSS
  char *dest, *contents;
  gsize length;
  GError *error = NULL;

  dest = g_build_filename (ephy_dot_dir (),
                           "gecko-passwords.txt",
                           NULL);
  if (!g_file_test (dest, G_FILE_TEST_EXISTS)) {
    g_free (dest);
    return;
  }

  if (!ephy_nss_glue_init ())
    return;

  if (!g_file_get_contents (dest, &contents, &length, &error)) {
    g_free (dest);
  }

  parse_and_decrypt_signons (contents, TRUE);
  g_free (contents);

  ephy_nss_glue_close ();
#endif
}

/* History migration */

static EphyHistoryService *history_service = NULL;
static gboolean all_done = FALSE;

typedef struct {
  char *title;
  char *location;
  char *current;
  long long int visit_count;
  long long int last_visit;
  long long int first_visit;
  double zoom_level;
  GList *visits;
} HistoryParseData;

static void
history_parse_start_element (GMarkupParseContext *context,
                             const char          *element_name,
                             const char         **attribute_names,
                             const char         **attribute_values,
                             gpointer             user_data,
                             GError             **error)
{
  HistoryParseData *parse_data = user_data;

  if (g_str_equal (element_name, "node") && parse_data) {
    /* Starting a new node, reset all values */
    g_free (parse_data->title);
    parse_data->title = NULL;

    g_free (parse_data->location);
    parse_data->location = NULL;

    parse_data->visit_count = 0;
    parse_data->last_visit = 0;
    parse_data->first_visit = 0;
    parse_data->zoom_level = 1.0;
  } else if (g_str_equal (element_name, "property")) {
    const char **name, **value;

    for (name = attribute_names, value = attribute_values; *name; name++, value++) {
      if (g_str_equal (*name, "id")) {
        parse_data->current = g_strdup (*value);
        break;
      }
    }
  }
}

static void
history_parse_text (GMarkupParseContext *context,
                    const char          *text,
                    gsize                text_len,
                    gpointer             user_data,
                    GError             **error)
{
  HistoryParseData *parse_data = user_data;

  if (!parse_data || ! parse_data->current)
    return;

  if (g_str_equal (parse_data->current, "2")) {
    /* Title */
    parse_data->title = g_strndup (text, text_len);
  } else if (g_str_equal (parse_data->current, "3")) {
    /* Location */
    parse_data->location = g_strndup (text, text_len);
  } else if (g_str_equal (parse_data->current, "4")) {
    /* Visit count */
    GString *data = g_string_new_len (text, text_len);
    sscanf (data->str, "%lld", &parse_data->visit_count);
    g_string_free (data, TRUE);
  } else if (g_str_equal (parse_data->current, "5")) {
    /* Last visit */
    GString *data = g_string_new_len (text, text_len);
    sscanf (data->str, "%lld", &parse_data->last_visit);
    g_string_free (data, TRUE);
  } else if (g_str_equal (parse_data->current, "6")) {
    /* First visit */
    GString *data = g_string_new_len (text, text_len);
    sscanf (data->str, "%lld", &parse_data->first_visit);
    g_string_free (data, TRUE);
  } else if (g_str_equal (parse_data->current, "10")) {
    /* Zoom level. */
    GString *data = g_string_new_len (text, text_len);
    sscanf (data->str, "%lf", &parse_data->zoom_level);
    g_string_free (data, TRUE);
  }

  g_free (parse_data->current);
  parse_data->current = NULL;
}

static void
visit_cb (EphyHistoryService *service, gboolean success, gpointer result, gpointer user_data)
{
  all_done = TRUE;
}

static void
history_parse_end_element (GMarkupParseContext *context,
                           const char          *element_name,
                           gpointer             user_data,
                           GError             **error)
{
  HistoryParseData *parse_data = user_data;

  if (g_str_equal (element_name, "node") && parse_data) {
    /* Add one item to History */
    EphyHistoryPageVisit *visit = ephy_history_page_visit_new (parse_data->location ? parse_data->location : "",
                                                               parse_data->last_visit, EPHY_PAGE_VISIT_TYPED);
    g_free (visit->url->title);
    visit->url->title = g_strdup (parse_data->title);

    if (parse_data->zoom_level != 1.0) {
      /* Zoom levels are only stored per host in the old history, so
       * creating a new host here is OK. */
      g_assert (!visit->url->host);
      visit->url->host = ephy_history_host_new (parse_data->location, parse_data->title,
                                                parse_data->visit_count, parse_data->zoom_level);
    }

    parse_data->visits = g_list_append (parse_data->visits, visit);
  }
}

static GMarkupParser history_parse_funcs =
{
  history_parse_start_element,
  history_parse_end_element,
  history_parse_text,
  NULL,
  NULL,
};

static void
migrate_history (void)
{
  GFileInputStream *input;
  GMarkupParseContext *context;
  GError *error = NULL;
  GFile *file;
  char *filename;
  char buffer[1024];
  HistoryParseData parse_data;

  gchar *temporary_file = g_build_filename (ephy_dot_dir (), EPHY_HISTORY_FILE, NULL);
  /* Do nothing if the history file already exists. Safer than wiping
   * it out. */
  if (g_file_test (temporary_file, G_FILE_TEST_EXISTS)) {
    g_warning ("Did not migrate history, the %s file already exists", EPHY_HISTORY_FILE);
    g_free (temporary_file);
    return;
  }

  history_service = ephy_history_service_new (temporary_file, FALSE);
  g_free (temporary_file);

  memset (&parse_data, 0, sizeof (HistoryParseData));
  parse_data.location = NULL;
  parse_data.title = NULL;
  parse_data.visits = NULL;

  filename = g_build_filename (ephy_dot_dir (),
                               "ephy-history.xml",
                               NULL);

  file = g_file_new_for_path (filename);
  g_free (filename);

  input = g_file_read (file, NULL, &error);
  g_object_unref (file);

  if (error) {
    if (error->code != G_IO_ERROR_NOT_FOUND)
      g_warning ("Could not load history data, migration aborted: %s", error->message);

    g_error_free (error);
    return;
  }

  context = g_markup_parse_context_new (&history_parse_funcs, 0, &parse_data, NULL);
  while (TRUE) {
    gssize count = g_input_stream_read (G_INPUT_STREAM (input), buffer, sizeof (buffer), NULL, &error);
    if (count <= 0)
      break;

    if (!g_markup_parse_context_parse (context, buffer, count, &error))
      break;
  }

  g_markup_parse_context_free (context);
  g_input_stream_close (G_INPUT_STREAM (input), NULL, NULL);
  g_object_unref (input);

  if (parse_data.visits) {
    ephy_history_service_add_visits (history_service, parse_data.visits, NULL, (EphyHistoryJobCallback)visit_cb, NULL);
    ephy_history_page_visit_list_free (parse_data.visits);

    while (!all_done)
      g_main_context_iteration (NULL, FALSE);
  }

  g_object_unref (history_service);
}

static void
migrate_tabs_visibility (void)
{
  gboolean always_show_tabs;

  always_show_tabs = g_settings_get_boolean (EPHY_SETTINGS_UI,
                                             EPHY_PREFS_UI_ALWAYS_SHOW_TABS_BAR);

  if (always_show_tabs)
    g_settings_set_enum (EPHY_SETTINGS_UI,
                         EPHY_PREFS_UI_TABS_BAR_VISIBILITY_POLICY,
                         EPHY_PREFS_UI_TABS_BAR_VISIBILITY_POLICY_ALWAYS);
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
  GRegex * regex;
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
    EphyWebApplication *app = (EphyWebApplication*)p->data;

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
                     GAsyncResult *res,
                     gpointer userdata)
{
  secret_service_clear_finish (service, res, NULL);

  if (g_atomic_int_dec_and_test (&form_passwords_migrating))
    g_main_loop_quit (loop);
}

static void
store_form_auth_data_cb (GObject *object,
                         GAsyncResult *res,
                         GHashTable *attributes)
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
                          GAsyncResult *res,
                          gpointer data)
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
    item = (SecretItem*)l->data;

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
    secret_collection_load_items ((SecretCollection*)c->data, NULL, (GAsyncReadyCallback)load_collection_items_cb,
                                  NULL);
  }

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  g_list_free_full (collections, (GDestroyNotify)g_object_unref);
  g_object_unref (service);
}

const EphyProfileMigrator migrators[] = {
  migrate_cookies,
  migrate_passwords,
 /* Yes, again! Version 2 had some bugs, so we need to run
    migrate_passwords again to possibly migrate more passwords*/
  migrate_passwords,
  /* Very similar to migrate_passwords, but this migrates
   * login/passwords for page forms, which we previously ignored */
  migrate_passwords2,
  migrate_history,
  migrate_tabs_visibility,
  migrate_web_app_links,
  migrate_new_urls_table,
  migrate_form_passwords_to_libsecret,
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
    m();

    return TRUE;
  }

  latest = ephy_profile_utils_get_migration_version ();

  LOG ("Running migrators up to version %d, current migration version is %d.",
       EPHY_PROFILE_MIGRATION_VERSION, latest);

  for (i = latest; i < EPHY_PROFILE_MIGRATION_VERSION; i++) {
    LOG ("Running migrator: %d of %d", i, EPHY_PROFILE_MIGRATION_VERSION);

    /* No need to run the password migration twice in a row. It
       appears twice in the list for the benefit of people that were
       using the development snapshots, since an early version didn't
       migrate all passwords correctly. */
    if (i == 1)
      continue;

    m = migrators[i];
    m();
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
  { "version", 'v', 0, G_OPTION_ARG_INT, &version,
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

  if (version != -1 && version != EPHY_PROFILE_MIGRATION_VERSION) {
    g_print ("Version mismatch, version %d requested but our version is %d\n", version, EPHY_PROFILE_MIGRATION_VERSION);
    
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
