/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2020 Epiphany Developers
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
#include <adwaita.h>

G_BEGIN_DECLS

#define EPHY_TYPE_FIREFOX_SYNC_DIALOG (ephy_firefox_sync_dialog_get_type ())

G_DECLARE_FINAL_TYPE (EphyFirefoxSyncDialog, ephy_firefox_sync_dialog, EPHY, FIREFOX_SYNC_DIALOG, AdwWindow)

GtkWidget *ephy_firefox_sync_dialog_new ();

G_END_DECLS
