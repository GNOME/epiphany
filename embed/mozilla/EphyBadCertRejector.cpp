/*
 *  Copyright © 2005 Christian Persch
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
 *  $Id$
 */

#include "mozilla-config.h"
#include "config.h"

#include "EphyBadCertRejector.h"

NS_IMPL_THREADSAFE_ISUPPORTS1 (EphyBadCertRejector, nsIBadCertListener)

/* boolean confirmUnknownIssuer (in nsIInterfaceRequestor socketInfo, in nsIX509Cert cert, out short certAddType); */
NS_IMETHODIMP
EphyBadCertRejector::ConfirmUnknownIssuer(nsIInterfaceRequestor *socketInfo,
					  nsIX509Cert *cert,
					  PRInt16 *certAddType,
					  PRBool *_retval)
{
	*certAddType = nsIBadCertListener::UNINIT_ADD_FLAG;
	*_retval = PR_FALSE;
	return NS_OK;
}

/* boolean confirmMismatchDomain (in nsIInterfaceRequestor socketInfo, in AUTF8String targetURL, in nsIX509Cert cert); */
NS_IMETHODIMP
EphyBadCertRejector::ConfirmMismatchDomain(nsIInterfaceRequestor *socketInfo,
					   const nsACString & targetURL,
					   nsIX509Cert *cert,
					   PRBool *_retval)
{
	*_retval = PR_FALSE;
	return NS_OK;
}

/* boolean confirmCertExpired (in nsIInterfaceRequestor socketInfo, in nsIX509Cert cert); */
NS_IMETHODIMP
EphyBadCertRejector::ConfirmCertExpired(nsIInterfaceRequestor *socketInfo,
					nsIX509Cert *cert,
					PRBool *_retval)
{
	*_retval = PR_FALSE;
	return NS_OK;
}

/* void notifyCrlNextupdate (in nsIInterfaceRequestor socketInfo, in AUTF8String targetURL, in nsIX509Cert cert); */
NS_IMETHODIMP
EphyBadCertRejector::NotifyCrlNextupdate(nsIInterfaceRequestor *socketInfo,
					 const nsACString & targetURL,
					 nsIX509Cert *cert)
{
	return NS_OK;
}
