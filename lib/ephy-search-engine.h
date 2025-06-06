/* ephy-search-engine.h
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

#include "ephy-opensearch-autodiscovery-link.h"

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define EPHY_TYPE_SEARCH_ENGINE (ephy_search_engine_get_type())

G_DECLARE_FINAL_TYPE (EphySearchEngine, ephy_search_engine, EPHY, SEARCH_ENGINE, GObject)

/* It's intended that there's no ephy_search_engine_new() as that just can't be
 * general enough to cover all the cases where you'd create an engine. So instead,
 * just use g_object_new() with the properties you already have available for the
 * new search engine, and all other properties will
 * have a reasonable default value (i.e. empty or NULL).
 */

const char *ephy_search_engine_get_name                  (EphySearchEngine *self);
void        ephy_search_engine_set_name                  (EphySearchEngine *self,
                                                          const char       *name);
const char *ephy_search_engine_get_url                   (EphySearchEngine *self);
void        ephy_search_engine_set_url                   (EphySearchEngine *self,
                                                          const char       *url);
const char *ephy_search_engine_get_bang                  (EphySearchEngine *self);
void        ephy_search_engine_set_bang                  (EphySearchEngine *self,
                                                          const char       *bang);
const char *ephy_search_engine_get_suggestions_url       (EphySearchEngine *self);
void        ephy_search_engine_set_suggestions_url       (EphySearchEngine *self,
                                                          const char       *suggestions_url);
const char *ephy_search_engine_get_opensearch_url        (EphySearchEngine *self);
void        ephy_search_engine_set_opensearch_url        (EphySearchEngine *self,
                                                          const char       *opensearch_url);
char       *ephy_search_engine_build_search_address      (EphySearchEngine *self,
                                                          const char       *search_query);
char       *ephy_search_engine_build_suggestions_address (EphySearchEngine *self,
                                                          const char       *search_query);
char       *ephy_search_engine_build_bang_for_name       (const char *name);
void        ephy_search_engine_load_suggestions_async    (const char          *built_suggestions_url,
                                                          EphySearchEngine    *engine,
                                                          GCancellable        *cancellable,
                                                          GAsyncReadyCallback  callback,
                                                          gpointer             user_data);
GSequence  *ephy_search_engine_load_suggestions_finish   (GAsyncResult *result,
                                                          GError      **error);
gboolean    ephy_search_engine_matches_by_autodiscovery_link (EphySearchEngine                *self,
                                                              EphyOpensearchAutodiscoveryLink *autodiscovery_link);

G_END_DECLS
