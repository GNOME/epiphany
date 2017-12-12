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
add_bookmark_to_table (EphyBookmark *bookmark, GHashTable *table)
{
  gvdb_hash_table_insert_variant (table,
                                  ephy_bookmark_get_url (bookmark),
                                  build_variant (bookmark));
}

static void
add_tag_to_table (const char *tag, GHashTable *table)
{
  gvdb_hash_table_insert (table, tag);
}

gboolean
ephy_bookmarks_export (EphyBookmarksManager  *manager,
                       const char            *filename,
                       GError               **error)
{
  GHashTable *root_table;
  GHashTable *table;
  gboolean result;

  root_table = gvdb_hash_table_new (NULL, NULL);

  table = gvdb_hash_table_new (root_table, "tags");
  g_sequence_foreach (ephy_bookmarks_manager_get_tags (manager), (GFunc)add_tag_to_table, table);
  g_hash_table_unref (table);

  table = gvdb_hash_table_new (root_table, "bookmarks");
  g_sequence_foreach (ephy_bookmarks_manager_get_bookmarks (manager), (GFunc)add_bookmark_to_table, table);
  g_hash_table_unref (table);

  result = gvdb_table_write_contents (root_table, filename, FALSE, error);
  g_hash_table_unref (root_table);

  return result;
}
