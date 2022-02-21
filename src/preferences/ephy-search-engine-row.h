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

#include <handy.h>
#include "ephy-search-engine-manager.h"

G_BEGIN_DECLS

#define EPHY_TYPE_SEARCH_ENGINE_ROW (ephy_search_engine_row_get_type())

G_DECLARE_FINAL_TYPE (EphySearchEngineRow, ephy_search_engine_row, EPHY, SEARCH_ENGINE_ROW, HdyExpanderRow)

EphySearchEngineRow *ephy_search_engine_row_new                    (EphySearchEngine        *engine,
                                                                    EphySearchEngineManager *manager);
void                 ephy_search_engine_row_set_can_remove         (EphySearchEngineRow *self,
                                                                    gboolean             can_remove);
void                 ephy_search_engine_row_set_radio_button_group (EphySearchEngineRow *self,
                                                                    GtkRadioButton      *radio_button_group);

G_END_DECLS
