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

#include "EphyWrapper.h"
#include "GlobalHistory.h"
#include "ProgressListener.h"
#include "ephy-embed.h"
#include "ephy-string.h"

#include <gtkmozembed_internal.h>
#include <unistd.h>

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
#include "nsIClipboardCommands.h"
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
#include "ContentHandler.h"
#include "EphyEventListener.h"

EphyWrapper::EphyWrapper ()
{
	mEventListener = nsnull;
	mEventReceiver = nsnull;	
}

EphyWrapper::~EphyWrapper ()
{
}

nsresult EphyWrapper::Init (GtkMozEmbed *mozembed)
{
	nsresult result;

	gtk_moz_embed_get_nsIWebBrowser (mozembed,
					 getter_AddRefs(mWebBrowser));
	if (!mWebBrowser) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIDocShell> DocShell;
	result = GetDocShell (getter_AddRefs(DocShell));
	if (NS_FAILED(result) || !DocShell) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIDocShellHistory> dsHistory = do_QueryInterface (DocShell);
	if (!dsHistory) return NS_ERROR_FAILURE;

	mEventListener = new EphyEventListener();
	mEventListener->Init (EPHY_EMBED (mozembed));
 	GetListener();
	AttachListeners();

	nsCOMPtr<nsIGlobalHistory> inst =  
	do_GetService(NS_GLOBALHISTORY_CONTRACTID, &result);

	return dsHistory->SetGlobalHistory(inst);
}

nsresult
EphyWrapper::GetListener (void)
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
EphyWrapper::AttachListeners(void)
{
  	if (!mEventReceiver) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIDOMEventTarget> target;
	target = do_QueryInterface (mEventReceiver);

	return target->AddEventListener(NS_LITERAL_STRING("DOMLinkAdded"),
			                mEventListener, PR_FALSE);
}

nsresult
EphyWrapper::DetachListeners(void)
{
	if (!mEventReceiver) return NS_ERROR_FAILURE;
	
	nsCOMPtr<nsIDOMEventTarget> target;
	target = do_QueryInterface (mEventReceiver);

	return target->RemoveEventListener(NS_LITERAL_STRING("DOMLinkAdded"),
					   mEventListener, PR_FALSE);
}

nsresult EphyWrapper::GetDocShell (nsIDocShell **aDocShell)
{
        nsCOMPtr<nsIDocShellTreeItem> browserAsItem;
        browserAsItem = do_QueryInterface(mWebBrowser);
        if (!browserAsItem) return NS_ERROR_FAILURE;

        // get the owner for that item
        nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
        browserAsItem->GetTreeOwner(getter_AddRefs(treeOwner));
        if (!treeOwner) return NS_ERROR_FAILURE;

        // get the primary content shell as an item
        nsCOMPtr<nsIDocShellTreeItem> contentItem;
        treeOwner->GetPrimaryContentShell(getter_AddRefs(contentItem));
        if (!contentItem) return NS_ERROR_FAILURE;

        // QI that back to a docshell
        nsCOMPtr<nsIDocShell> DocShell;
        DocShell = do_QueryInterface(contentItem);
        if (!DocShell) return NS_ERROR_FAILURE;

        *aDocShell = DocShell.get();

        NS_IF_ADDREF(*aDocShell);
        
        return NS_OK;
}
nsresult EphyWrapper::Print (nsIPrintSettings *options, PRBool preview)
{
	nsresult result;

	nsCOMPtr<nsIWebBrowserPrint> print(do_GetInterface(mWebBrowser, &result));
	if (NS_FAILED(result) || !print) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIDOMWindow> DOMWindow;
	result = mWebBrowser->GetContentDOMWindow (getter_AddRefs(DOMWindow));
	if (NS_FAILED(result) || !DOMWindow) return NS_ERROR_FAILURE;

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

nsresult EphyWrapper::PrintPreviewClose (void)
{
	nsresult rv;
	PRBool isPreview = PR_FALSE;

	nsCOMPtr<nsIWebBrowserPrint> print(do_GetInterface(mWebBrowser, &rv));
	if (NS_FAILED(rv) || !print) return NS_ERROR_FAILURE;

	rv = print->GetDoingPrintPreview(&isPreview);
	if (NS_SUCCEEDED (rv) && isPreview == PR_TRUE)
	{
		rv = print->ExitPrintPreview();
	}

	return rv;
}

nsresult EphyWrapper::PrintPreviewNumPages (int *numPages)
{
	nsresult rv;

	nsCOMPtr<nsIWebBrowserPrint> print(do_GetInterface(mWebBrowser, &rv));
	if (NS_FAILED(rv) || !print) return NS_ERROR_FAILURE;

	rv = print->GetPrintPreviewNumPages(numPages);
	return rv;
}

nsresult EphyWrapper::PrintPreviewNavigate(PRInt16 navType, PRInt32 pageNum)
{
	nsresult rv;

	nsCOMPtr<nsIWebBrowserPrint> print(do_GetInterface(mWebBrowser, &rv));
	if (NS_FAILED(rv) || !print) return NS_ERROR_FAILURE;

	rv = print->PrintPreviewNavigate(navType, pageNum);
	return rv;
}

nsresult EphyWrapper::GetPrintSettings (nsIPrintSettings **options)
{
	nsresult result;
	nsCOMPtr<nsIWebBrowserPrint> print(do_GetInterface(mWebBrowser, &result));
	if (NS_FAILED(result) || !print) return NS_ERROR_FAILURE;

	return print->GetGlobalPrintSettings(options);
}

nsresult EphyWrapper::GetSHistory (nsISHistory **aSHistory)
{
	nsresult result;

	nsCOMPtr<nsIDocShell> DocShell;
	result = GetDocShell (getter_AddRefs(DocShell));
	if (NS_FAILED(result) || !DocShell) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIWebNavigation> ContentNav = do_QueryInterface (DocShell,
								   &result);
	if (!ContentNav) return NS_ERROR_FAILURE;

	nsCOMPtr<nsISHistory> SessionHistory;
	result = ContentNav->GetSessionHistory (getter_AddRefs (SessionHistory));
	if (!SessionHistory) return NS_ERROR_FAILURE;

	*aSHistory = SessionHistory.get();
	NS_IF_ADDREF (*aSHistory);

	return NS_OK;
}

nsresult EphyWrapper::Destroy ()
{
	DetachListeners ();

      	mWebBrowser = nsnull;
	mChromeNav = nsnull;
	
	return NS_OK;
}

nsresult EphyWrapper::GoToHistoryIndex (PRInt16 index)
{
	nsresult result;

	nsCOMPtr<nsIDocShell> DocShell;
	result = GetDocShell (getter_AddRefs(DocShell));
	if (NS_FAILED(result) || !DocShell) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIWebNavigation> ContentNav = do_QueryInterface (DocShell,
								   &result);
	if (!ContentNav) return NS_ERROR_FAILURE;

	return  ContentNav->GotoIndex (index);
}

nsresult EphyWrapper::SetZoom (float aZoom, PRBool reflow)
{
	nsresult result;

	nsCOMPtr<nsIDocShell> DocShell;
	result = GetDocShell (getter_AddRefs(DocShell));
	if (NS_FAILED(result) || !DocShell) return NS_ERROR_FAILURE;

	if (reflow)
	{
		nsCOMPtr<nsIContentViewer> contentViewer;	
		result = DocShell->GetContentViewer (getter_AddRefs(contentViewer));
		if (!NS_SUCCEEDED (result) || !contentViewer) return NS_ERROR_FAILURE;

		nsCOMPtr<nsIMarkupDocumentViewer> mdv = do_QueryInterface(contentViewer,
								  &result);
		if (NS_FAILED(result) || !mdv) return NS_ERROR_FAILURE;

		return mdv->SetTextZoom (aZoom);
	}
	else
	{
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

nsresult EphyWrapper::SetZoomOnDocshell (float aZoom, nsIDocShell *DocShell)
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

nsresult EphyWrapper::GetZoom (float *aZoom)
{
	nsresult result;

	nsCOMPtr<nsIDocShell> DocShell;
	result = GetDocShell (getter_AddRefs(DocShell));
	if (NS_FAILED(result) || !DocShell) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIContentViewer> contentViewer;	
	result = DocShell->GetContentViewer (getter_AddRefs(contentViewer));
	if (!NS_SUCCEEDED (result) || !contentViewer) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIMarkupDocumentViewer> mdv = do_QueryInterface(contentViewer,
								  &result);
	if (NS_FAILED(result) || !mdv) return NS_ERROR_FAILURE;

	return mdv->GetTextZoom (aZoom);
}

nsresult EphyWrapper::GetFocusedDOMWindow (nsIDOMWindow **aDOMWindow)
{
	nsresult rv;
	
	nsCOMPtr<nsIWebBrowserFocus> focus = do_GetInterface(mWebBrowser, &rv);
	if (NS_FAILED(rv) || !focus) return NS_ERROR_FAILURE;

	rv = focus->GetFocusedWindow (aDOMWindow);
	if (NS_FAILED(rv))
		rv = mWebBrowser->GetContentDOMWindow (aDOMWindow);
	return rv;
}

nsresult EphyWrapper::GetDOMWindow (nsIDOMWindow **aDOMWindow)
{
	nsresult rv;
	
	rv = mWebBrowser->GetContentDOMWindow (aDOMWindow);
	
	return rv;
}

nsresult EphyWrapper::GetDOMDocument (nsIDOMDocument **aDOMDocument)
{
	nsresult result;

	/* Use the current target document */
	if (mTargetDocument)
	{
		*aDOMDocument = mTargetDocument.get();

		NS_IF_ADDREF(*aDOMDocument);

		return NS_OK;
	}

	/* Use the focused document */
	nsCOMPtr<nsIDOMWindow> DOMWindow;
	result = GetFocusedDOMWindow (getter_AddRefs(DOMWindow));
	if (NS_SUCCEEDED(result) && DOMWindow)
	{
		return DOMWindow->GetDocument (aDOMDocument);
	}

	/* Use the main document */
	return GetMainDOMDocument (aDOMDocument);
}

nsresult EphyWrapper::GetMainDOMDocument (nsIDOMDocument **aDOMDocument)
{
	nsresult result;

	nsCOMPtr<nsIDocShell> DocShell;
	result = GetDocShell (getter_AddRefs(DocShell));
	if (NS_FAILED(result) || !DocShell) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIContentViewer> contentViewer;	
	result = DocShell->GetContentViewer (getter_AddRefs(contentViewer));
	if (!NS_SUCCEEDED (result) || !contentViewer) return NS_ERROR_FAILURE;

	return contentViewer->GetDOMDocument (aDOMDocument);
}

nsresult EphyWrapper::GetSHInfo (PRInt32 *count, PRInt32 *index)
{
	nsresult result;

	nsCOMPtr<nsISHistory> SessionHistory;
	result = GetSHistory (getter_AddRefs(SessionHistory));
	if (NS_FAILED(result) || ! SessionHistory) return NS_ERROR_FAILURE;

	SessionHistory->GetCount (count);
	SessionHistory->GetIndex (index);	

	return NS_OK;
}

nsresult EphyWrapper::GetSHTitleAtIndex (PRInt32 index, PRUnichar **title)
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

nsresult EphyWrapper::GetSHUrlAtIndex (PRInt32 index, nsCString &url)
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

nsresult EphyWrapper::FindSetProperties (const PRUnichar *search_string,
			                 PRBool case_sensitive,
					 PRBool wrap_around)
{
	nsCOMPtr<nsIWebBrowserFind> finder (do_GetInterface(mWebBrowser));
	
	finder->SetSearchString (search_string);
	finder->SetMatchCase (case_sensitive);
	finder->SetWrapFind (wrap_around);

	return NS_OK;
}

nsresult EphyWrapper::Find (PRBool backwards,
			    PRBool *didFind)
{
	nsCOMPtr<nsIWebBrowserFind> finder (do_GetInterface(mWebBrowser));
	
	finder->SetFindBackwards (backwards);

	return finder->FindNext(didFind);
}

nsresult EphyWrapper::GetWebNavigation(nsIWebNavigation **aWebNavigation)
{
	nsresult result;

	nsCOMPtr<nsIDOMWindow> DOMWindow;
	result = GetFocusedDOMWindow (getter_AddRefs(DOMWindow));
	if (NS_FAILED(result) || !DOMWindow) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIScriptGlobalObject> scriptGlobal = do_QueryInterface(DOMWindow);
	if (!scriptGlobal) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIDocShell> docshell;
	if (NS_FAILED(scriptGlobal->GetDocShell(getter_AddRefs(docshell))))
        return NS_ERROR_FAILURE;

	nsCOMPtr<nsIWebNavigation> wn = do_QueryInterface (docshell, &result);
	if (!wn || !NS_SUCCEEDED (result)) return NS_ERROR_FAILURE;

	NS_IF_ADDREF(*aWebNavigation = wn);
	return NS_OK;
}

nsresult EphyWrapper::LoadDocument(nsISupports *aPageDescriptor,
				     PRUint32 aDisplayType)
{
	nsresult rv;

	nsCOMPtr<nsIWebNavigation> wn;
	rv = GetWebNavigation(getter_AddRefs(wn));
	if (!wn || !NS_SUCCEEDED(rv)) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIWebPageDescriptor> wpd = do_QueryInterface(wn, &rv);
	if (!wpd || !NS_SUCCEEDED(rv)) return NS_ERROR_FAILURE;

	return wpd->LoadPage(aPageDescriptor, aDisplayType);
}

nsresult EphyWrapper::GetPageDescriptor(nsISupports **aPageDescriptor)
{
	nsresult rv;

	nsCOMPtr<nsIWebNavigation> wn;
	rv = GetWebNavigation(getter_AddRefs(wn));
	if (!wn || !NS_SUCCEEDED(rv)) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIWebPageDescriptor> wpd = do_QueryInterface(wn, &rv);
	if (!wpd || !NS_SUCCEEDED(rv)) return NS_ERROR_FAILURE;

	return wpd->GetCurrentDescriptor(aPageDescriptor);
}

nsresult EphyWrapper::GetMainDocumentUrl (nsCString &url)
{
	nsresult result;

	nsCOMPtr<nsIDOMDocument> DOMDocument;

	result = GetMainDOMDocument (getter_AddRefs(DOMDocument));
	if (NS_FAILED(result) || !DOMDocument) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIDocument> doc = do_QueryInterface(DOMDocument);
	if(!doc) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIURI> uri;
	doc->GetDocumentURL(getter_AddRefs(uri));
	if (!uri) return NS_ERROR_FAILURE;

	return uri->GetSpec (url);
}

nsresult EphyWrapper::GetDocumentUrl (nsCString &url)
{
        nsresult result;

        nsCOMPtr<nsIDOMDocument> DOMDocument;

        result = GetDOMDocument (getter_AddRefs(DOMDocument));
        if (NS_FAILED(result) || !DOMDocument) return NS_ERROR_FAILURE;

        nsCOMPtr<nsIDocument> doc = do_QueryInterface(DOMDocument);
        if(!doc) return NS_ERROR_FAILURE;

        nsCOMPtr<nsIURI> uri;
        doc->GetDocumentURL(getter_AddRefs(uri));
	if (!uri) return NS_ERROR_FAILURE;

        return uri->GetSpec (url);
}

nsresult  EphyWrapper::CopyHistoryTo (EphyWrapper *dest)
{
	nsresult result;
	int count,index;

	nsCOMPtr<nsIDocShell> DocShell;
	result = GetDocShell (getter_AddRefs(DocShell));
	if (NS_FAILED(result) || !DocShell) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIWebNavigation> wn_src = do_QueryInterface (DocShell,
							       &result);
	if (!wn_src) return NS_ERROR_FAILURE;
	
	nsCOMPtr<nsISHistory> h_src;
	result = wn_src->GetSessionHistory (getter_AddRefs (h_src));
	if (!NS_SUCCEEDED(result) || (!h_src)) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIDocShell> destDocShell;
	result = dest->GetDocShell (getter_AddRefs(destDocShell));
	if (NS_FAILED(result) || !DocShell) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIWebNavigation> wn_dest = do_QueryInterface (destDocShell,
								&result);
	if (!wn_dest) return NS_ERROR_FAILURE;
	
	nsCOMPtr<nsISHistory> h_dest;
	result = wn_dest->GetSessionHistory (getter_AddRefs (h_dest));
	if (!NS_SUCCEEDED (result) || (!h_dest)) return NS_ERROR_FAILURE;

	nsCOMPtr<nsISHistoryInternal> hi_dest = do_QueryInterface (h_dest);
	if (!hi_dest) return NS_ERROR_FAILURE;

	h_src->GetCount (&count);
	h_src->GetIndex (&index);

	if (count) {
		nsCOMPtr<nsIHistoryEntry> he;
		nsCOMPtr<nsISHEntry> she;

		for (PRInt32 i = 0; i < count; i++) {

			result = h_src->GetEntryAtIndex (i, PR_FALSE,
							 getter_AddRefs (he));
			if (!NS_SUCCEEDED(result) || (!he))
				return NS_ERROR_FAILURE;

			she = do_QueryInterface (he);
			if (!she) return NS_ERROR_FAILURE;

			result = hi_dest->AddEntry (she, PR_TRUE);
			if (!NS_SUCCEEDED(result) || (!she))
				return NS_ERROR_FAILURE;
		}

		result = wn_dest->GotoIndex(index);
		if (!NS_SUCCEEDED(result)) return NS_ERROR_FAILURE;
	}

	return NS_OK;
}

nsresult EphyWrapper::ForceEncoding (const char *encoding) 
{
	nsresult result;

	nsCOMPtr<nsIDocShell> DocShell;
	result = GetDocShell (getter_AddRefs(DocShell));
	if (NS_FAILED(result) || !DocShell) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIContentViewer> contentViewer;	
	result = DocShell->GetContentViewer (getter_AddRefs(contentViewer));
	if (!NS_SUCCEEDED (result) || !contentViewer) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIMarkupDocumentViewer> mdv = do_QueryInterface(contentViewer,
								  &result);
	if (NS_FAILED(result) || !mdv) return NS_ERROR_FAILURE;

#if MOZILLA_SNAPSHOT > 9
	result = mdv->SetForceCharacterSet (nsDependentCString(encoding));
#else
	result = mdv->SetForceCharacterSet (NS_ConvertUTF8toUCS2(encoding).get());
#endif

	return result;
}

nsresult EphyWrapper::CanCutSelection(PRBool *result)
{
	nsCOMPtr<nsIClipboardCommands> clipboard (do_GetInterface(mWebBrowser));
	return clipboard->CanCutSelection (result);
}

nsresult EphyWrapper::CanCopySelection(PRBool *result)
{
	nsCOMPtr<nsIClipboardCommands> clipboard (do_GetInterface(mWebBrowser));
	return clipboard->CanCopySelection (result);
}

nsresult EphyWrapper::CanPaste(PRBool *result)
{
	nsCOMPtr<nsIClipboardCommands> clipboard (do_GetInterface(mWebBrowser));
	return clipboard->CanPaste (result);
}

nsresult EphyWrapper::CutSelection(void)
{
	nsCOMPtr<nsIClipboardCommands> clipboard (do_GetInterface(mWebBrowser));
	return clipboard->CutSelection ();
}

nsresult EphyWrapper::CopySelection(void)
{
	nsCOMPtr<nsIClipboardCommands> clipboard (do_GetInterface(mWebBrowser));
	return clipboard->CopySelection ();
}

nsresult EphyWrapper::Paste(void)
{
	nsCOMPtr<nsIClipboardCommands> clipboard (do_GetInterface(mWebBrowser));
	return clipboard->Paste ();
}

nsresult EphyWrapper::SelectAll (void)
{
	nsCOMPtr<nsIClipboardCommands> clipboard (do_GetInterface(mWebBrowser));
	return clipboard->SelectAll ();
}

nsresult EphyWrapper::PushTargetDocument (nsIDOMDocument *domDoc)
{
	mTargetDocument = domDoc;

	return NS_OK;
}

nsresult EphyWrapper::PopTargetDocument ()
{
	mTargetDocument = nsnull;

	return NS_OK;
}
