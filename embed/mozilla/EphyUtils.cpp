/*
 *  Copyright (C) 2004 Marco Pesenti Gritti
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2.1, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "EphyUtils.h"

#include <nsIServiceManager.h>
#include <nsEmbedString.h>

nsresult
EphyUtils::GetIOService (nsIIOService **ioService)
{
	nsresult rv;

	nsCOMPtr<nsIServiceManager> mgr; 
	NS_GetServiceManager (getter_AddRefs (mgr));
	if (!mgr) return NS_ERROR_FAILURE;

	rv = mgr->GetServiceByContractID ("@mozilla.org/network/io-service;1",
					  NS_GET_IID (nsIIOService),
					  (void **)ioService);
	return rv;
}

nsresult EphyUtils::NewURI (nsIURI **result, const nsAString &spec)
{
	nsEmbedCString cSpec;
	NS_UTF16ToCString (spec, NS_CSTRING_ENCODING_UTF8, cSpec);

	return NewURI (result, cSpec);
}

nsresult EphyUtils::NewURI (nsIURI **result, const nsACString &spec)
{
	nsresult rv;

	nsCOMPtr<nsIIOService> ioService;
	rv = EphyUtils::GetIOService (getter_AddRefs (ioService));
	NS_ENSURE_SUCCESS (rv, rv);

	rv = ioService->NewURI (spec, nsnull, nsnull, result);

	return rv;
}
