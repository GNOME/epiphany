/*
 *  Copyright (C) 2000 Marco Pesenti Gritti
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

#include <nsCOMPtr.h>

#include "EphyEventListener.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsString.h"
#include "nsUnicharUtils.h"
#include "nsIDOMDocument.h"
#include "nsIURI.h"
#include "nsIDocument.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMEvent.h"

EphyEventListener::EphyEventListener(void)
{
	mOwner = nsnull;
}

EphyEventListener::~EphyEventListener()
{
}

NS_IMPL_ISUPPORTS1(EphyEventListener, nsIDOMEventListener)

nsresult
EphyEventListener::Init(EphyEmbed *aOwner)
{
	mOwner = aOwner;
	return NS_OK;
}

nsresult
EphyEventListener::HandleFaviconLink (nsIDOMNode *node)
{
	nsresult result;

	nsCOMPtr<nsIDOMElement> linkElement;
	linkElement = do_QueryInterface (node);
	if (!linkElement) return NS_ERROR_FAILURE;

	NS_NAMED_LITERAL_STRING(attr_rel, "rel");
	nsAutoString value;
	result = linkElement->GetAttribute (attr_rel, value);
	if (NS_FAILED(result)) return NS_ERROR_FAILURE;

	if (value.Equals(NS_LITERAL_STRING("SHORTCUT ICON"),
			 nsCaseInsensitiveStringComparator()) ||
	    value.Equals(NS_LITERAL_STRING("ICON"),
	    		 nsCaseInsensitiveStringComparator()))
	{
		NS_NAMED_LITERAL_STRING(attr_href, "href");
		nsAutoString value;
		result = linkElement->GetAttribute (attr_href, value);
		if (NS_FAILED (result) || value.IsEmpty())
			return NS_ERROR_FAILURE;

		nsCOMPtr<nsIDOMDocument> domDoc;
		result = node->GetOwnerDocument(getter_AddRefs(domDoc));
		if (NS_FAILED(result) || !domDoc) return NS_ERROR_FAILURE;

		nsCOMPtr<nsIDocument> doc = do_QueryInterface (domDoc);
		if(!doc) return NS_ERROR_FAILURE;

#if MOZILLA_SNAPSHOT > 11
		nsIURI *uri;
		uri = doc->GetDocumentURL ();
		if (uri == NULL) return NS_ERROR_FAILURE;
#else
		nsCOMPtr<nsIURI> uri;
		result = doc->GetDocumentURL(getter_AddRefs(uri));
		if (NS_FAILED (result)) return NS_ERROR_FAILURE;
#endif

		const nsACString &link = NS_ConvertUCS2toUTF8(value);
		nsCAutoString favicon_url;
		result = uri->Resolve (link, favicon_url);
		if (NS_FAILED (result)) return NS_ERROR_FAILURE;
		
		char *url = g_strdup (favicon_url.get());
		g_signal_emit_by_name (mOwner, "ge_favicon", url);
		g_free (url);
	}

	return NS_OK;
}	

NS_IMETHODIMP
EphyEventListener::HandleEvent(nsIDOMEvent* aDOMEvent)
{
	nsCOMPtr<nsIDOMEventTarget> eventTarget;
	
	aDOMEvent->GetTarget(getter_AddRefs(eventTarget));
	
	nsresult result;
	nsCOMPtr<nsIDOMNode> node = do_QueryInterface(eventTarget, &result);
	if (NS_FAILED(result) || !node) return NS_ERROR_FAILURE;

	HandleFaviconLink (node);
	
	return NS_OK;
}
