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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * $Id$
 */

#ifndef MOZILLA_X509_CERT_H
#define MOZILLA_X509_CERT_H

#include "ephy-x509-cert.h"

#include <glib-object.h>

#include <nsCOMPtr.h>
#include <nsIX509Cert.h>

G_BEGIN_DECLS

#define MOZILLA_TYPE_X509_CERT             (mozilla_x509_cert_get_type ())
#define MOZILLA_X509_CERT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOZILLA_TYPE_X509_CERT, MozillaX509Cert))
#define MOZILLA_X509_CERT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), MOZILLA_TYPE_X509_CERT, MozillaX509CertClass))
#define MOZILLA_IS_X509_CERT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOZILLA_TYPE_X509_CERT))
#define MOZILLA_IS_X509_CERT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), MOZILLA_TYPE_X509_CERT))
#define MOZILLA_X509_CERT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), MOZILLA_TYPE_X509_CERT, MozillaX509CertClass))

typedef struct _MozillaX509Cert MozillaX509Cert;
typedef struct _MozillaX509CertPrivate MozillaX509CertPrivate;
typedef struct _MozillaX509CertClass MozillaX509CertClass;

struct _MozillaX509Cert
{
	GObject parent;
	MozillaX509CertPrivate *priv;
};

struct _MozillaX509CertClass
{
	GObjectClass parent_class;
};

GType	                   mozilla_x509_cert_get_type         (void);

MozillaX509Cert           *mozilla_x509_cert_new              (nsIX509Cert *aMozCert);

nsresult                   mozilla_x509_cert_get_mozilla_cert (MozillaX509Cert *cert, nsIX509Cert **cert);

G_END_DECLS

#endif
