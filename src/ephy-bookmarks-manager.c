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

#include "ephy-file-helpers.h"

#define EPHY_BOOKMARKS_FILE "bookmarks.gvdb"

struct _EphyBookmarksManager {
  GObject     parent_instance;

  GList      *bookmarks;

  gchar      *gvdb_file;
};

static void list_model_iface_init     (GListModelInterface *iface);

G_DEFINE_TYPE_EXTENDED (EphyBookmarksManager, ephy_bookmarks_manager, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static void
ephy_bookmarks_manager_class_init (EphyBookmarksManagerClass *klass)
{
}

static void
ephy_bookmarks_manager_init (EphyBookmarksManager *self)
{
  self->gvdb_file = g_build_filename (ephy_dot_dir (),
                                         EPHY_BOOKMARKS_FILE,
                                         NULL);
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

void
ephy_bookmarks_manager_add_bookmark (EphyBookmarksManager *self,
                                     EphyBookmark         *bookmark)
{
  g_return_if_fail (EPHY_IS_BOOKMARKS_MANAGER (self));
  g_return_if_fail (EPHY_IS_BOOKMARK (bookmark));

  if (g_list_find (self->bookmarks, bookmark))
    return;

  g_signal_connect_object (bookmark,
                           "removed",
                           G_CALLBACK (ephy_bookmarks_manager_remove_bookmark),
                           self,
                           G_CONNECT_SWAPPED);

  self->bookmarks = g_list_prepend (self->bookmarks, bookmark);
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
}

GList *
ephy_bookmarks_manager_get_bookmarks (EphyBookmarksManager *self)
{
  g_return_val_if_fail (EPHY_IS_BOOKMARKS_MANAGER (self), NULL);

  return self->bookmarks;
}

GSequence *
ephy_bookmarks_manager_get_tags (EphyBookmarksManager *self)
{
  GList *l;
  GSequence *tags;

  g_return_val_if_fail (EPHY_IS_BOOKMARKS_MANAGER (self), NULL);

  tags = g_sequence_new (g_free);
  for (l = self->bookmarks; l != NULL; l = l->next) {
    EphyBookmark *bookmark = EPHY_BOOKMARK (l->data);
    GSequence *bookmark_tags;
    GSequenceIter *iter;

    bookmark_tags = ephy_bookmark_get_tags (bookmark);
    for (iter = g_sequence_get_begin_iter (bookmark_tags);
         !g_sequence_iter_is_end (iter);
         iter = g_sequence_iter_next (iter)) {
      char *tag = g_sequence_get (iter);

      if (g_sequence_lookup (tags, tag, (GCompareDataFunc)g_strcmp0, NULL) == NULL)
        g_sequence_insert_sorted (tags,
                                  g_strdup (tag),
                                  (GCompareDataFunc)g_strcmp0,
                                  NULL);
    }
  }

  return tags;
}
