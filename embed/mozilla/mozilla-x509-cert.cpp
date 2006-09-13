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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * $Id$
 */

#include "mozilla-config.h"
#include "config.h"

#include <nsStringAPI.h>

#include "ephy-debug.h"

#include "mozilla-x509-cert.h"

#define MOZILLA_X509_CERT_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), \
                               MOZILLA_TYPE_X509_CERT, MozillaX509CertPrivate))

static void mozilla_x509_cert_class_init (MozillaX509CertClass *klass);
static void ephy_x509_cert_init (EphyX509CertIface *iface);
static void mozilla_x509_cert_init (MozillaX509Cert *cert);

struct _MozillaX509CertPrivate
{
	nsIX509Cert * mozilla_cert;
	gchar *title;
};

enum
{
	PROP_0,
	PROP_MOZILLA_CERT
};

static GObjectClass *parent_class = NULL;

GType 
mozilla_x509_cert_get_type (void)
{
	static GType mozilla_x509_cert_type = 0;

	if (mozilla_x509_cert_type == 0)
	{
		const GTypeInfo our_info =
		{
			sizeof (MozillaX509CertClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) mozilla_x509_cert_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (MozillaX509Cert),
			0, /* n_preallocs */
			(GInstanceInitFunc) mozilla_x509_cert_init
		};

		const GInterfaceInfo x509_cert_info =
		{
			(GInterfaceInitFunc) ephy_x509_cert_init,      /* interface_init */
			NULL,                                          /* interface_finalize */
			NULL                                           /* interface_data */
     		 };
		
		mozilla_x509_cert_type = g_type_register_static (G_TYPE_OBJECT,
                                                                 "MozillaX509Cert",
                                                                 &our_info, 
                                                                 (GTypeFlags)0);
		g_type_add_interface_static (mozilla_x509_cert_type,
					     EPHY_TYPE_X509_CERT,
					     &x509_cert_info);
	}

	return mozilla_x509_cert_type;
}

static void
mozilla_x509_cert_set_mozilla_cert (MozillaX509Cert *cert,
				    nsIX509Cert *mozilla_cert)
{
	nsCOMPtr<nsIX509Cert> tmpcert = cert->priv->mozilla_cert;

	if (cert->priv->mozilla_cert)
	{
		NS_RELEASE (cert->priv->mozilla_cert);
	}

	cert->priv->mozilla_cert = mozilla_cert;
	NS_IF_ADDREF (cert->priv->mozilla_cert);
}


nsresult
mozilla_x509_cert_get_mozilla_cert  (MozillaX509Cert *cert, nsIX509Cert**aCert)
{
	*aCert = cert->priv->mozilla_cert;
	NS_IF_ADDREF (*aCert);

	return *aCert ? NS_OK : NS_ERROR_FAILURE;
}

static const char*
impl_get_title (EphyX509Cert *cert)
{
	MozillaX509Cert *m_cert = MOZILLA_X509_CERT (cert);

	/* lazy initialization of the title private variable */
	if (m_cert->priv->title != NULL)
	{
		return m_cert->priv->title;
	}

	/* This title logic is adapted from Mozilla source at
	   mozilla/security/manager/ssl/src/nsCertTree.cpp */
	nsString name;
	m_cert->priv->mozilla_cert->GetCommonName (name);
	if (name.Length())
	{
		nsCString cname;
		NS_UTF16ToCString (name, NS_CSTRING_ENCODING_UTF8, cname);

		m_cert->priv->title = g_strdup (cname.get());
	}
	else
	{
		/* No common name, so get the nickname instead */
		nsString nick;
		m_cert->priv->mozilla_cert->GetNickname (nick);

		nsCString cnick;
		NS_UTF16ToCString (nick, NS_CSTRING_ENCODING_UTF8, cnick);

		const char * str = cnick.get();
		char * colon = strchr (str, ':');
		if (colon)
		{
			m_cert->priv->title = g_strdup (colon+1);
		}
		else
		{
			m_cert->priv->title = g_strdup (cnick.get());
		}
	}

	return m_cert->priv->title;
}

static void
impl_set_property (GObject         *object,
		   guint            prop_id,
		   const GValue    *value,
		   GParamSpec      *pspec)
{
	MozillaX509Cert *cert = MOZILLA_X509_CERT (object);

	switch (prop_id)
	{
		case PROP_MOZILLA_CERT:
			mozilla_x509_cert_set_mozilla_cert(cert, 
							   (nsIX509Cert*)g_value_get_pointer (value));
			break;
		default:
			break;
	}
}

static void
impl_get_property (GObject         *object,
		   guint            prop_id,
		   GValue    *value,
		   GParamSpec      *pspec)
{
	MozillaX509Cert *cert = MOZILLA_X509_CERT (object);

	switch (prop_id)
	{
		case PROP_MOZILLA_CERT:
			g_value_set_pointer (value, cert->priv->mozilla_cert);
			break;
		default:
		  	break;
	}
}

static void
mozilla_x509_cert_init (MozillaX509Cert *cert)
{
	cert->priv = MOZILLA_X509_CERT_GET_PRIVATE (cert);
}

static void
mozilla_x509_cert_finalize (GObject *object)
{
	MozillaX509Cert *cert = MOZILLA_X509_CERT (object);

	LOG ("Finalizing MozillaX509Cert %p", cert);
	
	if (cert->priv->mozilla_cert)
	{
		NS_RELEASE (cert->priv->mozilla_cert);
	}

	if (cert->priv->title)
	{
		g_free (cert->priv->title);
	}
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
ephy_x509_cert_init (EphyX509CertIface *iface)
{
	iface->get_title = impl_get_title;
}

static void
mozilla_x509_cert_class_init (MozillaX509CertClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = (GObjectClass*)g_type_class_peek_parent (klass);

	object_class->finalize = mozilla_x509_cert_finalize;
	object_class->set_property = impl_set_property;
	object_class->get_property = impl_get_property;

	g_object_class_install_property (object_class,
					 PROP_MOZILLA_CERT,
					 g_param_spec_pointer ("mozilla-cert",
							       "Mozilla-Cert",
							       "Mozilla XPCOM certificate",
							       (GParamFlags)(G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY)));

	g_type_class_add_private (object_class, sizeof (MozillaX509CertPrivate));
}

MozillaX509Cert *
mozilla_x509_cert_new (nsIX509Cert *moz_cert)
{
	MozillaX509Cert *cert;

	cert = (MozillaX509Cert*)g_object_new (MOZILLA_TYPE_X509_CERT,
					       "mozilla-cert", moz_cert,
					       (char *) NULL);
	return cert;
}
