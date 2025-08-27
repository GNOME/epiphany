/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2016 Iulian-Gabriel Radu <iulian.radu67@gmail.com>
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

#include "ephy-bookmarks-export.h"
#include "ephy-bookmarks-import.h"
#include "ephy-debug.h"
#include "ephy-file-helpers.h"
#include "ephy-settings.h"
#include "ephy-sync-utils.h"
#include "ephy-synchronizable-manager.h"

#include <string.h>

#define EPHY_BOOKMARKS_FILE "bookmarks.gvdb"

struct _EphyBookmarksManager {
  GObject parent_instance;

  GCancellable *cancellable;

  GSequence *bookmarks;
  GSequence *tags;
  GSequence *bookmarks_order;
  GSequence *tags_order;

  gchar *gvdb_filename;
};

static void list_model_iface_init (GListModelInterface *iface);
static void ephy_synchronizable_manager_iface_init (EphySynchronizableManagerInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (EphyBookmarksManager, ephy_bookmarks_manager, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL,
                                                      list_model_iface_init)
                               G_IMPLEMENT_INTERFACE (EPHY_TYPE_SYNCHRONIZABLE_MANAGER,
                                                      ephy_synchronizable_manager_iface_init))

enum {
  BOOKMARK_ADDED,
  BOOKMARK_REMOVED,
  BOOKMARK_TITLE_CHANGED,
  BOOKMARK_URL_CHANGED,
  BOOKMARK_TAG_ADDED,
  BOOKMARK_TAG_REMOVED,
  TAG_CREATED,
  TAG_DELETED,
  SORTED,
  SYNCHRONIZABLE_DELETED,
  SYNCHRONIZABLE_MODIFIED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
ephy_bookmarks_manager_copy_tags_from_bookmark (EphyBookmarksManager *self,
                                                EphyBookmark         *dest,
                                                EphyBookmark         *source)
{
  GSequenceIter *iter;

  g_assert (EPHY_IS_BOOKMARKS_MANAGER (self));
  g_assert (EPHY_IS_BOOKMARK (dest));
  g_assert (EPHY_IS_BOOKMARK (source));

  for (iter = g_sequence_get_begin_iter (ephy_bookmark_get_tags (source));
       !g_sequence_iter_is_end (iter); iter = g_sequence_iter_next (iter))
    ephy_bookmark_add_tag (dest, g_sequence_get (iter));
}

static void
ephy_bookmarks_manager_create_tags_from_bookmark (EphyBookmarksManager *self,
                                                  EphyBookmark         *bookmark)
{
  GSequenceIter *iter;

  g_assert (EPHY_IS_BOOKMARKS_MANAGER (self));
  g_assert (EPHY_IS_BOOKMARK (bookmark));

  for (iter = g_sequence_get_begin_iter (ephy_bookmark_get_tags (bookmark));
       !g_sequence_iter_is_end (iter); iter = g_sequence_iter_next (iter))
    ephy_bookmarks_manager_create_tag (self, g_sequence_get (iter));
}

static void
ephy_bookmarks_manager_dispose (GObject *object)
{
  EphyBookmarksManager *self = EPHY_BOOKMARKS_MANAGER (object);

  if (self->cancellable) {
    g_cancellable_cancel (self->cancellable);
    g_clear_object (&self->cancellable);
  }

  G_OBJECT_CLASS (ephy_bookmarks_manager_parent_class)->dispose (object);
}

static void
ephy_bookmarks_manager_finalize (GObject *object)
{
  EphyBookmarksManager *self = EPHY_BOOKMARKS_MANAGER (object);

  g_sequence_free (self->bookmarks);
  g_sequence_free (self->tags);
  g_free (self->gvdb_filename);

  G_OBJECT_CLASS (ephy_bookmarks_manager_parent_class)->finalize (object);
}

static void
ephy_bookmarks_manager_class_init (EphyBookmarksManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ephy_bookmarks_manager_dispose;
  object_class->finalize = ephy_bookmarks_manager_finalize;

  signals[BOOKMARK_ADDED] =
    g_signal_new ("bookmark-added",
                  EPHY_TYPE_BOOKMARKS_MANAGER,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  EPHY_TYPE_BOOKMARK);

  signals[BOOKMARK_REMOVED] =
    g_signal_new ("bookmark-removed",
                  EPHY_TYPE_BOOKMARKS_MANAGER,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  EPHY_TYPE_BOOKMARK);

  signals[BOOKMARK_TITLE_CHANGED] =
    g_signal_new ("bookmark-title-changed",
                  EPHY_TYPE_BOOKMARKS_MANAGER,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  EPHY_TYPE_BOOKMARK);

  signals[BOOKMARK_URL_CHANGED] =
    g_signal_new ("bookmark-url-changed",
                  EPHY_TYPE_BOOKMARKS_MANAGER,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  EPHY_TYPE_BOOKMARK);

  signals[BOOKMARK_TAG_ADDED] =
    g_signal_new ("bookmark-tag-added",
                  EPHY_TYPE_BOOKMARKS_MANAGER,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 2,
                  EPHY_TYPE_BOOKMARK,
                  G_TYPE_STRING);

  signals[BOOKMARK_TAG_REMOVED] =
    g_signal_new ("bookmark-tag-removed",
                  EPHY_TYPE_BOOKMARKS_MANAGER,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 2,
                  EPHY_TYPE_BOOKMARK,
                  G_TYPE_STRING);

  signals[TAG_CREATED] =
    g_signal_new ("tag-created",
                  EPHY_TYPE_BOOKMARKS_MANAGER,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_STRING);

  signals[TAG_DELETED] =
    g_signal_new ("tag-deleted",
                  EPHY_TYPE_BOOKMARKS_MANAGER,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_STRING);

  signals[SORTED] =
    g_signal_new ("sorted",
                  EPHY_TYPE_BOOKMARKS_MANAGER,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_STRING);

  signals[SYNCHRONIZABLE_DELETED] = g_signal_lookup ("synchronizable-deleted",
                                                     EPHY_TYPE_SYNCHRONIZABLE_MANAGER);

  signals[SYNCHRONIZABLE_MODIFIED] = g_signal_lookup ("synchronizable-modified",
                                                      EPHY_TYPE_SYNCHRONIZABLE_MANAGER);
}

static void
ephy_bookmarks_manager_init (EphyBookmarksManager *self)
{
  g_autoptr (GError) error = NULL;

  self->cancellable = g_cancellable_new ();

  self->gvdb_filename = g_build_filename (ephy_profile_dir (),
                                          EPHY_BOOKMARKS_FILE,
                                          NULL);

  self->bookmarks = g_sequence_new (g_object_unref);
  self->tags = g_sequence_new (g_free);
  self->bookmarks_order = g_sequence_new (g_free);
  self->tags_order = g_sequence_new (g_free);

  g_sequence_insert_sorted (self->tags,
                            g_strdup (EPHY_BOOKMARKS_FAVORITES_TAG),
                            (GCompareDataFunc)ephy_bookmark_tags_compare,
                            NULL);

  /* Create DB file if it doesn't already exists */
  if (!g_file_test (self->gvdb_filename, G_FILE_TEST_EXISTS)) {
    if (!ephy_bookmarks_manager_save_sync (self, &error)) {
      g_assert (error);
      g_warning ("Failed to save bookmarks: %s", error->message);
    }
  }

  ephy_bookmarks_import (self, self->gvdb_filename, NULL);

  ephy_bookmarks_manager_save (self, TRUE, TRUE, self->cancellable,
                               (GAsyncReadyCallback)ephy_bookmarks_manager_save_warn_on_error_cb,
                               NULL);
}

static void
bookmark_title_changed_cb (EphyBookmark         *bookmark,
                           GParamSpec           *pspec,
                           EphyBookmarksManager *self)
{
  g_signal_emit (self, signals[BOOKMARK_TITLE_CHANGED], 0, bookmark);
}

static void
bookmark_url_changed_cb (EphyBookmark         *bookmark,
                         GParamSpec           *pspec,
                         EphyBookmarksManager *self)
{
  g_signal_emit (self, signals[BOOKMARK_URL_CHANGED], 0, bookmark);
}

static void
bookmark_tag_added_cb (EphyBookmark         *bookmark,
                       const char           *tag,
                       EphyBookmarksManager *self)
{
  g_signal_emit (self, signals[BOOKMARK_TAG_ADDED], 0, bookmark, tag);
}

static void
bookmark_tag_removed_cb (EphyBookmark         *bookmark,
                         const char           *tag,
                         EphyBookmarksManager *self)
{
  g_signal_emit (self, signals[BOOKMARK_TAG_REMOVED], 0, bookmark, tag);
}

EphyBookmarksManager *
ephy_bookmarks_manager_new (void)
{
  return EPHY_BOOKMARKS_MANAGER (g_object_new (EPHY_TYPE_BOOKMARKS_MANAGER, NULL));
}

static void
ephy_bookmarks_manager_watch_bookmark (EphyBookmarksManager *self,
                                       EphyBookmark         *bookmark)
{
  g_signal_connect_object (bookmark, "notify::title",
                           G_CALLBACK (bookmark_title_changed_cb), self, 0);
  g_signal_connect_object (bookmark, "notify::bmkUri",
                           G_CALLBACK (bookmark_url_changed_cb), self, 0);
  g_signal_connect_object (bookmark, "tag-added",
                           G_CALLBACK (bookmark_tag_added_cb), self, 0);
  g_signal_connect_object (bookmark, "tag-removed",
                           G_CALLBACK (bookmark_tag_removed_cb), self, 0);
}

static void
ephy_bookmarks_manager_unwatch_bookmark (EphyBookmarksManager *self,
                                         EphyBookmark         *bookmark)
{
  g_signal_handlers_disconnect_by_func (bookmark, bookmark_title_changed_cb, self);
  g_signal_handlers_disconnect_by_func (bookmark, bookmark_url_changed_cb, self);
  g_signal_handlers_disconnect_by_func (bookmark, bookmark_tag_added_cb, self);
  g_signal_handlers_disconnect_by_func (bookmark, bookmark_tag_removed_cb, self);
}

static void
ephy_bookmarks_manager_add_bookmark_internal (EphyBookmarksManager *self,
                                              EphyBookmark         *bookmark,
                                              gboolean              should_save)
{
  GSequenceIter *iter;
  int position;

  g_assert (EPHY_IS_BOOKMARKS_MANAGER (self));
  g_assert (EPHY_IS_BOOKMARK (bookmark));

  iter = g_sequence_insert_sorted (self->bookmarks, g_object_ref (bookmark),
                                   (GCompareDataFunc)ephy_bookmark_bookmarks_compare_func,
                                   NULL);
  if (iter) {
    /* Update list */
    position = g_sequence_iter_get_position (iter);
    g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);

    g_signal_emit (self, signals[BOOKMARK_ADDED], 0, bookmark);
    ephy_bookmarks_manager_watch_bookmark (self, bookmark);
  }

  if (should_save)
    ephy_bookmarks_manager_save (self, FALSE, FALSE, self->cancellable,
                                 (GAsyncReadyCallback)ephy_bookmarks_manager_save_warn_on_error_cb,
                                 NULL);
}

void
ephy_bookmarks_manager_add_bookmark (EphyBookmarksManager *self,
                                     EphyBookmark         *bookmark)
{
  g_assert (EPHY_IS_BOOKMARKS_MANAGER (self));
  g_assert (EPHY_IS_BOOKMARK (bookmark));

  ephy_bookmarks_manager_add_bookmark_internal (self, bookmark, TRUE);
  g_signal_emit (self, signals[SYNCHRONIZABLE_MODIFIED], 0, bookmark, FALSE);
}

void
ephy_bookmarks_manager_add_bookmarks (EphyBookmarksManager *self,
                                      GSequence            *bookmarks)
{
  GSequenceIter *iter;

  g_assert (EPHY_IS_BOOKMARKS_MANAGER (self));
  g_assert (bookmarks);

  for (iter = g_sequence_get_begin_iter (bookmarks);
       !g_sequence_iter_is_end (iter); iter = g_sequence_iter_next (iter)) {
    EphyBookmark *bookmark = g_sequence_get (iter);

    ephy_bookmarks_manager_add_bookmark_internal (self, bookmark, FALSE);
    g_signal_emit (self, signals[SYNCHRONIZABLE_MODIFIED], 0, bookmark, FALSE);
  }
}

static void
ephy_bookmarks_manager_remove_bookmark_internal (EphyBookmarksManager *self,
                                                 EphyBookmark         *bookmark)
{
  GSequenceIter *iter;
  gint position;

  g_assert (EPHY_IS_BOOKMARKS_MANAGER (self));
  g_assert (EPHY_IS_BOOKMARK (bookmark));

  for (iter = g_sequence_get_begin_iter (self->bookmarks);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter)) {
    if (g_strcmp0 (ephy_bookmark_get_id (g_sequence_get (iter)),
                   ephy_bookmark_get_id (bookmark)) == 0)
      break;
  }
  g_assert (!g_sequence_iter_is_end (iter));

  /* Ensure the bookmark is removed from our list before the signal is emitted,
   * because this is the bookmark REMOVED signal after all, so callers expect
   * it to be already gone.
   */
  g_object_ref (bookmark);
  position = g_sequence_iter_get_position (iter);
  g_sequence_remove (iter);
  g_list_model_items_changed (G_LIST_MODEL (self), position, 1, 0);
  g_signal_emit (self, signals[BOOKMARK_REMOVED], 0, bookmark);

  ephy_bookmarks_manager_save (self, FALSE, FALSE, self->cancellable,
                               (GAsyncReadyCallback)ephy_bookmarks_manager_save_warn_on_error_cb,
                               NULL);

  ephy_bookmarks_manager_unwatch_bookmark (self, bookmark);
}

void
ephy_bookmarks_manager_remove_bookmark (EphyBookmarksManager *self,
                                        EphyBookmark         *bookmark)
{
  g_assert (EPHY_IS_BOOKMARKS_MANAGER (self));
  g_assert (EPHY_IS_BOOKMARK (bookmark));

  g_signal_emit (self, signals[SYNCHRONIZABLE_DELETED], 0, bookmark);
  ephy_bookmarks_manager_remove_bookmark_internal (self, bookmark);
}

EphyBookmark *
ephy_bookmarks_manager_get_bookmark_by_url (EphyBookmarksManager *self,
                                            const char           *url)
{
  GSequenceIter *iter;

  g_assert (EPHY_IS_BOOKMARKS_MANAGER (self));
  g_assert (url);

  for (iter = g_sequence_get_begin_iter (self->bookmarks);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter)) {
    EphyBookmark *bookmark = g_sequence_get (iter);

    if (g_strcmp0 (ephy_bookmark_get_url (bookmark), url) == 0)
      return bookmark;
  }

  return NULL;
}

EphyBookmark *
ephy_bookmarks_manager_get_bookmark_by_id (EphyBookmarksManager *self,
                                           const char           *id)
{
  GSequenceIter *iter;

  g_assert (EPHY_IS_BOOKMARKS_MANAGER (self));
  g_assert (id);

  for (iter = g_sequence_get_begin_iter (self->bookmarks);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter)) {
    EphyBookmark *bookmark = g_sequence_get (iter);

    if (g_strcmp0 (ephy_bookmark_get_id (bookmark), id) == 0)
      return bookmark;
  }

  return NULL;
}

void
ephy_bookmarks_manager_create_tag (EphyBookmarksManager *self,
                                   const char           *tag)
{
  GSequenceIter *tag_iter;
  GSequenceIter *prev_tag_iter;

  g_assert (EPHY_IS_BOOKMARKS_MANAGER (self));
  g_assert (tag);

  tag_iter = g_sequence_search (self->tags,
                                (gpointer)tag,
                                (GCompareDataFunc)ephy_bookmark_tags_compare,
                                NULL);

  prev_tag_iter = g_sequence_iter_prev (tag_iter);
  if (g_sequence_iter_is_end (prev_tag_iter)
      || g_strcmp0 (g_sequence_get (prev_tag_iter), tag) != 0) {
    g_sequence_insert_before (tag_iter, g_strdup (tag));
    g_signal_emit (self, signals[TAG_CREATED], 0, tag);
  }
}

void
ephy_bookmarks_manager_delete_tag (EphyBookmarksManager *self,
                                   const char           *tag)
{
  GSequenceIter *iter = NULL;

  g_assert (EPHY_IS_BOOKMARKS_MANAGER (self));
  g_assert (tag);

  if (strcmp (tag, EPHY_BOOKMARKS_FAVORITES_TAG) == 0)
    return;

  iter = g_sequence_lookup (self->tags,
                            (gpointer)tag,
                            (GCompareDataFunc)ephy_bookmark_tags_compare,
                            NULL);
  g_assert (iter);
  g_sequence_remove (iter);

  /* Also remove the tag from each bookmark if they have it */
  g_sequence_foreach (self->bookmarks, (GFunc)ephy_bookmark_remove_tag, (gpointer)tag);

  g_signal_emit (self, signals[TAG_DELETED], 0, tag);
}

gboolean
ephy_bookmarks_manager_tag_exists (EphyBookmarksManager *self,
                                   const char           *tag)
{
  GSequenceIter *iter;

  g_assert (EPHY_IS_BOOKMARKS_MANAGER (self));
  g_assert (tag);

  iter = g_sequence_lookup (self->tags,
                            (gpointer)tag,
                            (GCompareDataFunc)ephy_bookmark_tags_compare,
                            NULL);

  return !!iter;
}

GSequence *
ephy_bookmarks_manager_get_bookmarks (EphyBookmarksManager *self)
{
  g_assert (EPHY_IS_BOOKMARKS_MANAGER (self));

  return self->bookmarks;
}

GSequence *
ephy_bookmarks_manager_get_bookmarks_with_tag (EphyBookmarksManager *self,
                                               const char           *tag)
{
  GSequence *bookmarks;
  GSequenceIter *iter;

  g_assert (EPHY_IS_BOOKMARKS_MANAGER (self));

  bookmarks = g_sequence_new (g_object_unref);

  if (!tag) {
    for (iter = g_sequence_get_begin_iter (self->bookmarks);
         !g_sequence_iter_is_end (iter);
         iter = g_sequence_iter_next (iter)) {
      EphyBookmark *bookmark = g_sequence_get (iter);

      if (g_sequence_is_empty (ephy_bookmark_get_tags (bookmark))) {
        g_sequence_insert_sorted (bookmarks,
                                  g_object_ref (bookmark),
                                  (GCompareDataFunc)ephy_bookmark_bookmarks_compare_func,
                                  NULL);
      }
    }
  } else {
    for (iter = g_sequence_get_begin_iter (self->bookmarks);
         !g_sequence_iter_is_end (iter);
         iter = g_sequence_iter_next (iter)) {
      EphyBookmark *bookmark = g_sequence_get (iter);

      if (ephy_bookmark_has_tag (bookmark, tag))
        g_sequence_insert_sorted (bookmarks,
                                  g_object_ref (bookmark),
                                  (GCompareDataFunc)ephy_bookmark_bookmarks_compare_func,
                                  NULL);
    }
  }

  return bookmarks;
}

gboolean
ephy_bookmarks_manager_has_bookmarks_with_tag (EphyBookmarksManager *self,
                                               const char           *tag)
{
  g_autoptr (GSequence) bookmarks = ephy_bookmarks_manager_get_bookmarks_with_tag (self, tag);

  return !g_sequence_is_empty (bookmarks);
}

GSequence *
ephy_bookmarks_manager_get_tags (EphyBookmarksManager *self)
{
  g_assert (EPHY_IS_BOOKMARKS_MANAGER (self));

  return self->tags;
}

GSequence *
ephy_bookmarks_manager_get_bookmarks_order (EphyBookmarksManager *self)
{
  g_assert (EPHY_IS_BOOKMARKS_MANAGER (self));

  return self->bookmarks_order;
}

void
ephy_bookmarks_manager_add_to_bookmarks_order (EphyBookmarksManager *self,
                                               const char           *type,
                                               const char           *item,
                                               int                   index)
{
  GVariant *variant;

  g_assert (EPHY_IS_BOOKMARKS_MANAGER (self));

  variant = g_variant_new ("(ssi)", type, item, index);
  g_sequence_append (self->bookmarks_order, variant);
}

void
ephy_bookmarks_manager_clear_bookmarks_order (EphyBookmarksManager *self)
{
  g_assert (EPHY_IS_BOOKMARKS_MANAGER (self));

  g_free (self->bookmarks_order);
  self->bookmarks_order = g_sequence_new (g_free);
}

int
sort_bookmarks_order (GVariant *variant_1,
                      GVariant *variant_2)
{
  int index_1, index_2;

  g_variant_get (variant_1, "(ssi)", NULL, NULL, &index_1);
  g_variant_get (variant_2, "(ssi)", NULL, NULL, &index_2);

  return index_1 - index_2;
}

void
ephy_bookmarks_manager_sort_bookmarks_order (EphyBookmarksManager *self)
{
  g_sequence_sort (self->bookmarks_order, (GCompareDataFunc)sort_bookmarks_order, NULL);
}

GSequence *
ephy_bookmarks_manager_get_tags_order (EphyBookmarksManager *self)
{
  g_assert (EPHY_IS_BOOKMARKS_MANAGER (self));

  return self->tags_order;
}

GSequence *
ephy_bookmarks_manager_tags_order_get_tag (EphyBookmarksManager *self,
                                           const char           *tag)
{
  GSequence *urls = NULL;
  GSequenceIter *iter;

  g_assert (EPHY_IS_BOOKMARKS_MANAGER (self));

  for (iter = g_sequence_get_begin_iter (self->tags_order);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter)) {
    GVariant *variant = g_sequence_get (iter);
    const char *variant_tag;
    GVariantIter *variant_iter;

    g_variant_get (variant, "(sa(si))", &variant_tag, &variant_iter);

    if (g_strcmp0 (variant_tag, tag) == 0) {
      const char *url;

      urls = g_sequence_new (g_free);
      while (g_variant_iter_next (variant_iter, "(si)", &url, NULL))
        g_sequence_append (urls, g_strdup (url));
    }

    g_variant_iter_free (variant_iter);
    if (urls)
      break;
  }

  return urls;
}

void
ephy_bookmarks_manager_tags_order_clear_tag (EphyBookmarksManager *self,
                                             const char           *tag)
{
  GSequenceIter *iter;

  g_assert (EPHY_IS_BOOKMARKS_MANAGER (self));

  for (iter = g_sequence_get_begin_iter (self->tags_order);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter)) {
    GVariant *variant = g_sequence_get (iter);
    const char *variant_tag;

    g_variant_get (variant, "(sa(si))", &variant_tag, NULL);

    if (g_strcmp0 (variant_tag, tag) == 0) {
      g_sequence_remove (iter);
      return;
    }
  }
}

void
ephy_bookmarks_manager_tags_order_add_tag (EphyBookmarksManager *self,
                                           const char           *tag,
                                           GSequence            *urls)
{
  GVariantBuilder builder;
  GVariant *variant;
  GSequenceIter *iter;

  g_assert (EPHY_IS_BOOKMARKS_MANAGER (self));

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("(sa(si))"));
  g_variant_builder_add (&builder, "s", tag);
  g_variant_builder_open (&builder, G_VARIANT_TYPE ("a(si)"));

  for (iter = g_sequence_get_begin_iter (urls);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter)) {
    const char *url = g_sequence_get (iter);

    g_variant_builder_open (&builder, G_VARIANT_TYPE ("(si)"));
    g_variant_builder_add (&builder, "s", url);
    g_variant_builder_add (&builder, "i", g_sequence_iter_get_position (iter));
    g_variant_builder_close (&builder);
  }

  g_variant_builder_close (&builder);
  variant = g_variant_builder_end (&builder);
  g_sequence_append (self->tags_order, variant);

  g_sequence_free (urls);
}

void
ephy_bookmarks_manager_tags_order_add_tag_variant (EphyBookmarksManager *self,
                                                   GVariant             *variant)
{
  g_sequence_append (self->tags_order, variant);
}

void
ephy_bookmarks_manager_save_warn_on_error_cb (GObject      *object,
                                              GAsyncResult *result,
                                              gpointer      user_data)
{
  EphyBookmarksManager *self = EPHY_BOOKMARKS_MANAGER (object);
  gboolean ret;
  g_autoptr (GError) error = NULL;

  ret = ephy_bookmarks_manager_save_finish (self, result, &error);
  if (!ret)
    g_warning ("%s", error->message);
}

GCancellable *
ephy_bookmarks_manager_save_warn_on_error_cancellable (EphyBookmarksManager *self)
{
  g_assert (EPHY_IS_BOOKMARKS_MANAGER (self));

  return self->cancellable;
}

static void
bookmarks_export_cb (GObject      *source_object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  EphyBookmarksManager *self = EPHY_BOOKMARKS_MANAGER (source_object);
  g_autoptr (GTask) task = user_data;
  GError *error = NULL;

  if (!ephy_bookmarks_export_finish (self, result, &error)) {
    g_task_return_error (task, error);
    return;
  }

  g_task_return_boolean (task, TRUE);
}

void
ephy_bookmarks_manager_save (EphyBookmarksManager *self,
                             gboolean              with_bookmarks_order,
                             gboolean              with_tags_order,
                             GCancellable         *cancellable,
                             GAsyncReadyCallback   callback,
                             gpointer              user_data)
{
  GTask *task;

  task = g_task_new (self, cancellable, callback, user_data);

  ephy_bookmarks_export (self, self->gvdb_filename, with_bookmarks_order,
                         with_tags_order, cancellable, bookmarks_export_cb, task);
}

gboolean
ephy_bookmarks_manager_save_finish (EphyBookmarksManager  *self,
                                    GAsyncResult          *result,
                                    GError               **error)
{
  g_assert (g_task_is_valid (result, self));

  return g_task_propagate_boolean (G_TASK (result), error);
}

typedef struct {
  GMainLoop *main_loop;
  gboolean result;
  GError *error;
} SaveToFileData;

static void
save_to_file_cb (GObject      *source_object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
  EphyBookmarksManager *self = EPHY_BOOKMARKS_MANAGER (source_object);
  SaveToFileData *data = user_data;

  data->result = ephy_bookmarks_manager_save_finish (self, result, &data->error);

  g_main_loop_quit (data->main_loop);
}

gboolean
ephy_bookmarks_manager_save_sync (EphyBookmarksManager  *self,
                                  GError               **error)
{
  g_autoptr (GMainContext) context = NULL;
  SaveToFileData *data;
  gboolean result;

  context = g_main_context_new ();
  data = g_new0 (SaveToFileData, 1);
  data->main_loop = g_main_loop_new (context, FALSE);

  g_main_context_push_thread_default (context);
  ephy_bookmarks_manager_save (self, FALSE, FALSE, NULL, save_to_file_cb, data);
  g_main_loop_run (data->main_loop);
  g_main_context_pop_thread_default (context);

  result = data->result;
  if (data->error)
    g_propagate_error (error, data->error);

  g_main_loop_unref (data->main_loop);
  g_free (data);

  return result;
}

static GType
ephy_bookmarks_manager_list_model_get_item_type (GListModel *model)
{
  return EPHY_TYPE_BOOKMARK;
}

static guint
ephy_bookmarks_manager_list_model_get_n_items (GListModel *model)
{
  EphyBookmarksManager *self = EPHY_BOOKMARKS_MANAGER (model);

  return g_sequence_get_length (self->bookmarks);
}

static gpointer
ephy_bookmarks_manager_list_model_get_item (GListModel *model,
                                            guint       position)
{
  EphyBookmarksManager *self = EPHY_BOOKMARKS_MANAGER (model);
  GSequenceIter *iter;

  iter = g_sequence_get_iter_at_pos (self->bookmarks, position);

  return g_object_ref (g_sequence_get (iter));
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = ephy_bookmarks_manager_list_model_get_item_type;
  iface->get_n_items = ephy_bookmarks_manager_list_model_get_n_items;
  iface->get_item = ephy_bookmarks_manager_list_model_get_item;
}

static const char *
synchronizable_manager_get_collection_name (EphySynchronizableManager *manager)
{
  return ephy_sync_utils_sync_with_firefox () ? "bookmarks" : "ephy-bookmarks";
}

static GType
synchronizable_manager_get_synchronizable_type (EphySynchronizableManager *manager)
{
  return EPHY_TYPE_BOOKMARK;
}

static gboolean
synchronizable_manager_is_initial_sync (EphySynchronizableManager *manager)
{
  return ephy_sync_utils_get_bookmarks_sync_is_initial ();
}

static void
synchronizable_manager_set_is_initial_sync (EphySynchronizableManager *manager,
                                            gboolean                   is_initial)
{
  ephy_sync_utils_set_bookmarks_sync_is_initial (is_initial);
}

static gint64
synchronizable_manager_get_sync_time (EphySynchronizableManager *manager)
{
  return ephy_sync_utils_get_bookmarks_sync_time ();
}

static void
synchronizable_manager_set_sync_time (EphySynchronizableManager *manager,
                                      gint64                     sync_time)
{
  ephy_sync_utils_set_bookmarks_sync_time (sync_time);
}

static void
synchronizable_manager_add (EphySynchronizableManager *manager,
                            EphySynchronizable        *synchronizable)
{
  EphyBookmarksManager *self = EPHY_BOOKMARKS_MANAGER (manager);
  EphyBookmark *bookmark = EPHY_BOOKMARK (synchronizable);

  ephy_bookmarks_manager_add_bookmark_internal (self, bookmark, TRUE);
  ephy_bookmarks_manager_create_tags_from_bookmark (self, bookmark);
}

static void
synchronizable_manager_remove (EphySynchronizableManager *manager,
                               EphySynchronizable        *synchronizable)
{
  EphyBookmarksManager *self = EPHY_BOOKMARKS_MANAGER (manager);
  EphyBookmark *bookmark = EPHY_BOOKMARK (synchronizable);

  ephy_bookmarks_manager_remove_bookmark_internal (self, bookmark);
}

static void
synchronizable_manager_save (EphySynchronizableManager *manager,
                             EphySynchronizable        *synchronizable)
{
  EphyBookmarksManager *self = EPHY_BOOKMARKS_MANAGER (manager);

  ephy_bookmarks_manager_save (self, FALSE, FALSE, self->cancellable,
                               (GAsyncReadyCallback)ephy_bookmarks_manager_save_warn_on_error_cb,
                               NULL);
}

static GPtrArray *
ephy_bookmarks_manager_handle_initial_merge (EphyBookmarksManager *self,
                                             GList                *remote_bookmarks)
{
  GPtrArray *to_upload;
  EphyBookmark *bookmark;
  GSequence *bookmarks;
  GSequenceIter *iter;
  GHashTable *dont_upload;
  gint64 timestamp;

  g_assert (EPHY_IS_BOOKMARKS_MANAGER (self));

  to_upload = g_ptr_array_new_with_free_func (g_object_unref);
  dont_upload = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  for (GList *l = remote_bookmarks; l && l->data; l = l->next) {
    const char *id;
    const char *url;
    char *type;
    char *parent_id;

    g_object_get (l->data, "type", &type, "parentid", &parent_id, NULL);
    /* Ignore unfiled bookmarks and everything that is not of type bookmark. */
    if (g_strcmp0 (type, "bookmark") || !g_strcmp0 (parent_id, "unfiled"))
      goto next;

    if (!g_strcmp0 (parent_id, "mobile") &&
        !ephy_bookmark_has_tag (l->data, EPHY_BOOKMARKS_MOBILE_TAG))
      ephy_bookmark_add_tag (l->data, EPHY_BOOKMARKS_MOBILE_TAG);

    /* Bookmarks from server may miss the time added timestamp. */
    if (!ephy_bookmark_get_time_added (l->data))
      ephy_bookmark_set_time_added (l->data, g_get_real_time ());

    id = ephy_bookmark_get_id (l->data);
    url = ephy_bookmark_get_url (l->data);
    bookmark = ephy_bookmarks_manager_get_bookmark_by_id (self, id);

    if (bookmark) {
      if (!g_strcmp0 (ephy_bookmark_get_url (bookmark), url)) {
        /* Same id, same url. Merge tags and reupload. */
        ephy_bookmarks_manager_copy_tags_from_bookmark (self, bookmark, l->data);
        timestamp = ephy_synchronizable_get_server_time_modified (l->data);
        ephy_synchronizable_set_server_time_modified (EPHY_SYNCHRONIZABLE (bookmark), timestamp);
      } else {
        /* Same id, different url. Keep both and upload local one with new id. */
        char *new_id = ephy_sync_utils_get_random_sync_id ();
        ephy_bookmark_set_id (bookmark, new_id);
        ephy_bookmarks_manager_add_bookmark_internal (self, l->data, FALSE);
        g_hash_table_add (dont_upload, g_strdup (id));
        g_free (new_id);
      }
    } else {
      bookmark = ephy_bookmarks_manager_get_bookmark_by_url (self, url);
      if (bookmark) {
        /* Different id, same url. Keep remote id, merge tags and reupload. */
        ephy_bookmark_set_id (bookmark, id);
        ephy_bookmarks_manager_copy_tags_from_bookmark (self, bookmark, l->data);
        timestamp = ephy_synchronizable_get_server_time_modified (l->data);
        ephy_synchronizable_set_server_time_modified (EPHY_SYNCHRONIZABLE (bookmark), timestamp);
      } else {
        /* Different id, different url. Add remote bookmark. */
        ephy_bookmarks_manager_add_bookmark_internal (self, l->data, FALSE);
        g_hash_table_add (dont_upload, g_strdup (id));
      }
    }

    /* In any case, create new tags from the remote bookmark if any. */
    ephy_bookmarks_manager_create_tags_from_bookmark (self, l->data);

next:
    g_free (type);
    g_free (parent_id);
  }

  bookmarks = ephy_bookmarks_manager_get_bookmarks (self);
  for (iter = g_sequence_get_begin_iter (bookmarks);
       !g_sequence_iter_is_end (iter); iter = g_sequence_iter_next (iter)) {
    bookmark = g_sequence_get (iter);
    if (!g_hash_table_contains (dont_upload, ephy_bookmark_get_id (bookmark)))
      g_ptr_array_add (to_upload, g_object_ref (bookmark));
  }

  /* Commit changes to file. */
  ephy_bookmarks_manager_save (self, FALSE, FALSE, self->cancellable,
                               (GAsyncReadyCallback)ephy_bookmarks_manager_save_warn_on_error_cb,
                               NULL);
  g_hash_table_unref (dont_upload);

  return to_upload;
}

static GPtrArray *
ephy_bookmarks_manager_handle_regular_merge (EphyBookmarksManager *self,
                                             GList                *updated_bookmarks,
                                             GList                *deleted_bookmarks)
{
  GPtrArray *to_upload;
  EphyBookmark *bookmark;
  gint64 timestamp;

  g_assert (EPHY_IS_BOOKMARKS_MANAGER (self));

  to_upload = g_ptr_array_new_with_free_func (g_object_unref);

  for (GList *l = deleted_bookmarks; l && l->data; l = l->next) {
    bookmark = ephy_bookmarks_manager_get_bookmark_by_id (self, ephy_bookmark_get_id (l->data));
    if (bookmark)
      ephy_bookmarks_manager_remove_bookmark_internal (self, bookmark);
  }

  for (GList *l = updated_bookmarks; l && l->data; l = l->next) {
    const char *id;
    const char *url;
    char *type;
    char *parent_id;

    g_object_get (l->data, "type", &type, "parentid", &parent_id, NULL);
    /* Ignore unfiled bookmarks and everything that is not of type bookmark. */
    if (g_strcmp0 (type, "bookmark") || !g_strcmp0 (parent_id, "unfiled"))
      goto next;

    if (!g_strcmp0 (parent_id, "mobile") &&
        !ephy_bookmark_has_tag (l->data, EPHY_BOOKMARKS_MOBILE_TAG))
      ephy_bookmark_add_tag (l->data, EPHY_BOOKMARKS_MOBILE_TAG);

    /* Bookmarks from server may miss the time added timestamp. */
    if (!ephy_bookmark_get_time_added (l->data))
      ephy_bookmark_set_time_added (l->data, g_get_real_time ());

    id = ephy_bookmark_get_id (l->data);
    url = ephy_bookmark_get_url (l->data);
    bookmark = ephy_bookmarks_manager_get_bookmark_by_id (self, id);

    if (bookmark) {
      /* Same id. Overwrite local bookmark. */
      ephy_bookmarks_manager_remove_bookmark_internal (self, bookmark);
      ephy_bookmarks_manager_add_bookmark_internal (self, l->data, FALSE);
    } else {
      bookmark = ephy_bookmarks_manager_get_bookmark_by_url (self, url);
      if (bookmark) {
        /* Different id, same url. Keep remote id, merge tags and reupload. */
        ephy_bookmark_set_id (bookmark, id);
        ephy_bookmarks_manager_copy_tags_from_bookmark (self, bookmark, l->data);
        timestamp = ephy_synchronizable_get_server_time_modified (l->data);
        ephy_synchronizable_set_server_time_modified (EPHY_SYNCHRONIZABLE (bookmark), timestamp);
        g_ptr_array_add (to_upload, g_object_ref (bookmark));
      } else {
        /* Different id, different url. Add remote bookmark. */
        ephy_bookmarks_manager_add_bookmark_internal (self, l->data, FALSE);
      }
    }

    /* In any case, create new tags from the remote bookmark if any. */
    ephy_bookmarks_manager_create_tags_from_bookmark (self, l->data);

next:
    g_free (type);
    g_free (parent_id);
  }

  /* Commit changes to file. */
  ephy_bookmarks_manager_save (self, FALSE, FALSE, self->cancellable,
                               (GAsyncReadyCallback)ephy_bookmarks_manager_save_warn_on_error_cb,
                               NULL);

  return to_upload;
}

static void
synchronizable_manager_merge (EphySynchronizableManager              *manager,
                              gboolean                                is_initial,
                              GList                                  *remotes_deleted,
                              GList                                  *remotes_updated,
                              EphySynchronizableManagerMergeCallback  callback,
                              gpointer                                user_data)
{
  EphyBookmarksManager *self = EPHY_BOOKMARKS_MANAGER (manager);
  GPtrArray *to_upload;

  if (is_initial)
    to_upload = ephy_bookmarks_manager_handle_initial_merge (self, remotes_updated);
  else
    to_upload = ephy_bookmarks_manager_handle_regular_merge (self, remotes_updated, remotes_deleted);

  callback (to_upload, user_data);
}

static void
ephy_synchronizable_manager_iface_init (EphySynchronizableManagerInterface *iface)
{
  iface->get_collection_name = synchronizable_manager_get_collection_name;
  iface->get_synchronizable_type = synchronizable_manager_get_synchronizable_type;
  iface->is_initial_sync = synchronizable_manager_is_initial_sync;
  iface->set_is_initial_sync = synchronizable_manager_set_is_initial_sync;
  iface->get_sync_time = synchronizable_manager_get_sync_time;
  iface->set_sync_time = synchronizable_manager_set_sync_time;
  iface->add = synchronizable_manager_add;
  iface->remove = synchronizable_manager_remove;
  iface->save = synchronizable_manager_save;
  iface->merge = synchronizable_manager_merge;
}
