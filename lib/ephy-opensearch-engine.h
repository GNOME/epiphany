/* ephy-opensearch-engine.h
 *
 * Copyright 2022 vanadiae <vanadiae35@gmail.com>
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

#include <glib.h>
#include <gio/gio.h>
#include "ephy-search-engine.h"
#include "ephy-opensearch-autodiscovery-link.h"

EphySearchEngine *ephy_opensearch_engine_load_from_data        (EphyOpensearchAutodiscoveryLink  *autodiscovery_link,
                                                                const char                       *description_file,
                                                                gsize                             length,
                                                                GError                          **error);
void              ephy_opensearch_engine_load_from_link_async  (EphyOpensearchAutodiscoveryLink  *autodiscovery_link,
                                                                GCancellable                     *cancellable,
                                                                GAsyncReadyCallback               callback,
                                                                gpointer                          user_data);
EphySearchEngine *ephy_opensearch_engine_load_from_link_finish (EphyOpensearchAutodiscoveryLink  *autodiscovery_link,
                                                                GAsyncResult                     *result,
                                                                GError                          **error);
