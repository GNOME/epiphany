/*
 *  Copyright (C) 2000-2004 Marco Pesenti Gritti
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

#include "EphyBrowser.h"
#include "GlobalHistory.h"
#include "ContentHandler.h"
#include "ephy-embed.h"
#include "ephy-string.h"
#include "ephy-debug.h"

#include <gtkmozembed_internal.h>
#include <unistd.h>

#include "nsIInterfaceRequestorUtils.h"
#include "nsIURI.h"
#include "nsISimpleEnumerator.h"

#include "nsIContentViewer.h"
#include "nsIGlobalHistory.h"
#include "nsIWebBrowserFind.h"
#include "nsIWebBrowserFocus.h"
#include "nsICommandManager.h"
#include "nsIWebBrowserPrint.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDocumentCharsetInfo.h"
#include "nsIWebPageDescriptor.h"
#include "nsISHEntry.h"
#include "nsIHistoryEntry.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"

#ifdef ALLOW_PRIVATE_API
#include "nsPIDOMWindow.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIChromeEventHandler.h"
#include "nsIDOMWindowInternal.h"
#endif

#ifdef ALLOW_PRIVATE_STRINGS
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIDocument.h"
#include "nsIDeviceContext.h"
#include "nsIPresContext.h"
#include "nsIAtom.h"
#endif

EphyEventListener::EphyEventListener(void)
{
	LOG ("EphyEventListener ctor (%p)", this)
	mOwner = nsnull;
}

EphyEventListener::~EphyEventListener()
{
	LOG ("EphyEventListener dtor (%p)", this)
}

NS_IMPL_ISUPPORTS1(EphyEventListener, nsIDOMEventListener)

nsresult
EphyEventListener::Init(EphyEmbed *aOwner)
{
	mOwner = aOwner;
	return NS_OK;
}

nsresult
EphyFaviconEventListener::HandleFaviconLink (nsIDOMNode *node)
{
	nsresult result;

	nsCOMPtr<nsIDOMElement> linkElement;
	linkElement = do_QueryInterface (node);
	if (!linkElement) return NS_ERROR_FAILURE;

	NS_NAMED_LITERAL_STRING(attr_rel, "rel");
	nsAutoString value;
	result = linkElement->GetAttribute (attr_rel, value);
	if (NS_FAILED (result)) return NS_ERROR_FAILURE;

	if (value.EqualsIgnoreCase("SHORTCUT ICON") ||
	    value.EqualsIgnoreCase("ICON"))
	{
		NS_NAMED_LITERAL_STRING(attr_href, "href");
		nsAutoString value;
		result = linkElement->GetAttribute (attr_href, value);
		if (NS_FAILED (result) || value.IsEmpty()) return NS_ERROR_FAILURE;

		nsCOMPtr<nsIDOMDocument> domDoc;
		node->GetOwnerDocument(getter_AddRefs(domDoc));
		NS_ENSURE_TRUE (domDoc, NS_ERROR_FAILURE);

		nsCOMPtr<nsIDocument> doc = do_QueryInterface (domDoc);
		NS_ENSURE_TRUE (doc, NS_ERROR_FAILURE);

#if MOZILLA_SNAPSHOT > 13
		nsIURI *uri;
		uri = doc->GetDocumentURI ();
#elif MOZILLA_SNAPSHOT > 11
		nsIURI *uri;
		uri = doc->GetDocumentURL ();
#endif
		if (!uri) return NS_ERROR_FAILURE;

		const nsACString &link = NS_ConvertUTF16toUTF8(value);
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
EphyFaviconEventListener::HandleEvent(nsIDOMEvent* aDOMEvent)
{
	nsCOMPtr<nsIDOMEventTarget> eventTarget;

	aDOMEvent->GetTarget(getter_AddRefs(eventTarget));

	nsCOMPtr<nsIDOMNode> node = do_QueryInterface(eventTarget);
	NS_ENSURE_TRUE (node, NS_ERROR_FAILURE);

	HandleFaviconLink (node);

	return NS_OK;
}

EphyBrowser::EphyBrowser ()
{
	mFaviconEventListener = nsnull;
	mEventReceiver = nsnull;
	mInitialized = PR_FALSE;
}

EphyBrowser::~EphyBrowser ()
{
}

nsresult EphyBrowser::Init (GtkMozEmbed *mozembed)
{
	nsresult rv;

	if (mInitialized) return NS_OK;

	gtk_moz_embed_get_nsIWebBrowser (mozembed,
					 getter_AddRefs(mWebBrowser));
	NS_ENSURE_TRUE (mWebBrowser, NS_ERROR_FAILURE);

	mWebBrowser->GetContentDOMWindow (getter_AddRefs (mDOMWindow));
	NS_ENSURE_TRUE (mDOMWindow, NS_ERROR_FAILURE);

	/* This will instantiate an about:blank doc if necessary */
	nsCOMPtr<nsIDOMDocument> domDocument;
	rv = mDOMWindow->GetDocument (getter_AddRefs (domDocument));
	NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

	mFaviconEventListener = new EphyFaviconEventListener();
	if (!mFaviconEventListener) return NS_ERROR_OUT_OF_MEMORY;

	rv = mFaviconEventListener->Init (EPHY_EMBED (mozembed));
	NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

 	rv = GetListener();
	NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

	mInitialized = PR_TRUE;

	return AttachListeners();
}

nsresult
EphyBrowser::GetListener (void)
{
  	if (mEventReceiver) return NS_ERROR_FAILURE;

  	nsCOMPtr<nsIDOMWindow> domWindowExternal;
  	mWebBrowser->GetContentDOMWindow(getter_AddRefs(domWindowExternal));
  
  	nsCOMPtr<nsIDOMWindowInternal> domWindow;
        domWindow = do_QueryInterface(domWindowExternal);
	
	nsCOMPtr<nsPIDOMWindow> piWin(do_QueryInterface(domWindow));
	NS_ENSURE_TRUE (piWin, NS_ERROR_FAILURE);

  	nsCOMPtr<nsIChromeEventHandler> chromeHandler;
  	piWin->GetChromeEventHandler(getter_AddRefs(chromeHandler));

  	mEventReceiver = do_QueryInterface(chromeHandler);
	NS_ENSURE_TRUE (mEventReceiver, NS_ERROR_FAILURE);

	return NS_OK;
}

nsresult
EphyBrowser::AttachListeners(void)
{
	nsresult rv;

	NS_ENSURE_TRUE (mEventReceiver, NS_ERROR_FAILURE);

	nsCOMPtr<nsIDOMEventTarget> target;
	target = do_QueryInterface (mEventReceiver);

	rv = target->AddEventListener(NS_LITERAL_STRING("DOMLinkAdded"),
				      mFaviconEventListener, PR_FALSE);
	NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

	return NS_OK;
}

nsresult
EphyBrowser::DetachListeners(void)
{
	nsresult rv;

	NS_ENSURE_TRUE (mEventReceiver, NS_ERROR_FAILURE);
	
	nsCOMPtr<nsIDOMEventTarget> target;
	target = do_QueryInterface (mEventReceiver);

	rv = target->RemoveEventListener(NS_LITERAL_STRING("DOMLinkAdded"),
					 mFaviconEventListener, PR_FALSE);
	NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

	return NS_OK;
}

nsresult EphyBrowser::Print (nsIPrintSettings *options, PRBool preview)
{
	nsresult result;

	NS_ENSURE_TRUE (mWebBrowser, NS_ERROR_FAILURE);

	nsCOMPtr<nsIWebBrowserPrint> print(do_GetInterface(mWebBrowser));
	NS_ENSURE_TRUE (print, NS_ERROR_FAILURE);

	if (!preview)
	{
		result = print->Print (options, nsnull);
	}
	else
	{
		result = print->PrintPreview(options, nsnull, nsnull);
	}

	return result;
}

nsresult EphyBrowser::PrintPreviewClose (void)
{
	nsresult rv;
	PRBool isPreview = PR_FALSE;

	NS_ENSURE_TRUE (mWebBrowser, NS_ERROR_FAILURE);

	nsCOMPtr<nsIWebBrowserPrint> print(do_GetInterface(mWebBrowser));
	NS_ENSURE_TRUE (print, NS_ERROR_FAILURE);

	rv = print->GetDoingPrintPreview(&isPreview);
	NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

	if (isPreview == PR_TRUE)
	{
		rv = print->ExitPrintPreview();
	}

	return rv;
}

nsresult EphyBrowser::PrintPreviewNumPages (int *numPages)
{
	NS_ENSURE_TRUE (mWebBrowser, NS_ERROR_FAILURE);

	nsCOMPtr<nsIWebBrowserPrint> print(do_GetInterface(mWebBrowser));
	NS_ENSURE_TRUE (print, NS_ERROR_FAILURE);

	return print->GetPrintPreviewNumPages(numPages);
}

nsresult EphyBrowser::PrintPreviewNavigate(PRInt16 navType, PRInt32 pageNum)
{
	NS_ENSURE_TRUE (mWebBrowser, NS_ERROR_FAILURE);

	nsCOMPtr<nsIWebBrowserPrint> print(do_GetInterface(mWebBrowser));
	NS_ENSURE_TRUE (print, NS_ERROR_FAILURE);

	return print->PrintPreviewNavigate(navType, pageNum);
}

nsresult EphyBrowser::GetPrintSettings (nsIPrintSettings **options)
{
	NS_ENSURE_TRUE (mWebBrowser, NS_ERROR_FAILURE);

	nsCOMPtr<nsIWebBrowserPrint> print(do_GetInterface(mWebBrowser));
	NS_ENSURE_TRUE (print, NS_ERROR_FAILURE);

	return print->GetGlobalPrintSettings(options);
}

nsresult EphyBrowser::GetSHistory (nsISHistory **aSHistory)
{
	nsresult result;

	NS_ENSURE_TRUE (mWebBrowser, NS_ERROR_FAILURE);

	nsCOMPtr<nsIWebNavigation> ContentNav = do_QueryInterface (mWebBrowser);
	NS_ENSURE_TRUE (ContentNav, NS_ERROR_FAILURE);

	nsCOMPtr<nsISHistory> SessionHistory;
	result = ContentNav->GetSessionHistory (getter_AddRefs (SessionHistory));
	NS_ENSURE_TRUE (SessionHistory, NS_ERROR_FAILURE);

	*aSHistory = SessionHistory.get();
	NS_IF_ADDREF (*aSHistory);

	return NS_OK;
}

nsresult EphyBrowser::Destroy ()
{
	DetachListeners ();

      	mWebBrowser = nsnull;
	
	return NS_OK;
}

nsresult EphyBrowser::GoToHistoryIndex (PRInt16 index)
{
	NS_ENSURE_TRUE (mWebBrowser, NS_ERROR_FAILURE);

	nsCOMPtr<nsIWebNavigation> ContentNav = do_QueryInterface (mWebBrowser);
	NS_ENSURE_TRUE (ContentNav, NS_ERROR_FAILURE);

	return ContentNav->GotoIndex (index);
}

nsresult EphyBrowser::SetZoom (float aZoom, PRBool reflow)
{
	NS_ENSURE_TRUE (mWebBrowser, NS_ERROR_FAILURE);

	if (reflow)
	{
		nsCOMPtr<nsIContentViewer> contentViewer;	
		GetContentViewer (getter_AddRefs(contentViewer));
		NS_ENSURE_TRUE (contentViewer, NS_ERROR_FAILURE);

		nsCOMPtr<nsIMarkupDocumentViewer> mdv = do_QueryInterface(contentViewer);
		NS_ENSURE_TRUE (mdv, NS_ERROR_FAILURE);

		return mdv->SetTextZoom (aZoom);
	}
	else
	{
		nsCOMPtr<nsIDocShell> DocShell;
		DocShell = do_GetInterface (mWebBrowser);
		NS_ENSURE_TRUE (DocShell, NS_ERROR_FAILURE);

		SetZoomOnDocshell (aZoom, DocShell);

		nsCOMPtr<nsIDocShellTreeNode> docShellNode(do_QueryInterface(DocShell));
		if (docShellNode)
		{
			PRInt32 i;
			PRInt32 n;
			docShellNode->GetChildCount(&n);
			for (i=0; i < n; i++) 
			{
				nsCOMPtr<nsIDocShellTreeItem> child;
				docShellNode->GetChildAt(i, getter_AddRefs(child));
				nsCOMPtr<nsIDocShell> childAsShell(do_QueryInterface(child));
				if (childAsShell) 
				{
					return SetZoomOnDocshell (aZoom, childAsShell);
				}
			}
		}
	}

	return NS_OK;
}

nsresult EphyBrowser::SetZoomOnDocshell (float aZoom, nsIDocShell *DocShell)
{
	nsCOMPtr<nsIPresContext> PresContext;
	DocShell->GetPresContext (getter_AddRefs(PresContext));
	NS_ENSURE_TRUE (PresContext, NS_ERROR_FAILURE);

#if MOZILLA_SNAPSHOT > 13
	nsIDeviceContext *DeviceContext;
	DeviceContext = PresContext->DeviceContext();
	NS_ENSURE_TRUE (DeviceContext, NS_ERROR_FAILURE);
#else				
	nsCOMPtr<nsIDeviceContext> DeviceContext;
	PresContext->GetDeviceContext (getter_AddRefs(DeviceContext));
	NS_ENSURE_TRUE (DeviceContext, NS_ERROR_FAILURE);
#endif

	return DeviceContext->SetTextZoom (aZoom);
}

nsresult EphyBrowser::GetContentViewer (nsIContentViewer **aViewer)
{
	NS_ENSURE_TRUE (mWebBrowser, NS_ERROR_FAILURE);

	nsCOMPtr<nsIDocShell> ourDocShell(do_GetInterface(mWebBrowser));
	NS_ENSURE_TRUE (ourDocShell, NS_ERROR_FAILURE);

	return ourDocShell->GetContentViewer(aViewer);
}

nsresult EphyBrowser::GetZoom (float *aZoom)
{
	nsCOMPtr<nsIContentViewer> contentViewer;	
	GetContentViewer (getter_AddRefs(contentViewer));
	NS_ENSURE_TRUE (contentViewer, NS_ERROR_FAILURE);

	nsCOMPtr<nsIMarkupDocumentViewer> mdv = do_QueryInterface(contentViewer);
	NS_ENSURE_TRUE (mdv, NS_ERROR_FAILURE);

	return mdv->GetTextZoom (aZoom);
}

nsresult EphyBrowser::GetDocument (nsIDOMDocument **aDOMDocument)
{
	return mDOMWindow->GetDocument (aDOMDocument);
}

nsresult EphyBrowser::GetTargetDocument (nsIDOMDocument **aDOMDocument)
{
	nsresult result;

	NS_ENSURE_TRUE (mWebBrowser, NS_ERROR_FAILURE);

	/* Use the current target document */
	if (mTargetDocument)
	{
		*aDOMDocument = mTargetDocument.get();

		NS_IF_ADDREF(*aDOMDocument);

		return NS_OK;
	}

	/* Use the focused document */
	nsCOMPtr<nsIWebBrowserFocus> webBrowserFocus;
	webBrowserFocus = do_QueryInterface (mWebBrowser);
	NS_ENSURE_TRUE (webBrowserFocus, NS_ERROR_FAILURE);

	nsCOMPtr<nsIDOMWindow> DOMWindow;
	result = webBrowserFocus->GetFocusedWindow (getter_AddRefs(DOMWindow));
	if (NS_SUCCEEDED(result) && DOMWindow)
	{
		return DOMWindow->GetDocument (aDOMDocument);
	}

	/* Use the main document */
	return mDOMWindow->GetDocument (aDOMDocument);
}

nsresult EphyBrowser::GetSHInfo (PRInt32 *count, PRInt32 *index)
{
	nsCOMPtr<nsISHistory> SessionHistory;
	GetSHistory (getter_AddRefs(SessionHistory));
	NS_ENSURE_TRUE (SessionHistory, NS_ERROR_FAILURE);

	SessionHistory->GetCount (count);
	SessionHistory->GetIndex (index);	

	return NS_OK;
}

nsresult EphyBrowser::GetSHTitleAtIndex (PRInt32 index, PRUnichar **title)
{
	nsCOMPtr<nsISHistory> SessionHistory;
	GetSHistory (getter_AddRefs(SessionHistory));
	NS_ENSURE_TRUE (SessionHistory, NS_ERROR_FAILURE);

	nsCOMPtr<nsIHistoryEntry> he;
	SessionHistory->GetEntryAtIndex (index, PR_FALSE,
					 getter_AddRefs (he));
	NS_ENSURE_TRUE (he, NS_ERROR_FAILURE);

	nsresult result;
	result = he->GetTitle (title);
	NS_ENSURE_TRUE (NS_SUCCEEDED (result) && title, NS_ERROR_FAILURE);

	return NS_OK;
}

nsresult EphyBrowser::GetSHUrlAtIndex (PRInt32 index, nsCString &url)
{
	nsCOMPtr<nsISHistory> SessionHistory;
	GetSHistory (getter_AddRefs(SessionHistory));
	NS_ENSURE_TRUE (SessionHistory, NS_ERROR_FAILURE);

	nsCOMPtr<nsIHistoryEntry> he;
	SessionHistory->GetEntryAtIndex (index, PR_FALSE,
					 getter_AddRefs (he));
	NS_ENSURE_TRUE (he, NS_ERROR_FAILURE);

	nsCOMPtr<nsIURI> uri;
	he->GetURI (getter_AddRefs(uri));
	NS_ENSURE_TRUE (uri, NS_ERROR_FAILURE);

	nsresult result;
	result = uri->GetSpec(url);
	NS_ENSURE_TRUE (NS_SUCCEEDED (result) && !url.IsEmpty(), NS_ERROR_FAILURE);

	return NS_OK;
}

nsresult EphyBrowser::FindSetProperties (const PRUnichar *search_string,
			                 PRBool case_sensitive,
					 PRBool wrap_around)
{
	NS_ENSURE_TRUE (mWebBrowser, NS_ERROR_FAILURE);

	nsCOMPtr<nsIWebBrowserFind> finder (do_GetInterface(mWebBrowser));
	NS_ENSURE_TRUE (finder, NS_ERROR_FAILURE);

	finder->SetSearchString (search_string);
	finder->SetMatchCase (case_sensitive);
	finder->SetWrapFind (wrap_around);

	return NS_OK;
}

nsresult EphyBrowser::Find (PRBool backwards,
			    PRBool *didFind)
{
	NS_ENSURE_TRUE (mWebBrowser, NS_ERROR_FAILURE);

	nsCOMPtr<nsIWebBrowserFind> finder (do_GetInterface(mWebBrowser));
	NS_ENSURE_TRUE (finder, NS_ERROR_FAILURE);

	finder->SetFindBackwards (backwards);

	return finder->FindNext(didFind);
}

nsresult EphyBrowser::GetPageDescriptor(nsISupports **aPageDescriptor)
{
	NS_ENSURE_TRUE (mWebBrowser, NS_ERROR_FAILURE);

	nsCOMPtr<nsIDocShell> ds = do_GetInterface (mWebBrowser);

	nsCOMPtr<nsIWebPageDescriptor> wpd = do_QueryInterface (ds);
	NS_ENSURE_TRUE (wpd, NS_ERROR_FAILURE);

	*aPageDescriptor = wpd.get();
	NS_IF_ADDREF (*aPageDescriptor);

	return NS_OK;
}

nsresult EphyBrowser::GetDocumentUrl (nsCString &url)
{
	if (!mDOMWindow) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIDOMDocument> DOMDocument;
	mDOMWindow->GetDocument (getter_AddRefs(DOMDocument));
	NS_ENSURE_TRUE (DOMDocument, NS_ERROR_FAILURE);

	nsCOMPtr<nsIDocument> doc = do_QueryInterface(DOMDocument);
	NS_ENSURE_TRUE (doc, NS_ERROR_FAILURE);

#if MOZILLA_SNAPSHOT > 13
	nsIURI *uri;
	uri = doc->GetDocumentURI ();
#elif MOZILLA_SNAPSHOT > 11
	nsIURI *uri;
	uri = doc->GetDocumentURL ();
#endif
	NS_ENSURE_TRUE (uri, NS_ERROR_FAILURE);

	return uri->GetSpec (url);
}

nsresult EphyBrowser::GetTargetDocumentUrl (nsCString &url)
{
        nsCOMPtr<nsIDOMDocument> DOMDocument;
	GetTargetDocument (getter_AddRefs(DOMDocument));
	NS_ENSURE_TRUE (DOMDocument, NS_ERROR_FAILURE);

        nsCOMPtr<nsIDocument> doc = do_QueryInterface(DOMDocument);
	NS_ENSURE_TRUE (doc, NS_ERROR_FAILURE);

#if MOZILLA_SNAPSHOT > 13
	nsIURI *uri;
	uri = doc->GetDocumentURI ();
#elif MOZILLA_SNAPSHOT > 11
	nsIURI *uri;
	uri = doc->GetDocumentURL ();
#endif
	NS_ENSURE_TRUE (uri, NS_ERROR_FAILURE);

	return uri->GetSpec (url);
}

nsresult EphyBrowser::ForceEncoding (const char *encoding) 
{
	nsCOMPtr<nsIContentViewer> contentViewer;	
	GetContentViewer (getter_AddRefs(contentViewer));
	NS_ENSURE_TRUE (contentViewer, NS_ERROR_FAILURE);

	nsCOMPtr<nsIMarkupDocumentViewer> mdv = do_QueryInterface(contentViewer);
	NS_ENSURE_TRUE (mdv, NS_ERROR_FAILURE);

	nsresult result;
	result = mdv->SetForceCharacterSet (nsDependentCString(encoding));

	return result;
}

nsresult EphyBrowser::PushTargetDocument (nsIDOMDocument *domDoc)
{
	mTargetDocument = domDoc;

	return NS_OK;
}

nsresult EphyBrowser::PopTargetDocument ()
{
	mTargetDocument = nsnull;

	return NS_OK;
}

nsresult EphyBrowser::GetEncodingInfo (EphyEncodingInfo **infoptr)
{
	nsresult result;
	EphyEncodingInfo *info;

	nsCOMPtr<nsIDOMDocument> domDoc;
	GetTargetDocument (getter_AddRefs(domDoc));
	NS_ENSURE_TRUE (domDoc, NS_ERROR_FAILURE);

	nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc, &result);
	NS_ENSURE_TRUE (doc, NS_ERROR_FAILURE);

	info = g_new0 (EphyEncodingInfo, 1);
	*infoptr = info;

	PRInt32 source;
	source = doc->GetDocumentCharacterSetSource ();
	info->encoding_source = (EphyEncodingSource) source;

	nsCOMPtr<nsIDocShell> ds;
	ds = do_GetInterface (mWebBrowser);
	NS_ENSURE_TRUE (ds, NS_ERROR_FAILURE);

	nsCOMPtr<nsIDocumentCharsetInfo> ci;
	result = ds->GetDocumentCharsetInfo (getter_AddRefs (ci));
	NS_ENSURE_TRUE (ci, NS_ERROR_FAILURE);

	nsCOMPtr<nsIAtom> atom;
	ci->GetForcedCharset (getter_AddRefs (atom));
	if (atom)
	{
		nsCAutoString atomstr;
		atom->ToUTF8String (atomstr);
		info->forced_encoding = g_strdup (atomstr.get());
	}

	ci->GetParentCharset (getter_AddRefs (atom));
	if (atom)
	{
		nsCAutoString atomstr;
		atom->ToUTF8String (atomstr);
		info->parent_encoding = g_strdup (atomstr.get());
	}

	result = ci->GetParentCharsetSource (&source);
	NS_ENSURE_SUCCESS (result, NS_ERROR_FAILURE);
	info->parent_encoding_source = (EphyEncodingSource) source;

	nsCOMPtr<nsIContentViewer> contentViewer;	
	ds->GetContentViewer (getter_AddRefs(contentViewer));
	NS_ENSURE_TRUE (contentViewer, NS_ERROR_FAILURE);

	nsCOMPtr<nsIMarkupDocumentViewer> mdv = do_QueryInterface(contentViewer);
	NS_ENSURE_TRUE (mdv, NS_ERROR_FAILURE);

	const nsACString& charsetEnc = doc->GetDocumentCharacterSet ();
	NS_ENSURE_TRUE (!charsetEnc.IsEmpty(), NS_ERROR_FAILURE);

	info->encoding = g_strdup (PromiseFlatCString(charsetEnc).get());

	nsCAutoString enc;
	
	result = mdv->GetDefaultCharacterSet (enc);
	NS_ENSURE_SUCCESS (result, NS_ERROR_FAILURE);
	info->default_encoding = g_strdup (enc.get());

	result = mdv->GetForceCharacterSet (enc);
	NS_ENSURE_SUCCESS (result, NS_ERROR_FAILURE);
	info->forced_encoding = g_strdup (enc.get());

	result = mdv->GetHintCharacterSet (enc);
	NS_ENSURE_SUCCESS (result, NS_ERROR_FAILURE);
	info->hint_encoding = g_strdup (enc.get());

	result = mdv->GetPrevDocCharacterSet (enc);
	NS_ENSURE_SUCCESS (result, NS_ERROR_FAILURE);
	info->prev_doc_encoding = g_strdup (enc.get());

	mdv->GetHintCharacterSetSource (&source);
	NS_ENSURE_SUCCESS (result, NS_ERROR_FAILURE);
	info->hint_encoding_source = (EphyEncodingSource) source;

	return NS_OK;
}

nsresult EphyBrowser::DoCommand (const char *command)
{
	nsCOMPtr<nsICommandManager> cmdManager;

	NS_ENSURE_TRUE (mWebBrowser, NS_ERROR_FAILURE);

	cmdManager = do_GetInterface (mWebBrowser);
	NS_ENSURE_TRUE (cmdManager, NS_ERROR_FAILURE);

	return cmdManager->DoCommand (command, nsnull, nsnull);
}

nsresult EphyBrowser::GetCommandState (const char *command, PRBool *enabled)
{
	nsCOMPtr<nsICommandManager> cmdManager;

	NS_ENSURE_TRUE (mWebBrowser, NS_ERROR_FAILURE);

	cmdManager = do_GetInterface (mWebBrowser);
	NS_ENSURE_TRUE (cmdManager, NS_ERROR_FAILURE);

	return cmdManager->IsCommandEnabled (command, nsnull, enabled);
}

#define NUM_MODIFIED_TEXTFIELDS_REQUIRED	2

nsresult EphyBrowser::GetDocumentHasModifiedForms (nsIDOMDocument *aDomDoc, PRUint32 *aNumTextFields, PRBool *aHasTextArea)
{
	nsCOMPtr<nsIDOMHTMLDocument> htmlDoc = do_QueryInterface(aDomDoc);
	/* it's okay not to be a HTML doc (happens for XUL documents, like about:config) */
	if (!htmlDoc) return NS_OK;

	nsCOMPtr<nsIDOMHTMLCollection> forms;
	htmlDoc->GetForms (getter_AddRefs (forms));
	if (!forms) return NS_OK; /* it's ok not to have any forms */

	PRUint32 formNum;
	forms->GetLength (&formNum);

	/* check all forms */
	for (PRUint32 formIndex = 0; formIndex < formNum; formIndex++)
	{
		nsCOMPtr<nsIDOMNode> formNode;
		forms->Item (formIndex, getter_AddRefs (formNode));
		if (!formNode) continue;

		nsCOMPtr<nsIDOMHTMLFormElement> formElement = do_QueryInterface (formNode);
		if (!formElement) continue;

		nsCOMPtr<nsIDOMHTMLCollection> formElements;
		formElement->GetElements (getter_AddRefs (formElements));
		if (!formElements) continue;

		PRUint32 elementNum;
		formElements->GetLength (&elementNum);

		/* check all input elements in the form for user input */
		for (PRUint32 elementIndex = 0; elementIndex < elementNum; elementIndex++)
		{
			nsCOMPtr<nsIDOMNode> domNode;
			formElements->Item (elementIndex, getter_AddRefs (domNode));
			if (!domNode) continue;

			nsCOMPtr<nsIDOMHTMLTextAreaElement> areaElement = do_QueryInterface (domNode);
			if (areaElement)
			{
				nsAutoString default_text, user_text;
				areaElement->GetDefaultValue (default_text);
				areaElement->GetValue (user_text);

				/* Mozilla Bug 218277, 195946 and others */
				default_text.ReplaceChar(0xa0, ' ');

				if (!user_text.Equals (default_text))
				{
					*aHasTextArea = PR_TRUE;
					return NS_OK;
				}

				continue;
			}

			nsCOMPtr<nsIDOMHTMLInputElement> inputElement = do_QueryInterface(domNode);
			if (!inputElement) continue;
	
			nsAutoString type;
			inputElement->GetType(type);

			if (type.EqualsIgnoreCase("text"))
			{
				nsAutoString default_text, user_text;
				PRInt32 max_length;
				inputElement->GetDefaultValue (default_text);
				inputElement->GetValue (user_text);
				inputElement->GetMaxLength (&max_length);

				/* Guard against arguably broken forms where
				 * default_text is longer than maxlength
				 * (user_text is cropped, default_text is not)
				 * Mozilla bug 232057
				 */
				if (default_text.Length() > (PRUint32)max_length)
				{
					default_text.Truncate (max_length);
				}

				/* Mozilla Bug 218277, 195946 and others */
				default_text.ReplaceChar(0xa0, ' ');

				if (!user_text.Equals (default_text))
				{
					(*aNumTextFields)++;
					if (*aNumTextFields >= NUM_MODIFIED_TEXTFIELDS_REQUIRED)
					{
						return NS_OK;
					}
				}
			}
		}
	}

	return NS_OK;
}

nsresult EphyBrowser::GetHasModifiedForms (PRBool *modified)
{
	*modified = PR_FALSE;

	nsCOMPtr<nsIDocShell> rootDocShell = do_GetInterface (mWebBrowser);
	NS_ENSURE_TRUE (rootDocShell, NS_ERROR_FAILURE);

	nsCOMPtr<nsISimpleEnumerator> enumerator;
	rootDocShell->GetDocShellEnumerator(nsIDocShellTreeItem::typeContent,
					    nsIDocShell::ENUMERATE_FORWARDS,
					    getter_AddRefs(enumerator));
	NS_ENSURE_TRUE (enumerator, NS_ERROR_FAILURE);

	PRBool hasMore;
	PRBool hasTextArea = PR_FALSE;
	PRUint32 numTextFields = 0;
	while (NS_SUCCEEDED(enumerator->HasMoreElements(&hasMore)) && hasMore)
	{
		nsCOMPtr<nsISupports> element;
		enumerator->GetNext (getter_AddRefs(element));
		if (!element) continue;

		nsCOMPtr<nsIDocShell> docShell = do_QueryInterface (element);
		if (!docShell) continue;

		nsCOMPtr<nsIContentViewer> contentViewer;
		docShell->GetContentViewer (getter_AddRefs(contentViewer));
		if (!contentViewer) continue;

		nsCOMPtr<nsIDOMDocument> domDoc;
		contentViewer->GetDOMDocument (getter_AddRefs (domDoc));

		nsresult result;
		result = GetDocumentHasModifiedForms (domDoc, &numTextFields, &hasTextArea);
		if (NS_SUCCEEDED (result) &&
		    (numTextFields >= NUM_MODIFIED_TEXTFIELDS_REQUIRED || hasTextArea))
		{
			*modified = PR_TRUE;
			break;
		}
	}

	return NS_OK;
}
