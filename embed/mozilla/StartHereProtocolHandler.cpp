/*
 *  Copyright (C) 2001 Matt Aubury, Philip Langdale
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

#include "ephy-file-helpers.h"
#include "ephy-start-here.h"
#include "ephy-embed-shell.h"

#include <string.h>

#include "nsCOMPtr.h"
#include "nsIFactory.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsIURI.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsXPComFactory.h"
#include "nsIStorageStream.h"

static NS_DEFINE_CID(kSimpleURICID,     NS_SIMPLEURI_CID);

class GStartHereProtocolHandler : public nsIProtocolHandler
{
  public:
	NS_DECL_ISUPPORTS
	NS_DECL_NSIPROTOCOLHANDLER

	GStartHereProtocolHandler (void);
	virtual ~GStartHereProtocolHandler();

	nsCOMPtr<nsIChannel> mChannel;
};

/* Implementation file */
NS_IMPL_ISUPPORTS1 (GStartHereProtocolHandler, nsIProtocolHandler)

GStartHereProtocolHandler::GStartHereProtocolHandler (void)
{
	NS_INIT_ISUPPORTS();

}

GStartHereProtocolHandler::~GStartHereProtocolHandler()
{
	/* destructor code */
}

/* readonly attribute string scheme; */
NS_IMETHODIMP GStartHereProtocolHandler::GetScheme(nsACString &aScheme)
{
	aScheme = NS_LITERAL_CSTRING("start-here");
	return NS_OK;
}

/* readonly attribute long defaultPort; */
NS_IMETHODIMP GStartHereProtocolHandler::GetDefaultPort(PRInt32 *aDefaultPort)
{
	nsresult rv = NS_OK;
	if (aDefaultPort)
		*aDefaultPort = -1;
	else
		rv = NS_ERROR_NULL_POINTER;
	return rv;
}

/* readonly attribute short protocolFlags; */
NS_IMETHODIMP GStartHereProtocolHandler::GetProtocolFlags(PRUint32 *aProtocolFlags)
{
	if (aProtocolFlags)
		*aProtocolFlags = nsIProtocolHandler::URI_STD;
	else
		return NS_ERROR_NULL_POINTER;
	return NS_OK;
} 

/* nsIURI newURI (in string aSpec, in nsIURI aBaseURI); */
NS_IMETHODIMP GStartHereProtocolHandler::NewURI(const nsACString &aSpec,
					        const char *aOriginCharset,
					        nsIURI *aBaseURI,
					        nsIURI **_retval)
{
	nsresult rv = NS_OK;
	nsCOMPtr <nsIURI> newUri;
	
	rv = nsComponentManager::CreateInstance(kSimpleURICID, NULL,
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
NS_IMETHODIMP GStartHereProtocolHandler::NewChannel(nsIURI *aURI,
					            nsIChannel **_retval)
{
	nsresult rv;
	EphyStartHere *sh;
	char *buf;
	const char *aBaseURI;
	PRUint32 bytesWritten;
		
	nsCAutoString path;
	rv = aURI->GetPath(path);
	if (NS_FAILED(rv)) return rv;

	if (g_str_has_prefix (path.get(), "import-mozilla-bookmarks"))
	{
		g_signal_emit_by_name (embed_shell, "command", "import-mozilla-bookmarks",
				       path.get() + strlen ("import-bookmarks?"));
		return NS_ERROR_FAILURE;
	}
	else if (g_str_has_prefix (path.get(), "import-galeon-bookmarks"))
	{
		g_signal_emit_by_name (embed_shell, "command",
				       "import-galeon-bookmarks",
				       path.get() + strlen ("import-galeon-bookmarks?"));
		return NS_ERROR_FAILURE;
	}
	else if (g_str_has_prefix (path.get(), "import-konqueror-bookmarks"))
	{
		g_signal_emit_by_name (embed_shell, "command",
				       "import-konqueror-bookmarks",
				       path.get() + strlen ("import-konqueror-bookmarks?"));
		return NS_ERROR_FAILURE;
	}
	else if (g_str_has_prefix (path.get(), "configure-network"))
	{
		g_signal_emit_by_name (embed_shell, "command", "configure-network",
				       NULL);
		return NS_ERROR_FAILURE;	
	}

    	nsCOMPtr<nsIStorageStream> sStream;
	nsCOMPtr<nsIOutputStream> stream;

	rv = NS_NewStorageStream(16384, (PRUint32)-1, getter_AddRefs(sStream));
	if (NS_FAILED(rv)) return rv;

	rv = sStream->GetOutputStream(0, getter_AddRefs(stream));
	if (NS_FAILED(rv)) return rv;

	sh = ephy_start_here_new ();
	buf = ephy_start_here_get_page
		(sh, path.IsEmpty() ? "index" : path.get ());
	if (buf == NULL) return NS_ERROR_FAILURE;
	aBaseURI = ephy_start_here_get_base_uri (sh);	
	
	rv = stream->Write (buf, strlen (buf), &bytesWritten);
	if (NS_FAILED (rv)) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIURI> uri;
	nsCAutoString spec(aBaseURI);
	rv = NS_NewURI(getter_AddRefs(uri), spec.get());
	if (NS_FAILED (rv)) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIInputStream> iStream;
	
	rv = sStream->NewInputStream(0, getter_AddRefs(iStream));
	if (NS_FAILED(rv)) return rv;
	
	rv = NS_NewInputStreamChannel(getter_AddRefs(mChannel), uri,
				      iStream, NS_LITERAL_CSTRING("text/xml"),
				      NS_LITERAL_CSTRING("utf-8"));
	
	g_free (buf);
	g_object_unref (sh);
	
	NS_IF_ADDREF (*_retval = mChannel);
	
	return rv;
}

/* boolean allowPort (in long port, in string scheme); */
NS_IMETHODIMP GStartHereProtocolHandler::AllowPort(PRInt32 port, const char *scheme,
					           PRBool *_retval)
{
	*_retval = PR_FALSE;
	return NS_OK;
}

NS_DEF_FACTORY (GStartHereProtocolHandler, GStartHereProtocolHandler);

/**
 * NS_NewStartHereProtocolHandlerFactory:
 */ 
nsresult NS_NewStartHereHandlerFactory(nsIFactory** aFactory)
{
	NS_ENSURE_ARG_POINTER(aFactory);
	*aFactory = nsnull;

	nsGStartHereProtocolHandlerFactory *result = new nsGStartHereProtocolHandlerFactory;
	if (result == NULL)
	{
		return NS_ERROR_OUT_OF_MEMORY;
	}
    
	NS_ADDREF(result);
	*aFactory = result;

	return NS_OK;
}
