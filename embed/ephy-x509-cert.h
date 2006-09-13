/*
 *  Copyright © 2003 Robert Marcano
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

#ifndef EPHY_X509_CERT_H
#define EPHY_X509_CERT_H

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum
{
	PERSONAL_CERTIFICATE,
	SERVER_CERTIFICATE,
	CA_CERTIFICATE
} EphyX509CertType;


#define EPHY_TYPE_X509_CERT             (ephy_x509_cert_get_type ())
#define EPHY_X509_CERT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_X509_CERT, EphyX509Cert))
#define EPHY_X509_CERT_IFACE(klass)     (G_TYPE_CHECK_IFACE_CAST ((klass), EPHY_TYPE_X509_CERT, EphyX509CertIface))
#define EPHY_IS_X509_CERT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_X509_CERT))
#define EPHY_IS_X509_CERT_IFACE(klass)  (G_TYPE_CHECK_IFACE_TYPE ((klass), EPHY_TYPE_X509_CERT))
#define EPHY_X509_CERT_GET_IFACE(inst)  (G_TYPE_INSTANCE_GET_INTERFACE ((inst), EPHY_TYPE_X509_CERT, EphyX509CertIface))

typedef struct _EphyX509Cert EphyX509Cert;
typedef struct _EphyX509CertIface EphyX509CertIface;

struct _EphyX509CertIface
{
	GTypeInterface base_iface;

	/* Methods  */
        const char * (* get_title) (EphyX509Cert *cert);
};

GType            ephy_x509_cert_get_type             (void);

/* Base */
const char * ephy_x509_cert_get_title (EphyX509Cert *cert);

G_END_DECLS

#endif
