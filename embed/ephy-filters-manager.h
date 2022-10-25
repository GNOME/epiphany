/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*
 *  Copyright © 2017 Igalia S.L.
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

#include <glib-object.h>
#include <webkit/webkit.h>

G_BEGIN_DECLS

#define EPHY_TYPE_FILTERS_MANAGER (ephy_filters_manager_get_type ())

G_DECLARE_FINAL_TYPE (EphyFiltersManager, ephy_filters_manager, EPHY, FILTERS_MANAGER, GObject)

EphyFiltersManager *ephy_filters_manager_new                     (const char         *adblock_filters_dir);
const char         *ephy_filters_manager_get_adblock_filters_dir (EphyFiltersManager *manager);
gboolean            ephy_filters_manager_get_is_initialized      (EphyFiltersManager *manager);
void                ephy_filters_manager_set_ucm_forbids_ads     (EphyFiltersManager       *manager,
                                                                  WebKitUserContentManager *ucm,
                                                                  gboolean                  forbids_ads);


G_END_DECLS
