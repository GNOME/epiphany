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

#include "nsIFactory.h"
#include "nsXPComFactory.h"
#include "nsString.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsIExternalProtocolService.h"
#include "nsCExternalHandlerService.h"
#include "nsCRT.h"

#include "BaseProtocolContentHandler.h"

class GMailtoProtocolHandler : public GBaseProtocolContentHandler
{
  public:
	NS_DECL_ISUPPORTS
	GMailtoProtocolHandler() : GBaseProtocolContentHandler("mailto")
				   {NS_INIT_ISUPPORTS();};
	virtual ~GMailtoProtocolHandler() {};
  private:
};

/* Implementation file */
NS_IMPL_ISUPPORTS2 (GMailtoProtocolHandler, nsIProtocolHandler, nsIContentHandler)

NS_DEF_FACTORY (GMailtoProtocolHandler, GMailtoProtocolHandler);

/**
 * NS_NewMailtoHandlerFactory:
 */ 
nsresult NS_NewMailtoHandlerFactory(nsIFactory** aFactory)
{
	NS_ENSURE_ARG_POINTER(aFactory);
	*aFactory = nsnull;

	nsGMailtoProtocolHandlerFactory *result = 
					new nsGMailtoProtocolHandlerFactory;
	if (result == NULL)
	{
		return NS_ERROR_OUT_OF_MEMORY;
	}
    
	NS_ADDREF(result);
	*aFactory = result;

	return NS_OK;
}
