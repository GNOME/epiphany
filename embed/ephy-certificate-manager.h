/*
 *  Copyright (C) 2003 Robert Marcano
 *  Copyright (C) 2005 Crispin Flowerday
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * $Id$
 */

#ifndef EPHY_CERTIFICATE_MANAGER_H
#define EPHY_CERTIFICATE_MANAGER_H

#include <glib-object.h>

#include "ephy-x509-cert.h"

G_BEGIN_DECLS

#define EPHY_TYPE_CERTIFICATE_MANAGER             (ephy_certificate_manager_get_type ())
#define EPHY_CERTIFICATE_MANAGER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_CERTIFICATE_MANAGER, EphyCertificateManager))
#define EPHY_CERTIFICATE_MANAGER_IFACE(klass)     (G_TYPE_CHECK_IFACE_CAST ((klass), EPHY_TYPE_CERTIFICATE_MANAGER, EphyCertificateManagerIface))
#define EPHY_IS_CERTIFICATE_MANAGER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_CERTIFICATE_MANAGER))
#define EPHY_IS_CERTIFICATE_MANAGER_IFACE(klass)  (G_TYPE_CHECK_IFACE_TYPE ((klass), EPHY_TYPE_CERTIFICATE_MANAGER))
#define EPHY_CERTIFICATE_MANAGER_GET_IFACE(inst)  (G_TYPE_INSTANCE_GET_INTERFACE ((inst), EPHY_TYPE_CERTIFICATE_MANAGER, EphyCertificateManagerIface))

typedef struct _EphyCertificateManager      EphyCertificateManager;
typedef struct _EphyCertificateManagerIface EphyCertificateManagerIface;

struct _EphyCertificateManagerIface
{
	GTypeInterface base_iface;

	/* Methods  */
	GList *          (* get_certificates)    (EphyCertificateManager *manager,
						 EphyX509CertType type);
	gboolean         (* remove_certificate)  (EphyCertificateManager *manager,
						 EphyX509Cert *cert);
	gboolean         (* import)              (EphyCertificateManager *manager,
						 const gchar *file);
};

GType            ephy_certificate_manager_get_type        (void);

/* Certificate */
GList *          ephy_certificate_manager_get_certificates     (EphyCertificateManager *manager,
							       EphyX509CertType type);

gboolean         ephy_certificate_manager_remove_certificate   (EphyCertificateManager *manager,
								EphyX509Cert *cert);

gboolean         ephy_certificate_manager_import               (EphyCertificateManager *manager,
								const gchar *file);

G_END_DECLS

#endif
