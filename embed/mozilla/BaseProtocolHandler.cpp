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

#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsIURI.h"
#include "nsNetCID.h"

#include "BaseProtocolHandler.h"

static NS_DEFINE_CID(kSimpleURICID, NS_SIMPLEURI_CID);

/* Implementation file */
NS_IMPL_ISUPPORTS1 (GBaseProtocolHandler, nsIProtocolHandler)

GBaseProtocolHandler::GBaseProtocolHandler(const char *aScheme)
{
	NS_INIT_ISUPPORTS();
	/* member initializers and constructor code */
	mScheme.Assign(aScheme);
}

GBaseProtocolHandler::~GBaseProtocolHandler()
{
	/* destructor code */
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

