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

G_DEFINE_TYPE (EphyBookmarksManager, ephy_bookmarks_manager, G_TYPE_OBJECT)

enum {
  BOOKMARK_ADDED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
ephy_bookmarks_manager_class_init (EphyBookmarksManagerClass *klass)
{
  signals[BOOKMARK_ADDED] =
    g_signal_new ("bookmark-added",
                  EPHY_TYPE_BOOKMARKS_MANAGER,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  EPHY_TYPE_BOOKMARK);
}

static void
ephy_bookmarks_manager_init (EphyBookmarksManager *self)
{
  self->gvdb_file = g_build_filename (ephy_dot_dir (),
                                         EPHY_BOOKMARKS_FILE,
                                         NULL);
}

void
ephy_bookmarks_manager_add_bookmark (EphyBookmarksManager *self,
                                     EphyBookmark         *bookmark)
{
  g_return_if_fail (EPHY_IS_BOOKMARKS_MANAGER (self));
  g_return_if_fail (EPHY_IS_BOOKMARK (bookmark));

  if (g_list_find (self->bookmarks, bookmark))
    return;

  self->bookmarks = g_list_prepend (self->bookmarks, bookmark);

  g_signal_emit (self, signals[BOOKMARK_ADDED], 0, bookmark);
}

GList *
ephy_bookmarks_manager_get_bookmarks (EphyBookmarksManager *self)
{
  g_return_val_if_fail (EPHY_IS_BOOKMARKS_MANAGER (self), NULL);

  return self->bookmarks;
}
