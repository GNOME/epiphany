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

#include "mozilla-config.h"

#include "config.h"

#include "EphyBrowser.h"
#include "EphyUtils.h"
#include "EventContext.h"
#include "ephy-embed.h"
#include "ephy-string.h"
#include "ephy-debug.h"
#include "print-dialog.h"
#include "mozilla-embed.h"
#include "mozilla-embed-event.h"

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
#include "nsIDOMKeyEvent.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMNSEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMPopupBlockedEvent.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDOMWindow2.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMAbstractView.h"
#undef MOZILLA_INTERNAL_API
#include "nsEmbedString.h"
#define MOZILLA_INTERNAL_API 1
#include "nsMemory.h"
#include "nsIChannel.h"
#include "nsIScriptSecurityManager.h"
#include "nsIServiceManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMXMLDocument.h"

#ifdef ALLOW_PRIVATE_API
#include "nsIImageDocument.h"
/* not frozen yet */
#include "nsIContentPolicy.h"
/* will never be frozen */
#include "nsIDocShell.h"
#include "nsIMarkupDocumentViewer.h"
#ifdef HAVE_MOZILLA_PSM
/* not sure about this one: */
#include <nsITransportSecurityInfo.h>
/* these are in pipnss/, are they really private? */
#include <nsISSLStatus.h>
#include <nsISSLStatusProvider.h>
#include <nsIX509Cert.h>
#include <nsICertificateDialogs.h>
#endif
#endif

static PRUnichar DOMLinkAdded[] = { 'D', 'O', 'M', 'L', 'i', 'n', 'k',
				    'A', 'd', 'd', 'e', 'd', '\0' };
static PRUnichar ContextMenu[] = { 'c', 'o', 'n', 't', 'e', 'x', 't', 'm',
				   'e', 'n', 'u', '\0' };
static PRUnichar DOMPopupBlocked[] = { 'D', 'O', 'M', 'P', 'o', 'p',
				       'u', 'p', 'B', 'l', 'o', 'c',
				       'k', 'e', 'd', '\0' };
static PRUnichar DOMWillOpenModalDialog[] = { 'D', 'O', 'M', 'W', 'i', 'l', 'l',
					      'O', 'p', 'e', 'n', 'M', 'o', 'd',
					      'a', 'l', 'D', 'i', 'a', 'l', 'o',
					      'g', '\0' };
static PRUnichar DOMModalDialogClosed[] = { 'D', 'O', 'M', 'M', 'o', 'd', 'a',
					    'l', 'D', 'i', 'a', 'l', 'o', 'g',
					    'C', 'l', 'o', 's', 'e', 'd', '\0' };

EphyEventListener::EphyEventListener()
: mOwner(nsnull)
{
	LOG ("EphyEventListener ctor (%p)", this);
}

EphyEventListener::~EphyEventListener()
{
	LOG ("EphyEventListener dtor (%p)", this);
}

NS_IMPL_ISUPPORTS1(EphyEventListener, nsIDOMEventListener)

nsresult
EphyEventListener::Init (EphyBrowser *aOwner)
{
	mOwner = aOwner;
	return NS_OK;
}

NS_IMETHODIMP
EphyFaviconEventListener::HandleEvent(nsIDOMEvent* aDOMEvent)
{
#ifdef MOZ_NSIDOMNSEVENT_GETISTRUSTED
	/* make sure the event is trusted */
	nsCOMPtr<nsIDOMNSEvent> nsEvent (do_QueryInterface (aDOMEvent));
	NS_ENSURE_TRUE (nsEvent, NS_ERROR_FAILURE);
	PRBool isTrusted = PR_FALSE;
	nsEvent->GetIsTrusted (&isTrusted);
	if (!isTrusted) return NS_OK;
#endif /* MOZ_NSIDOMNSEVENT_GETISTRUSTED */

	nsCOMPtr<nsIDOMEventTarget> eventTarget;
	aDOMEvent->GetTarget(getter_AddRefs(eventTarget));

	nsCOMPtr<nsIDOMNode> node = do_QueryInterface(eventTarget);
	NS_ENSURE_TRUE (node, NS_ERROR_FAILURE);

	nsCOMPtr<nsIDOMElement> linkElement;
	linkElement = do_QueryInterface (node);
	if (!linkElement) return NS_ERROR_FAILURE;

	PRUnichar relAttr[] = { 'r', 'e', 'l', '\0' };
	nsresult rv;
	nsEmbedString value;
	rv = linkElement->GetAttribute (nsEmbedString(relAttr), value);
	if (NS_FAILED (rv)) return NS_ERROR_FAILURE;

	nsEmbedCString rel;
	NS_UTF16ToCString (value, NS_CSTRING_ENCODING_UTF8, rel);	

	if (g_ascii_strcasecmp (rel.get(), "SHORTCUT ICON") == 0 ||
	    g_ascii_strcasecmp (rel.get(), "ICON") == 0)
	{
		PRUnichar hrefAttr[] = { 'h', 'r', 'e', 'f', '\0' };
		nsEmbedString hrefValue;
		rv = linkElement->GetAttribute (nsEmbedString (hrefAttr), hrefValue);
		if (NS_FAILED (rv) || !hrefValue.Length()) return NS_ERROR_FAILURE;

		nsEmbedCString link;
		NS_UTF16ToCString (hrefValue, NS_CSTRING_ENCODING_UTF8, link);

		nsCOMPtr<nsIDOMDocument> domDoc;
		node->GetOwnerDocument(getter_AddRefs(domDoc));
		NS_ENSURE_TRUE (domDoc, NS_ERROR_FAILURE);

		nsCOMPtr<nsIDOMDocumentView> docView (do_QueryInterface (domDoc));
		NS_ENSURE_TRUE (docView, NS_ERROR_FAILURE);

		nsCOMPtr<nsIDOMAbstractView> abstractView;
		docView->GetDefaultView (getter_AddRefs (abstractView));

		nsCOMPtr<nsIDOMWindow> domWin (do_QueryInterface (abstractView));
		NS_ENSURE_TRUE (domWin, NS_ERROR_FAILURE);

		nsCOMPtr<nsIDOMWindow> topDomWin;
		domWin->GetTop (getter_AddRefs (topDomWin));

		nsCOMPtr<nsISupports> domWinAsISupports (do_QueryInterface (domWin));
		nsCOMPtr<nsISupports> topDomWinAsISupports (do_QueryInterface (topDomWin));
		/* disallow subframes to set favicon */
		if (domWinAsISupports != topDomWinAsISupports) return NS_OK;

		nsCOMPtr<nsIDOM3Document> doc (do_QueryInterface (domDoc));
		NS_ENSURE_TRUE (doc, NS_ERROR_FAILURE);

		nsEmbedString spec;
		rv = doc->GetDocumentURI (spec);
		NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

                nsEmbedCString encoding;
                mOwner->GetEncoding (encoding);

		nsCOMPtr<nsIURI> docUri;
		EphyUtils::NewURI (getter_AddRefs(docUri), spec, encoding.get());
		NS_ENSURE_TRUE (docUri, NS_ERROR_FAILURE);

		nsEmbedCString faviconUrl;
		rv = docUri->Resolve (link, faviconUrl);
		NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

		nsCOMPtr<nsIURI> favUri;
		EphyUtils::NewURI (getter_AddRefs (favUri), faviconUrl);
		NS_ENSURE_TRUE (favUri, NS_ERROR_FAILURE);

		/* check if load is allowed */
		nsCOMPtr<nsIScriptSecurityManager> secMan
			(do_GetService("@mozilla.org/scriptsecuritymanager;1"));
		/* refuse if we can't check */
		NS_ENSURE_TRUE (secMan, NS_OK);

		rv = secMan->CheckLoadURI(docUri, favUri,
					  nsIScriptSecurityManager::STANDARD);
		/* failure means it didn't pass the security check */
		if (NS_FAILED (rv)) return NS_OK;

		/* security check passed, now check with content policy */
		nsCOMPtr<nsIContentPolicy> policy =
			do_GetService("@mozilla.org/layout/content-policy;1");
		/* refuse if we can't check */
		NS_ENSURE_TRUE (policy, NS_OK);

#if MOZ_NSICONTENTPOLICY_VARIANT == 2
		/* FIXME: mozilla tabbrowser.xml passes
		 * safeGetProperty(event.target, "type") as mimetype guess:
		 */
		PRUnichar typeAttr[] = { 't', 'y', 'p', 'e', '\0' };
		nsEmbedString typeVal;
		linkElement->GetAttribute (nsEmbedString (typeAttr), typeVal);

		nsEmbedCString cTypeVal;
		NS_UTF16ToCString (typeVal, NS_CSTRING_ENCODING_UTF8, cTypeVal);

		PRInt16 decision = 0;
		rv = policy->ShouldLoad (nsIContentPolicy::TYPE_IMAGE,
					 favUri, docUri, eventTarget,
					 cTypeVal, nsnull,
					 &decision);
		NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);
		if (decision != nsIContentPolicy::ACCEPT) return NS_OK;
#else
		PRBool shouldLoad = PR_FALSE;
		rv = policy->ShouldLoad (nsIContentPolicy::IMAGE,
					 favUri, eventTarget,
					 domWin,
					 &shouldLoad);
		NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);
		if (!shouldLoad) return NS_OK;
#endif
		
		/* Hide password part */
		nsEmbedCString user;
		favUri->GetUsername (user);
		favUri->SetUserPass (user);

		nsEmbedCString favUriSpec;
		favUri->GetSpec (favUriSpec);

		/* ok, we accept this as a valid favicon for this site */
		g_signal_emit_by_name (mOwner->mEmbed, "ge_favicon", favUriSpec.get());
	}

	return NS_OK;
}

NS_IMETHODIMP
EphyPopupBlockEventListener::HandleEvent (nsIDOMEvent * aDOMEvent)
{
#ifdef MOZ_NSIDOMNSEVENT_GETISTRUSTED
	/* make sure the event is trusted */
	nsCOMPtr<nsIDOMNSEvent> nsEvent (do_QueryInterface (aDOMEvent));
	NS_ENSURE_TRUE (nsEvent, NS_ERROR_FAILURE);
	PRBool isTrusted = PR_FALSE;
	nsEvent->GetIsTrusted (&isTrusted);
	if (!isTrusted) return NS_OK;
#endif /* MOZ_NSIDOMNSEVENT_GETISTRUSTED */

	nsCOMPtr<nsIDOMPopupBlockedEvent> popupEvent =
		do_QueryInterface (aDOMEvent);
	NS_ENSURE_TRUE (popupEvent, NS_ERROR_FAILURE);

	nsCOMPtr<nsIURI> popupWindowURI;
	popupEvent->GetPopupWindowURI (getter_AddRefs (popupWindowURI));
	NS_ENSURE_TRUE (popupWindowURI, NS_ERROR_FAILURE);

	nsresult rv;
	nsEmbedCString popupWindowURIString;
	rv = popupWindowURI->GetSpec (popupWindowURIString);
	NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

	nsEmbedString popupWindowFeatures;
	rv = popupEvent->GetPopupWindowFeatures (popupWindowFeatures);
	NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

	nsEmbedCString popupWindowFeaturesString;
	NS_UTF16ToCString (popupWindowFeatures,
			   NS_CSTRING_ENCODING_UTF8,
			   popupWindowFeaturesString);

	g_signal_emit_by_name(mOwner->mEmbed, "ge_popup_blocked",
			      popupWindowURIString.get(),
			      popupWindowFeaturesString.get());

	return NS_OK;
}

NS_IMETHODIMP
EphyModalAlertEventListener::HandleEvent (nsIDOMEvent * aDOMEvent)
{
#ifdef MOZ_NSIDOMNSEVENT_GETISTRUSTED
	NS_ENSURE_TRUE (mOwner, NS_ERROR_FAILURE);

	/* make sure the event is trusted */
	nsCOMPtr<nsIDOMNSEvent> nsEvent (do_QueryInterface (aDOMEvent));
	NS_ENSURE_TRUE (nsEvent, NS_ERROR_FAILURE);
	PRBool isTrusted = PR_FALSE;
	nsEvent->GetIsTrusted (&isTrusted);
	if (!isTrusted) return NS_OK;

	nsresult rv;
	nsEmbedString type;
	rv = aDOMEvent->GetType (type);
	NS_ENSURE_SUCCESS (rv, rv);

	nsEmbedCString cType;
	NS_UTF16ToCString (type, NS_CSTRING_ENCODING_UTF8, cType);

	LOG ("ModalAlertListener event %s", cType.get());

	if (strcmp (cType.get(), "DOMWillOpenModalDialog") == 0)
	{
		gboolean retval = FALSE;
		g_signal_emit_by_name (mOwner->mEmbed, "ge-modal-alert", &retval);

		/* suppress alert */
		if (retval)
		{
			aDOMEvent->PreventDefault ();
			aDOMEvent->StopPropagation();
		}
	}
	else if (strcmp (cType.get(), "DOMModalDialogClosed") == 0)
	{
		g_signal_emit_by_name (mOwner->mEmbed, "ge-modal-alert-closed");
	}
#endif

	return NS_OK;
}

EphyContextMenuListener::EphyContextMenuListener()
: mOwner(nsnull)
{
	LOG ("EphyContextMenuListener ctor (%p)", this);
}

EphyContextMenuListener::~EphyContextMenuListener()
{
	LOG ("EphyContextMenuListener dtor (%p)", this);
}

NS_IMPL_ISUPPORTS1(EphyContextMenuListener, nsIDOMContextMenuListener)

nsresult
EphyContextMenuListener::Init(EphyBrowser *aOwner)
{
	mOwner = aOwner;
	return NS_OK;
}

NS_IMETHODIMP
EphyContextMenuListener::ContextMenu (nsIDOMEvent* aDOMEvent)
{
	nsCOMPtr<nsIDOMMouseEvent> mouseEvent = do_QueryInterface(aDOMEvent);
	NS_ENSURE_TRUE (mouseEvent, NS_ERROR_FAILURE);

	MozillaEmbedEvent *info;
	info = mozilla_embed_event_new (NS_STATIC_CAST (gpointer, aDOMEvent));

	nsresult rv;
	EventContext context;
	context.Init (mOwner);
        rv = context.GetMouseEventInfo (mouseEvent, MOZILLA_EMBED_EVENT (info));

	/* Don't do any magic handling if we can't actually show the context
	 * menu, this can happen for XUL pages (e.g. about:config)
	 */
	if (NS_FAILED (rv))
	{
		g_object_unref (info);
		return NS_OK;   
	}

	if (info->button == 0)
	{
		/* Translate relative coordinates to absolute values, and try
		 * to avoid covering links by adding a little offset
		 */
		int x, y;
		gdk_window_get_origin (GTK_WIDGET (mOwner->mEmbed)->window, &x, &y);
		info->x += x + 6;       
		info->y += y + 6;

		// Set the keycode to something sensible
		info->keycode = nsIDOMKeyEvent::DOM_VK_CONTEXT_MENU;
	}

	if (info->modifier == GDK_CONTROL_MASK)
	{
		info->context = EPHY_EMBED_CONTEXT_DOCUMENT;
	}

	gboolean retval = FALSE;
	nsCOMPtr<nsIDOMDocument> domDoc;
	rv = context.GetTargetDocument (getter_AddRefs(domDoc));
	if (NS_SUCCEEDED(rv))
	{
		mOwner->PushTargetDocument (domDoc);

		g_signal_emit_by_name (mOwner->mEmbed, "ge_context_menu",
				       info, &retval);

		mOwner->PopTargetDocument ();
	}

	/* We handled the event, block javascript calls */
	if (retval)
	{
		aDOMEvent->PreventDefault();
		aDOMEvent->StopPropagation();
	}
	
	g_object_unref (info);

	return NS_OK;
}

NS_IMETHODIMP
EphyContextMenuListener::HandleEvent (nsIDOMEvent* aDOMEvent)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

EphyBrowser::EphyBrowser ()
: mFaviconEventListener(nsnull)
, mPopupBlockEventListener(nsnull)
, mModalAlertListener(nsnull)
, mContextMenuListener(nsnull)
, mInitialized(PR_FALSE)
{
	LOG ("EphyBrowser ctor (%p)", this);
}

EphyBrowser::~EphyBrowser ()
{
	LOG ("EphyBrowser dtor (%p)", this);
}

nsresult EphyBrowser::Init (GtkMozEmbed *mozembed)
{
	if (mInitialized) return NS_OK;

	mEmbed = GTK_WIDGET (mozembed);

	gtk_moz_embed_get_nsIWebBrowser (mozembed,
					 getter_AddRefs(mWebBrowser));
	NS_ENSURE_TRUE (mWebBrowser, NS_ERROR_FAILURE);

	mWebBrowserFocus = do_QueryInterface (mWebBrowser);
	NS_ENSURE_TRUE (mWebBrowserFocus, NS_ERROR_FAILURE);

	mWebBrowser->GetContentDOMWindow (getter_AddRefs (mDOMWindow));
	NS_ENSURE_TRUE (mDOMWindow, NS_ERROR_FAILURE);

	/* This will instantiate an about:blank doc if necessary */
	nsresult rv;
	nsCOMPtr<nsIDOMDocument> domDocument;
	rv = mDOMWindow->GetDocument (getter_AddRefs (domDocument));
	NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

	mFaviconEventListener = new EphyFaviconEventListener();
	if (!mFaviconEventListener) return NS_ERROR_OUT_OF_MEMORY;

	rv = mFaviconEventListener->Init (this);
	NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

	mPopupBlockEventListener = new EphyPopupBlockEventListener();
	if (!mPopupBlockEventListener) return NS_ERROR_OUT_OF_MEMORY;

	rv = mPopupBlockEventListener->Init (this);
	NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

	mModalAlertListener = new EphyModalAlertEventListener ();
	if (!mModalAlertListener) return NS_ERROR_OUT_OF_MEMORY;

	rv = mModalAlertListener->Init (this);
	NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

	mContextMenuListener = new EphyContextMenuListener();
	if (!mContextMenuListener) return NS_ERROR_OUT_OF_MEMORY;

	rv = mContextMenuListener->Init (this);
	NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

 	rv = GetListener();
	NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

	rv = AttachListeners();
	NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

#ifdef HAVE_MOZILLA_PSM
#ifdef HAVE_GECKO_1_8
	nsCOMPtr<nsIDocShell> docShell (do_GetInterface (mWebBrowser, &rv));
	NS_ENSURE_SUCCESS (rv, rv);

	rv = docShell->GetSecurityUI (getter_AddRefs (mSecurityInfo));
	NS_ENSURE_SUCCESS (rv, rv);
#else
	/* FIXME: mozilla sucks! nsWebBrowser already has an instance of this,
	 * but we cannot get to it!
	 * See https://bugzilla.mozilla.org/show_bug.cgi?id=94974
	 */
	/* First try GI */
	mSecurityInfo = do_GetInterface (mWebBrowser);
	/* Try to instantiate it under the re-registered contract ID */
	if (!mSecurityInfo)
	{
		/* This will cause all security warning dialogs to be shown
		 * twice (once by this instance, and another time by nsWebBrowser's
		 * instance of nsSecurityBrowserUIImpl), but there appears to be
		 * no other way :-(
		 */
		mSecurityInfo = do_CreateInstance("@gnome.org/project/epiphany/hacks/secure-browser-ui;1", &rv);
		if (NS_SUCCEEDED (rv) && mSecurityInfo)
		{
			rv = mSecurityInfo->Init (mDOMWindow);
			NS_ENSURE_SUCCESS (rv, rv);
		}
	}
	/* Try the original contract ID */
	if (!mSecurityInfo)
	{
		/* This will cause all security warning dialogs to be shown
		 * twice (once by this instance, and another time by nsWebBrowser's
		 * instance of nsSecurityBrowserUIImpl), but there appears to be
		 * no other way :-(
		 */
		mSecurityInfo = do_CreateInstance(NS_SECURE_BROWSER_UI_CONTRACTID, &rv);
		if (NS_SUCCEEDED (rv) && mSecurityInfo)
		{
			rv = mSecurityInfo->Init (mDOMWindow);
			NS_ENSURE_SUCCESS (rv, rv);
		}
	}
#endif /* HAVE_GECKO_1_8 */
	if (!mSecurityInfo)
	{
		g_warning ("Failed to get nsISecureBrowserUI!\n");
 	}
#endif /* HAVE_MOZILLA_PSM */

	mInitialized = PR_TRUE;

	return NS_OK;
}

nsresult
EphyBrowser::GetListener (void)
{
  	if (mEventTarget) return NS_ERROR_FAILURE;

  	nsCOMPtr<nsIDOMWindow> domWindowExternal;
  	mWebBrowser->GetContentDOMWindow (getter_AddRefs(domWindowExternal));
  
  	nsCOMPtr<nsIDOMWindow2> domWindow (do_QueryInterface (domWindowExternal));
	NS_ENSURE_TRUE (domWindow, NS_ERROR_FAILURE);
	
  	domWindow->GetWindowRoot (getter_AddRefs(mEventTarget));
	NS_ENSURE_TRUE (mEventTarget, NS_ERROR_FAILURE);

	return NS_OK;
}

nsresult
EphyBrowser::AttachListeners(void)
{
	NS_ENSURE_TRUE (mEventTarget, NS_ERROR_FAILURE);

	nsresult rv;
	rv = mEventTarget->AddEventListener(nsEmbedString(DOMLinkAdded),
					    mFaviconEventListener, PR_FALSE);
	rv |= mEventTarget->AddEventListener(nsEmbedString(DOMPopupBlocked),
					     mPopupBlockEventListener, PR_FALSE);
	rv |= mEventTarget->AddEventListener(nsEmbedString(DOMWillOpenModalDialog),
					     mModalAlertListener, PR_TRUE);
	rv |= mEventTarget->AddEventListener(nsEmbedString(DOMModalDialogClosed),
					     mModalAlertListener, PR_TRUE);
	rv |= mEventTarget->AddEventListener(nsEmbedString(ContextMenu),
					     mContextMenuListener, PR_TRUE /* capture */);
	NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

	return NS_OK;
}

nsresult
EphyBrowser::DetachListeners(void)
{
	if (!mEventTarget) return NS_OK;

	nsresult rv;
	rv = mEventTarget->RemoveEventListener(nsEmbedString(DOMLinkAdded),
					       mFaviconEventListener, PR_FALSE);
	rv |= mEventTarget->RemoveEventListener(nsEmbedString(DOMPopupBlocked),
					        mPopupBlockEventListener, PR_FALSE);
	rv |= mEventTarget->RemoveEventListener(nsEmbedString(DOMWillOpenModalDialog),
						mModalAlertListener, PR_TRUE);
	rv |= mEventTarget->RemoveEventListener(nsEmbedString(DOMModalDialogClosed),
						mModalAlertListener, PR_TRUE);
	rv |= mEventTarget->RemoveEventListener(nsEmbedString(ContextMenu),
					        mContextMenuListener, PR_TRUE /* capture */);
	NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

	return NS_OK;
}

nsresult EphyBrowser::Print ()
{
	NS_ENSURE_TRUE (mWebBrowser, NS_ERROR_FAILURE);

	nsCOMPtr<nsIWebBrowserPrint> print(do_GetInterface(mWebBrowser));
	NS_ENSURE_TRUE (print, NS_ERROR_FAILURE);

	return  print->Print (nsnull, nsnull);
}

nsresult EphyBrowser::SetPrintPreviewMode (PRBool previewMode)
{
	nsresult rv;

	NS_ENSURE_TRUE (mWebBrowser, NS_ERROR_FAILURE);

	nsCOMPtr<nsIWebBrowserPrint> print(do_GetInterface(mWebBrowser));
	NS_ENSURE_TRUE (print, NS_ERROR_FAILURE);

	if (previewMode)
	{
		EmbedPrintInfo *info;

		nsCOMPtr<nsIPrintSettings> settings;
		print->GetGlobalPrintSettings (getter_AddRefs(settings));

		info = ephy_print_get_print_info ();
		EphyUtils::CollatePrintSettings (info, settings, TRUE);
		ephy_print_info_free (info);

		rv = print->PrintPreview (nsnull, mDOMWindow, nsnull);
	}
	else
	{
		PRBool isPreview = PR_FALSE;

		rv = print->GetDoingPrintPreview(&isPreview);
		NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

		if (isPreview == PR_TRUE)
		{
			rv = print->ExitPrintPreview();
		}
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

nsresult EphyBrowser::GetSHistory (nsISHistory **aSHistory)
{
	NS_ENSURE_TRUE (mWebBrowser, NS_ERROR_FAILURE);

	nsCOMPtr<nsIWebNavigation> ContentNav = do_QueryInterface (mWebBrowser);
	NS_ENSURE_TRUE (ContentNav, NS_ERROR_FAILURE);

	nsCOMPtr<nsISHistory> SessionHistory;
	ContentNav->GetSessionHistory (getter_AddRefs (SessionHistory));
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
	mEventTarget = nsnull;
	mEmbed = nsnull;

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
	NS_ENSURE_TRUE (mWebBrowser, NS_ERROR_FAILURE);

	/* Use the current target document */
	if (mTargetDocument)
	{
		*aDOMDocument = mTargetDocument.get();

		NS_IF_ADDREF(*aDOMDocument);

		return NS_OK;
	}

	/* Use the focused document */
	nsresult rv;
	nsCOMPtr<nsIDOMWindow> DOMWindow;
	rv = mWebBrowserFocus->GetFocusedWindow (getter_AddRefs(DOMWindow));
	if (NS_SUCCEEDED (rv) && DOMWindow)
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

	nsresult rv;
	rv = he->GetTitle (title);
	NS_ENSURE_TRUE (NS_SUCCEEDED (rv) && title, NS_ERROR_FAILURE);

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

	nsresult rv;
	rv = uri->GetSpec(url);
	NS_ENSURE_TRUE (NS_SUCCEEDED (rv) && url.Length(), NS_ERROR_FAILURE);

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

nsresult EphyBrowser::GetDOMWindow (nsIDOMWindow **aDOMWindow)
{
	NS_ENSURE_TRUE (mWebBrowser, NS_ERROR_FAILURE);

	NS_IF_ADDREF (*aDOMWindow = mDOMWindow);

	return NS_OK;
}

nsresult EphyBrowser::GetDocumentURI (nsIURI **aURI)
{
	if (!mDOMWindow) return NS_ERROR_NOT_INITIALIZED;

	nsresult rv;
	nsCOMPtr<nsIWebNavigation> webNav (do_GetInterface (mDOMWindow, &rv));
	NS_ENSURE_SUCCESS (rv, rv);

	return webNav->GetCurrentURI (aURI);
}

nsresult EphyBrowser::GetTargetDocumentURI (nsIURI **aURI)
{
	if (!mWebBrowser) return NS_ERROR_NOT_INITIALIZED;

        nsCOMPtr<nsIDOMDocument> domDoc;
	GetTargetDocument (getter_AddRefs(domDoc));
	NS_ENSURE_TRUE (domDoc, NS_ERROR_FAILURE);

	nsCOMPtr<nsIDOMDocumentView> docView (do_QueryInterface (domDoc));
	NS_ENSURE_TRUE (docView, NS_ERROR_FAILURE);

	nsCOMPtr<nsIDOMAbstractView> abstractView;
	docView->GetDefaultView (getter_AddRefs (abstractView));
	NS_ENSURE_TRUE (abstractView, NS_ERROR_FAILURE);
	/* the abstract view is really the DOM window */

	nsresult rv;
	nsCOMPtr<nsIWebNavigation> webNav (do_GetInterface (abstractView, &rv));
	NS_ENSURE_SUCCESS (rv, rv);

	return webNav->GetCurrentURI (aURI);
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

	nsresult rv;
	rv = mdv->GetForceCharacterSet (encoding);
	NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

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

				/* There are forms for which defaultValue is longer than
				 * userValue. Mozilla consider this not a bug [see WONTFIXed
				 * bug 232057], but we need to check for this here.
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

		nsresult rv;
		rv = GetDocumentHasModifiedForms (domDoc, &numTextFields, &hasTextArea);
		if (NS_SUCCEEDED (rv) &&
		    (numTextFields >= NUM_MODIFIED_TEXTFIELDS_REQUIRED || hasTextArea))
		{
			*modified = PR_TRUE;
			break;
		}
	}

	return NS_OK;
}

nsresult
EphyBrowser::GetSecurityInfo (PRUint32 *aState, nsACString &aDescription)
{
#ifdef HAVE_MOZILLA_PSM
	NS_ENSURE_TRUE (mSecurityInfo, NS_ERROR_FAILURE);

	nsresult rv;
	rv = mSecurityInfo->GetState (aState);
	NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

	nsEmbedString tooltip;
	rv = mSecurityInfo->GetTooltipText (tooltip);
	NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

	NS_UTF16ToCString (tooltip,
			   NS_CSTRING_ENCODING_UTF8, aDescription);

	return NS_OK;
#else
	return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

nsresult
EphyBrowser::ShowCertificate ()
{
#ifdef HAVE_MOZILLA_PSM
	NS_ENSURE_TRUE (mSecurityInfo, NS_ERROR_FAILURE);

	nsCOMPtr<nsISSLStatusProvider> statusProvider (do_QueryInterface (mSecurityInfo));
	NS_ENSURE_TRUE (statusProvider, NS_ERROR_FAILURE);

	nsCOMPtr<nsISSLStatus> SSLStatus;
	statusProvider->GetSSLStatus (getter_AddRefs (SSLStatus));
	NS_ENSURE_TRUE (SSLStatus, NS_ERROR_FAILURE);

	nsCOMPtr<nsIX509Cert> serverCert;
	SSLStatus->GetServerCert (getter_AddRefs (serverCert));
	NS_ENSURE_TRUE (serverCert, NS_ERROR_FAILURE);

	nsCOMPtr<nsICertificateDialogs> certDialogs (do_GetService (NS_CERTIFICATEDIALOGS_CONTRACTID));
	NS_ENSURE_TRUE (certDialogs, NS_ERROR_FAILURE);

	nsCOMPtr<nsIInterfaceRequestor> requestor(do_QueryInterface (mDOMWindow));

	return certDialogs->ViewCert (requestor, serverCert);
#else
	return NS_OK;
#endif
}

EphyEmbedDocumentType
EphyBrowser::GetDocumentType ()
{
  EphyEmbedDocumentType type = EPHY_EMBED_DOCUMENT_OTHER;

  NS_ENSURE_TRUE (mDOMWindow, type);

  nsresult rv;
  nsCOMPtr<nsIDOMDocument> domDoc;
  rv = GetDocument (getter_AddRefs (domDoc));
  NS_ENSURE_SUCCESS (rv, type);

  nsCOMPtr<nsIDOMHTMLDocument> htmlDoc (do_QueryInterface (domDoc));
  nsCOMPtr<nsIDOMXMLDocument> xmlDoc (do_QueryInterface (domDoc));
  nsCOMPtr<nsIImageDocument> imgDoc (do_QueryInterface (domDoc));

  if (xmlDoc)
    {
      type = EPHY_EMBED_DOCUMENT_XML;
    }
  else if (imgDoc)
    {
      type = EPHY_EMBED_DOCUMENT_IMAGE;
    }
  else if (htmlDoc)
    {
      type = EPHY_EMBED_DOCUMENT_HTML;
    }

  return type;
}

#ifdef GTKMOZEMBED_BROKEN_FOCUS
nsresult
EphyBrowser::FocusActivate ()
{
	NS_ENSURE_STATE (mWebBrowserFocus);

	return mWebBrowserFocus->Activate();
}

nsresult
EphyBrowser::FocusDeactivate ()
{
	NS_ENSURE_STATE (mWebBrowserFocus);

	return mWebBrowserFocus->Deactivate();
}
#endif /* GTKMOZEMBED_BROKEN_FOCUS */
