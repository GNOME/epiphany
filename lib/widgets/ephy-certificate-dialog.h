/*
 *  Copyright © 2012 Igalia S.L.
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef EPHY_CERTIFICATE_DIALOG_H
#define EPHY_CERTIFICATE_DIALOG_H

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

#endif
