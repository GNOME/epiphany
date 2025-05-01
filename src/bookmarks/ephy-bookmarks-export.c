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
#include "ephy-bookmarks-export.h"

#include "ephy-synchronizable.h"
#include "gvdb-builder.h"

static void
gvdb_hash_table_insert_variant (GHashTable *table,
                                const char *key,
                                GVariant   *value)
{
  GvdbItem *item;

  item = gvdb_hash_table_insert (table, key);
  gvdb_item_set_value (item, value);
}

static GVariant *
build_variant (EphyBookmark *bookmark)
{
  GVariantBuilder builder;
  GSequence *tags;
  GSequenceIter *iter;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("(xssxbas)"));

  g_variant_builder_add (&builder, "x", ephy_bookmark_get_time_added (bookmark));
  g_variant_builder_add (&builder, "s", ephy_bookmark_get_title (bookmark));
  g_variant_builder_add (&builder, "s", ephy_bookmark_get_id (bookmark));
  g_variant_builder_add (&builder, "x", ephy_synchronizable_get_server_time_modified (EPHY_SYNCHRONIZABLE (bookmark)));
  g_variant_builder_add (&builder, "b", ephy_bookmark_is_uploaded (bookmark));

  g_variant_builder_open (&builder, G_VARIANT_TYPE ("as"));
  tags = ephy_bookmark_get_tags (bookmark);
  for (iter = g_sequence_get_begin_iter (tags);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter)) {
    g_variant_builder_add (&builder, "s", g_sequence_get (iter));
  }
  g_variant_builder_close (&builder);

  return g_variant_builder_end (&builder);
}

static void
add_bookmark_to_table (EphyBookmark *bookmark,
                       GHashTable   *table)
{
  gvdb_hash_table_insert_variant (table,
                                  ephy_bookmark_get_url (bookmark),
                                  build_variant (bookmark));
}

static void
add_tag_to_table (const char *tag,
                  GHashTable *table)
{
  gvdb_hash_table_insert (table, tag);
}

static void
add_to_tags_order_table (GVariant   *variant,
                         GHashTable *table)
{
  const char *tag;
  GVariantBuilder builder;
  GVariant *saved_variant;
  GVariantIter *iter;
  const char *url;
  int index;

  g_variant_get (variant, "(sa(si))", &tag, &iter);

  /* A duplicate of the variant is stored so Epiphany can still access the
   * original one while running. */
  g_variant_builder_init (&builder, G_VARIANT_TYPE ("(sa(si))"));
  g_variant_builder_add (&builder, "s", tag);
  g_variant_builder_open (&builder, G_VARIANT_TYPE ("a(si)"));

  while (g_variant_iter_next (iter, "(si)", &url, &index)) {
    g_variant_builder_open (&builder, G_VARIANT_TYPE ("(si)"));
    g_variant_builder_add (&builder, "s", url);
    g_variant_builder_add (&builder, "i", index);
    g_variant_builder_close (&builder);
  }
  g_variant_iter_free (iter);

  g_variant_builder_close (&builder);
  saved_variant = g_variant_builder_end (&builder);
  gvdb_hash_table_insert_variant (table, tag, saved_variant);
}

static void
add_to_bookmarks_order_table (GVariant   *variant,
                              GHashTable *table)
{
  const char *type, *item, *key;
  int index;
  GVariant *saved_variant;

  g_variant_get (variant, "(ssi)", &type, &item, &index);
  key = g_strconcat (type, ":", item, NULL);

  /* A duplicate of the variant is stored so Epiphany can still access the
   * original one while running. */
  saved_variant = g_variant_new ("(ssi)", type, item, index);
  gvdb_hash_table_insert_variant (table, key, saved_variant);
}

static void
write_contents_cb (GObject      *source_object,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  g_autoptr (GTask) task = user_data;
  GHashTable *root_table;
  GError *error = NULL;

  root_table = g_task_get_task_data (task);

  if (!gvdb_table_write_contents_finish (root_table, result, &error)) {
    g_task_return_error (task, error);
    return;
  }

  g_task_return_boolean (task, TRUE);
}

static void
add_tags_to_string (const char *tag,
                    GString    *tags)
{
  g_string_append_printf (tags, "%s%s", tags->len ? ", " : "", tag);
}

static void
add_bookmark_to_html (EphyBookmark *bookmark,
                      GString      *html)
{
  GSequence *tag_sequence;
  g_autoptr (GString) tags = NULL;

  tag_sequence = ephy_bookmark_get_tags (bookmark);
  if (tag_sequence) {
    tags = g_string_new ("");
    g_sequence_foreach (tag_sequence, (GFunc)add_tags_to_string, tags);
  }

  g_string_append_printf (html,
                          "<DT><A HREF=\"%s\" ADD_DATE=\"%ld\" TAGS=\"%s\">%s</A>\n",
                          ephy_bookmark_get_url (bookmark),
                          ephy_bookmark_get_time_added (bookmark),
                          tags ? tags->str : "",
                          ephy_bookmark_get_title (bookmark));
}

static void
write_html_contents_cb (GObject      *source_object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  g_autoptr (GTask) task = user_data;
  GFile *file;
  GError *error = NULL;

  file = g_task_get_task_data (task);

  if (!g_file_replace_contents_finish (file, result, NULL, &error)) {
    g_task_return_error (task, error);
    return;
  }

  g_task_return_boolean (task, TRUE);
}

void
ephy_bookmarks_export (EphyBookmarksManager *manager,
                       const char           *filename,
                       gboolean              with_bookmarks_order,
                       gboolean              with_tags_order,
                       GCancellable         *cancellable,
                       GAsyncReadyCallback   callback,
                       gpointer              user_data)
{
  if (g_str_has_suffix (filename, ".gvdb")) {
    GHashTable *root_table;
    GHashTable *table;
    GTask *task;

    root_table = gvdb_hash_table_new (NULL, NULL);

    if (with_tags_order) {
      table = gvdb_hash_table_new (root_table, "tags-order");
      g_sequence_foreach (ephy_bookmarks_manager_get_tags_order (manager), (GFunc)add_to_tags_order_table, table);
      g_hash_table_unref (table);
    }

    table = gvdb_hash_table_new (root_table, "tags");
    g_sequence_foreach (ephy_bookmarks_manager_get_tags (manager), (GFunc)add_tag_to_table, table);
    g_hash_table_unref (table);

    if (with_bookmarks_order) {
      table = gvdb_hash_table_new (root_table, "bookmarks-order");
      g_sequence_foreach (ephy_bookmarks_manager_get_bookmarks_order (manager), (GFunc)add_to_bookmarks_order_table, table);
      g_hash_table_unref (table);
    }

    table = gvdb_hash_table_new (root_table, "bookmarks");
    g_sequence_foreach (ephy_bookmarks_manager_get_bookmarks (manager), (GFunc)add_bookmark_to_table, table);
    g_hash_table_unref (table);

    task = g_task_new (manager, cancellable, callback, user_data);
    g_task_set_task_data (task, root_table, (GDestroyNotify)g_hash_table_unref);

    gvdb_table_write_contents_async (root_table, filename, FALSE,
                                     cancellable, write_contents_cb, task);
  } else {
    g_autoptr (GString) html = NULL;
    g_autoptr (GBytes) bytes = NULL;
    g_autoptr (GFile) file = NULL;
    GTask *task;

    html = g_string_new ("<!DOCTYPE NETSCAPE-Bookmark-file-1>\n");
    g_string_append (html, "<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=UTF-8\">\n");
    g_string_append (html, "<TITLE>Bookmarks</TITLE>\n");
    g_string_append (html, "<H1>Epiphany Bookmarks</H1>\n");
    g_string_append (html, "<DL><p>\n");
    g_string_append (html, "<DT><H3>Epiphany</H3>\n");
    g_string_append (html, "<DL><p>\n");

    g_sequence_foreach (ephy_bookmarks_manager_get_bookmarks (manager), (GFunc)add_bookmark_to_html, html);

    g_string_append (html, "</DL>\n");

    file = g_file_new_for_path (filename);

    task = g_task_new (manager, cancellable, callback, user_data);
    g_task_set_task_data (task, file, (GDestroyNotify)g_object_unref);

    bytes = g_bytes_new (html->str, html->len);
    g_file_replace_contents_bytes_async (g_steal_pointer (&file),
                                         bytes,
                                         NULL,
                                         FALSE,
                                         G_FILE_CREATE_REPLACE_DESTINATION,
                                         cancellable,
                                         write_html_contents_cb,
                                         g_steal_pointer (&task));
  }
}

gboolean
ephy_bookmarks_export_finish (EphyBookmarksManager  *manager,
                              GAsyncResult          *result,
                              GError               **error)
{
  g_assert (g_task_is_valid (result, manager));

  return g_task_propagate_boolean (G_TASK (result), error);
}
