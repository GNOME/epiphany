/*
 *  Copyright (C) 2001 Philip Langdale
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ExternalProtocolHandlers.h"
#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsIURI.h"
#include "nsCOMPtr.h"
#include "nsIChannel.h"
#include "nsIStorageStream.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsNetUtil.h"
#include "nsIExternalProtocolService.h"
#include "nsCExternalHandlerService.h"

NS_IMPL_ISUPPORTS2 (GIRCProtocolHandler, nsIProtocolHandler, nsIContentHandler)
NS_IMPL_ISUPPORTS2 (GFtpProtocolHandler, nsIProtocolHandler, nsIContentHandler)
NS_IMPL_ISUPPORTS2 (GMailtoProtocolHandler, nsIProtocolHandler, nsIContentHandler)
NS_IMPL_ISUPPORTS2 (GNewsProtocolHandler, nsIProtocolHandler, nsIContentHandler)

static NS_DEFINE_CID(kSimpleURICID, NS_SIMPLEURI_CID);

/* Implementation file */
NS_IMPL_ISUPPORTS1 (GBaseProtocolHandler, nsIProtocolHandler)

GBaseProtocolHandler::GBaseProtocolHandler(const char *aScheme)
{
	mScheme.Assign(aScheme);
}

GBaseProtocolHandler::~GBaseProtocolHandler()
{
}

/* readonly attribute string scheme; */
NS_IMETHODIMP GBaseProtocolHandler::GetScheme(nsACString &aScheme)
{
        aScheme = mScheme;
        return NS_OK;
}

/* readonly attribute long defaultPort; */
NS_IMETHODIMP GBaseProtocolHandler::GetDefaultPort(PRInt32 *aDefaultPort)
{
        if (aDefaultPort)
                *aDefaultPort = -1;
        else
                return NS_ERROR_NULL_POINTER;
        return NS_OK;
}

/* readonly attribute short protocolFlags; */
NS_IMETHODIMP GBaseProtocolHandler::GetProtocolFlags(PRUint32 *aProtocolFlags)
{
	if (aProtocolFlags)
		*aProtocolFlags = nsIProtocolHandler::URI_STD;
	else
		return NS_ERROR_NULL_POINTER;
	return NS_OK;
} 

/* nsIURI newURI (in string aSpec, in nsIURI aBaseURI); */
NS_IMETHODIMP GBaseProtocolHandler::NewURI(const nsACString &aSpec,
					   const char *aOriginCharset, nsIURI *aBaseURI,
					   nsIURI **_retval)
{
	nsresult rv = NS_OK;
	nsCOMPtr <nsIURI> newUri;

	rv = nsComponentManager::CreateInstance(kSimpleURICID, nsnull,
						NS_GET_IID(nsIURI),
						getter_AddRefs(newUri));

        if (NS_SUCCEEDED(rv)) 
        {
		newUri->SetSpec(aSpec);
		rv = newUri->QueryInterface(NS_GET_IID(nsIURI),
					    (void **) _retval);
        }
	return rv;

}

/* nsIChannel newChannel (in nsIURI aURI); */
NS_IMETHODIMP GBaseProtocolHandler::NewChannel(nsIURI *aURI,
					       nsIChannel **_retval)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean allowPort (in long port, in string scheme); */
NS_IMETHODIMP GBaseProtocolHandler::AllowPort(PRInt32 port, const char *scheme,
					      PRBool *_retval)
{
	*_retval = PR_FALSE;
	return NS_OK;
}

/* Implementation file */
NS_IMPL_ISUPPORTS2 (GBaseProtocolContentHandler, nsIProtocolHandler, nsIContentHandler)

GBaseProtocolContentHandler::GBaseProtocolContentHandler(const char *aScheme) :
		      GBaseProtocolHandler(aScheme)
{
	mMimeType = NS_LITERAL_CSTRING("application-x-gnome-") + mScheme;
}

GBaseProtocolContentHandler::~GBaseProtocolContentHandler()
{
}

/* nsIChannel newChannel (in nsIURI aURI); */
NS_IMETHODIMP GBaseProtocolContentHandler::NewChannel(nsIURI *aURI,
						      nsIChannel **_retval)
{
	nsresult rv;
	nsCOMPtr<nsIStorageStream> sStream;
	rv = NS_NewStorageStream(1, 16, getter_AddRefs(sStream));
	NS_ENSURE_SUCCESS (rv, rv);

	nsCOMPtr<nsIOutputStream> oStream;
	rv = sStream->GetOutputStream(0, getter_AddRefs(oStream));
	NS_ENSURE_SUCCESS (rv, rv);

	PRUint32 bytes;
	rv = oStream->Write("Dummy stream\0", 13, &bytes);
	NS_ENSURE_SUCCESS (rv, rv);

	nsCOMPtr<nsIInputStream> iStream;
	rv = sStream->NewInputStream(0, getter_AddRefs(iStream));
	NS_ENSURE_SUCCESS (rv, rv);

	nsCOMPtr<nsIChannel> channel;
	rv = NS_NewInputStreamChannel(getter_AddRefs(channel), aURI,
				      iStream, mMimeType, NS_LITERAL_CSTRING(""));
	NS_ENSURE_SUCCESS (rv, rv);

	NS_IF_ADDREF (*_retval = channel);

	return rv;
}

NS_IMETHODIMP GBaseProtocolContentHandler::HandleContent (
					const char * aContentType,
					const char * aCommand,
					nsISupports * aWindowContext,
					nsIRequest *aRequest)
{
	NS_ENSURE_ARG (aRequest);

	nsresult rv = NS_OK;

  	// First of all, get the content type and make sure it is a 
  	// content type we know how to handle!
	if (mMimeType.Equals(aContentType))
	{
		nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
		NS_ENSURE_TRUE (channel, NS_ERROR_FAILURE);

		nsCOMPtr<nsIURI> uri;
		channel->GetURI(getter_AddRefs(uri));
		NS_ENSURE_TRUE (uri, NS_ERROR_FAILURE);

		rv = aRequest->Cancel(NS_BINDING_ABORTED);
		NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

		nsCOMPtr<nsIExternalProtocolService> ps = 
			do_GetService (NS_EXTERNALPROTOCOLSERVICE_CONTRACTID);
		NS_ENSURE_TRUE (ps, NS_ERROR_FAILURE);

		rv = ps->LoadUrl (uri);
	}

	return rv;
}
