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
#include "EphyUtils.h"
#include "ephy-embed.h"
#include "ephy-string.h"
#include "ephy-debug.h"

#include <gtkmozembed_internal.h>
#include <unistd.h>

#include "nsIInterfaceRequestorUtils.h"
#include "nsIURI.h"
#include "nsISimpleEnumerator.h"

#include "nsIContentViewer.h"
#include "nsIWebBrowserFind.h"
#include "nsIWebBrowserFocus.h"
#include "nsICommandManager.h"
#include "nsIWebBrowserPrint.h"
#include "nsIDocCharset.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIDocShellTreeOwner.h"
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
#include "nsIDOM3Document.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDOMWindow2.h"
#include "nsEmbedString.h"
#include "nsMemory.h"

#ifdef ALLOW_PRIVATE_API
#include "nsIMarkupDocumentViewer.h"
#endif

static PRUnichar DOMLinkAdded[] = { 'D', 'O', 'M', 'L', 'i', 'n', 'k',
				    'A', 'd', 'd', 'e', 'd', '\0' };

EphyEventListener::EphyEventListener(void)
: mOwner(nsnull)
{
	LOG ("EphyEventListener ctor (%p)", this)
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

	PRUnichar relAttr[] = { 'r', 'e', 'l', '\0' };
	nsEmbedString value;
	result = linkElement->GetAttribute (nsEmbedString(relAttr), value);
	if (NS_FAILED (result)) return NS_ERROR_FAILURE;

	nsEmbedCString rel;
	NS_UTF16ToCString (value, NS_CSTRING_ENCODING_UTF8, rel);	

	if (g_ascii_strcasecmp (rel.get(), "SHORTCUT ICON") == 0 ||
	    g_ascii_strcasecmp (rel.get(), "ICON") == 0)
	{
		PRUnichar hrefAttr[] = { 'h', 'r', 'e', 'f', '\0' };
		nsEmbedString value;
		result = linkElement->GetAttribute (nsEmbedString (hrefAttr), value);
		if (NS_FAILED (result) || !value.Length()) return NS_ERROR_FAILURE;

		nsEmbedCString link;
		NS_UTF16ToCString (value, NS_CSTRING_ENCODING_UTF8, link);

		nsCOMPtr<nsIDOMDocument> domDoc;
		node->GetOwnerDocument(getter_AddRefs(domDoc));
		NS_ENSURE_TRUE (domDoc, NS_ERROR_FAILURE);

		nsCOMPtr<nsIDOM3Document> doc = do_QueryInterface (domDoc);
		NS_ENSURE_TRUE (doc, NS_ERROR_FAILURE);

		nsEmbedString spec;
		result = doc->GetDocumentURI (spec);
		NS_ENSURE_SUCCESS (result, result);

		nsCOMPtr<nsIURI> uri;
		result = EphyUtils::NewURI (getter_AddRefs(uri), spec);
		NS_ENSURE_SUCCESS (result, result);

		nsEmbedCString favicon_url;
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
: mFaviconEventListener(nsnull)
, mInitialized(PR_FALSE)
{
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
  	mWebBrowser->GetContentDOMWindow (getter_AddRefs(domWindowExternal));
  
  	nsCOMPtr<nsIDOMWindow2> domWindow (do_QueryInterface (domWindowExternal));
	NS_ENSURE_TRUE (domWindow, NS_ERROR_FAILURE);
	
  	nsCOMPtr<nsIDOMEventTarget> rootWindow;
  	domWindow->GetWindowRoot (getter_AddRefs(rootWindow));

  	mEventReceiver = do_QueryInterface (rootWindow);
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

	rv = target->AddEventListener(nsEmbedString(DOMLinkAdded),
				      mFaviconEventListener, PR_FALSE);
	NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

	return NS_OK;
}

nsresult
EphyBrowser::DetachListeners(void)
{
	nsresult rv;

	if (!mEventReceiver) return NS_OK;

	nsCOMPtr<nsIDOMEventTarget> target;
	target = do_QueryInterface (mEventReceiver);
	NS_ENSURE_TRUE (target, NS_ERROR_FAILURE);

	rv = target->RemoveEventListener(nsEmbedString(DOMLinkAdded),
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
	mDOMWindow = nsnull;
	mEventReceiver = nsnull;

	mInitialized = PR_FALSE;

	return NS_OK;
}

nsresult EphyBrowser::GoToHistoryIndex (PRInt16 index)
{
	NS_ENSURE_TRUE (mWebBrowser, NS_ERROR_FAILURE);

	nsCOMPtr<nsIWebNavigation> ContentNav = do_QueryInterface (mWebBrowser);
	NS_ENSURE_TRUE (ContentNav, NS_ERROR_FAILURE);

	return ContentNav->GotoIndex (index);
}

nsresult EphyBrowser::SetZoom (float aZoom)
{
	NS_ENSURE_TRUE (mWebBrowser, NS_ERROR_FAILURE);

	nsCOMPtr<nsIContentViewer> contentViewer;	
	GetContentViewer (getter_AddRefs(contentViewer));
	NS_ENSURE_TRUE (contentViewer, NS_ERROR_FAILURE);

	nsCOMPtr<nsIMarkupDocumentViewer> mdv = do_QueryInterface(contentViewer);
	NS_ENSURE_TRUE (mdv, NS_ERROR_FAILURE);

	return mdv->SetTextZoom (aZoom);
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
	NS_ENSURE_TRUE (mWebBrowser, NS_ERROR_FAILURE);

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
	NS_ENSURE_TRUE (mWebBrowser, NS_ERROR_FAILURE);

	nsCOMPtr<nsISHistory> SessionHistory;
	GetSHistory (getter_AddRefs(SessionHistory));
	NS_ENSURE_TRUE (SessionHistory, NS_ERROR_FAILURE);

	SessionHistory->GetCount (count);
	SessionHistory->GetIndex (index);	

	return NS_OK;
}

nsresult EphyBrowser::GetSHTitleAtIndex (PRInt32 index, PRUnichar **title)
{
	NS_ENSURE_TRUE (mWebBrowser, NS_ERROR_FAILURE);

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

nsresult EphyBrowser::GetSHUrlAtIndex (PRInt32 index, nsACString &url)
{
	NS_ENSURE_TRUE (mWebBrowser, NS_ERROR_FAILURE);

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
	NS_ENSURE_TRUE (NS_SUCCEEDED (result) && url.Length(), NS_ERROR_FAILURE);

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

nsresult EphyBrowser::GetDocumentUrl (nsACString &url)
{
	nsresult rv;

	NS_ENSURE_TRUE (mDOMWindow, NS_ERROR_FAILURE);

	nsCOMPtr<nsIDOMDocument> DOMDocument;
	mDOMWindow->GetDocument (getter_AddRefs(DOMDocument));
	NS_ENSURE_TRUE (DOMDocument, NS_ERROR_FAILURE);

	nsCOMPtr<nsIDOM3Document> doc = do_QueryInterface(DOMDocument);
	NS_ENSURE_TRUE (doc, NS_ERROR_FAILURE);

	nsEmbedString docURI;
	rv = doc->GetDocumentURI (docURI);
	NS_ENSURE_SUCCESS (rv, rv);

	NS_UTF16ToCString (docURI, NS_CSTRING_ENCODING_UTF8, url);

	return NS_OK;
}

nsresult EphyBrowser::GetTargetDocumentUrl (nsACString &url)
{
	nsresult rv;

	NS_ENSURE_TRUE (mWebBrowser, NS_ERROR_FAILURE);

        nsCOMPtr<nsIDOMDocument> DOMDocument;
	GetTargetDocument (getter_AddRefs(DOMDocument));
	NS_ENSURE_TRUE (DOMDocument, NS_ERROR_FAILURE);

	nsCOMPtr<nsIDOM3Document> doc = do_QueryInterface(DOMDocument);
	NS_ENSURE_TRUE (doc, NS_ERROR_FAILURE);

	nsEmbedString docURI;
	rv = doc->GetDocumentURI (docURI);
	NS_ENSURE_SUCCESS (rv, rv);

	NS_UTF16ToCString (docURI, NS_CSTRING_ENCODING_UTF8, url);

	return NS_OK;
}

nsresult EphyBrowser::ForceEncoding (const char *encoding) 
{
	NS_ENSURE_TRUE (mWebBrowser, NS_ERROR_FAILURE);

	nsCOMPtr<nsIContentViewer> contentViewer;	
	GetContentViewer (getter_AddRefs(contentViewer));
	NS_ENSURE_TRUE (contentViewer, NS_ERROR_FAILURE);

	nsCOMPtr<nsIMarkupDocumentViewer> mdv = do_QueryInterface(contentViewer);
	NS_ENSURE_TRUE (mdv, NS_ERROR_FAILURE);

	return mdv->SetForceCharacterSet (nsEmbedCString(encoding));
}

nsresult EphyBrowser::GetEncoding (nsACString &encoding)
{
	NS_ENSURE_TRUE (mWebBrowser, NS_ERROR_FAILURE);

	nsCOMPtr<nsIDocCharset> docCharset = do_GetInterface (mWebBrowser);
	NS_ENSURE_TRUE (docCharset, NS_ERROR_FAILURE);

	char *charset;
	docCharset->GetCharset (&charset);
	encoding = charset;
	nsMemory::Free (charset);

	return NS_OK;
}

nsresult EphyBrowser::GetForcedEncoding (nsACString &encoding)
{
	NS_ENSURE_TRUE (mWebBrowser, NS_ERROR_FAILURE);

	nsCOMPtr<nsIContentViewer> contentViewer;	
	GetContentViewer (getter_AddRefs(contentViewer));
	NS_ENSURE_TRUE (contentViewer, NS_ERROR_FAILURE);

	nsCOMPtr<nsIMarkupDocumentViewer> mdv = do_QueryInterface(contentViewer);
	NS_ENSURE_TRUE (mdv, NS_ERROR_FAILURE);

	nsresult result = mdv->GetForceCharacterSet (encoding);
	NS_ENSURE_SUCCESS (result, NS_ERROR_FAILURE);

	return NS_OK;
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

nsresult EphyBrowser::DoCommand (const char *command)
{
	NS_ENSURE_TRUE (mWebBrowser, NS_ERROR_FAILURE);

	nsCOMPtr<nsICommandManager> cmdManager;
	cmdManager = do_GetInterface (mWebBrowser);
	NS_ENSURE_TRUE (cmdManager, NS_ERROR_FAILURE);

	return cmdManager->DoCommand (command, nsnull, nsnull);
}

nsresult EphyBrowser::GetCommandState (const char *command, PRBool *enabled)
{
	NS_ENSURE_TRUE (mWebBrowser, NS_ERROR_FAILURE);

	nsCOMPtr<nsICommandManager> cmdManager;
	cmdManager = do_GetInterface (mWebBrowser);
	NS_ENSURE_TRUE (cmdManager, NS_ERROR_FAILURE);

	return cmdManager->IsCommandEnabled (command, nsnull, enabled);
}

#define NUM_MODIFIED_TEXTFIELDS_REQUIRED	2

PRBool
EphyBrowser::CompareFormsText (nsAString &aDefaultText, nsAString &aUserText)
{
	if (aDefaultText.Length() != aUserText.Length())
	{
		return FALSE;
	}

	/* Mozilla Bug 218277, 195946 and others */
	const PRUnichar *text = aDefaultText.BeginReading();
	for (PRUint32 i = 0; i < aDefaultText.Length(); i++)
	{
		if (text[i] == 0xa0)
		{
			aDefaultText.Replace (i, 1, ' ');
		}
	}

	return (memcmp (aDefaultText.BeginReading(),
		        aUserText.BeginReading(),
		        aUserText.Length() * sizeof (PRUnichar)) == 0);
}

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
				nsEmbedString defaultText, userText;
				areaElement->GetDefaultValue (defaultText);
				areaElement->GetValue (userText);

				if (!CompareFormsText (defaultText, userText))
				{
					*aHasTextArea = PR_TRUE;
					return NS_OK;
				}

				continue;
			}

			nsCOMPtr<nsIDOMHTMLInputElement> inputElement = do_QueryInterface(domNode);
			if (!inputElement) continue;
	
			nsEmbedString type;
			inputElement->GetType(type);

			nsEmbedCString cType;
			NS_UTF16ToCString (type, NS_CSTRING_ENCODING_UTF8, cType);

			if (g_ascii_strcasecmp (cType.get(), "text") == 0)
			{
				nsEmbedString defaultText, userText;
				PRInt32 max_length;
				inputElement->GetDefaultValue (defaultText);
				inputElement->GetValue (userText);
				inputElement->GetMaxLength (&max_length);

				/* Guard against arguably broken forms where
				 * defaultText is longer than maxlength
				 * (userText is cropped, defaultText is not)
				 * Mozilla bug 232057
				 */
				if (defaultText.Length() > (PRUint32)max_length)
				{
					defaultText.Cut (max_length, PR_UINT32_MAX);
				}

				if (!CompareFormsText (defaultText, userText))
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

	NS_ENSURE_TRUE (mWebBrowser, NS_ERROR_FAILURE);

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
