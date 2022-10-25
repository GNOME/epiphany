/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2012, 2013 Igalia S.L.
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

#include "ephy-history-service.h"

#include <webkit/webkit.h>

G_BEGIN_DECLS

#define EPHY_TYPE_ABOUT_HANDLER (ephy_about_handler_get_type ())

G_DECLARE_FINAL_TYPE (EphyAboutHandler, ephy_about_handler, EPHY, ABOUT_HANDLER, GObject)

#define EPHY_ABOUT_SCHEME "ephy-about"
#define EPHY_ABOUT_SCHEME_LEN 10

EphyAboutHandler *ephy_about_handler_new            (void);
void              ephy_about_handler_handle_request (EphyAboutHandler       *handler,
                                                     WebKitURISchemeRequest *request);

EphyHistoryQuery *ephy_history_query_new_for_overview (void);

G_END_DECLS
