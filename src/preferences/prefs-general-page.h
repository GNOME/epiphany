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

#include <adwaita.h>

#include "ephy-prefs-dialog.h"
#include "ephy-search-engine-listbox.h"

G_BEGIN_DECLS

#define EPHY_TYPE_PREFS_GENERAL_PAGE (prefs_general_page_get_type ())

G_DECLARE_FINAL_TYPE (PrefsGeneralPage, prefs_general_page, EPHY, PREFS_GENERAL_PAGE, AdwPreferencesPage)

void prefs_general_page_on_pd_close_request (PrefsGeneralPage *general_page);
EphySearchEngineListBox *prefs_general_page_get_search_engine_list_box (PrefsGeneralPage *general_page);

G_END_DECLS
