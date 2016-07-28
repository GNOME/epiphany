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

#ifndef EPHY_BOOKMARKS_MANAGER_H
#define EPHY_BOOKMARKS_MANAGER_H

#include "ephy-bookmark.h"

G_BEGIN_DECLS

#define EPHY_TYPE_BOOKMARKS_MANAGER (ephy_bookmarks_manager_get_type ())

G_DECLARE_FINAL_TYPE (EphyBookmarksManager, ephy_bookmarks_manager, EPHY, BOOKMARKS_MANAGER, GObject)

void         ephy_bookmarks_manager_add_bookmark         (EphyBookmarksManager *self,
                                                          EphyBookmark         *bookmark);
void         ephy_bookmarks_manager_remove_bookmark      (EphyBookmarksManager *self,
                                                          EphyBookmark         *bookmark);

void         ephy_bookmarks_manager_add_tag              (EphyBookmarksManager *self,
                                                          const char           *tag);
void         ephy_bookmarks_manager_remove_tag           (EphyBookmarksManager *self,
                                                          const char           *tag);
gboolean     ephy_bookmarks_manager_tag_exists           (EphyBookmarksManager *self,
                                                          const char           *tag);

GList       *ephy_bookmarks_manager_get_bookmarks        (EphyBookmarksManager *self);
GSequence   *ephy_bookmarks_manager_get_tags             (EphyBookmarksManager *self);

void         ephy_bookmarks_manager_save_to_file         (EphyBookmarksManager *self);
void         ephy_bookmarks_manager_load_from_file       (EphyBookmarksManager *self);

G_END_DECLS

#endif /* EPHY_BOOKMARKS_MANAGER_H */
