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

#include "ephy-embed-shell.h"

#include "nsCOMPtr.h"
#include "nsISupportsArray.h"
#include "nsIFactory.h"
#include "nsIServiceManager.h"
#include "nsXPComFactory.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIGlobalHistory.h"
#include "nsIBrowserHistory.h"
#include "nsIRequestObserver.h"

/**
 * class GlobalHistory: 
 *
 */
class MozGlobalHistory: public nsIGlobalHistory,
	public nsIBrowserHistory
{
	public:
		MozGlobalHistory ();
  		virtual ~MozGlobalHistory();

		NS_DECL_ISUPPORTS
		NS_DECL_NSIGLOBALHISTORY
		NS_DECL_NSIBROWSERHISTORY

	private:
		EphyHistory *mGlobalHistory;
};

NS_IMPL_ADDREF(MozGlobalHistory)
NS_IMPL_RELEASE(MozGlobalHistory)
NS_INTERFACE_MAP_BEGIN(MozGlobalHistory)
  NS_INTERFACE_MAP_ENTRY(nsIGlobalHistory)
  NS_INTERFACE_MAP_ENTRY(nsIBrowserHistory)
NS_INTERFACE_MAP_END

MozGlobalHistory::MozGlobalHistory ()
{
	NS_INIT_ISUPPORTS();

	mGlobalHistory = ephy_embed_shell_get_global_history (embed_shell);
}

MozGlobalHistory::~MozGlobalHistory ()
{
}

/* void addPage (in string aURL); */
NS_IMETHODIMP MozGlobalHistory::AddPage (const char *aURL)
{
	ephy_history_add_page (mGlobalHistory, aURL);
	
	return NS_OK;
}

/* boolean isVisited (in string aURL); */
NS_IMETHODIMP MozGlobalHistory::IsVisited (const char *aURL, PRBool *_retval)
{
	*_retval = ephy_history_is_page_visited (mGlobalHistory, aURL);
	
	return NS_OK;
}

/* void setPageTitle (in string aURL, in wstring aTitle); */
NS_IMETHODIMP MozGlobalHistory::SetPageTitle (const char *aURL, 
					      const PRUnichar *aTitle)
{
	const nsACString &title = NS_ConvertUCS2toUTF8 (aTitle);
	
	ephy_history_set_page_title (mGlobalHistory, aURL, PromiseFlatCString(title).get());

	/* done */
	return NS_OK;
}

/* void removePage (in string aURL); */
NS_IMETHODIMP MozGlobalHistory::RemovePage(const char *aURL)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

/* void removePagesFromHost (in string aHost, in boolean aEntireDomain); */
NS_IMETHODIMP MozGlobalHistory::RemovePagesFromHost(const char *aHost, 
						 PRBool aEntireDomain)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

/* void removeAllPages (); */
NS_IMETHODIMP MozGlobalHistory::RemoveAllPages()
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute string lastPageVisited; */
NS_IMETHODIMP MozGlobalHistory::GetLastPageVisited(char **aLastPageVisited)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP MozGlobalHistory::HidePage(const char *url)
{
	return NS_ERROR_NOT_IMPLEMENTED;

}

/* readonly attribute PRUint32 count; */
NS_IMETHODIMP MozGlobalHistory::GetCount(PRUint32 *aCount)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
 
/* void startBatchUpdate (); */
NS_IMETHODIMP MozGlobalHistory::StartBatchUpdate()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void endBatchUpdate (); */
NS_IMETHODIMP MozGlobalHistory::EndBatchUpdate()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void markPageAsTyped (in string url); */
NS_IMETHODIMP MozGlobalHistory::MarkPageAsTyped(const char *url)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* Described by mozilla.org as a temporary ugly hack. We will never need to
 * implement it. It is here to allow compilation.
 */
/* void outputReferrerURL (in string aURL, in string aReferrer); */
NS_IMETHODIMP MozGlobalHistory::OutputReferrerURL(const char *aURL, const char *aReferrer)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}
	
NS_DEF_FACTORY (MozGlobalHistory, MozGlobalHistory);

nsresult NS_NewGlobalHistoryFactory(nsIFactory** aFactory)
{
	NS_ENSURE_ARG_POINTER(aFactory);
	*aFactory = nsnull;

	nsMozGlobalHistoryFactory *result = new nsMozGlobalHistoryFactory;
	if (result == NULL)
	{
		return NS_ERROR_OUT_OF_MEMORY;
	}
    
	NS_ADDREF(result);
	*aFactory = result;

	return NS_OK;
}
