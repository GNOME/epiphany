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

#include <gtk/gtk.h>

#include "ephy-adaptive-mode.h"
#include "ephy-bookmark-states.h"
#include "ephy-security-levels.h"

G_BEGIN_DECLS

#define EPHY_TYPE_LOCATION_ENTRY (ephy_location_entry_get_type())

G_DECLARE_FINAL_TYPE (EphyLocationEntry, ephy_location_entry, EPHY, LOCATION_ENTRY, GtkBin)


GtkWidget      *ephy_location_entry_new                        (void);

gboolean        ephy_location_entry_get_can_undo               (EphyLocationEntry *entry);

gboolean        ephy_location_entry_get_can_redo               (EphyLocationEntry *entry);

gboolean        ephy_location_entry_reset                      (EphyLocationEntry *entry);

void            ephy_location_entry_undo_reset                 (EphyLocationEntry *entry);

void            ephy_location_entry_focus                      (EphyLocationEntry *entry);

void            ephy_location_entry_set_bookmark_icon_state    (EphyLocationEntry     *entry,
                                                                EphyBookmarkIconState  state);

void            ephy_location_entry_set_lock_tooltip           (EphyLocationEntry *entry,
                                                                const char        *tooltip);

void            ephy_location_entry_set_add_bookmark_popover   (EphyLocationEntry *entry,
                                                                GtkPopover        *popover);

void            ephy_location_entry_show_add_bookmark_popover  (EphyLocationEntry *entry);

GtkWidget      *ephy_location_entry_get_entry                  (EphyLocationEntry *entry);

void            ephy_location_entry_set_reader_mode_visible    (EphyLocationEntry *entry,
                                                                gboolean           visible);

void            ephy_location_entry_set_reader_mode_state      (EphyLocationEntry *entry,
                                                                gboolean           active);

gboolean        ephy_location_entry_get_reader_mode_state      (EphyLocationEntry *entry);

void            ephy_location_entry_set_progress               (EphyLocationEntry *entry,
                                                                gdouble            progress,
                                                                gboolean           loading);
void            ephy_location_entry_page_action_add            (EphyLocationEntry *entry,
                                                                GtkWidget         *action);

void            ephy_location_entry_page_action_clear          (EphyLocationEntry *entry);

void            ephy_location_entry_set_adaptive_mode          (EphyLocationEntry *entry,
                                                                EphyAdaptiveMode   adaptive_mode);

G_END_DECLS
