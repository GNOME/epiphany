/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2010 Igalia S.L.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <glib.h>
#include <gio/gio.h>

#include "ephy-prefs.h"

G_BEGIN_DECLS

#define EPHY_SETTINGS_MAIN      ephy_settings_get (EPHY_PREFS_SCHEMA)
#define EPHY_SETTINGS_UI        ephy_settings_get (EPHY_PREFS_UI_SCHEMA)
#define EPHY_SETTINGS_WEB       ephy_settings_get (EPHY_PREFS_WEB_SCHEMA)
#define EPHY_SETTINGS_LOCKDOWN  ephy_settings_get (EPHY_PREFS_LOCKDOWN_SCHEMA)
#define EPHY_SETTINGS_STATE     ephy_settings_get (EPHY_PREFS_STATE_SCHEMA)

GSettings *ephy_settings_get (const char *schema);

void ephy_settings_shutdown (void);

G_END_DECLS
