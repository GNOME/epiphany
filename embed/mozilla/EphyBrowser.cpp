/*
 *  Copyright (C) 2000-2003 Marco Pesenti Gritti
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
#include "ephy-embed.h"
#include "ephy-string.h"

#include <gtkmozembed_internal.h>
#include <unistd.h>

#include "nsICommandManager.h"
#include "nsIContentViewer.h"
#include "nsIGlobalHistory.h"
#include "nsIDocShellHistory.h"
#include "nsIWebBrowserFind.h"
#include "nsIWebBrowserFocus.h"
#include "nsIDocument.h"
#include "nsISHEntry.h"
#include "nsISHistoryInternal.h"
#include "nsIHistoryEntry.h"
#include "nsIWebBrowserPrint.h"
#include "nsIURI.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIComponentManager.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindowInternal.h"
#include "nsIInterfaceRequestor.h"
#include "nsIFocusController.h"
#include "nsIWebBrowserPersist.h"
#include "nsCWebBrowserPersist.h"
#include "nsNetUtil.h"
#include "nsIChromeEventHandler.h"
#include "nsIDOMDocumentStyle.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIHTMLContentContainer.h"
#include "nsICSSLoader.h"
#include "nsICSSStyleSheet.h"
#include "nsICSSLoaderObserver.h"
#include "nsIStyleSet.h"
#include "nsIDocumentObserver.h"
#include "nsCWebBrowser.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsIDOMNSHTMLDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDeviceContext.h"
#include "nsIPresContext.h"
#include "nsIAtom.h"
#include "nsIDocumentCharsetInfo.h"
#include "nsPromiseFlatString.h"
#include "ContentHandler.h"
#include "EphyEventListener.h"

EphyBrowser::EphyBrowser ()
{
	mEventListener = nsnull;
	mEventReceiver = nsnull;	
}

EphyBrowser::~EphyBrowser ()
{
}

nsresult EphyBrowser::Init (GtkMozEmbed *mozembed)
{
	nsresult rv;

	gtk_moz_embed_get_nsIWebBrowser (mozembed,
					 getter_AddRefs(mWebBrowser));
	if (!mWebBrowser) return NS_ERROR_FAILURE;

	mWebBrowser->GetContentDOMWindow (getter_AddRefs (mDOMWindow));

	/* This will instantiate an about:blank doc if necessary */
	nsCOMPtr<nsIDOMDocument> domDocument;
	rv = mDOMWindow->GetDocument (getter_AddRefs (mDOMDocument));
	if (NS_FAILED (rv)) return NS_ERROR_FAILURE;

	mEventListener = new EphyEventListener();

	rv = mEventListener->Init (EPHY_EMBED (mozembed));
	if (NS_FAILED (rv)) return NS_ERROR_FAILURE;

 	rv = GetListener();
	if (NS_FAILED (rv)) return NS_ERROR_FAILURE;

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
  	if (!piWin) return NS_ERROR_FAILURE;

  	nsCOMPtr<nsIChromeEventHandler> chromeHandler;
  	piWin->GetChromeEventHandler(getter_AddRefs(chromeHandler));

  	mEventReceiver = do_QueryInterface(chromeHandler);
	if (!mEventReceiver) return NS_ERROR_FAILURE;

	return NS_OK;
}

nsresult
EphyBrowser::AttachListeners(void)
{
  	if (!mEventReceiver) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIDOMEventTarget> target;
	target = do_QueryInterface (mEventReceiver);

	return target->AddEventListener(NS_LITERAL_STRING("DOMLinkAdded"),
			                mEventListener, PR_FALSE);
}

nsresult
EphyBrowser::DetachListeners(void)
{
	if (!mEventReceiver) return NS_ERROR_FAILURE;
	
	nsCOMPtr<nsIDOMEventTarget> target;
	target = do_QueryInterface (mEventReceiver);

	return target->RemoveEventListener(NS_LITERAL_STRING("DOMLinkAdded"),
					   mEventListener, PR_FALSE);
}

nsresult EphyBrowser::Print (nsIPrintSettings *options, PRBool preview)
{
	nsresult result;

	g_return_val_if_fail (mWebBrowser, NS_ERROR_FAILURE);

	nsCOMPtr<nsIWebBrowserPrint> print(do_GetInterface(mWebBrowser, &result));
	if (NS_FAILED(result) || !print) return NS_ERROR_FAILURE;

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

	g_return_val_if_fail (mWebBrowser, NS_ERROR_FAILURE);

	nsCOMPtr<nsIWebBrowserPrint> print(do_GetInterface(mWebBrowser, &rv));
	if (NS_FAILED(rv) || !print) return NS_ERROR_FAILURE;

	rv = print->GetDoingPrintPreview(&isPreview);
	if (NS_SUCCEEDED (rv) && isPreview == PR_TRUE)
	{
		rv = print->ExitPrintPreview();
	}

	return rv;
}

nsresult EphyBrowser::PrintPreviewNumPages (int *numPages)
{
	nsresult rv;

	g_return_val_if_fail (mWebBrowser, NS_ERROR_FAILURE);

	nsCOMPtr<nsIWebBrowserPrint> print(do_GetInterface(mWebBrowser, &rv));
	if (NS_FAILED(rv) || !print) return NS_ERROR_FAILURE;

	rv = print->GetPrintPreviewNumPages(numPages);
	return rv;
}

nsresult EphyBrowser::PrintPreviewNavigate(PRInt16 navType, PRInt32 pageNum)
{
	nsresult rv;

	g_return_val_if_fail (mWebBrowser, NS_ERROR_FAILURE);

	nsCOMPtr<nsIWebBrowserPrint> print(do_GetInterface(mWebBrowser, &rv));
	if (NS_FAILED(rv) || !print) return NS_ERROR_FAILURE;

	rv = print->PrintPreviewNavigate(navType, pageNum);
	return rv;
}

nsresult EphyBrowser::GetPrintSettings (nsIPrintSettings **options)
{
	nsresult result;

	g_return_val_if_fail (mWebBrowser, NS_ERROR_FAILURE);

	nsCOMPtr<nsIWebBrowserPrint> print(do_GetInterface(mWebBrowser, &result));
	if (NS_FAILED(result) || !print) return NS_ERROR_FAILURE;

	return print->GetGlobalPrintSettings(options);
}

nsresult EphyBrowser::GetSHistory (nsISHistory **aSHistory)
{
	nsresult result;

	g_return_val_if_fail (mWebBrowser, NS_ERROR_FAILURE);

	nsCOMPtr<nsIWebNavigation> ContentNav = do_QueryInterface (mWebBrowser);
	if (!ContentNav) return NS_ERROR_FAILURE;

	nsCOMPtr<nsISHistory> SessionHistory;
	result = ContentNav->GetSessionHistory (getter_AddRefs (SessionHistory));
	if (!SessionHistory) return NS_ERROR_FAILURE;

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
	nsresult result;

	g_return_val_if_fail (mWebBrowser, NS_ERROR_FAILURE);

	nsCOMPtr<nsIWebNavigation> ContentNav = do_QueryInterface (mWebBrowser);
	if (!ContentNav) return NS_ERROR_FAILURE;

	return  ContentNav->GotoIndex (index);
}

nsresult EphyBrowser::SetZoom (float aZoom, PRBool reflow)
{
	nsresult result;

	g_return_val_if_fail (mWebBrowser, NS_ERROR_FAILURE);

	if (reflow)
	{
		nsCOMPtr<nsIContentViewer> contentViewer;	
		result = GetContentViewer (getter_AddRefs(contentViewer));
		if (!NS_SUCCEEDED (result) || !contentViewer) return NS_ERROR_FAILURE;

		nsCOMPtr<nsIMarkupDocumentViewer> mdv = do_QueryInterface(contentViewer,
								  &result);
		if (NS_FAILED(result) || !mdv) return NS_ERROR_FAILURE;

		return mdv->SetTextZoom (aZoom);
	}
	else
	{
		nsCOMPtr<nsIDocShell> DocShell;
		DocShell = do_GetInterface (mWebBrowser);
		if (!DocShell) return NS_ERROR_FAILURE;

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
	nsresult result;

	nsCOMPtr<nsIPresContext> PresContext;
	result = DocShell->GetPresContext (getter_AddRefs(PresContext));
	if (NS_FAILED(result) || !PresContext) return NS_ERROR_FAILURE;
					
	nsCOMPtr<nsIDeviceContext> DeviceContext;
	result = PresContext->GetDeviceContext (getter_AddRefs(DeviceContext));
	if (NS_FAILED(result) || !DeviceContext) return NS_ERROR_FAILURE;

	return DeviceContext->SetTextZoom (aZoom);
}

nsresult EphyBrowser::GetContentViewer (nsIContentViewer **aViewer)
{
	g_return_val_if_fail (mWebBrowser, NS_ERROR_FAILURE);

	nsCOMPtr<nsIDocShell> ourDocShell(do_GetInterface(mWebBrowser));
	NS_ENSURE_TRUE(ourDocShell, NS_ERROR_FAILURE);
	return ourDocShell->GetContentViewer(aViewer);
}

nsresult EphyBrowser::GetZoom (float *aZoom)
{
	nsresult result;

	nsCOMPtr<nsIContentViewer> contentViewer;	
	result = GetContentViewer (getter_AddRefs(contentViewer));
	if (!NS_SUCCEEDED (result) || !contentViewer) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIMarkupDocumentViewer> mdv = do_QueryInterface(contentViewer,
								  &result);
	if (NS_FAILED(result) || !mdv) return NS_ERROR_FAILURE;

	return mdv->GetTextZoom (aZoom);
}

nsresult EphyBrowser::GetDocument (nsIDOMDocument **aDOMDocument)
{
        NS_ENSURE_ARG_POINTER(aDOMDocument);
        NS_IF_ADDREF(*aDOMDocument = mDOMDocument);

	return NS_OK;
}

nsresult EphyBrowser::GetTargetDocument (nsIDOMDocument **aDOMDocument)
{
	nsresult result;

	g_return_val_if_fail (mWebBrowser, NS_ERROR_FAILURE);

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
	if (!webBrowserFocus) return NS_ERROR_FAILURE;

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
	nsresult result;

	nsCOMPtr<nsISHistory> SessionHistory;
	result = GetSHistory (getter_AddRefs(SessionHistory));
	if (NS_FAILED(result) || ! SessionHistory) return NS_ERROR_FAILURE;

	SessionHistory->GetCount (count);
	SessionHistory->GetIndex (index);	

	return NS_OK;
}

nsresult EphyBrowser::GetSHTitleAtIndex (PRInt32 index, PRUnichar **title)
{
	nsresult result;

	nsCOMPtr<nsISHistory> SessionHistory;
	result = GetSHistory (getter_AddRefs(SessionHistory));
	if (NS_FAILED(result) || ! SessionHistory) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIHistoryEntry> he;
	result = SessionHistory->GetEntryAtIndex (index, PR_FALSE,
						  getter_AddRefs (he));
	if (!NS_SUCCEEDED(result) || (!he)) return NS_ERROR_FAILURE;

	result = he->GetTitle (title);
	if (!NS_SUCCEEDED(result) || (!title)) return NS_ERROR_FAILURE;

	return NS_OK;
}

nsresult EphyBrowser::GetSHUrlAtIndex (PRInt32 index, nsCString &url)
{
	nsresult result;

	nsCOMPtr<nsISHistory> SessionHistory;
	result = GetSHistory (getter_AddRefs(SessionHistory));
	if (NS_FAILED(result) || ! SessionHistory) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIHistoryEntry> he;
	result = SessionHistory->GetEntryAtIndex (index, PR_FALSE,
						  getter_AddRefs (he));
	if (NS_FAILED(result) || (!he)) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIURI> uri;
	result = he->GetURI (getter_AddRefs(uri));
	if (NS_FAILED(result) || (!uri)) return NS_ERROR_FAILURE;

	result = uri->GetSpec(url);
	if (NS_FAILED(result) || url.IsEmpty()) return NS_ERROR_FAILURE;

	return NS_OK;
}

nsresult EphyBrowser::FindSetProperties (const PRUnichar *search_string,
			                 PRBool case_sensitive,
					 PRBool wrap_around)
{
	g_return_val_if_fail (mWebBrowser, NS_ERROR_FAILURE);

	nsCOMPtr<nsIWebBrowserFind> finder (do_GetInterface(mWebBrowser));
	
	finder->SetSearchString (search_string);
	finder->SetMatchCase (case_sensitive);
	finder->SetWrapFind (wrap_around);

	return NS_OK;
}

nsresult EphyBrowser::Find (PRBool backwards,
			    PRBool *didFind)
{
	g_return_val_if_fail (mWebBrowser, NS_ERROR_FAILURE);

	nsCOMPtr<nsIWebBrowserFind> finder (do_GetInterface(mWebBrowser));
	
	finder->SetFindBackwards (backwards);

	return finder->FindNext(didFind);
}

nsresult EphyBrowser::GetPageDescriptor(nsISupports **aPageDescriptor)
{
	nsresult rv;

	g_return_val_if_fail (mWebBrowser, NS_ERROR_FAILURE);

	nsCOMPtr<nsIDocShell> ds;
	ds = do_GetInterface (mWebBrowser);
	if (!ds) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIWebPageDescriptor> wpd = do_QueryInterface(ds, &rv);
	if (!wpd || !NS_SUCCEEDED(rv)) return NS_ERROR_FAILURE;

	return wpd->GetCurrentDescriptor(aPageDescriptor);
}

nsresult EphyBrowser::GetDocumentUrl (nsCString &url)
{
	nsresult result;

	nsCOMPtr<nsIDOMDocument> DOMDocument;

	result = mDOMWindow->GetDocument (getter_AddRefs(DOMDocument));
	if (NS_FAILED(result) || !DOMDocument) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIDocument> doc = do_QueryInterface(DOMDocument);
	if(!doc) return NS_ERROR_FAILURE;

#if MOZILLA_SNAPSHOT > 11
	nsIURI *uri;
	uri = doc->GetDocumentURL ();
#else
	nsCOMPtr<nsIURI> uri;
	doc->GetDocumentURL(getter_AddRefs(uri));
#endif

	return uri->GetSpec (url);
}

nsresult EphyBrowser::GetTargetDocumentUrl (nsCString &url)
{
        nsresult result;

        nsCOMPtr<nsIDOMDocument> DOMDocument;

        result = GetTargetDocument (getter_AddRefs(DOMDocument));
        if (NS_FAILED(result) || !DOMDocument) return NS_ERROR_FAILURE;

        nsCOMPtr<nsIDocument> doc = do_QueryInterface(DOMDocument);
        if(!doc) return NS_ERROR_FAILURE;

#if MOZILLA_SNAPSHOT > 11
	nsIURI *uri;
	uri = doc->GetDocumentURL ();
#else
        nsCOMPtr<nsIURI> uri;
        doc->GetDocumentURL(getter_AddRefs(uri));
#endif

        uri->GetSpec (url);

        return NS_OK;
}

nsresult EphyBrowser::ForceEncoding (const char *encoding) 
{
	nsresult result;

	nsCOMPtr<nsIContentViewer> contentViewer;	
	result = GetContentViewer (getter_AddRefs(contentViewer));
	if (!NS_SUCCEEDED (result) || !contentViewer) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIMarkupDocumentViewer> mdv = do_QueryInterface(contentViewer);
	if (!mdv) return NS_ERROR_FAILURE;

	#if MOZILLA_SNAPSHOT > 9 
		result = mdv->SetForceCharacterSet (nsDependentCString(encoding));
	#else
		result = mdv->SetForceCharacterSet (NS_ConvertUTF8toUCS2(encoding).get());
	#endif

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
	result = GetTargetDocument (getter_AddRefs(domDoc));
	if (NS_FAILED (result) || !domDoc) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc, &result);
	if (NS_FAILED (result) || !doc) return NS_ERROR_FAILURE;

	info = g_new0 (EphyEncodingInfo, 1);
	*infoptr = info;

	PRInt32 source;
#if MOZILLA_SNAPSHOT > 11
	source = doc->GetDocumentCharacterSetSource ();
#else
	result = doc->GetDocumentCharacterSetSource (&source);
	if (NS_FAILED (result)) return NS_ERROR_FAILURE;
#endif
	info->encoding_source = (EphyEncodingSource) source;

	nsCOMPtr<nsIDocShell> ds;
	ds = do_GetInterface (mWebBrowser);
	if (!ds) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIDocumentCharsetInfo> ci;
	result = ds->GetDocumentCharsetInfo (getter_AddRefs (ci));
	if (NS_FAILED(result) || !ci) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIAtom> atom;
	result = ci->GetForcedCharset (getter_AddRefs (atom));
	if (NS_FAILED(result)) return NS_ERROR_FAILURE;
	if (atom)
	{
		nsCAutoString atomstr;
		atom->ToUTF8String (atomstr);
		info->forced_encoding = g_strdup (atomstr.get());
	}

	result = ci->GetParentCharset (getter_AddRefs (atom));
	if (NS_FAILED(result)) return NS_ERROR_FAILURE;
	if (atom)
	{
		nsCAutoString atomstr;
		atom->ToUTF8String (atomstr);
		info->parent_encoding = g_strdup (atomstr.get());
	}

	result = ci->GetParentCharsetSource (&source);
	if (NS_FAILED (result)) return NS_ERROR_FAILURE;
	info->parent_encoding_source = (EphyEncodingSource) source;

	nsCOMPtr<nsIContentViewer> contentViewer;	
	result = ds->GetContentViewer (getter_AddRefs(contentViewer));
	if (!NS_SUCCEEDED (result) || !contentViewer) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIMarkupDocumentViewer> mdv = do_QueryInterface(contentViewer,
								  &result);
	if (NS_FAILED(result) || !mdv) return NS_ERROR_FAILURE;

#if MOZILLA_SNAPSHOT > 11
	const nsACString& charsetEnc = doc->GetDocumentCharacterSet ();
	if (charsetEnc.IsEmpty()) return NS_ERROR_FAILURE;

	info->encoding = g_strdup (PromiseFlatCString(charsetEnc).get());
#elif MOZILLA_SNAPSHOT >= 10
	nsCAutoString charsetEnc;	
	result = doc->GetDocumentCharacterSet (charsetEnc);
	if (NS_FAILED (result)) return NS_ERROR_FAILURE;

	info->encoding = g_strdup (charsetEnc.get());
#else
	nsAutoString charsetEnc;
	result = doc->GetDocumentCharacterSet (charsetEnc);
	if (NS_FAILED (result)) return NS_ERROR_FAILURE;

	info->encoding = g_strdup (NS_ConvertUCS2toUTF8(charsetEnc).get());
#endif

#if MOZILLA_SNAPSHOT >= 10
	nsCAutoString enc;
	
	result = mdv->GetDefaultCharacterSet (enc);
	if (NS_FAILED (result)) return NS_ERROR_FAILURE;
	info->default_encoding = g_strdup (enc.get());

	result = mdv->GetForceCharacterSet (enc);
	if (NS_FAILED (result)) return NS_ERROR_FAILURE;
	info->forced_encoding = g_strdup (enc.get());

	result = mdv->GetHintCharacterSet (enc);
	if (NS_FAILED (result)) return NS_ERROR_FAILURE;
	info->hint_encoding = g_strdup (enc.get());

	result = mdv->GetPrevDocCharacterSet (enc);
	if (NS_FAILED (result)) return NS_ERROR_FAILURE;
	info->prev_doc_encoding = g_strdup (enc.get());
#else
	PRUnichar *str;

	result = mdv->GetDefaultCharacterSet (&str);
	if (NS_FAILED (result)) return NS_ERROR_FAILURE;
	info->default_encoding = g_strdup (NS_ConvertUCS2toUTF8(str).get());

	result = mdv->GetForceCharacterSet (&str);
	if (NS_FAILED (result)) return NS_ERROR_FAILURE;
	info->forced_encoding = g_strdup (NS_ConvertUCS2toUTF8(str).get());

	result = mdv->GetHintCharacterSet (&str);
	if (NS_FAILED (result)) return NS_ERROR_FAILURE;
	info->hint_encoding = g_strdup (NS_ConvertUCS2toUTF8(str).get());

	result = mdv->GetPrevDocCharacterSet (&str);
	if (NS_FAILED (result)) return NS_ERROR_FAILURE;
	info->prev_doc_encoding = g_strdup (NS_ConvertUCS2toUTF8(str).get());
#endif

	mdv->GetHintCharacterSetSource (&source);
	if (NS_FAILED (result)) return NS_ERROR_FAILURE;
	info->hint_encoding_source = (EphyEncodingSource) source;

	return NS_OK;
}

nsresult EphyBrowser::DoCommand (const char *command)
{
	nsCOMPtr<nsICommandManager> cmdManager;

	g_return_val_if_fail (mWebBrowser, NS_ERROR_FAILURE);

	cmdManager = do_GetInterface (mWebBrowser);
	if (!cmdManager) return NS_ERROR_FAILURE;

	return cmdManager->DoCommand (command, nsnull, nsnull);
}
