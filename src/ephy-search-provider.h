/*
 * Copyright (c) 2013 Giovanni Campagna <scampa.giovanni@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _EPHY_SEARCH_PROVIDER_H
#define _EPHY_SEARCH_PROVIDER_H

#include "ephy-shell-search-provider-generated.h"

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define EPHY_TYPE_SEARCH_PROVIDER (ephy_search_provider_get_type ())

G_DECLARE_FINAL_TYPE (EphySearchProvider, ephy_search_provider, EPHY, SEARCH_PROVIDER, GApplication)

EphySearchProvider *ephy_search_provider_new (void);

G_END_DECLS

#endif /* _EPHY_SEARCH_PROVIDER_H */
