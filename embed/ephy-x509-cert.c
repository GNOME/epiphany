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

#include <config.h>

#include "ephy-x509-cert.h"

GType
ephy_x509_cert_get_type (void)
{
	static GType ephy_x509_cert_type = 0;

	if (ephy_x509_cert_type == 0)
	{
		const GTypeInfo our_info =
		{
			sizeof (EphyX509CertIface),
			NULL,
			NULL,
		};
		ephy_x509_cert_type = g_type_register_static (G_TYPE_INTERFACE,
							      "EphyEmbedX509cert",
							      &our_info,
							      (GTypeFlags)0);
	}

	return ephy_x509_cert_type;
}

const char *
ephy_x509_cert_get_title (EphyX509Cert *cert)
{
	EphyX509CertIface *iface = EPHY_X509_CERT_GET_IFACE (cert);
	return iface->get_title (cert);
}
