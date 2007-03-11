/*
 *  Copyright © 2003 Robert Marcano
 *  Copyright © 2005 Crispin Flowerday
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
 * $Id$
 */

#include <config.h>

#include "ephy-certificate-manager.h"

GType
ephy_certificate_manager_get_type (void)
{
	static GType ephy_certificate_manager_type = 0;

	if (ephy_certificate_manager_type == 0)
	{
		const GTypeInfo our_info =
		{
			sizeof (EphyCertificateManagerIface),
			NULL,
			NULL,
		};

		ephy_certificate_manager_type = g_type_register_static (G_TYPE_INTERFACE,
									"EphyCertificateManager",
									&our_info,
									(GTypeFlags)0);
	}

	return ephy_certificate_manager_type;
}

/* Certificates */
GList *
ephy_certificate_manager_get_certificates (EphyCertificateManager *manager,
					   EphyX509CertType type)
{
	EphyCertificateManagerIface *iface = EPHY_CERTIFICATE_MANAGER_GET_IFACE (manager);
	return iface->get_certificates (manager, type);
}

gboolean
ephy_certificate_manager_remove_certificate (EphyCertificateManager *manager,
					     EphyX509Cert *cert)
{
	EphyCertificateManagerIface *iface = EPHY_CERTIFICATE_MANAGER_GET_IFACE (manager);
	return iface->remove_certificate (manager, cert);
}

gboolean
ephy_certificate_manager_import (EphyCertificateManager *manager,
				 const gchar *file)
{
	EphyCertificateManagerIface *iface = EPHY_CERTIFICATE_MANAGER_GET_IFACE (manager);
	return iface->import (manager, file);
}
