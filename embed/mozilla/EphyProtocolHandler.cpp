/*
 *  Copyright (C) 2001 Matt Aubury, Philip Langdale
 *  Copyright (C) 2004 Crispin Flowerday
 *  Copyright (C) 2005 Christian Persch
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

#include <nsCOMPtr.h>
#include <nsIIOService.h>
#include <nsIServiceManager.h>
#include <nsIURI.h>
#include <nsIChannel.h>
#include <nsIOutputStream.h>
#include <nsIInputStream.h>
#include <nsILoadGroup.h>
#include <nsIInterfaceRequestor.h>
#include <nsIStorageStream.h>
#include <nsIInputStreamChannel.h>
#include <nsIScriptSecurityManager.h>
#include <nsNetCID.h>
#include <nsString.h>
#include <nsEscape.h>

#include <glib/gi18n.h>

#include "EphyProtocolHandler.h"
#include "EphyUtils.h"

#include "ephy-debug.h"

#include <string.h>

static NS_DEFINE_CID(kSimpleURICID, NS_SIMPLEURI_CID);
static NS_DEFINE_CID(kInputStreamChannelCID, NS_INPUTSTREAMCHANNEL_CID);

EphyProtocolHandler::EphyProtocolHandler()
{
	LOG ("EphyProtocolHandler ctor [%p]\n", this);
}

EphyProtocolHandler::~EphyProtocolHandler()
{
	LOG ("EphyProtocolHandler dtor [%p]\n", this);
}

NS_IMPL_ISUPPORTS2 (EphyProtocolHandler, nsIProtocolHandler, nsIAboutModule)

/* readonly attribute string scheme; */
NS_IMETHODIMP
EphyProtocolHandler::GetScheme(nsACString &aScheme)
{
	aScheme.Assign("epiphany");
	return NS_OK;
}

/* readonly attribute long defaultPort; */
NS_IMETHODIMP
EphyProtocolHandler::GetDefaultPort(PRInt32 *aDefaultPort)
{
	NS_ENSURE_ARG_POINTER (aDefaultPort);

	*aDefaultPort = -1;
	return NS_OK;
}

/* readonly attribute short protocolFlags; */
NS_IMETHODIMP
EphyProtocolHandler::GetProtocolFlags(PRUint32 *aProtocolFlags)
{
	NS_ENSURE_ARG_POINTER (aProtocolFlags);

	*aProtocolFlags = nsIProtocolHandler::URI_NORELATIVE | nsIProtocolHandler::URI_NOAUTH;
	return NS_OK;
} 

/* nsIURI newURI (in string aSpec, in nsIURI aBaseURI); */
NS_IMETHODIMP
EphyProtocolHandler::NewURI(const nsACString &aSpec,
			    const char *aOriginCharset,
			    nsIURI *aBaseURI,
			    nsIURI **_retval)
{
	nsresult rv;
	nsCOMPtr<nsIURI> uri (do_CreateInstance(kSimpleURICID, &rv));
	NS_ENSURE_SUCCESS (rv, rv);

	rv = uri->SetSpec (aSpec);
	NS_ENSURE_SUCCESS (rv, rv);

	NS_ADDREF(*_retval = uri);
	return NS_OK;
}

/* nsIChannel newChannel (in nsIURI aURI); */
NS_IMETHODIMP
EphyProtocolHandler::NewChannel(nsIURI *aURI,
				nsIChannel **_retval)
{
	NS_ENSURE_ARG(aURI);

#if 0
	PRBool isEpiphany = PR_FALSE;
	if (NS_SUCCEEDED (aURI->SchemeIs ("epiphany", &isEpiphany)) && isEpiphany)
	{
		return HandleEpiphany (aURI, _retval);
	}
#endif

	PRBool isAbout = PR_FALSE;
	if (NS_SUCCEEDED (aURI->SchemeIs ("about", &isAbout)) && isAbout)
	{
		return Redirect (nsDependentCString ("file://" SHARE_DIR "/epiphany.xhtml"), _retval);
	}

	return NS_ERROR_ILLEGAL_VALUE;
}

/* boolean allowPort (in long port, in string scheme); */
NS_IMETHODIMP 
EphyProtocolHandler::AllowPort(PRInt32 port,
			       const char *scheme,
			       PRBool *_retval)
{
	*_retval = PR_FALSE;
	return NS_OK;
}

/* private functions */

nsresult
EphyProtocolHandler::Redirect (const nsACString &aURL,
			       nsIChannel **_retval)
{
	nsresult rv;
	nsCOMPtr<nsIIOService> ioService;
	rv = EphyUtils::GetIOService (getter_AddRefs (ioService));
	NS_ENSURE_SUCCESS (rv, rv);

	nsCOMPtr<nsIChannel> tempChannel;
	rv = ioService->NewChannel(aURL, nsnull, nsnull, getter_AddRefs(tempChannel));
	NS_ENSURE_SUCCESS (rv, rv);

	nsCOMPtr<nsIURI> uri;
	rv = ioService->NewURI(aURL, nsnull, nsnull, getter_AddRefs(uri));
	NS_ENSURE_SUCCESS (rv, rv);

	tempChannel->SetOriginalURI (uri);

	nsCOMPtr<nsIScriptSecurityManager> securityManager = 
			do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
	NS_ENSURE_SUCCESS (rv, rv);

	nsCOMPtr<nsIPrincipal> principal;
	rv = securityManager->GetCodebasePrincipal(uri, getter_AddRefs(principal));
	NS_ENSURE_SUCCESS (rv, rv);

	nsCOMPtr<nsISupports> owner (do_QueryInterface(principal));
	rv = tempChannel->SetOwner(owner);

	NS_ADDREF(*_retval = tempChannel);
	return rv;
}
