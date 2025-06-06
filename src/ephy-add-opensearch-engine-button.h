/* ephy-add-opensearch-engine-button.h
 *
 * Copyright 2021 vanadiae <vanadiae35@gmail.com>
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

#include <gtk/gtk.h>
#include <adwaita.h>

G_BEGIN_DECLS

#define EPHY_TYPE_ADD_OPENSEARCH_ENGINE_BUTTON (ephy_add_opensearch_engine_button_get_type())

G_DECLARE_FINAL_TYPE (EphyAddOpensearchEngineButton, ephy_add_opensearch_engine_button, EPHY, ADD_OPENSEARCH_ENGINE_BUTTON, GtkButton)

EphyAddOpensearchEngineButton *ephy_add_opensearch_engine_button_new       (void);
void                           ephy_add_opensearch_engine_button_set_model (EphyAddOpensearchEngineButton *self,
                                                                            GListModel                    *model);

G_END_DECLS
