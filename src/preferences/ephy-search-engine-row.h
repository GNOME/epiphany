/* ephy-search-engine-row.h
 *
 * Copyright 2020 vanadiae <vanadiae35@gmail.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#pragma once

#include <adwaita.h>
#include "ephy-search-engine-manager.h"
#include "ephy-search-engine.h"

G_BEGIN_DECLS

#define EPHY_TYPE_SEARCH_ENGINE_ROW (ephy_search_engine_row_get_type())

G_DECLARE_FINAL_TYPE (EphySearchEngineRow, ephy_search_engine_row, EPHY, SEARCH_ENGINE_ROW, AdwExpanderRow)

EphySearchEngineRow *ephy_search_engine_row_new                    (EphySearchEngine        *engine,
                                                                    EphySearchEngineManager *manager);
EphySearchEngine    *ephy_search_engine_row_get_engine             (EphySearchEngineRow     *self);
void                 ephy_search_engine_row_set_radio_button_group (EphySearchEngineRow *self,
                                                                    GtkCheckButton      *radio_button_group);
void                 ephy_search_engine_row_focus_bang_entry       (EphySearchEngineRow *self);
void                 ephy_search_engine_row_focus_name_entry       (EphySearchEngineRow *self);

G_END_DECLS
