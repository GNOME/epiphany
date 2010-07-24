/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef EPHY_SETTINGS_H
#define EPHY_SETTINGS_H

#include <glib.h>
#include <gio/gio.h>

#include "ephy-prefs.h"

#define EPHY_SETTINGS_MAIN      ephy_settings_get (EPHY_PREFS_SCHEMA)
#define EPHY_SETTINGS_UI        ephy_settings_get (EPHY_PREFS_UI_SCHEMA)
#define EPHY_SETTINGS_WEB       ephy_settings_get (EPHY_PREFS_WEB_SCHEMA)
#define EPHY_SETTINGS_LOCKDOWN  ephy_settings_get (EPHY_PREFS_LOCKDOWN_SCHEMA)
#define EPHY_SETTINGS_STATE     ephy_settings_get (EPHY_PREFS_STATE_SCHEMA)

GSettings *ephy_settings_get (const char *schema);

void ephy_settings_shutdown (void);

#endif /* EPHY_SETTINGS_H */
