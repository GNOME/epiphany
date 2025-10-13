/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2013 Bastien Nocera <hadess@hadess.net>
 *  Copyright © 2016 Igalia S.L.
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

#include <glib.h>

G_BEGIN_DECLS

/* Epiphany and WebKit internally store URLs using percent-encoded characters
 * and punycode rather than UTF-8. Encoded URLs should be used almost
 * everywhere, but should not be displayed to the user. Use ephy_uri_decode()
 * immediately before displaying the URL in user interface.
 */

char *ephy_uri_decode (const char *uri);
char *ephy_uri_get_decoded_host (const char *decoded_uri);
char *ephy_uri_to_security_origin (const char *uri);
char *ephy_uri_get_base_domain (const char *hostname);

G_END_DECLS
