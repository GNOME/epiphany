/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2004  Tommi Komulainen
 *  Copyright © 2004, 2005  Christian Persch
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

#include "ephy-web-view.h"

G_BEGIN_DECLS

#define EPHY_TYPE_FIND_TOOLBAR (ephy_find_toolbar_get_type ())

G_DECLARE_FINAL_TYPE (EphyFindToolbar, ephy_find_toolbar, EPHY, FIND_TOOLBAR, AdwBin)

EphyFindToolbar *ephy_find_toolbar_new           (WebKitWebView *web_view);

const char      *ephy_find_toolbar_get_text      (EphyFindToolbar *toolbar);

void             ephy_find_toolbar_find_next     (EphyFindToolbar *toolbar);

void             ephy_find_toolbar_find_previous (EphyFindToolbar *toolbar);

void             ephy_find_toolbar_open          (EphyFindToolbar *toolbar);

void             ephy_find_toolbar_close         (EphyFindToolbar *toolbar);

void             ephy_find_toolbar_request_close (EphyFindToolbar *toolbar);

G_END_DECLS
