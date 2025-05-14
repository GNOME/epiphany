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

#pragma once

#include "ephy-bookmark.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define EPHY_TYPE_BOOKMARKS_MANAGER (ephy_bookmarks_manager_get_type ())

G_DECLARE_FINAL_TYPE (EphyBookmarksManager, ephy_bookmarks_manager, EPHY, BOOKMARKS_MANAGER, GObject)

#define EPHY_BOOKMARKS_FAVORITES_TAG    _("Favorites")
#define EPHY_BOOKMARKS_MOBILE_TAG       _("Mobile")
#define FIREFOX_BOOKMARKS_MOBILE_FOLDER "Mobile Bookmarks"

EphyBookmarksManager *ephy_bookmarks_manager_new                    (void);

void         ephy_bookmarks_manager_add_bookmark                    (EphyBookmarksManager *self,
                                                                     EphyBookmark         *bookmark);
void         ephy_bookmarks_manager_add_bookmarks                   (EphyBookmarksManager *self,
                                                                     GSequence            *bookmarks);
void         ephy_bookmarks_manager_remove_bookmark                 (EphyBookmarksManager *self,
                                                                     EphyBookmark         *bookmark);
EphyBookmark *ephy_bookmarks_manager_get_bookmark_by_url            (EphyBookmarksManager *self,
                                                                     const char           *url);
EphyBookmark *ephy_bookmarks_manager_get_bookmark_by_id             (EphyBookmarksManager *self,
                                                                     const char           *id);

void         ephy_bookmarks_manager_create_tag                      (EphyBookmarksManager *self,
                                                                     const char           *tag);
void         ephy_bookmarks_manager_delete_tag                      (EphyBookmarksManager *self,
                                                                     const char           *tag);
gboolean     ephy_bookmarks_manager_tag_exists                      (EphyBookmarksManager *self,
                                                                     const char           *tag);

GSequence   *ephy_bookmarks_manager_get_bookmarks                   (EphyBookmarksManager *self);
GSequence   *ephy_bookmarks_manager_get_bookmarks_with_tag          (EphyBookmarksManager *self,
                                                                     const char           *tag);
gboolean     ephy_bookmarks_manager_has_bookmarks_with_tag          (EphyBookmarksManager *self,
                                                                     const char           *tag);
GSequence   *ephy_bookmarks_manager_get_tags                        (EphyBookmarksManager *self);

gboolean     ephy_bookmarks_manager_save_sync                       (EphyBookmarksManager  *self,
                                                                     GError               **error);
void         ephy_bookmarks_manager_save                            (EphyBookmarksManager  *self,
                                                                     gboolean               with_bookmarks_order,
                                                                     gboolean               with_tags_order,
                                                                     GCancellable          *cancellable,
                                                                     GAsyncReadyCallback    callback,
                                                                     gpointer               user_data);
gboolean     ephy_bookmarks_manager_save_finish                     (EphyBookmarksManager  *self,
                                                                     GAsyncResult          *result,
                                                                     GError               **error);

GSequence   *ephy_bookmarks_manager_get_bookmarks_order             (EphyBookmarksManager   *self);

void         ephy_bookmarks_manager_sort_bookmarks_order            (EphyBookmarksManager   *self);

void         ephy_bookmarks_manager_add_to_bookmarks_order          (EphyBookmarksManager   *self,
                                                                     const char             *type,
                                                                     const char             *item,
                                                                     int                     index);

void         ephy_bookmarks_manager_clear_bookmarks_order           (EphyBookmarksManager   *self);

void         ephy_bookmarks_manager_add_to_bookmarks_order          (EphyBookmarksManager   *self,
                                                                     const char             *tag,
                                                                     const char             *bookmark_url,
                                                                     int                     index);

GSequence   *ephy_bookmarks_manager_get_tags_order                  (EphyBookmarksManager   *self);

GSequence   *ephy_bookmarks_manager_tags_order_get_tag              (EphyBookmarksManager   *self,
                                                                     const char             *tag);

void         ephy_bookmarks_manager_tags_order_clear_tag            (EphyBookmarksManager   *self,
                                                                     const char             *tag);

void         ephy_bookmarks_manager_tags_order_add_tag               (EphyBookmarksManager   *self,
                                                                      const char             *tag,
                                                                      GSequence              *urls);

void         ephy_bookmarks_manager_tags_order_add_tag_variant       (EphyBookmarksManager   *self,
                                                                      GVariant               *variant);

void          ephy_bookmarks_manager_save_warn_on_error_cb          (GObject               *object,
                                                                     GAsyncResult          *result,
                                                                     gpointer               user_data);
GCancellable *ephy_bookmarks_manager_save_warn_on_error_cancellable (EphyBookmarksManager  *self);

G_END_DECLS
