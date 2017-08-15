/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2017 Gabriel Ivascu <gabrielivascu@gnome.org>
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

#include "ephy-open-tabs-record.h"
#include "ephy-tabs-catalog.h"

#include <glib-object.h>

G_BEGIN_DECLS

#define EPHY_TYPE_OPEN_TABS_MANAGER (ephy_open_tabs_manager_get_type ())

G_DECLARE_FINAL_TYPE (EphyOpenTabsManager, ephy_open_tabs_manager, EPHY, OPEN_TABS_MANAGER, GObject)

EphyOpenTabsManager *ephy_open_tabs_manager_new             (EphyTabsCatalog *catalog);
EphyOpenTabsRecord  *ephy_open_tabs_manager_get_local_tabs  (EphyOpenTabsManager *self);
GList               *ephy_open_tabs_manager_get_remote_tabs (EphyOpenTabsManager *self);
void                 ephy_open_tabs_manager_clear_cache     (EphyOpenTabsManager *self);

G_END_DECLS
