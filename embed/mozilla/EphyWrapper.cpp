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
#include "PrintProgressListener.h"
#include "ephy-embed.h"
#include "ephy-string.h"

#include <gtkmozembed_internal.h>
#include <unistd.h>

#include "nsIContentViewer.h"
#include "nsIPermissionManager.h"
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
#include "nsIPresShell.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIComponentManager.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"

#include "nsIDOMWindowInternal.h"
#include "nsICharsetConverterManager.h"
#include "nsICharsetConverterManager2.h"
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
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "caps/nsIPrincipal.h"
#include "nsIDeviceContext.h"
#include "nsIPresContext.h"
#include "ContentHandler.h"
#include "nsITypeAheadFind.h"
#include "nsSupportsPrimitives.h"
#include "EphyEventListener.h"

EphyWrapper::EphyWrapper ()
{
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

	static NS_DEFINE_CID(kGlobalHistoryCID, GALEON_GLOBALHISTORY_CID);

	nsCOMPtr<nsIFactory> GHFactory;
	result = NS_NewGlobalHistoryFactory(getter_AddRefs(GHFactory));
	if (NS_FAILED(result)) return NS_ERROR_FAILURE;

	result = nsComponentManager::RegisterFactory(kGlobalHistoryCID,
						     "Global history",
						     NS_GLOBALHISTORY_CONTRACTID,
						     GHFactory,
						     PR_TRUE);

	nsCOMPtr<nsIGlobalHistory> inst =  
		do_GetService(NS_GLOBALHISTORY_CONTRACTID, &result);
	
	mEventListener = new EphyEventListener();
	mEventListener->Init (EPHY_EMBED (mozembed));
 	GetListener();
	AttachListeners();
	
	return dsHistory->SetGlobalHistory(inst);
}

void
EphyWrapper::GetListener (void)
{
  	if (mEventReceiver) return;

  	nsCOMPtr<nsIDOMWindow> domWindowExternal;
  	mWebBrowser->GetContentDOMWindow(getter_AddRefs(domWindowExternal));
  
  	nsCOMPtr<nsIDOMWindowInternal> domWindow;
        domWindow = do_QueryInterface(domWindowExternal);
	
	nsCOMPtr<nsPIDOMWindow> piWin(do_QueryInterface(domWindow));
  	if (!piWin) return;

  	nsCOMPtr<nsIChromeEventHandler> chromeHandler;
  	piWin->GetChromeEventHandler(getter_AddRefs(chromeHandler));

  	mEventReceiver = do_QueryInterface(chromeHandler);
}

void
EphyWrapper::AttachListeners(void)
{
  	if (!mEventReceiver || mListenersAttached)
    		return;

	nsCOMPtr<nsIDOMEventTarget> target;
	target = do_QueryInterface (mEventReceiver);

	target->AddEventListener(NS_LITERAL_STRING("DOMLinkAdded"), mEventListener, PR_FALSE);

  	mListenersAttached = PR_TRUE;
}

void
EphyWrapper::DetachListeners(void)
{
  	if (!mListenersAttached || !mEventReceiver)
    		return;

	nsCOMPtr<nsIDOMEventTarget> target;
	target = do_QueryInterface (mEventReceiver);

	target->RemoveEventListener(NS_LITERAL_STRING("DOMLinkAdded"), mEventListener, PR_FALSE);
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
		GPrintListener *listener = new GPrintListener();
		result = print->Print (options, listener);
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
	if (isPreview == PR_TRUE)
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
	if (NS_FAILED(result)) return NS_ERROR_FAILURE;
					
	nsCOMPtr<nsIDeviceContext> DeviceContext;
	result = PresContext->GetDeviceContext (getter_AddRefs(DeviceContext));

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

nsresult EphyWrapper::Find (const PRUnichar *search_string, 
			    PRBool interactive,
			    PRBool matchcase, PRBool search_backwards,
			    PRBool search_wrap_around,
			    PRBool search_for_entire_word,
			    PRBool search_in_frames,
			    PRBool *didFind)
{
	if (!interactive)
	{
		nsresult rv;
		nsCOMPtr<nsITypeAheadFind> tAFinder
			(do_GetService(NS_TYPEAHEADFIND_CONTRACTID, &rv));
		if (NS_SUCCEEDED(rv))
		{
			nsCOMPtr<nsIDOMWindow> aFocusedWindow;
			rv = GetFocusedDOMWindow(getter_AddRefs(aFocusedWindow));
			if (NS_SUCCEEDED(rv))
			{
				nsSupportsInterfacePointerImpl windowPtr;
				windowPtr.SetData(aFocusedWindow);

				tAFinder->FindNext(search_backwards, &windowPtr);

				nsCOMPtr<nsISupports> retValue;
				rv = windowPtr.GetData(getter_AddRefs(retValue));
				if (NS_SUCCEEDED(rv) && !retValue)
				{
					*didFind = PR_TRUE;
					return NS_OK;
				}
			}
		}

	}

	nsCOMPtr<nsIWebBrowserFind> finder (do_GetInterface(mWebBrowser));
	
	finder->SetSearchString (search_string);
	finder->SetFindBackwards (search_backwards);
	finder->SetWrapFind (search_wrap_around);
	finder->SetEntireWord (search_for_entire_word);
	finder->SetMatchCase (matchcase);
	finder->SetSearchFrames (search_in_frames);
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

nsresult EphyWrapper::ReloadDocument ()
{
	nsresult result;

	nsCOMPtr<nsIWebNavigation> wn;
	result = GetWebNavigation(getter_AddRefs(wn));
	if (!wn || !NS_SUCCEEDED (result)) return NS_ERROR_FAILURE;

	result = wn->Reload (nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE | 
			     nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY);
	if (!NS_SUCCEEDED (result)) return NS_ERROR_FAILURE;

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

        uri->GetSpec (url);

        return NS_OK;
}

nsresult EphyWrapper::GetDocumentTitle (char **title)
{
	nsresult result;

	nsCOMPtr<nsIDOMDocument> DOMDocument;

	result = GetDOMDocument (getter_AddRefs(DOMDocument));
	if (NS_FAILED(result) || !DOMDocument) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIDocument> doc = do_QueryInterface(DOMDocument);
	if(!doc) return NS_ERROR_FAILURE;

	const nsString* t;
	t = doc->GetDocumentTitle();

	*title = g_strdup (NS_ConvertUCS2toUTF8(*t).get());

	return NS_OK;
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

nsresult EphyWrapper::ForceCharacterSet (char *charset) 
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

	result = mdv->SetForceCharacterSet (NS_ConvertUTF8toUCS2(charset).get());

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

nsresult EphyWrapper::GetLinkInterfaceItems (GList **list)
{
#ifdef NOT_PORTED
	nsresult result;
	PRUint32 links_count;

	/* we accept these rel=.. elements, specified by the w3c */
	const gchar *rel_types[] = {
		"START", "NEXT", "PREV", "PREVIOUS", "CONTENTS", "TOC", "INDEX",
		"GLOSSARY", "COPYRIGHT", "CHAPTER",  "SECTION",
		"SUBSECTION", "APPENDIX", "HELP", "TOP", "SEARCH", "MADE",
		"BOOKMARK", "HOME",
		NULL /* terminator, must be last */
	};

	nsCOMPtr<nsIDOMDocument> DOMDocument;
	result = GetMainDOMDocument (getter_AddRefs(DOMDocument));
	if (NS_FAILED(result) || !DOMDocument) return NS_ERROR_FAILURE;

	/* get list of link elements*/
	NS_NAMED_LITERAL_STRING(strname, "LINK");

	nsCOMPtr<nsIDOMNodeList> links;
	result = aDOMDocument->GetElementsByTagName (strname, 
						     getter_AddRefs (links));
	if (NS_FAILED (result)) return NS_ERROR_FAILURE;

	result = links->GetLength (&links_count);
	if (NS_FAILED (result)) return NS_ERROR_FAILURE;

	for (PRUint32 i = 0; i < links_count; i++)
	{
		/* get to the link element */
		nsCOMPtr<nsIDOMNode> link;
		result = links->Item (i, getter_AddRefs (link));
		if (NS_FAILED (result)) return NS_ERROR_FAILURE;

		nsCOMPtr<nsIDOMElement> linkElement;
		linkElement = do_QueryInterface (aLink);
		if (!linkElement) return NS_ERROR_FAILURE;

		/* get rel=.. element */
		NS_NAMED_LITERAL_STRING(attr_rel, "rel");
		nsAutoString value;
		linkElement->GetAttribute (attr_rel, value);

		if (value.IsEmpty())
		{
			NS_NAMED_LITERAL_STRING(attr_rev, "rev");
			linkElement->GetAttribute (attr_rev, value);
			if (value.IsEmpty()) continue;
		}

		nsCString relstr = NS_ConvertUCS2toUTF8(value);
		ToUpperCase(relstr);

		/* check for elements we want */
		for (gint j = 0; (rel_types[j] != NULL); j++)
		{
			if (strcmp (relstr.get(), rel_types[j]) == 0)
			{
				/* found one! */
				LinkInterfaceItem *lti =
					g_new0 (LinkInterfaceItem, 1);

				/* fill in struct */
				lti->type = (LinkInterfaceItemType) j;

				/* get href=.. element */
				NS_NAMED_LITERAL_STRING(attr_href, "href");
				nsAutoString value;
				linkElement->GetAttribute (attr_href, value);

				if (value.IsEmpty())
				{
					g_free (lti);
					continue;
				}

				/* resolve uri */
				nsCOMPtr<nsIDocument> doc = 
					do_QueryInterface (aDOMDocument);
				if(!doc) return NS_ERROR_FAILURE;
			
				nsCOMPtr<nsIURI> uri;
				doc->GetDocumentURL(getter_AddRefs(uri));

				const nsACString &link = NS_ConvertUCS2toUTF8(value);
				nsCAutoString href;
				result = uri->Resolve (link, href);
				if (NS_FAILED (result)) return NS_ERROR_FAILURE;
				lti->href = g_strdup (href.get());
		
				/* append to list of items */
				*list = g_list_append (*list, lti);
		
				/* get optional title=... element */
				NS_NAMED_LITERAL_STRING(attr_title, "title");
				linkElement->GetAttribute (attr_title, value);
				if (value.IsEmpty()) continue;

				const nsACString &title = NS_ConvertUCS2toUTF8 (value);
				lti->title = gul_string_strip_newline (PromiseFlatCString(title).get());
			}
		}
	}
#endif
	return NS_OK;
}

nsresult EphyWrapper::GetRealURL (nsCString &ret)
{
	nsresult result;

	nsCOMPtr<nsIDocShell> DocShell;
	result = GetDocShell (getter_AddRefs(DocShell));
	if (NS_FAILED(result) || !DocShell) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIWebNavigation> ContentNav = do_QueryInterface (DocShell,
								   &result);
	if (!ContentNav) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIURI> uri;
	result = ContentNav->GetCurrentURI (getter_AddRefs(uri));
	if (!NS_SUCCEEDED(result) || (!uri)) return NS_ERROR_FAILURE;

	result = uri->GetSpec(ret);
	if (!NS_SUCCEEDED(result) || ret.IsEmpty()) return NS_ERROR_FAILURE;

	return NS_OK;
}

nsresult EphyWrapper::SelectAll (void)
{
	nsCOMPtr<nsIClipboardCommands> clipboard (do_GetInterface(mWebBrowser));
	return clipboard->SelectAll ();
}

nsresult EphyWrapper::ScrollUp (void)
{
	nsresult result;

	nsCOMPtr<nsIDOMWindow> DOMWindow;
	result = GetFocusedDOMWindow (getter_AddRefs(DOMWindow));
	if (NS_FAILED(result) || !DOMWindow) return NS_ERROR_FAILURE;

	DOMWindow->ScrollByLines(-1);

	return NS_OK;
}

nsresult EphyWrapper::ScrollDown (void)
{
	nsresult result;

	nsCOMPtr<nsIDOMWindow> DOMWindow;
	result = GetFocusedDOMWindow (getter_AddRefs(DOMWindow));
	if (NS_FAILED(result) || !DOMWindow) return NS_ERROR_FAILURE;

	DOMWindow->ScrollByLines(1);
	
	return NS_OK;
}

nsresult EphyWrapper::ScrollLeft (void)
{
	nsresult result;

	nsCOMPtr<nsIDOMWindow> DOMWindow;
	result = GetFocusedDOMWindow (getter_AddRefs(DOMWindow));
	if (NS_FAILED(result) || !DOMWindow) return NS_ERROR_FAILURE;

	DOMWindow->ScrollBy(-16, 0);
	
	return NS_OK;
}

nsresult EphyWrapper::ScrollRight (void)
{
	nsresult result;

	nsCOMPtr<nsIDOMWindow> DOMWindow;
	result = GetFocusedDOMWindow (getter_AddRefs(DOMWindow));
	if (NS_FAILED(result) || !DOMWindow) return NS_ERROR_FAILURE;

	DOMWindow->ScrollBy(16, 0);
	
	return NS_OK;
}

nsresult EphyWrapper::FineScroll (int horiz, int vert)
{
	nsresult result;

	nsCOMPtr<nsIDOMWindow> DOMWindow;
	result = GetFocusedDOMWindow (getter_AddRefs(DOMWindow));
	if (NS_FAILED(result) || !DOMWindow) return NS_ERROR_FAILURE;

	DOMWindow->ScrollBy(horiz, vert);
	
	return NS_OK;
}

nsresult EphyWrapper::GetLastModified (gchar **ret)
{
	nsresult result;

	nsCOMPtr<nsIDOMDocument> DOMDocument;

	result = GetDOMDocument (getter_AddRefs(DOMDocument));
	if (NS_FAILED(result) || !DOMDocument) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIDOMNSHTMLDocument> doc = do_QueryInterface(DOMDocument);
	if(!doc) return NS_ERROR_FAILURE;

	nsAutoString value;
	doc->GetLastModified(value);

	*ret = g_strdup (NS_ConvertUCS2toUTF8(value).get());

	return NS_OK;
}

nsresult EphyWrapper::GetImages (GList **ret)
{
#ifdef NOT_PORTED
	nsresult result;
	GHashTable *hash = g_hash_table_new (g_str_hash, g_str_equal);

	nsCOMPtr<nsIDOMDocument> DOMDocument;

	result = GetDOMDocument (getter_AddRefs(DOMDocument));
	if (NS_FAILED(result) || !DOMDocument) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIDOMHTMLDocument> doc = do_QueryInterface(DOMDocument);
	if(!doc) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIDOMHTMLCollection> col;
	doc->GetImages(getter_AddRefs(col));

	PRUint32 count, i;
	col->GetLength(&count);
	for (i = 0; i < count; i++)
	{
		nsCOMPtr<nsIDOMNode> node;
		col->Item(i, getter_AddRefs(node));
		if (!node) continue;

		nsCOMPtr<nsIDOMHTMLElement> element;
		element = do_QueryInterface(node);
		if (!element) continue;
		
		nsCOMPtr<nsIDOMHTMLImageElement> img;
		img = do_QueryInterface(element);
		if (!img) continue;

		ImageListItem *item = g_new0 (ImageListItem, 1);
		
		nsAutoString tmp;
		result = img->GetSrc (tmp);
		if (NS_SUCCEEDED(result))
		{
			const nsACString &c = NS_ConvertUCS2toUTF8(tmp);
			if (g_hash_table_lookup (hash, PromiseFlatCString(c).get()))
			{
				g_free (item);
				continue;
			}
			item->url = g_strdup (c.get());
			g_hash_table_insert (hash, item->url,
					     GINT_TO_POINTER (TRUE));
		}
		result = img->GetAlt (tmp);
		if (NS_SUCCEEDED(result))
		{
			const nsACString &c = NS_ConvertUCS2toUTF8(tmp);
			item->alt = gul_string_strip_newline (PromiseFlatCString(c).get());
		}
		result = element->GetTitle (tmp);
		if (NS_SUCCEEDED(result))
		{
			const nsACString &c = NS_ConvertUCS2toUTF8(tmp);
			item->title = gul_string_strip_newline (PromiseFlatCString(c).get());
		}
		result = img->GetWidth (&(item->width));
		result = img->GetHeight (&(item->height));

		*ret = g_list_append (*ret, item);
	}

	g_hash_table_destroy (hash);
#endif
	return NS_OK;
}

nsresult EphyWrapper::GetForms (GList **ret)
{
#ifdef NOT_PORTED
	nsresult result;

	nsCOMPtr<nsIDOMDocument> DOMDocument;

	result = GetDOMDocument (getter_AddRefs(DOMDocument));
	if (NS_FAILED(result) || !DOMDocument) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIDOMHTMLDocument> doc = do_QueryInterface(DOMDocument);
	if(!doc) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIDOMHTMLCollection> col;
	doc->GetForms(getter_AddRefs(col));

	PRUint32 count, i;
	col->GetLength(&count);
	for (i = 0; i < count; i++)
	{
		nsCOMPtr<nsIDOMNode> node;
		col->Item(i, getter_AddRefs(node));
		if (!node) continue;

		nsCOMPtr<nsIDOMHTMLElement> element;
		element = do_QueryInterface(node);
		if (!element) continue;
		
		nsCOMPtr<nsIDOMHTMLFormElement> form;
		form = do_QueryInterface(element);
		if (!form) continue;

		FormListItem *item = g_new0 (FormListItem, 1);
		
		nsAutoString tmp;
		result = form->GetAction (tmp);
		if (NS_SUCCEEDED(result))
		{
			nsCOMPtr<nsIDocument> doc = 
				do_QueryInterface (aDOMDocument);
			if(!doc) return NS_ERROR_FAILURE;
			
			nsCOMPtr<nsIURI> uri;
			doc->GetDocumentURL(getter_AddRefs(uri));

			const nsACString &s = NS_ConvertUTF8toUCS2(tmp);
			nsCAutoString c;
			result = uri->Resolve (c, s);

			item->action = s.Length() ? g_strdup (s.get()) : g_strdup (c.get());
		}
		result = form->GetMethod (tmp);
		if (NS_SUCCEEDED(result))
		{
			const nsACString &c = NS_ConvertUTF8toUCS2(tmp);
			item->method = g_strdup (PromiseFlatCString(c).get());
		}
		result = form->GetName (tmp);
		if (NS_SUCCEEDED(result))
		{
			const nsACString &c = NS_ConvertUTF8toUCS2(tmp);
			item->name = g_strdup (PromiseFlatCString(c).get());
		}

		*ret = g_list_append (*ret, item);
	}
#endif
	return NS_OK;
}

nsresult EphyWrapper::GetLinks (GList **ret)
{
#ifdef NOT_PORTED
	nsresult result;

	nsCOMPtr<nsIDOMDocument> DOMDocument;
	result = GetMainDOMDocument (getter_AddRefs(DOMDocument));
	if (NS_FAILED(result) || !DOMDocument) return NS_ERROR_FAILURE;

	/* first, get a list of <link> elements */
	PRUint32 links_count;

	NS_NAMED_LITERAL_STRING(strname, "LINK");

	nsCOMPtr<nsIDOMNodeList> links;
	result = DOMDocument->GetElementsByTagName (strname, 
						     getter_AddRefs (links));
	if (NS_FAILED (result)) return NS_ERROR_FAILURE;

	result = aLinks->GetLength (&links_count);
	if (NS_FAILED (result)) return NS_ERROR_FAILURE;

	for (PRUint32 i = 0; i < links_count; i++)
	{
		nsCOMPtr<nsIDOMNode> link;
		result = links->Item (i, getter_AddRefs (link));
		if (NS_FAILED (result)) continue;

		nsCOMPtr<nsIDOMElement> linkElement;
		linkElement = do_QueryInterface (link);
		if (!linkElement) continue;

		NS_NAMED_LITERAL_STRING(attr_href, "href");
		nsAutoString value;
		linkElement->GetAttribute (attr_href, value);
		if (value.IsEmpty()) continue;

		const nsACString &link = NS_ConvertUCS2toUTF8(value);

		if (link.IsEmpty()) continue;
			
		nsCOMPtr<nsIDocument> doc = 
			do_QueryInterface (aDOMDocument);
		if(!doc) continue;
		
		nsCOMPtr<nsIURI> uri;
		doc->GetDocumentURL(getter_AddRefs(uri));

		nsCAutoString tmp;
		result = uri->Resolve (link, tmp);
		
		LinkListItem *i = g_new0 (LinkListItem, 1);

		if (!tmp.IsEmpty())
		{
			i->url = g_strdup (tmp.get());
		}
		else
		{
			i->url = g_strdup (link.get());
		}

		NS_NAMED_LITERAL_STRING(attr_title, "title");
		linkElement->GetAttribute (attr_title, value);
		if (!value.IsEmpty())
		{
			const nsACString &s = NS_ConvertUCS2toUTF8(value);
			i->title = gul_string_strip_newline (PromiseFlatCString(s).get());
		}

		NS_NAMED_LITERAL_STRING(attr_rel, "rel");
		linkElement->GetAttribute (attr_rel, value);
		if (!value.IsEmpty())
		{
			const nsACString &s = NS_ConvertUCS2toUTF8(value);
			i->rel = g_strdup (PromiseFlatCString(s).get());
			g_strdown (i->rel);
		}
		if (!i->rel || strlen (i->rel) == 0)
		{
			NS_NAMED_LITERAL_STRING(attr_rev, "rev");
			linkElement->GetAttribute (attr_rev, value);
			if (!value.IsEmpty())
			{
				const nsACString &s = NS_ConvertUCS2toUTF8(value);
				i->rel = g_strdup (PromiseFlatCString(s).get());
				g_strdown (i->rel);
			}
		}
		
		*ret = g_list_append (*ret, i);
	}

	/* next, get a list of anchors */
	nsCOMPtr<nsIDOMHTMLDocument> doc = do_QueryInterface(aDOMDocument);
	if(!doc) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIDOMHTMLCollection> col;
	doc->GetLinks(getter_AddRefs(col));

	PRUint32 count, i;
	col->GetLength(&count);
	for (i = 0; i < count; i++)
	{
		nsCOMPtr<nsIDOMNode> node;
		col->Item(i, getter_AddRefs(node));
		if (!node) continue;

		nsCOMPtr<nsIDOMHTMLElement> element;
		element = do_QueryInterface(node);
		if (!element) continue;
		
		nsCOMPtr<nsIDOMHTMLAnchorElement> lnk;
		lnk = do_QueryInterface(element);
		if (!lnk) continue;

		LinkListItem *i = g_new0 (LinkListItem, 1);

		nsAutoString tmp;

		result = lnk->GetHref (tmp);
		if (NS_SUCCEEDED(result))
		{
			const nsACString &c = NS_ConvertUCS2toUTF8(tmp);
			i->url = g_strdup (PromiseFlatCString(c).get());
		}

		result = lnk->GetRel (tmp);
		if (NS_SUCCEEDED(result))
		{
			const nsACString &c = NS_ConvertUCS2toUTF8(tmp);
			i->rel = g_strdup (PromiseFlatCString(c).get());
			g_strdown (i->rel);
		}

		if (!i->rel || strlen (i->rel) == 0)
		{
			result = lnk->GetRev (tmp);
			if (NS_SUCCEEDED(result))
			{
				const nsACString &c = NS_ConvertUCS2toUTF8(tmp);
				i->rel = g_strdup (PromiseFlatCString(c).get());
				g_strdown (i->rel);
			}
		}

		i->title = mozilla_get_link_text (node);
		if (i->title == NULL)
		{
			result = element->GetTitle (tmp);
			if (NS_SUCCEEDED(result))
			{
				const nsACString &c = NS_ConvertUCS2toUTF8(tmp);
				i->title = gul_string_strip_newline (PromiseFlatCString(c).get());
			}
		}


		*ret = g_list_append (*ret, i);
	}
#endif
	return NS_OK;
}

nsresult EphyWrapper::EvaluateJS (char *script)
{
	nsresult rv;

	nsCOMPtr<nsIDOMWindow> DOMWindow;
	rv = mWebBrowser->GetContentDOMWindow(getter_AddRefs(DOMWindow));

	nsCOMPtr<nsIScriptGlobalObject> globalObject;
	globalObject = do_QueryInterface (DOMWindow);
	if (!globalObject) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIScriptContext> context;
	rv = globalObject->GetContext(getter_AddRefs(context));
	if (NS_FAILED(rv) || !context) {
		return NS_ERROR_FAILURE;
	}

	context->SetProcessingScriptTag(PR_TRUE);

	PRBool isUndefined;
	nsAutoString ret;
	const nsAString &aScript = NS_ConvertUTF8toUCS2(script);
	context->EvaluateString(aScript, nsnull, nsnull, nsnull,
				0, nsnull, 
				ret, &isUndefined);  

	context->SetProcessingScriptTag(PR_FALSE);

	return NS_OK;
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
