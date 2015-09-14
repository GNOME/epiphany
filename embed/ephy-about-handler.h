/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2012, 2013 Igalia S.L.
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

#ifndef EPHY_ABOUT_HANDLER_H
#define EPHY_ABOUT_HANDLER_H

#include <webkit2/webkit2.h>

G_BEGIN_DECLS

#define EPHY_TYPE_ABOUT_HANDLER (ephy_about_handler_get_type ())

G_DECLARE_FINAL_TYPE (EphyAboutHandler, ephy_about_handler, EPHY, ABOUT_HANDLER, GObject)

#define EPHY_ABOUT_SCHEME "ephy-about"
#define EPHY_ABOUT_SCHEME_LEN 10
#define EPHY_ABOUT_OVERVIEW_MAX_ITEMS 10

EphyAboutHandler *ephy_about_handler_new            (void);
void              ephy_about_handler_handle_request (EphyAboutHandler       *handler,
                                                     WebKitURISchemeRequest *request);
G_END_DECLS

#endif /* EPHY_ABOUT_HANDLER_H */
