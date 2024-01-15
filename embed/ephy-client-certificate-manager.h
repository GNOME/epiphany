/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2024 Jan-Michael Brummer <jan-michael.brummer1@volkswagen.de>
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

#include <gio/gio.h>

#include "ephy-web-view.h"

G_BEGIN_DECLS

typedef struct EphyClientCertificateManager EphyClientCertificateManager;

EphyClientCertificateManager *ephy_client_certificate_manager_request_certificate (WebKitWebView               *web_view,
                                                                                   WebKitAuthenticationRequest *request);

void ephy_client_certificate_manager_request_certificate_pin (EphyClientCertificateManager *self,
                                                              WebKitWebView                *web_view,
                                                              WebKitAuthenticationRequest  *request);

void
ephy_client_certificate_manager_free (EphyClientCertificateManager *self);

G_END_DECLS
