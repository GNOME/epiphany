/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2016 Iulian-Gabriel Radu <iulian.radu67@gnome.org>
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
#include "ephy-bookmarks-import.h"

#include "ephy-shell.h"
#include "ephy-sqlite-connection.h"
#include "gvdb-builder.h"
#include "gvdb-reader.h"

#include <glib/gi18n.h>

GQuark bookmarks_import_error_quark (void);
G_DEFINE_QUARK (bookmarks - import - error - quark, bookmarks_import_error)
#define BOOKMARKS_IMPORT_ERROR bookmarks_import_error_quark ()

typedef enum {
  BOOKMARKS_IMPORT_ERROR_TAGS = 1001,
  BOOKMARKS_IMPORT_ERROR_BOOKMARKS = 1002
} BookmarksImportErrorCode;

static GSequence *
get_bookmarks_from_table (GvdbTable *table)
{
  GSequence *bookmarks = NULL;
  char **list = NULL;
  gsize length;
  guint i;

  bookmarks = g_sequence_new (g_object_unref);

  /* Iterate over all keys (url's) in the table. */
  list = gvdb_table_get_names (table, &length);
  for (i = 0; i < length; i++) {
    EphyBookmark *bookmark;
    GVariant *value;
    GVariantIter *iter;
    GSequence *tags;
    char *tag;
    const char *title;
    gint64 time_added;
    char *id;
    gint64 server_time_modified;
    gboolean is_uploaded;

    /* Obtain the corresponding GVariant. */
    value = gvdb_table_get_value (table, list[i]);

    g_variant_get (value, "(x&s&sxbas)",
                   &time_added, &title, &id,
                   &server_time_modified, &is_uploaded, &iter);

    /* Add all stored tags in a GSequence. */
    tags = g_sequence_new (g_free);
    while (g_variant_iter_next (iter, "s", &tag)) {
      g_sequence_insert_sorted (tags, tag,
                                (GCompareDataFunc)ephy_bookmark_tags_compare,
                                NULL);
    }
    g_variant_iter_free (iter);

    /* Create the new bookmark. */
    bookmark = ephy_bookmark_new (list[i], title, tags, id);
    ephy_bookmark_set_time_added (bookmark, time_added);
    ephy_synchronizable_set_server_time_modified (EPHY_SYNCHRONIZABLE (bookmark), server_time_modified);
    ephy_bookmark_set_is_uploaded (bookmark, is_uploaded);
    g_sequence_prepend (bookmarks, bookmark);

    g_variant_unref (value);
  }

  g_strfreev (list);

  return bookmarks;
}

gboolean
ephy_bookmarks_import (EphyBookmarksManager  *manager,
                       const char            *filename,
                       GError               **error)
{
  GvdbTable *root_table = NULL;
  GvdbTable *table = NULL;
  GSequence *bookmarks = NULL;
  char **list = NULL;
  gboolean res = TRUE;
  gsize length;
  guint i;

  /* Create a new table to hold data stored in file.
   *
   * FIXME: This uses mmap so it's doing sync I/O, which is not cool. It's
   * not straightforward to fix, but it would be nice to have an async
   * constructor in GVDB. Then we could make ephy_bookmarks_import() async.
   */
  root_table = gvdb_table_new (filename, TRUE, error);
  if (!root_table) {
    res = FALSE;
    goto out;
  }

  /* Add tags to the bookmark manager's sequence. */
  table = gvdb_table_get_table (root_table, "tags");
  if (!table) {
    g_set_error (error,
                 BOOKMARKS_IMPORT_ERROR,
                 BOOKMARKS_IMPORT_ERROR_TAGS,
                 _("File is not a valid Epiphany bookmarks file: missing tags table"));
    res = FALSE;
    goto out;
  }

  /* Iterate over all keys (url's) in the table. */
  list = gvdb_table_get_names (table, &length);
  for (i = 0; i < length; i++)
    ephy_bookmarks_manager_create_tag (manager, list[i]);
  g_strfreev (list);
  gvdb_table_free (table);

  /* Get bookmarks table */
  table = gvdb_table_get_table (root_table, "bookmarks");
  if (!table) {
    g_set_error (error,
                 BOOKMARKS_IMPORT_ERROR,
                 BOOKMARKS_IMPORT_ERROR_BOOKMARKS,
                 _("File is not a valid Epiphany bookmarks file: missing bookmarks table"));
    res = FALSE;
    goto out;
  }

  bookmarks = get_bookmarks_from_table (table);
  ephy_bookmarks_manager_add_bookmarks (manager, bookmarks);

out:
  if (table)
    gvdb_table_free (table);
  if (bookmarks)
    g_sequence_free (bookmarks);
  if (root_table)
    gvdb_table_free (root_table);

  return res;
}

static void
load_tags_for_bookmark (EphySQLiteConnection *connection,
                        EphyBookmark         *bookmark,
                        int                   bookmark_id)
{
  EphyBookmarksManager *manager = ephy_shell_get_bookmarks_manager (ephy_shell_get_default ());
  EphySQLiteStatement *statement = NULL;
  GError *error = NULL;
  const char *statement_str = "SELECT tag.title "
                              "FROM moz_bookmarks b, moz_bookmarks tag "
                              "WHERE b.fk=(SELECT fk FROM moz_bookmarks WHERE id=?) "
                              "AND b.title IS NULL "
                              "AND tag.id=b.parent "
                              "ORDER BY tag.title ";

  statement = ephy_sqlite_connection_create_statement (connection,
                                                       statement_str,
                                                       &error);
  if (error) {
    g_warning ("[Bookmark %d] Could not build tags query statement: %s", bookmark_id, error->message);
    goto out;
  }

  if (ephy_sqlite_statement_bind_int (statement, 0, bookmark_id, &error) == FALSE) {
    g_warning ("[Bookmark %d] Could not bind tag id in statement: %s", bookmark_id, error->message);
    goto out;
  }

  while (ephy_sqlite_statement_step (statement, &error)) {
    const char *tag = ephy_sqlite_statement_get_column_as_string (statement, 0);

    if (!ephy_bookmarks_manager_tag_exists (manager, tag))
      ephy_bookmarks_manager_create_tag (manager, tag);

    ephy_bookmark_add_tag (bookmark, tag);
  }

  if (error) {
    g_warning ("[Bookmark %d] Could not execute tags query statement: %s", bookmark_id, error->message);
    goto out;
  }

out:
  if (statement)
    g_object_unref (statement);
  if (error)
    g_error_free (error);
}

gboolean
ephy_bookmarks_import_from_firefox (EphyBookmarksManager  *manager,
                                    const gchar           *profile,
                                    GError               **error)
{
  EphySQLiteConnection *connection = NULL;
  EphySQLiteStatement *statement = NULL;
  GSequence *bookmarks = NULL;
  gboolean ret = TRUE;
  gchar *filename;
  const char *statement_str = "SELECT b.id, p.url, b.title, b.dateAdded, b.guid, g.title "
                              "FROM moz_bookmarks b "
                              "JOIN moz_places p ON b.fk=p.id "
                              "JOIN moz_bookmarks g ON b.parent=g.id "
                              "WHERE b.type=1 AND p.url NOT LIKE 'about%' "
                              "               AND p.url NOT LIKE 'place%' "
                              "               AND b.title IS NOT NULL "
                              "ORDER BY p.url ";

  filename = g_build_filename (g_get_home_dir (),
                               FIREFOX_PROFILES_DIR,
                               profile,
                               FIREFOX_BOOKMARKS_FILE,
                               NULL);

  connection = ephy_sqlite_connection_new (EPHY_SQLITE_CONNECTION_MODE_READ_ONLY, filename);
  ephy_sqlite_connection_open (connection, error);
  if (*error) {
    g_warning ("Could not open database at %s: %s", filename, (*error)->message);
    g_error_free (*error);
    g_set_error (error,
                 BOOKMARKS_IMPORT_ERROR,
                 BOOKMARKS_IMPORT_ERROR_BOOKMARKS,
                 _("Firefox bookmarks database could not be opened. Close Firefox and try again."));
    goto out;
  }

  statement = ephy_sqlite_connection_create_statement (connection,
                                                       statement_str,
                                                       error);
  if (statement == NULL) {
    g_warning ("Could not build bookmarks query statement: %s", (*error)->message);
    g_error_free (*error);
    g_set_error (error,
                 BOOKMARKS_IMPORT_ERROR,
                 BOOKMARKS_IMPORT_ERROR_BOOKMARKS,
                 _("Firefox bookmarks could not be retrieved!"));
    ret = FALSE;
    goto out;
  }

  bookmarks = g_sequence_new (g_object_unref);
  while (ephy_sqlite_statement_step (statement, error)) {
    int bookmark_id = ephy_sqlite_statement_get_column_as_int (statement, 0);
    const char *url = ephy_sqlite_statement_get_column_as_string (statement, 1);
    const char *title = ephy_sqlite_statement_get_column_as_string (statement, 2);
    gint64 time_added = ephy_sqlite_statement_get_column_as_int64 (statement, 3);
    const char *guid = ephy_sqlite_statement_get_column_as_string (statement, 4);
    const char *parent_title = ephy_sqlite_statement_get_column_as_string (statement, 5);
    EphyBookmark *bookmark;
    GSequence *tags;

    tags = g_sequence_new (g_free);
    bookmark = ephy_bookmark_new (url, title, tags, guid);
    ephy_bookmark_set_time_added (bookmark, time_added);
    if (!g_strcmp0 (parent_title, FIREFOX_BOOKMARKS_MOBILE_FOLDER))
      ephy_bookmark_add_tag (bookmark, EPHY_BOOKMARKS_MOBILE_TAG);
    load_tags_for_bookmark (connection, bookmark, bookmark_id);

    g_sequence_prepend (bookmarks, bookmark);
  }

  if (*error) {
    g_warning ("Could not execute bookmarks query statement: %s", (*error)->message);
    g_error_free (*error);
    g_set_error (error,
                 BOOKMARKS_IMPORT_ERROR,
                 BOOKMARKS_IMPORT_ERROR_BOOKMARKS,
                 _("Firefox bookmarks could not be retrieved!"));
    ret = FALSE;
    goto out;
  }

  ephy_bookmarks_manager_add_bookmarks (manager, bookmarks);

out:
  g_free (filename);
  if (connection) {
    ephy_sqlite_connection_close (connection);
    g_object_unref (connection);
  }
  if (statement)
    g_object_unref (statement);
  if (bookmarks)
    g_sequence_free (bookmarks);

  return ret;
}
