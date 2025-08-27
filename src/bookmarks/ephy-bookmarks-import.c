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
#include "ephy-sync-utils.h"
#include "gvdb-builder.h"
#include "gvdb-reader.h"

#include <glib/gi18n.h>

GQuark bookmarks_import_error_quark (void);
G_DEFINE_QUARK (BookmarksImportErrorQuark, bookmarks_import_error)
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

  /* Get tags order table */
  /* Add tags to the bookmark manager's sequence. */
  table = gvdb_table_get_table (root_table, "tags-order");
  if (table) {
    list = gvdb_table_get_names (table, &length);
    for (i = 0; i < length; i++) {
      GVariant *variant = gvdb_table_get_value (table, list[i]);
      const char *variant_tag;

      g_variant_get (variant, "(sa(si))", &variant_tag, NULL);

      ephy_bookmarks_manager_tags_order_add_tag_variant (manager, variant);
    }
    g_strfreev (list);
  }

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
  gvdb_table_free (table);

  /* Get bookmarks order table */
  table = gvdb_table_get_table (root_table, "bookmarks-order");
  if (table) {
    list = gvdb_table_get_names (table, &length);
    for (i = 0; i < length; i++) {
      GVariant *variant = gvdb_table_get_value (table, list[i]);
      const char *type, *item;
      int index;

      g_variant_get (variant, "(ssi)", &type, &item, &index);

      ephy_bookmarks_manager_add_to_bookmarks_order (manager, type, item, index);
    }
    g_strfreev (list);
  }

out:
  if (table)
    gvdb_table_free (table);
  if (bookmarks)
    g_sequence_free (bookmarks);
  if (root_table)
    gvdb_table_free (root_table);

  return res;
}

static EphyBookmark *
get_existing_bookmark (const char           *url,
                       GSequence            *tags,
                       EphyBookmarksManager *manager)
{
  GSequence *bookmarks = ephy_bookmarks_manager_get_bookmarks (manager);
  GSequenceIter *iter;
  EphyBookmark *existing_bookmark = NULL;

  for (iter = g_sequence_get_begin_iter (bookmarks);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter)) {
    EphyBookmark *bookmark = g_sequence_get (iter);
    GSequence *bookmark_tags = ephy_bookmark_get_tags (bookmark);
    GSequenceIter *tags_iter;

    if (g_strcmp0 (ephy_bookmark_get_url (bookmark), url) != 0)
      continue;

    existing_bookmark = bookmark;

    /* If the bookmark already exists, add any tags the imported bookmark has
     * that the existing one doesn't. */
    for (tags_iter = g_sequence_get_begin_iter (tags);
         !g_sequence_iter_is_end (tags_iter);
         tags_iter = g_sequence_iter_next (tags_iter)) {
      const char *tag = g_sequence_get (tags_iter);
      GSequenceIter *search_iter = g_sequence_lookup (bookmark_tags, (gpointer)tag,
                                                      (GCompareDataFunc)ephy_bookmark_tags_compare,
                                                      NULL);

      if (!search_iter)
        ephy_bookmark_add_tag (bookmark, tag);
    }
  }

  return existing_bookmark;
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

  if (!ephy_sqlite_statement_bind_int (statement, 0, bookmark_id, &error)) {
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
  GError *my_error = NULL;
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

  connection = ephy_sqlite_connection_new (EPHY_SQLITE_CONNECTION_MODE_MEMORY, filename);
  ephy_sqlite_connection_open (connection, &my_error);
  if (my_error) {
    g_warning ("Could not open database at %s: %s", filename, my_error->message);
    g_error_free (my_error);
    g_set_error (error,
                 BOOKMARKS_IMPORT_ERROR,
                 BOOKMARKS_IMPORT_ERROR_BOOKMARKS,
                 _("Firefox bookmarks database could not be opened. Close Firefox and try again."));
    goto out;
  }

  statement = ephy_sqlite_connection_create_statement (connection,
                                                       statement_str,
                                                       &my_error);
  if (!statement) {
    g_warning ("Could not build bookmarks query statement: %s", my_error->message);
    g_error_free (my_error);
    g_set_error (error,
                 BOOKMARKS_IMPORT_ERROR,
                 BOOKMARKS_IMPORT_ERROR_BOOKMARKS,
                 _("Firefox bookmarks could not be retrieved!"));
    ret = FALSE;
    goto out;
  }

  bookmarks = g_sequence_new (g_object_unref);
  while (ephy_sqlite_statement_step (statement, &my_error)) {
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
    tags = ephy_bookmark_get_tags (bookmark);

    if (get_existing_bookmark (url, tags, manager))
      g_sequence_prepend (bookmarks, get_existing_bookmark (url, tags, manager));
    else
      g_sequence_prepend (bookmarks, bookmark);
  }

  if (my_error) {
    g_warning ("Could not execute bookmarks query statement: %s", my_error->message);
    g_error_free (my_error);
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

void
replace_str (char **src,
             char  *find,
             char  *replace)
{
  g_auto (GStrv) split = g_strsplit (*src, find, -1);
  g_free (*src);
  *src = g_strjoinv (replace, split);
}

typedef struct {
  GQueue *tags_stack;
  GHashTable *urls_table;
  GPtrArray *tags;
  GPtrArray *urls;
  GPtrArray *add_dates;
  GPtrArray *titles;
  gboolean read_title;
  gboolean read_tag;
  gboolean skip_bookmark;
} ParserData;

static ParserData *
parser_data_new ()
{
  ParserData *data;

  data = g_new (ParserData, 1);
  data->tags_stack = g_queue_new ();
  data->urls_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_ptr_array_unref);
  data->tags = g_ptr_array_new_with_free_func (g_free);
  data->urls = g_ptr_array_new_with_free_func (g_free);
  data->add_dates = g_ptr_array_new_with_free_func (g_free);
  data->titles = g_ptr_array_new_with_free_func (g_free);
  data->read_title = FALSE;
  data->read_tag = FALSE;
  data->skip_bookmark = FALSE;

  return data;
}

static void
parser_data_free (ParserData *data)
{
  g_queue_free_full (data->tags_stack, g_free);
  g_hash_table_destroy (data->urls_table);
  g_ptr_array_free (data->tags, TRUE);
  g_ptr_array_free (data->urls, TRUE);
  g_ptr_array_free (data->titles, TRUE);
  g_ptr_array_free (data->add_dates, TRUE);
  g_free (data);
}

static void
xml_start_element (GMarkupParseContext  *context,
                   const gchar          *element_name,
                   const gchar         **attribute_names,
                   const gchar         **attribute_values,
                   gpointer              user_data,
                   GError              **error)
{
  ParserData *data = user_data;
  const gchar **names = attribute_names;
  const gchar **values = attribute_values;

  if (strcmp (element_name, "H3") == 0) {
    data->read_tag = TRUE;
  } else if (strcmp (element_name, "A") == 0) {
    data->read_title = TRUE;

    while (*names) {
      if (strcmp (*names, "HREF") == 0) {
        GPtrArray *tags;
        const char *tag = g_queue_peek_head (data->tags_stack);

        if (g_hash_table_lookup_extended (data->urls_table, *values, NULL, (gpointer *)&tags)) {
          g_ptr_array_add (tags, g_strdup (tag));
          data->skip_bookmark = TRUE;
        } else {
          tags = g_ptr_array_new_with_free_func (g_free);
          g_ptr_array_add (tags, g_strdup (tag));
          g_hash_table_insert (data->urls_table, g_strdup (*values), tags);
          g_ptr_array_add (data->urls, g_strdup (*values));
          data->skip_bookmark = FALSE;
        }
      } else if (strcmp (*names, "ADD_DATE") == 0 && !data->skip_bookmark)
        g_ptr_array_add (data->add_dates, g_strdup (*values));
      names++;
      values++;
    }
  }
}

static void
xml_end_element (GMarkupParseContext  *context,
                 const gchar          *element_name,
                 gpointer              user_data,
                 GError              **error)
{
  ParserData *data = user_data;

  if (strcmp (element_name, "H3") == 0)
    data->read_tag = FALSE;
  else if (strcmp (element_name, "A") == 0)
    data->read_title = FALSE;
  else if (strcmp (element_name, "DL") == 0)
    g_free (g_queue_pop_head (data->tags_stack));
}

static void
xml_text (GMarkupParseContext  *context,
          const gchar          *text,
          gsize                 text_len,
          gpointer              user_data,
          GError              **error)
{
  ParserData *data = user_data;

  if (data->read_tag) {
    g_queue_push_head (data->tags_stack, g_strdup (text));
    g_ptr_array_add (data->tags, g_strdup (text));
  }

  if (data->read_title && !data->skip_bookmark)
    g_ptr_array_add (data->titles, g_strdup (text));
}

gboolean
ephy_bookmarks_import_from_html (EphyBookmarksManager  *manager,
                                 const char            *filename,
                                 GError               **error)
{
  GMarkupParser parser;
  g_autofree gchar *buf = NULL;
  g_autoptr (GMarkupParseContext) context = NULL;
  g_autoptr (GError) my_error = NULL;
  g_autoptr (GMappedFile) mapped = NULL;
  g_autoptr (GSequence) bookmarks = NULL;
  ParserData *data;

  mapped = g_mapped_file_new (filename, FALSE, &my_error);

  if (!mapped) {
    g_set_error (error,
                 BOOKMARKS_IMPORT_ERROR,
                 BOOKMARKS_IMPORT_ERROR_BOOKMARKS,
                 _("HTML bookmarks database could not be opened: %s"),
                 my_error->message);
    return FALSE;
  }

  buf = g_strdup (g_mapped_file_get_contents (mapped));

  if (!buf) {
    g_set_error_literal (error,
                         BOOKMARKS_IMPORT_ERROR,
                         BOOKMARKS_IMPORT_ERROR_BOOKMARKS,
                         _("HTML bookmarks database could not be read."));
    return FALSE;
  }

  replace_str (&buf, "<DT>", "");
  replace_str (&buf, "<p>", "");
  replace_str (&buf, "&", "&amp;");
  replace_str (&buf, "<HR>", "<HR/>");

  parser.start_element = xml_start_element;
  parser.end_element = xml_end_element;
  parser.text = xml_text;
  parser.passthrough = NULL;
  parser.error = NULL;

  data = parser_data_new ();

  context = g_markup_parse_context_new (&parser, 0, (gpointer)data, NULL);
  if (!g_markup_parse_context_parse (context, buf, strlen (buf), &my_error)) {
    g_set_error (error,
                 BOOKMARKS_IMPORT_ERROR,
                 BOOKMARKS_IMPORT_ERROR_BOOKMARKS,
                 _("HTML bookmarks database could not be parsed: %s"),
                 my_error->message);
    parser_data_free (data);
    return FALSE;
  }

  for (guint i = 0; i < data->tags->len; i++)
    ephy_bookmarks_manager_create_tag (manager, g_ptr_array_index (data->tags, i));

  bookmarks = g_sequence_new (g_object_unref);
  for (guint i = 0; i < data->urls->len; i++) {
    g_autofree const char *guid = ephy_bookmark_generate_random_id ();
    const char *url = g_ptr_array_index (data->urls, i);
    const char *title = g_ptr_array_index (data->titles, i);
    gint64 time_added = (gint64)g_ptr_array_index (data->add_dates, i);
    EphyBookmark *bookmark;
    GSequence *tags;
    GPtrArray *val;

    tags = g_sequence_new (g_free);
    g_hash_table_lookup_extended (data->urls_table, url, NULL, (gpointer *)&val);
    for (guint j = 0; j < val->len; j++) {
      char *tag = g_ptr_array_index (val, j);
      if (tag)
        g_sequence_append (tags, g_strdup (tag));
    }

    bookmark = get_existing_bookmark (url, tags, manager);
    if (!bookmark) {
      bookmark = ephy_bookmark_new (url, title, tags, guid);
      ephy_bookmark_set_time_added (bookmark, time_added);
      ephy_synchronizable_set_server_time_modified (EPHY_SYNCHRONIZABLE (bookmark), time_added);

      g_sequence_prepend (bookmarks, bookmark);
    }
  }
  ephy_bookmarks_manager_add_bookmarks (manager, bookmarks);

  parser_data_free (data);
  return TRUE;
}

static void chrome_import_folder (JsonObject *object,
                                  GSequence  *bookmarks);

static void
chrome_add_child (JsonArray *array,
                  guint      index_,
                  JsonNode  *element_node,
                  gpointer   user_data)
{
  GSequence *bookmarks = user_data;
  JsonObject *object = json_node_get_object (element_node);
  const char *title;
  const char *time;
  const char *type;

  if (!object)
    return;

  title = json_object_get_string_member (object, "name");
  type = json_object_get_string_member (object, "type");
  time = json_object_get_string_member (object, "date_added");

  if (g_strcmp0 (type, "url") == 0) {
    const char *url;

    url = json_object_get_string_member (object, "url");

    if (title && url && !g_str_has_prefix (url, "chrome://") && time) {
      g_autofree const char *guid = ephy_bookmark_generate_random_id ();
      EphyBookmark *bookmark;
      GSequence *tags;
      gint64 time_added;

      tags = g_sequence_new (g_free);
      time_added = g_ascii_strtoll (time, NULL, 0);

      bookmark = ephy_bookmark_new (url, title, tags, guid);
      ephy_bookmark_set_time_added (bookmark, time_added);
      ephy_synchronizable_set_server_time_modified (EPHY_SYNCHRONIZABLE (bookmark), time_added);

      g_sequence_prepend (bookmarks, bookmark);
    }
  } else if (g_strcmp0 (type, "folder") == 0) {
    chrome_import_folder (object, bookmarks);
  }
}

static void
chrome_import_folder (JsonObject *object,
                      GSequence  *bookmarks)
{
  JsonArray *children;
  const char *type;

  type = json_object_get_string_member (object, "type");
  if (g_strcmp0 (type, "folder") != 0)
    return;

  children = json_object_get_array_member (object, "children");
  if (children)
    json_array_foreach_element (children, chrome_add_child, bookmarks);
}

static void
chrome_parse_root (JsonObject  *object,
                   const gchar *member_name,
                   JsonNode    *member_node,
                   gpointer     user_data)
{
  JsonObject *member_object;

  member_object = json_node_get_object (member_node);
  chrome_import_folder (member_object, user_data);
}

gboolean
ephy_bookmarks_import_from_chrome (EphyBookmarksManager  *manager,
                                   const char            *filename,
                                   GError               **error)
{
  g_autoptr (GSequence) bookmarks = NULL;
  g_autoptr (JsonParser) parser = NULL;
  JsonNode *root;
  JsonObject *object;
  JsonObject *roots_object;
  GSequenceIter *iter;

  parser = json_parser_new ();

  if (!json_parser_load_from_file (parser, filename, error))
    return FALSE;

  root = json_parser_get_root (parser);
  if (!root)
    goto parser_error;

  object = json_node_get_object (root);
  if (!object)
    goto parser_error;

  roots_object = json_object_get_object_member (object, "roots");
  if (!roots_object)
    goto parser_error;

  bookmarks = g_sequence_new (g_object_unref);

  json_object_foreach_member (roots_object, chrome_parse_root, bookmarks);

  for (iter = g_sequence_get_begin_iter (bookmarks);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter)) {
    EphyBookmark *bookmark = g_sequence_get (iter);
    const char *url = ephy_bookmark_get_url (bookmark);
    GSequence *tags = ephy_bookmark_get_tags (bookmark);
    EphyBookmark *existing_bookmark = get_existing_bookmark (url, tags, manager);

    if (existing_bookmark) {
      g_sequence_insert_before (iter, existing_bookmark);
      g_sequence_remove (iter);
    }
  }

  ephy_bookmarks_manager_add_bookmarks (manager, bookmarks);

  return TRUE;

parser_error:
  g_set_error (error,
               BOOKMARKS_IMPORT_ERROR,
               BOOKMARKS_IMPORT_ERROR_BOOKMARKS,
               _("Bookmarks file could not be parsed:"));

  return FALSE;
}
