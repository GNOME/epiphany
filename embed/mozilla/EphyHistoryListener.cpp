/*
 *  Copyright © 2004 Christian Persch
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
 *  $Id$
 */

#include <xpcom-config.h>
#include "config.h"

#include <nsStringAPI.h>

#include <nsCOMPtr.h>
#include <nsCURILoader.h>
#include <nsIChannel.h>
#include <nsIDocumentLoader.h>
#include <nsIHttpChannel.h>
#include <nsIRequest.h>
#include <nsIRequestObserver.h>
#include <nsISupportsUtils.h>
#include <nsIURI.h>
#include <nsIWebProgress.h>
#include <nsNetCID.h>
#include <nsServiceManagerUtils.h>
 
#include "EphyUtils.h"

#include "ephy-debug.h"

#include "EphyHistoryListener.h"

EphyHistoryListener::EphyHistoryListener ()
{
	LOG ("EphyHistoryListener ctor");
}

EphyHistoryListener::~EphyHistoryListener ()
{
	LOG ("EphyHistoryListener dtor");
}

nsresult
EphyHistoryListener::Init (EphyHistory *aHistory)
{
	mHistory = aHistory;

	nsresult rv;
	nsCOMPtr<nsIWebProgress> webProgress
		(do_GetService(NS_DOCUMENTLOADER_SERVICE_CONTRACTID, &rv));
	NS_ENSURE_TRUE (NS_SUCCEEDED (rv) && webProgress, rv);
		
	rv = webProgress->AddProgressListener
			(static_cast<nsIWebProgressListener*>(this),
			 nsIWebProgress::NOTIFY_STATE_REQUEST);

	return rv;
}

NS_IMPL_ISUPPORTS2 (EphyHistoryListener,
		    nsIWebProgressListener,
		    nsISupportsWeakReference)

/* void onStateChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aStateFlags, in nsresult aStatus); */
NS_IMETHODIMP
EphyHistoryListener::OnStateChange (nsIWebProgress *aWebProgress,
                                     nsIRequest *aRequest,
                                     PRUint32 aStateFlags,
				     nsresult aStatus)
{
	nsresult rv = NS_OK;

	/* we only care about redirects */
	if (! (aStateFlags & nsIWebProgressListener::STATE_REDIRECTING))
	{
		return rv;
	}

	nsCOMPtr<nsIChannel> channel (do_QueryInterface (aRequest));
	nsCOMPtr<nsIHttpChannel> httpChannel (do_QueryInterface (channel));
	if (!httpChannel) return rv;

	PRUint32 status = 0;
	rv = httpChannel->GetResponseStatus (&status);
	if (rv == NS_ERROR_NOT_AVAILABLE) return NS_OK;
	NS_ENSURE_SUCCESS (rv, rv);

	/* we're only interested in 301 redirects (moved permanently) */
	if (status != 301) return NS_OK;

	nsCOMPtr<nsIURI> fromURI;
	rv = channel->GetURI (getter_AddRefs (fromURI));
	NS_ENSURE_TRUE (NS_SUCCEEDED (rv) && fromURI, rv);

	nsCString location;
	rv = httpChannel->GetResponseHeader
		(nsCString ("Location"), location);
	NS_ENSURE_TRUE (NS_SUCCEEDED (rv) && location.Length(), rv);

	nsCOMPtr<nsIURI> toURI;
	rv = EphyUtils::NewURI (getter_AddRefs (toURI), location,
				nsnull /* use origin charset of fromURI */, fromURI);
	NS_ENSURE_TRUE (NS_SUCCEEDED (rv) && toURI, rv);

	nsCString fromSpec, toSpec;
	rv = fromURI->GetSpec (fromSpec);
	rv |= toURI->GetSpec(toSpec);
	NS_ENSURE_SUCCESS (rv, rv);

	g_signal_emit_by_name (mHistory, "redirect",
			       fromSpec.get(), toSpec.get());
	
	return rv;
}

/* void onProgressChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aCurSelfProgress, in long aMaxSelfProgress, in long aCurTotalProgress, in long aMaxTotalProgress); */
NS_IMETHODIMP
EphyHistoryListener::OnProgressChange (nsIWebProgress *aWebProgress,
                                        nsIRequest *aRequest,
                                        PRInt32 aCurSelfProgress,
                                        PRInt32 aMaxSelfProgress,
                                        PRInt32 aCurTotalProgress,
                                        PRInt32 aMaxTotalProgress)
{
	NS_NOTREACHED("notification excluded in AddProgressListener(...)");
	return NS_OK;
}

/* void onLocationChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsIURI location); */
NS_IMETHODIMP
EphyHistoryListener::OnLocationChange (nsIWebProgress *aWebProgress,
					nsIRequest *aRequest,
					nsIURI *location)
{
	NS_NOTREACHED("notification excluded in AddProgressListener(...)");
	return NS_OK;
}

/* void onStatusChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsresult aStatus, in wstring aMessage); */
NS_IMETHODIMP
EphyHistoryListener::OnStatusChange (nsIWebProgress *aWebProgress,
				      nsIRequest *aRequest,
				      nsresult aStatus,
				      const PRUnichar *aMessage)
{
	NS_NOTREACHED("notification excluded in AddProgressListener(...)");
	return NS_OK;
}

/* void onSecurityChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long state); */
NS_IMETHODIMP
EphyHistoryListener::OnSecurityChange (nsIWebProgress *aWebProgress,
					nsIRequest *aRequest,
					PRUint32 state)
{
	NS_NOTREACHED("notification excluded in AddProgressListener(...)");
	return NS_OK;
}
