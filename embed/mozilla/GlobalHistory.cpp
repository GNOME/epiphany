/*
 *  Copyright (C) 2001, 2004 Philip Langdale
 *  Copyright (C) 2004 Christian Persch
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ephy-embed-shell.h"

#include "GlobalHistory.h"

#include <nsString.h>
#include <nsIURI.h>

#if MOZILLA_SNAPSHOT > 13
NS_IMPL_ISUPPORTS2(MozGlobalHistory, nsIGlobalHistory2, nsIBrowserHistory)
#else
NS_IMPL_ISUPPORTS2(MozGlobalHistory, nsIGlobalHistory, nsIBrowserHistory)
#endif

MozGlobalHistory::MozGlobalHistory ()
{
	mGlobalHistory = EPHY_HISTORY (ephy_embed_shell_get_global_history (embed_shell));
}

MozGlobalHistory::~MozGlobalHistory ()
{
}

#if MOZILLA_SNAPSHOT > 13

/* void addURI (in nsIURI aURI, in boolean aRedirect, in boolean aToplevel); */
NS_IMETHODIMP MozGlobalHistory::AddURI(nsIURI *aURI, PRBool aRedirect, PRBool aToplevel)
{
	nsresult rv;
	NS_ENSURE_ARG_POINTER(aURI);

	PRBool isJavascript;
	rv = aURI->SchemeIs("javascript", &isJavascript);
	NS_ENSURE_SUCCESS(rv, rv);

	if (isJavascript || aRedirect || !aTopLevel)
	{
		return NS_OK;
	}

	// filter out unwanted URIs such as chrome: etc
	// The model is really if we don't know differently then add which basically
	// means we are suppose to try all the things we know not to allow in and
	// then if we don't bail go on and allow it in.  But here lets compare
	// against the most common case we know to allow in and go on and say yes
	// to it.

	PRBool isHTTP = PR_FALSE;
	PRBool isHTTPS = PR_FALSE;

	rv = aURI->SchemeIs("http", &isHTTP);
	rv |= aURI->SchemeIs("https", &isHTTPS);
	NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

	if (!isHTTP && !isHTTPS)
	{
		PRBool isAbout, isViewSource, isChrome, isData;

		rv = aURI->SchemeIs("about", &isAbout);
		rv |= aURI->SchemeIs("view-source", &isViewSource);
		rv |= aURI->SchemeIs("chrome", &isChrome);
		rv |= aURI->SchemeIs("data", &isData);
		NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

		if (isAbout || isViewSource || isChrome || isData)
		{
		      return NS_OK;
		}
	}

	nsCAutoString spec;
	rv = aURI->GetSpec(spec);
	NS_ENSURE_SUCCESS(rv, rv);

	ephy_history_add_page (mGlobalHistory, spec.get());
	
	return NS_OK;
}

/* boolean isVisited (in nsIURI aURI); */
NS_IMETHODIMP MozGlobalHistory::IsVisited(nsIURI *aURI, PRBool *_retval)
{
	NS_ENSURE_ARG_POINTER(aURI);

	nsCAutoString spec;
	aURI->GetSpec(spec);

	*_retval = ephy_history_is_page_visited (mGlobalHistory, spec.get());
	
	return NS_OK;
}

/* void setPageTitle (in nsIURI aURI, in AString aTitle); */
NS_IMETHODIMP MozGlobalHistory::SetPageTitle(nsIURI *aURI, const nsAString & aTitle)
{
	NS_ENSURE_ARG_POINTER(aURI);

	const nsACString &title = NS_ConvertUTF16toUTF8(aTitle);

	nsCAutoString spec;
	aURI->GetSpec(spec);
	
	ephy_history_set_page_title (mGlobalHistory, spec.get(),
				     PromiseFlatCString(title).get());
	
	return NS_OK;
}

/* void hidePage (in nsIURI url); */
NS_IMETHODIMP MozGlobalHistory::HidePage(nsIURI *url)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

#else

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

NS_IMETHODIMP MozGlobalHistory::HidePage(const char *url)
{
	return NS_ERROR_NOT_IMPLEMENTED;

}
#endif /* MOZILLA_SNAPSHOT > 13 */

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

#if MOZILLA_SNAPSHOT > 14
/* readonly attribute AUTF8String lastPageVisited; */
NS_IMETHODIMP MozGlobalHistory::GetLastPageVisited(nsACString & aLastPageVisited)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}
#else
/* readonly attribute string lastPageVisited; */
NS_IMETHODIMP MozGlobalHistory::GetLastPageVisited(char **aLastPageVisited)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}
#endif

#if MOZILLA_SNAPSHOT > 8 && MOZILLA_SNAPSHOT < 14
NS_IMETHODIMP MozGlobalHistory::SetLastPageVisited(const char *aLastPageVisited)
{
        return NS_ERROR_NOT_IMPLEMENTED;
}
#endif

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
