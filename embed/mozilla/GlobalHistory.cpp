/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 2001, 2004 Philip Langdale
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

#include <nsIURI.h>

#include "ephy-embed-shell.h"

#include "GlobalHistory.h"


#define MAX_TITLE_LENGTH	2048
#define MAX_URL_LENGTH		16384

#ifdef HAVE_NSIGLOBALHISTORY3_H
NS_IMPL_ISUPPORTS2 (MozGlobalHistory, nsIGlobalHistory2, nsIGlobalHistory3)
#else
NS_IMPL_ISUPPORTS1 (MozGlobalHistory, nsIGlobalHistory2)
#endif /* HAVE_NSIGLOBALHISTORY3_H */

MozGlobalHistory::MozGlobalHistory ()
{
	mGlobalHistory = EPHY_HISTORY (ephy_embed_shell_get_global_history (embed_shell));

	mHistoryListener = new EphyHistoryListener ();
	mHistoryListener->Init (mGlobalHistory);
}

MozGlobalHistory::~MozGlobalHistory ()
{
}

/* void addURI (in nsIURI aURI, in boolean aRedirect, in boolean aToplevel, in nsIURI aReferrer); */
NS_IMETHODIMP MozGlobalHistory::AddURI(nsIURI *aURI,
				       PRBool aRedirect,
				       PRBool aToplevel,
				       nsIURI *aReferrer)
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
		static const char *schemes[] = { "javascript",
						 "data",
						 "about",
						 "chrome",
						 "resource",
						 "view-source" };

		for (PRUint32 i = 0; i < G_N_ELEMENTS (schemes); ++i)
		{
			PRBool result = PR_FALSE;
			if (NS_SUCCEEDED (aURI->SchemeIs (schemes[i], &result)) && result)
			{
				return NS_OK;
			}
		}
	}

	nsCString spec;
	rv = aURI->GetSpec(spec);
	NS_ENSURE_TRUE (NS_SUCCEEDED(rv) && spec.Length(), rv);

	if (spec.Length () > MAX_URL_LENGTH) return NS_OK;

	ephy_history_add_page (mGlobalHistory, spec.get(), aRedirect, aToplevel);
	
	return NS_OK;
}

/* boolean isVisited (in nsIURI aURI); */
NS_IMETHODIMP MozGlobalHistory::IsVisited(nsIURI *aURI,
					  PRBool *_retval)
{
	NS_ENSURE_ARG (aURI);

	*_retval = PR_FALSE;

	nsCString spec;
	aURI->GetSpec(spec);

	if (spec.Length () > MAX_URL_LENGTH) return NS_OK;

	*_retval = ephy_history_is_page_visited (mGlobalHistory, spec.get());
	
	return NS_OK;
}

/* void setPageTitle (in nsIURI aURI, in AString aTitle); */
NS_IMETHODIMP MozGlobalHistory::SetPageTitle(nsIURI *aURI,
					     const nsAString & aTitle)
{
	NS_ENSURE_ARG (aURI);

	nsCString spec;
	aURI->GetSpec(spec);

	if (spec.Length () > MAX_URL_LENGTH) return NS_OK;

	nsString uTitle (aTitle);

	/* This depends on the assumption that 
	 * typeof(PRUnichar) == typeof (gunichar2) == uint16,
	 * which should be pretty safe.
	 */
	glong n_read = 0, n_written = 0;
	char *converted = g_utf16_to_utf8 ((gunichar2*) uTitle.get(), MAX_TITLE_LENGTH,
					    &n_read, &n_written, NULL);
	/* FIXME loop from the end while !g_unichar_isspace (char)? */
	if (converted == NULL) return NS_OK;

	ephy_history_set_page_title (mGlobalHistory, spec.get(), converted);

	g_free (converted);

	return NS_OK;
}

#ifdef HAVE_NSIGLOBALHISTORY3_H

#ifdef HAVE_GECKO_1_9

/* unsigned long getURIGeckoFlags(in nsIURI aURI); */
NS_IMETHODIMP
MozGlobalHistory::GetURIGeckoFlags(nsIURI *aURI,
				   PRUint32* aFlags)
{
	*aFlags = 0;

	nsCString spec;
	aURI->GetSpec(spec);

	if (spec.Length () > MAX_URL_LENGTH) return NS_OK;

	EphyNode *page = ephy_history_get_page (mGlobalHistory, spec.get());

	GValue value = { 0, };
	if (page != NULL &&
	    ephy_node_get_property (page, EPHY_NODE_PAGE_PROP_GECKO_FLAGS, &value))
	{
		*aFlags = (PRUint32) (gulong) g_value_get_long (&value);
		g_value_unset (&value);

		return NS_OK;
	}

	return NS_ERROR_FAILURE;
}

/* void setURIGeckoFlags(in nsIURI aURI, in unsigned long aFlags); */
NS_IMETHODIMP
MozGlobalHistory::SetURIGeckoFlags(nsIURI *aURI,
				   PRUint32 aFlags)
{
	nsCString spec;
	aURI->GetSpec(spec);

	if (spec.Length () > MAX_URL_LENGTH) return NS_OK;

	EphyNode *page = ephy_history_get_page (mGlobalHistory, spec.get());
	if (page != NULL)
	{
		ephy_node_set_property_long (page,
					     EPHY_NODE_PAGE_PROP_GECKO_FLAGS,
					     aFlags);
		return NS_OK;
	}

	return NS_ERROR_FAILURE;
}

#endif /* HAVE_GECKO_1_9 */

/* void addDocumentRedirect (in nsIChannel 
   		             aOldChannel, 
			     in nsIChannel aNewChannel, 
			     in PRInt32 aFlags, 
			     in boolean aTopLevel); */
NS_IMETHODIMP 
MozGlobalHistory::AddDocumentRedirect(nsIChannel *aOldChannel, 
				      nsIChannel *aNewChannel, 
				      PRInt32 aFlags, 
				      PRBool aTopLevel)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

#endif /* HAVE_NSIGLOBALHISTORY3_H */
