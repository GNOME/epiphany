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

G_BEGIN_DECLS

/* TRANSLATORS: Please modify the main address of duckduckgo in order to match
 * the version used in your country. For example for the french version :
 * replace the ".com" with ".fr" :  "https://duckduckgo.fr/?q=%s&amp;t=epiphany"
*/
#define EPHY_SEARCH_ENGINE_DEFAULT_ADDRESS _("https://duckduckgo.com/?q=%s&amp;t=epiphany")

#define EPHY_TYPE_SEARCH_ENGINE_MANAGER (ephy_search_engine_manager_get_type ())

G_DECLARE_FINAL_TYPE (EphySearchEngineManager, ephy_search_engine_manager, EPHY, SEARCH_ENGINE_MANAGER, GObject)

EphySearchEngineManager     *ephy_search_engine_manager_new                      (void);
const char                  *ephy_search_engine_manager_get_address              (EphySearchEngineManager *manager,
                                                                                  const char              *name);
const char                  *ephy_search_engine_manager_get_bang                 (EphySearchEngineManager *manager,
                                                                                  const char              *name);
char                        *ephy_search_engine_manager_get_default_engine       (EphySearchEngineManager *manager);
gboolean                     ephy_search_engine_manager_set_default_engine       (EphySearchEngineManager *manager,
                                                                                  const char              *name);
char                       **ephy_search_engine_manager_get_names                (EphySearchEngineManager *manager);
char                       **ephy_search_engine_manager_get_bangs                (EphySearchEngineManager *manager);
void                         ephy_search_engine_manager_add_engine               (EphySearchEngineManager *manager,
                                                                                  const char              *name,
                                                                                  const char              *address,
                                                                                  const char              *bang);
void                         ephy_search_engine_manager_delete_engine            (EphySearchEngineManager *manager,
                                                                                  const char              *name);
void                         ephy_search_engine_manager_modify_engine            (EphySearchEngineManager *manager,
                                                                                  const char              *name,
                                                                                  const char              *address,
                                                                                  const char              *bang);
const char                  *ephy_search_engine_manager_engine_from_bang         (EphySearchEngineManager *manager,
                                                                                  const char              *bang);
char                        *ephy_search_engine_manager_build_search_address     (EphySearchEngineManager *manager,
                                                                                  const char              *name,
                                                                                  const char              *search);
char                        *ephy_search_engine_manager_parse_bang_search        (EphySearchEngineManager *manager,
                                                                                  const char              *search);

G_END_DECLS
