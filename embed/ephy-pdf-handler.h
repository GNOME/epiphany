/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2020 Jan-Michael Brummer <jan.brummer@tabos.org>
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

#include <webkit2/webkit2.h>

G_BEGIN_DECLS

#define EPHY_TYPE_PDF_HANDLER (ephy_pdf_handler_get_type ())

G_DECLARE_FINAL_TYPE (EphyPDFHandler, ephy_pdf_handler, EPHY, PDF_HANDLER, GObject)

#define EPHY_PDF_SCHEME "ephy-pdf"

EphyPDFHandler *ephy_pdf_handler_new            (void);

void            ephy_pdf_handler_handle_request (EphyPDFHandler  *handler,
                                                 WebKitURISchemeRequest *request);

void            ephy_pdf_handler_stop (EphyPDFHandler *handler,
                                       WebKitWebView  *webv_view);

G_END_DECLS
