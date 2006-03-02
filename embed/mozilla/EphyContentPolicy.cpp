/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Copyright (C) 2003 Christian Persch
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

#include "EphyContentPolicy.h"
#include "EphyUtils.h"

#include "ephy-embed-shell.h"
#include "ephy-embed-single.h"
#include "eel-gconf-extensions.h"
#include "ephy-adblock-manager.h"
#include "ephy-debug.h"

#include <nsCOMPtr.h>
#undef MOZILLA_INTERNAL_API
#include <nsEmbedString.h>
#define MOZILLA_INTERNAL_API 1
#include <nsIDOMAbstractView.h>
#include <nsIDOMDocument.h>
#include <nsIDOMDocumentView.h>
#include <nsIDOMNode.h>
#include <nsIDOMWindow.h>
#include <nsIURI.h>

#define CONF_LOCKDOWN_DISABLE_UNSAFE_PROTOCOLS	"/apps/epiphany/lockdown/disable_unsafe_protocols"
#define CONF_LOCKDOWN_ADDITIONAL_SAFE_PROTOCOLS	"/apps/epiphany/lockdown/additional_safe_protocols"

NS_IMPL_ISUPPORTS1(EphyContentPolicy, nsIContentPolicy)

EphyContentPolicy::EphyContentPolicy()
{
	LOG ("EphyContentPolicy ctor (%p)", this);

	mLocked = eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_UNSAFE_PROTOCOLS);

	mSafeProtocols = eel_gconf_get_string_list (CONF_LOCKDOWN_ADDITIONAL_SAFE_PROTOCOLS);
}

EphyContentPolicy::~EphyContentPolicy()
{
	LOG ("EphyContentPolicy dtor (%p)", this);

	g_slist_foreach (mSafeProtocols, (GFunc) g_free, NULL);
	g_slist_free (mSafeProtocols);
}

GtkWidget *
EphyContentPolicy::GetEmbedFromContext (nsISupports *aContext)
{
	/*
	 * aContext is either an nsIDOMWindow, an nsIDOMNode, or NULL. If it's
	 * an nsIDOMNode, we need the nsIDOMWindow to get the EphyEmbed.
	 */
	if (aContext == NULL) return NULL;

	nsCOMPtr<nsIDOMWindow> window;

	nsCOMPtr<nsIDOMNode> node (do_QueryInterface (aContext));
	if (node != NULL)
	{
		nsCOMPtr<nsIDOMDocument> domDocument;

		node->GetOwnerDocument (getter_AddRefs (domDocument));
		if (domDocument == NULL) return NULL; /* resource://... */

		nsCOMPtr<nsIDOMDocumentView> docView = 
			do_QueryInterface (domDocument);
		NS_ENSURE_TRUE (docView, NULL);

		nsCOMPtr<nsIDOMAbstractView> view;
		
		docView->GetDefaultView (getter_AddRefs (view));

		window = do_QueryInterface (view);
	}
	else
	{
		window = do_QueryInterface (aContext);
	}
	NS_ENSURE_TRUE (window, NULL);

	GtkWidget *embed = EphyUtils::FindEmbed (window);
	if (!EPHY_IS_EMBED (embed)) NULL;

	return embed;
}

#if MOZ_NSICONTENTPOLICY_VARIANT == 2
NS_IMETHODIMP
EphyContentPolicy::ShouldLoad(PRUint32 aContentType,
			      nsIURI *aContentLocation,
			      nsIURI *aRequestingLocation,
			      nsISupports *aContext,
			      const nsACString &aMimeTypeGuess,
			      nsISupports *aExtra,
			      PRInt16 *aDecision)
{
	NS_ENSURE_ARG (aContentLocation);
	NS_ENSURE_ARG_POINTER (aDecision);

	*aDecision = nsIContentPolicy::ACCEPT;

	/* We have to always allow these, else forms and scrollbars break */
	PRBool isChrome = PR_FALSE, isResource = PR_FALSE;
	aContentLocation->SchemeIs ("chrome", &isChrome);
	aContentLocation->SchemeIs ("resource", &isResource);
	if (isChrome || isResource) return NS_OK;

	PRBool isHttps = PR_FALSE;
	aContentLocation->SchemeIs ("https", &isHttps);
	if (isHttps) return NS_OK;

	/* is this url allowed ? */
	nsEmbedCString spec;
	aContentLocation->GetSpec(spec);

	EphyAdBlockManager *adblock_manager = 
		EPHY_ADBLOCK_MANAGER (ephy_embed_shell_get_adblock_manager (embed_shell));

	static PRBool kBlockType[nsIContentPolicy::TYPE_REFRESH + 1] = {
		PR_FALSE /* unused/unknown, don't block */,
		PR_TRUE  /* TYPE_OTHER */,
		PR_TRUE  /* TYPE_SCRIPT */,
		PR_TRUE  /* TYPE_IMAGE */,
		PR_FALSE /* TYPE_STYLESHEET */,
		PR_TRUE  /* TYPE_OBJECT */,
		PR_FALSE /* TYPE_DOCUMENT */,
		PR_TRUE  /* TYPE_SUBDOCUMENT */,
		PR_TRUE  /* TYPE_REFRESH */
	};

	if (kBlockType[aContentType < G_N_ELEMENTS (kBlockType) ? aContentType : 0] &&
	    !ephy_adblock_manager_should_load (adblock_manager, spec.get (), AdUriCheckType (aContentType)))
	{
		*aDecision = nsIContentPolicy::REJECT_REQUEST;

		GtkWidget *embed = GetEmbedFromContext (aContext); 

		if (embed) 
		{
			g_signal_emit_by_name (embed, "content-blocked", spec.get ());
		}
		return NS_OK;
	}

	PRBool isHttp = PR_FALSE;
	aContentLocation->SchemeIs ("http", &isHttp);
	if (isHttp) return NS_OK;

	nsEmbedCString contentSpec;
	aContentLocation->GetSpec (contentSpec);
	if (strcmp (contentSpec.get(), "about:blank") == 0) return NS_OK;

	nsEmbedCString contentScheme;
	aContentLocation->GetScheme (contentScheme);

	/* first general lockdown check */
	if (mLocked &&
	    !g_slist_find_custom (mSafeProtocols, contentScheme.get(), (GCompareFunc) strcmp))
	{
		*aDecision = nsIContentPolicy::REJECT_REQUEST;
	}

	return NS_OK;
}

NS_IMETHODIMP
EphyContentPolicy::ShouldProcess(PRUint32 aContentType,
			         nsIURI *aContentLocation,
			         nsIURI *aRequestingLocation,
				 nsISupports *aContext,
			         const nsACString &aMimeType,
			         nsISupports *aExtra,
			         PRInt16 *aDecision)
{
	*aDecision = nsIContentPolicy::ACCEPT;
	return NS_OK;
}

#else

/* boolean shouldLoad (in PRInt32 contentType, in nsIURI contentLocation, in nsISupports ctxt, in nsIDOMWindow window); */
NS_IMETHODIMP EphyContentPolicy::ShouldLoad(PRInt32 aContentType,
					    nsIURI *aContentLocation,
					    nsISupports *aContext,
					    nsIDOMWindow *aWindow,
					    PRBool *_retval)
{
	NS_ENSURE_ARG (aContentLocation);
	NS_ENSURE_ARG_POINTER (_retval);

	*_retval = PR_TRUE;

	PRBool isHttp = PR_FALSE, isHttps = PR_FALSE;
	aContentLocation->SchemeIs ("http", &isHttp);
	aContentLocation->SchemeIs ("https", &isHttps);
	if (isHttp || isHttps) return NS_OK;

	/* We have to always allow these, else forms and scrollbars break */
	PRBool isChrome = PR_FALSE, isResource = PR_FALSE;
	aContentLocation->SchemeIs ("chrome", &isChrome);
	aContentLocation->SchemeIs ("resource", &isResource);
	if (isChrome || isResource) return NS_OK;

	nsEmbedCString contentSpec;
	aContentLocation->GetSpec (contentSpec);
	if (strcmp (contentSpec.get(), "about:blank") == 0) return NS_OK;

	nsEmbedCString contentScheme;
	aContentLocation->GetScheme (contentScheme);

	/* first general lockdown check */
	if (mLocked &&
	    !g_slist_find_custom (mSafeProtocols, contentScheme.get(), (GCompareFunc) strcmp))
	{
		*_retval = PR_FALSE;
	}
	return NS_OK;
}

/* boolean shouldProcess (in PRInt32 contentType, in nsIURI documentLocation, in nsISupports ctxt, in nsIDOMWindow window); */
NS_IMETHODIMP EphyContentPolicy::ShouldProcess(PRInt32 contentType,
					       nsIURI *documentLocation,
					       nsISupports *ctxt,
					       nsIDOMWindow *window,
					       PRBool *_retval)
{
	/* This is never called. */
	LOG ("ShouldProcess: this is quite unexpected!");

	*_retval = PR_TRUE;
	return NS_OK;
}
#endif /* MOZ_NSICONTENTPOLICY_VARIANT == 2 */
