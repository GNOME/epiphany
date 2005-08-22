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

#include "mozilla-config.h"

#include "config.h"

#include "ephy-embed-shell.h"

#include "GlobalHistory.h"

#include <nsIURI.h>
#undef MOZILLA_INTERNAL_API
#include <nsEmbedString.h>
#define MOZILLA_INTERNAL_API 1

NS_IMPL_ISUPPORTS1 (MozGlobalHistory, nsIGlobalHistory2)

MozGlobalHistory::MozGlobalHistory ()
{
	mGlobalHistory = EPHY_HISTORY (ephy_embed_shell_get_global_history (embed_shell));

	mHistoryListener = new EphyHistoryListener ();
	mHistoryListener->Init (mGlobalHistory);
}

MozGlobalHistory::~MozGlobalHistory ()
{
}

#ifdef HAVE_GECKO_1_8
/* void addURI (in nsIURI aURI, in boolean aRedirect, in boolean aToplevel, in nsIURI aReferrer); */
NS_IMETHODIMP MozGlobalHistory::AddURI(nsIURI *aURI, PRBool aRedirect, PRBool aToplevel, nsIURI *aReferrer)
#else
/* void addURI (in nsIURI aURI, in boolean aRedirect, in boolean aToplevel); */
NS_IMETHODIMP MozGlobalHistory::AddURI(nsIURI *aURI, PRBool aRedirect, PRBool aToplevel)
#endif
{
	nsresult rv;

	NS_ENSURE_ARG (aURI);

	// filter out unwanted URIs such as chrome: etc
	// The model is really if we don't know differently then add which basically
	// means we are suppose to try all the things we know not to allow in and
	// then if we don't bail go on and allow it in.  But here lets compare
	// against the most common case we know to allow in and go on and say yes
	// to it.

	PRBool isHTTP = PR_FALSE, isHTTPS = PR_FALSE;
	rv = aURI->SchemeIs("http", &isHTTP);
	rv |= aURI->SchemeIs("https", &isHTTPS);
	NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

	if (!isHTTP && !isHTTPS)
	{
		PRBool isJavascript, isAbout, isViewSource, isChrome, isData;

	        rv = aURI->SchemeIs("javascript", &isJavascript);
		rv |= aURI->SchemeIs("about", &isAbout);
		rv |= aURI->SchemeIs("view-source", &isViewSource);
		rv |= aURI->SchemeIs("chrome", &isChrome);
		rv |= aURI->SchemeIs("data", &isData);
		NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

		if (isJavascript ||isAbout || isViewSource || isChrome || isData)
		{
		      return NS_OK;
		}
	}

	nsEmbedCString spec;
	rv = aURI->GetSpec(spec);
	NS_ENSURE_TRUE (NS_SUCCEEDED(rv) && spec.Length(), rv);

	ephy_history_add_page (mGlobalHistory, spec.get());
	
	return NS_OK;
}

/* boolean isVisited (in nsIURI aURI); */
NS_IMETHODIMP MozGlobalHistory::IsVisited(nsIURI *aURI, PRBool *_retval)
{
	NS_ENSURE_ARG (aURI);

	nsEmbedCString spec;
	aURI->GetSpec(spec);

	*_retval = ephy_history_is_page_visited (mGlobalHistory, spec.get());
	
	return NS_OK;
}

/* void setPageTitle (in nsIURI aURI, in AString aTitle); */
NS_IMETHODIMP MozGlobalHistory::SetPageTitle(nsIURI *aURI, const nsAString & aTitle)
{
	NS_ENSURE_ARG (aURI);

	nsEmbedCString title;
	NS_UTF16ToCString (nsEmbedString (aTitle),
			   NS_CSTRING_ENCODING_UTF8, title);

	nsEmbedCString spec;
	aURI->GetSpec(spec);
	
	ephy_history_set_page_title (mGlobalHistory, spec.get(), title.get());
	
	return NS_OK;
}
