/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2022 Purism SPC
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

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_SEARCH_ENTRY (ephy_search_entry_get_type())

G_DECLARE_FINAL_TYPE (EphySearchEntry, ephy_search_entry, EPHY, SEARCH_ENTRY, GtkWidget)

typedef enum {
  EPHY_FIND_RESULT_FOUND = 0,
  EPHY_FIND_RESULT_NOTFOUND = 1,
  EPHY_FIND_RESULT_FOUNDWRAPPED = 2
} EphyFindResult;

GtkWidget     *ephy_search_entry_new                  (void);

const char    *ephy_search_entry_get_placeholder_text (EphySearchEntry *self);
void           ephy_search_entry_set_placeholder_text (EphySearchEntry *self,
                                                       const char      *placeholder_text);

gboolean       ephy_search_entry_get_show_matches     (EphySearchEntry *self);
void           ephy_search_entry_set_show_matches     (EphySearchEntry *self,
                                                       gboolean         show_matches);

guint          ephy_search_entry_get_n_matches        (EphySearchEntry *self);
void           ephy_search_entry_set_n_matches        (EphySearchEntry *self,
                                                       guint            n_matches);

guint          ephy_search_entry_get_current_match    (EphySearchEntry *self);
void           ephy_search_entry_set_current_match    (EphySearchEntry *self,
                                                       guint            current_match);

EphyFindResult ephy_search_entry_get_find_result      (EphySearchEntry *self);
void           ephy_search_entry_set_find_result      (EphySearchEntry *self,
                                                       EphyFindResult   result);

G_END_DECLS
