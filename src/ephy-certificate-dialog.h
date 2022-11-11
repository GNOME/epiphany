/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2012 Igalia S.L.
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

#include "ephy-security-levels.h"

#include <gio/gio.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_CERTIFICATE_DIALOG (ephy_certificate_dialog_get_type())

G_DECLARE_FINAL_TYPE (EphyCertificateDialog, ephy_certificate_dialog, EPHY, CERTIFICATE_DIALOG, GtkDialog)

GtkWidget *ephy_certificate_dialog_new      (GtkWindow           *parent,
                                             const char          *address,
                                             GTlsCertificate     *certificate,
                                             GTlsCertificateFlags tls_errors,
                                             EphySecurityLevel    security_level);

G_END_DECLS
