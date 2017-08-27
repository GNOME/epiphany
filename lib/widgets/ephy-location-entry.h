/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2002  Ricardo Fernández Pascual
 *  Copyright © 2003, 2004  Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2005  Christian Persch
 *  Copyright © 2016  Igalia S.L.
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

#include <dazzle.h>
#include <gtk/gtk.h>

#include "ephy-security-levels.h"

G_BEGIN_DECLS

#define EPHY_TYPE_LOCATION_ENTRY (ephy_location_entry_get_type())

G_DECLARE_FINAL_TYPE (EphyLocationEntry, ephy_location_entry, EPHY, LOCATION_ENTRY, DzlSuggestionEntry)

typedef enum {
  EPHY_LOCATION_ENTRY_BOOKMARK_ICON_HIDDEN,
  EPHY_LOCATION_ENTRY_BOOKMARK_ICON_EMPTY,
  EPHY_LOCATION_ENTRY_BOOKMARK_ICON_BOOKMARKED
} EphyLocationEntryBookmarkIconState;

GtkWidget      *ephy_location_entry_new                        (void);

gboolean        ephy_location_entry_get_can_undo               (EphyLocationEntry *entry);

gboolean        ephy_location_entry_get_can_redo               (EphyLocationEntry *entry);

GSList         *ephy_location_entry_get_search_terms           (EphyLocationEntry *entry);

gboolean        ephy_location_entry_reset                      (EphyLocationEntry *entry);

void            ephy_location_entry_undo_reset                 (EphyLocationEntry *entry);

void            ephy_location_entry_activate                   (EphyLocationEntry *entry);

void            ephy_location_entry_set_bookmark_icon_state    (EphyLocationEntry                  *entry,
                                                                EphyLocationEntryBookmarkIconState  state);

void            ephy_location_entry_set_lock_tooltip           (EphyLocationEntry *entry,
                                                                const char        *tooltip);

void            ephy_location_entry_set_add_bookmark_popover   (EphyLocationEntry *entry,
                                                                GtkPopover        *popover);

GtkPopover     *ephy_location_entry_get_add_bookmark_popover   (EphyLocationEntry *entry);

G_END_DECLS
