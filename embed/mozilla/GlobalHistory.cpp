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
#include <config.h>
#endif

#include "ephy-embed-shell.h"
#include "GlobalHistory.h"
#include "nsString.h"

NS_IMPL_ISUPPORTS2(MozGlobalHistory, nsIGlobalHistory, nsIBrowserHistory)

MozGlobalHistory::MozGlobalHistory ()
{
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

#if MOZILLA_SNAPSHOT > 8
NS_IMETHODIMP MozGlobalHistory::SetLastPageVisited(const char *aLastPageVisited)
{
        return NS_ERROR_NOT_IMPLEMENTED;
}
#endif

NS_IMETHODIMP MozGlobalHistory::HidePage(const char *url)
{
	return NS_ERROR_NOT_IMPLEMENTED;

}

/* readonly attribute PRUint32 count; */
NS_IMETHODIMP MozGlobalHistory::GetCount(PRUint32 *aCount)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void markPageAsTyped (in string url); */
NS_IMETHODIMP MozGlobalHistory::MarkPageAsTyped(const char *url)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

