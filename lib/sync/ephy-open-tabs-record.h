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

#include <glib-object.h>

G_BEGIN_DECLS

#define EPHY_TYPE_OPEN_TABS_RECORD (ephy_open_tabs_record_get_type ())

G_DECLARE_FINAL_TYPE (EphyOpenTabsRecord, ephy_open_tabs_record, EPHY, OPEN_TABS_RECORD, GObject)

EphyOpenTabsRecord *ephy_open_tabs_record_new             (const char *id,
                                                           const char *client_name);
const char         *ephy_open_tabs_record_get_id          (EphyOpenTabsRecord *self);
const char         *ephy_open_tabs_record_get_client_name (EphyOpenTabsRecord *self);
GList              *ephy_open_tabs_record_get_tabs        (EphyOpenTabsRecord *self);
void                ephy_open_tabs_record_add_tab         (EphyOpenTabsRecord *self,
                                                           const char         *title,
                                                           const char         *url,
                                                           const char         *favicon);

G_END_DECLS
