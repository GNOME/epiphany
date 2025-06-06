/* ephy-opensearch-autodiscovery-link.h
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

#include <glib-object.h>

G_BEGIN_DECLS

#define EPHY_TYPE_OPENSEARCH_AUTODISCOVERY_LINK (ephy_opensearch_autodiscovery_link_get_type())

G_DECLARE_FINAL_TYPE (EphyOpensearchAutodiscoveryLink, ephy_opensearch_autodiscovery_link, EPHY, OPENSEARCH_AUTODISCOVERY_LINK, GObject)

EphyOpensearchAutodiscoveryLink *ephy_opensearch_autodiscovery_link_new      (const char                      *name,
                                                                              const char                      *url);
const char                      *ephy_opensearch_autodiscovery_link_get_name (EphyOpensearchAutodiscoveryLink *self);
const char                      *ephy_opensearch_autodiscovery_link_get_url  (EphyOpensearchAutodiscoveryLink *self);

G_END_DECLS
