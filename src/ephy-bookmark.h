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

#ifndef _EPHY_BOOKMARK_H
#define _EPHY_BOOKMARK_H

#include <glib-object.h>

G_BEGIN_DECLS

#define EPHY_TYPE_BOOKMARK (ephy_bookmark_get_type ())

G_DECLARE_FINAL_TYPE (EphyBookmark, ephy_bookmark, EPHY, BOOKMARK, GObject)

EphyBookmark        *ephy_bookmark_new          (char      *url,
                                                 char      *title,
                                                 GSequence *tags);

void                 ephy_bookmark_set_url      (EphyBookmark *self,
                                                 const char   *url);
const char          *ephy_bookmark_get_url      (EphyBookmark *self);

void                 ephy_bookmark_set_title    (EphyBookmark *self,
                                                 const char   *title);
const char          *ephy_bookmark_get_title    (EphyBookmark *self);

void                 ephy_bookmark_add_tag      (EphyBookmark *self,
                                                 const char   *tag);
void                 ephy_bookmark_remove_tag   (EphyBookmark *self,
                                                 const char   *tag);
gboolean             ephy_bookmark_has_tag      (EphyBookmark *self,
                                                 const char   *tag);
GSequence           *ephy_bookmark_get_tags     (EphyBookmark *self);
int                  ephy_bookmark_tags_compare (const char *tag1,
                                                 const char *tag2);

G_END_DECLS

#endif /* _EPHY_BOOKMARK_H */
