/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2017 Cedric Le Moigne <cedlemo@gmx.com>
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

#include <gio/gio.h>
#include <glib-object.h>
#include <glib/gi18n.h>

#include "ephy-search-engine.h"

G_BEGIN_DECLS

#define EPHY_TYPE_SEARCH_ENGINE_MANAGER (ephy_search_engine_manager_get_type ())

G_DECLARE_FINAL_TYPE (EphySearchEngineManager, ephy_search_engine_manager, EPHY, SEARCH_ENGINE_MANAGER, GObject)

EphySearchEngineManager *ephy_search_engine_manager_new                 (void);
EphySearchEngine        *ephy_search_engine_manager_get_default_engine  (EphySearchEngineManager *manager);
void                     ephy_search_engine_manager_set_default_engine  (EphySearchEngineManager *manager,
                                                                         EphySearchEngine        *engine);
EphySearchEngine        *ephy_search_engine_manager_get_incognito_engine  (EphySearchEngineManager *manager);
void                     ephy_search_engine_manager_set_incognito_engine  (EphySearchEngineManager *manager,
                                                                           EphySearchEngine        *engine);
void                     ephy_search_engine_manager_add_engine          (EphySearchEngineManager *manager,
                                                                         EphySearchEngine        *engine);
void                     ephy_search_engine_manager_delete_engine       (EphySearchEngineManager *manager,
                                                                         EphySearchEngine        *engine);
EphySearchEngine        *ephy_search_engine_manager_find_engine_by_name (EphySearchEngineManager *manager,
                                                                         const char              *engine_name);
gboolean                 ephy_search_engine_manager_has_bang            (EphySearchEngineManager *manager,
                                                                         const char              *bang);
char                    *ephy_search_engine_manager_parse_bang_search   (EphySearchEngineManager *manager,
                                                                         const char              *search);
char                    *ephy_search_engine_manager_parse_bang_suggestions (EphySearchEngineManager *manager,
                                                                            const char              *search,
                                                                            EphySearchEngine       **out_engine);
void                     ephy_search_engine_manager_save_to_settings    (EphySearchEngineManager *manager);

G_END_DECLS
