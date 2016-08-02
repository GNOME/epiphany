/*
 * Copyright (C) 2016 Iulian-Gabriel Radu <iulian.radu67@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "ephy-bookmarks-manager.h"

#include "ephy-debug.h"
#include "ephy-file-helpers.h"
#include "gvdb-builder.h"
#include "gvdb-reader.h"

#define EPHY_BOOKMARKS_FILE "bookmarks.gvdb"

struct _EphyBookmarksManager {
  GObject     parent_instance;

  GList      *bookmarks;
  GSequence  *tags;

  gchar      *gvdb_filename;
};

static void list_model_iface_init     (GListModelInterface *iface);

G_DEFINE_TYPE_EXTENDED (EphyBookmarksManager, ephy_bookmarks_manager, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

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

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));
  g_variant_builder_add (&builder, "s", ephy_bookmark_get_title (bookmark));

  tags = ephy_bookmark_get_tags (bookmark);
  for (iter = g_sequence_get_begin_iter (tags);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter)) {
    g_variant_builder_add (&builder, "s", g_sequence_get (iter));
  }

  return g_variant_builder_end (&builder);
}

static void
data_saved_cb (GObject      *object,
               GAsyncResult *result,
               gpointer      user_data)
{
  EphyBookmarksManager *self = EPHY_BOOKMARKS_MANAGER (object);
  gboolean ret;

  ret = ephy_bookmarks_manager_save_to_file_finish (self, result, NULL);
  if (ret)
    LOG ("Bookmarks data saved");
}

static void
add_tag_to_table (const char *tag, GHashTable *table)
{
  gvdb_hash_table_insert (table, tag);
}


static gboolean
ephy_bookmarks_manager_save_to_file (EphyBookmarksManager *self)
{
  GHashTable *root_table;
  GHashTable *table;
  GList *l;
  gboolean result;

  root_table = gvdb_hash_table_new (NULL, NULL);

  table = gvdb_hash_table_new (root_table, "bookmarks");
  for (l = self->bookmarks; l != NULL; l = l->next) {
    gvdb_hash_table_insert_variant (table,
                                    ephy_bookmark_get_url (l->data),
                                    build_variant (l->data));
  }
  g_hash_table_unref (table);

  table = gvdb_hash_table_new (root_table, "tags");
  g_sequence_foreach (self->tags, (GFunc)add_tag_to_table, table);
  g_hash_table_unref (table);

  result = gvdb_table_write_contents (root_table, self->gvdb_filename, FALSE, NULL);
  g_hash_table_unref (root_table);

  return result;
}

static void
ephy_bookmarks_manager_finalize (GObject *object)
{
  EphyBookmarksManager *self = EPHY_BOOKMARKS_MANAGER (object);

  if (self->bookmarks != NULL) {
    g_list_free_full (self->bookmarks, g_object_unref);
    self->bookmarks = NULL;
  }

  g_clear_pointer (&self->tags, g_sequence_free);

  G_OBJECT_CLASS (ephy_bookmarks_manager_parent_class)->finalize (object);
}

static void
ephy_bookmarks_manager_class_init (EphyBookmarksManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ephy_bookmarks_manager_finalize;
}

static void
ephy_bookmarks_manager_init (EphyBookmarksManager *self)
{
  self->gvdb_filename = g_build_filename (ephy_dot_dir (),
                                          EPHY_BOOKMARKS_FILE,
                                          NULL);

  self->tags = g_sequence_new (g_free);

  /* Create DB file if it doesn't already exists */
  if (!g_file_test (self->gvdb_filename, G_FILE_TEST_EXISTS))
    ephy_bookmarks_manager_save_to_file (self);

  ephy_bookmarks_manager_load_from_file (self);
}

static GType
ephy_bookmarks_manager_get_item_type (GListModel *model)
{
  return EPHY_TYPE_BOOKMARK;
}

static guint
ephy_bookmarks_manager_get_n_items (GListModel *model)
{
  EphyBookmarksManager *self = (EphyBookmarksManager *)model;

  g_assert (EPHY_IS_BOOKMARKS_MANAGER (self));

  return g_list_length (self->bookmarks);
}

static gpointer
ephy_bookmarks_manager_get_item (GListModel *model,
                                 guint       position)
{
  EphyBookmarksManager *self = (EphyBookmarksManager *)model;

  g_return_val_if_fail (EPHY_IS_BOOKMARKS_MANAGER (self), NULL);
  g_return_val_if_fail (position < g_list_length (self->bookmarks), NULL);

  return g_object_ref (g_list_nth_data (self->bookmarks, position));
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = ephy_bookmarks_manager_get_item_type;
  iface->get_n_items = ephy_bookmarks_manager_get_n_items;
  iface->get_item = ephy_bookmarks_manager_get_item;
}

static int
bookmark_url_compare (gpointer a, gpointer b)
{
  EphyBookmark *bookmark1 = EPHY_BOOKMARK (a);
  EphyBookmark *bookmark2 = EPHY_BOOKMARK (b);
  const char *url1;
  const char *url2;

  url1 = ephy_bookmark_get_url (bookmark1);
  url2 = ephy_bookmark_get_url (bookmark2);

  return g_strcmp0 (url1, url2);
}

void
ephy_bookmarks_manager_add_bookmark (EphyBookmarksManager *self,
                                     EphyBookmark         *bookmark)
{
  g_return_if_fail (EPHY_IS_BOOKMARKS_MANAGER (self));
  g_return_if_fail (EPHY_IS_BOOKMARK (bookmark));

  if (g_list_find_custom (self->bookmarks,
                          bookmark,
                          (GCompareFunc)bookmark_url_compare))
    return;

  g_signal_connect_object (bookmark,
                           "removed",
                           G_CALLBACK (ephy_bookmarks_manager_remove_bookmark),
                           self,
                           G_CONNECT_SWAPPED);

  self->bookmarks = g_list_prepend (self->bookmarks, bookmark);

  ephy_bookmarks_manager_save_to_file_async (self, NULL,
                                             (GAsyncReadyCallback) data_saved_cb,
                                             NULL);
}

void
ephy_bookmarks_manager_remove_bookmark (EphyBookmarksManager *self,
                                        EphyBookmark         *bookmark)
{
  gint position;

  g_return_if_fail (EPHY_IS_BOOKMARKS_MANAGER (self));
  g_return_if_fail (EPHY_IS_BOOKMARK (bookmark));

  position = g_list_position (self->bookmarks,
                              g_list_find (self->bookmarks, bookmark));

  self->bookmarks = g_list_remove (self->bookmarks, bookmark);
  g_list_model_items_changed (G_LIST_MODEL (self), position, 1, 0);

  ephy_bookmarks_manager_save_to_file_async (self, NULL,
                                             (GAsyncReadyCallback) data_saved_cb,
                                             NULL);
}

static int
bookmark_with_url_compare (gpointer *ebookmark, gconstpointer url)
{
  EphyBookmark *bookmark = EPHY_BOOKMARK (ebookmark);
  const char *bookmark_url;

  bookmark_url = ephy_bookmark_get_url (bookmark);

  return g_strcmp0 (bookmark_url, url);
}

EphyBookmark *
ephy_bookmarks_manager_get_bookmark_by_url (EphyBookmarksManager *self,
                                            const char           *url)
{
  GList *bookmark_node;

  g_return_val_if_fail (EPHY_IS_BOOKMARKS_MANAGER (self), FALSE);
  g_return_val_if_fail (url != NULL, FALSE);

  bookmark_node = g_list_find_custom (self->bookmarks,
                                      url,
                                      (GCompareFunc)bookmark_with_url_compare);

  if (!bookmark_node)
    return NULL;

  return bookmark_node->data;
}

void
ephy_bookmarks_manager_add_tag (EphyBookmarksManager *self, const char *tag)
{
  GSequenceIter *tag_iter;
  GSequenceIter *prev_tag_iter;

  g_return_if_fail (EPHY_IS_BOOKMARKS_MANAGER (self));
  g_return_if_fail (tag != NULL);

  tag_iter = g_sequence_search (self->tags,
                                (gpointer)tag,
                                (GCompareDataFunc)ephy_bookmark_tags_compare,
                                NULL);

  prev_tag_iter = g_sequence_iter_prev (tag_iter);
  if (g_sequence_iter_is_end (prev_tag_iter)
      || g_strcmp0 (g_sequence_get (prev_tag_iter), tag) != 0)
    g_sequence_insert_before (tag_iter, g_strdup (tag));
}

void
ephy_bookmarks_manager_remove_tag (EphyBookmarksManager *self, const char *tag)
{
  GSequenceIter *iter = NULL;
  GList *l;

  g_return_if_fail (EPHY_IS_BOOKMARKS_MANAGER (self));
  g_return_if_fail (tag != NULL);

  iter = g_sequence_lookup (self->tags,
                            (gpointer)tag,
                            (GCompareDataFunc)ephy_bookmark_tags_compare,
                            NULL);
  g_assert (iter != NULL);

  g_sequence_remove (iter);

  /* Also remove the tag from each bookmark if they have it */
  for (l = self->bookmarks; l != NULL; l = l->next)
    ephy_bookmark_remove_tag (l->data, tag);
}

gboolean
ephy_bookmarks_manager_tag_exists (EphyBookmarksManager *self, const char *tag)
{
  GSequenceIter *iter;

  g_return_val_if_fail (EPHY_IS_BOOKMARKS_MANAGER (self), FALSE);
  g_return_val_if_fail (tag != NULL, FALSE);

  iter = g_sequence_lookup (self->tags,
                            (gpointer)tag,
                            (GCompareDataFunc)ephy_bookmark_tags_compare,
                            NULL);

  return iter != NULL;
}

GList *
ephy_bookmarks_manager_get_bookmarks (EphyBookmarksManager *self)
{
  g_return_val_if_fail (EPHY_IS_BOOKMARKS_MANAGER (self), NULL);

  return self->bookmarks;
}

GList *
ephy_bookmarks_manager_get_bookmarks_with_tag (EphyBookmarksManager *self,
                                               const char           *tag)
{
  GList *bookmarks = NULL;
  GList *l;

  g_return_val_if_fail (EPHY_IS_BOOKMARKS_MANAGER (self), NULL);

  if (tag == NULL) {
    for (l = self->bookmarks; l != NULL; l = l->next) {
      EphyBookmark *bookmark = EPHY_BOOKMARK (l->data);

      if (g_sequence_get_length (ephy_bookmark_get_tags (bookmark)) == 0)
        bookmarks = g_list_prepend (bookmarks, bookmark);
    }
  } else {
    for (l = self->bookmarks; l != NULL; l = l->next) {
      EphyBookmark *bookmark = EPHY_BOOKMARK (l->data);

      if (ephy_bookmark_has_tag (bookmark, tag))
        bookmarks = g_list_prepend (bookmarks, bookmark);
    }
  }

  return bookmarks;
}

GSequence *
ephy_bookmarks_manager_get_tags (EphyBookmarksManager *self)
{
  g_return_val_if_fail (EPHY_IS_BOOKMARKS_MANAGER (self), NULL);

  return self->tags;
}

static void
ephy_bookmarks_manager_save_to_file_thread (GTask        *task,
                                            gpointer      source_object,
                                            gpointer      task_data,
                                            GCancellable *cancellable)
{
  EphyBookmarksManager *self = source_object;
  gboolean result;

  g_assert (G_IS_TASK (task));
  g_assert (EPHY_IS_BOOKMARKS_MANAGER (self));

  result = ephy_bookmarks_manager_save_to_file (self);

  g_task_return_boolean (task, result);
}

void
ephy_bookmarks_manager_save_to_file_async (EphyBookmarksManager *self,
                                           GCancellable         *cancellable,
                                           GAsyncReadyCallback   callback,
                                           gpointer              user_data)
{
  GTask *task;

  task = g_task_new (self, cancellable, callback, user_data);

  g_task_run_in_thread (task, ephy_bookmarks_manager_save_to_file_thread);
  g_object_unref (task);
}

gboolean
ephy_bookmarks_manager_save_to_file_finish (EphyBookmarksManager *self,
                                            GAsyncResult         *result,
                                            GError              **error)
{
  g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

void
ephy_bookmarks_manager_load_from_file (EphyBookmarksManager *self)
{
  GvdbTable *root_table;
  GvdbTable *table;
  char **list;
  int length;
  int i;

  /* Create a new table to hold data stored in file. */
  root_table = gvdb_table_new (self->gvdb_filename, TRUE, NULL);
  g_assert (root_table);

  /* Get bookmarks table */
  table = gvdb_table_get_table (root_table, "bookmarks");
  g_assert (table);

  /* Iterate over all keys (url's) in the table. */
  list = gvdb_table_get_names (table, &length);
  for (i = 0; i < length; i++) {
    EphyBookmark *bookmark;
    GVariant *value;
    GVariantIter iter;
    GSequence *tags;
    char *tag;
    char *title;

    /* Obtain the correspoding GVariant. */
    value = gvdb_table_get_value (table, list[i]);

    g_variant_iter_init (&iter, value);

    /* The first string in the array is the bookmark's title. */
    g_variant_iter_next (&iter, "s", &title);

    /* Add all stored tags in a GSequence. */
    tags = g_sequence_new (g_free);
    while (g_variant_iter_next (&iter, "s", &tag)) {
      g_sequence_insert_sorted (tags, tag,
                                (GCompareDataFunc)ephy_bookmark_tags_compare,
                                NULL);
    }

    /* Create the new bookmark. */
    bookmark = ephy_bookmark_new (g_strdup (list[i]), title, tags);
    ephy_bookmarks_manager_add_bookmark (self, bookmark);
  }
  gvdb_table_free (table);

  /* Also add tags to the bookmark manager's sequence. */
  table = gvdb_table_get_table (root_table, "tags");
  g_assert (table);

  /* Iterate over all keys (url's) in the table. */
  list = gvdb_table_get_names (table, &length);
  for (i = 0; i < length; i++)
    ephy_bookmarks_manager_add_tag (self, list[i]);

  gvdb_table_free (table);
  gvdb_table_free (root_table);
}
