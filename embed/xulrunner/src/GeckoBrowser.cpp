/*
 *  Copyright © Christopher Blizzard
 *  Copyright © 2006 Christian Persch
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2.1, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  ---------------------------------------------------------------------------
 *  Derived from Mozilla.org code, which had the following attributions:
 *
 *  The Original Code is mozilla.org code.
 *
 *  The Initial Developer of the Original Code is
 *  Christopher Blizzard. Portions created by Christopher Blizzard are Copyright © Christopher Blizzard.  All Rights Reserved.
 *  Portions created by the Initial Developer are Copyright © 2001
 *  the Initial Developer. All Rights Reserved.
 *
 *  Contributor(s):
 *    Christopher Blizzard <blizzard@mozilla.org>
 *  ---------------------------------------------------------------------------
 *
 *  $Id$
 */

#include <xpcom-config.h>
#include "config.h"

#include "nsIDocShell.h"
#include "nsIWebProgress.h"
#include "nsIWebBrowserStream.h"
#include "nsIWebBrowserFocus.h"
#include "nsIWidget.h"
#include <stdlib.h>

// for NS_APPSHELL_CID
#include "nsWidgetsCID.h"

// for do_GetInterface
#include "nsIInterfaceRequestor.h"
// for do_CreateInstance
#include "nsIComponentManager.h"

// for initializing our window watcher service
#include "nsIWindowWatcher.h"

#include "nsILocalFile.h"
#include "nsEmbedAPI.h"

// all of the crap that we need for event listeners
// and when chrome windows finish loading
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMWindowInternal.h"

// For seting scrollbar visibilty
#include <nsIDOMBarProp.h>

// for the focus hacking we need to do
#include "nsIFocusController.h"

// for profiles
#define STANDALONE_PROFILEDIRSERVICE
#include "nsProfileDirServiceProvider.h"

// app component registration
#include "nsIGenericFactory.h"
#include "nsIComponentRegistrar.h"

// all of our local includes
#include "GeckoBrowser.h"
#include "EmbedWindow.h"
#include "EmbedProgress.h"
#include "EmbedContentListener.h"
#include "EmbedEventListener.h"
#include "EmbedWindowCreator.h"

#ifdef MOZ_ACCESSIBILITY_ATK
#include "nsIAccessibilityService.h"
#include "nsIAccessible.h"
#include "nsIDOMDocument.h"
#endif

#include "GeckoSingle.h"

GeckoBrowser::GeckoBrowser(void)
  : mOwningWidget(nsnull)
  , mWindow(nsnull)
  , mProgress(nsnull)
  , mContentListener(nsnull)
  , mEventListener(nsnull)
  , mChromeMask(nsIWebBrowserChrome::CHROME_ALL)
  , mIsChrome(PR_FALSE)
  , mChromeLoaded(PR_FALSE)
  , mListenersAttached(PR_FALSE)
  , mMozWindowWidget(nsnull)
  , mIsDestroyed(PR_FALSE)
{
  GeckoSingle::AddBrowser(this);
}

GeckoBrowser::~GeckoBrowser()
{
  GeckoSingle::RemoveBrowser(this);
}

nsresult
GeckoBrowser::Init(GeckoEmbed *aOwningWidget)
{
  // are we being re-initialized?
  if (mOwningWidget)
    return NS_OK;

  // hang on with a reference to the owning widget
  mOwningWidget = aOwningWidget;

  // Create our embed window, and create an owning reference to it and
  // initialize it.  It is assumed that this window will be destroyed
  // when we go out of scope.
  mWindow = new EmbedWindow();
  mWindowGuard = static_cast<nsIWebBrowserChrome *>(mWindow);
  mWindow->Init(this);

  // Create our progress listener object, make an owning reference,
  // and initialize it.  It is assumed that this progress listener
  // will be destroyed when we go out of scope.
  mProgress = new EmbedProgress();
  mProgressGuard = static_cast<nsIWebProgressListener *>
                              (mProgress);
  mProgress->Init(this);

  // Create our content listener object, initialize it and attach it.
  // It is assumed that this will be destroyed when we go out of
  // scope.
  mContentListener = new EmbedContentListener();
  mContentListenerGuard = static_cast<nsISupports*>(static_cast<nsIURIContentListener*>(mContentListener));
  mContentListener->Init(this);

  // Create our key listener object and initialize it.  It is assumed
  // that this will be destroyed before we go out of scope.
  mEventListener = new EmbedEventListener(this);
  mEventListenerGuard =
    static_cast<nsISupports *>(static_cast<nsIDOMKeyListener *>
                           (mEventListener));

  return NS_OK;
}

nsresult
GeckoBrowser::Realize(PRBool *aAlreadyRealized)
{

  *aAlreadyRealized = PR_FALSE;

  // Have we ever been initialized before?  If so then just reparent
  // from the offscreen window.
  if (mMozWindowWidget) {
    gtk_widget_reparent(mMozWindowWidget, GTK_WIDGET(mOwningWidget));
    *aAlreadyRealized = PR_TRUE;
    return NS_OK;
  }

  // Get the nsIWebBrowser object for our embedded window.
  nsCOMPtr<nsIWebBrowser> webBrowser;
  mWindow->GetWebBrowser(getter_AddRefs(webBrowser));

  // get a handle on the navigation object
  mNavigation = do_QueryInterface(webBrowser);

  // Create our session history object and tell the navigation object
  // to use it.  We need to do this before we create the web browser
  // window.
  mSessionHistory = do_CreateInstance(NS_SHISTORY_CONTRACTID);
  mNavigation->SetSessionHistory(mSessionHistory);

  // create the window
  mWindow->CreateWindow();

  // bind the progress listener to the browser object
  nsCOMPtr<nsISupportsWeakReference> supportsWeak;
  supportsWeak = do_QueryInterface(mProgressGuard);
  nsCOMPtr<nsIWeakReference> weakRef;
  supportsWeak->GetWeakReference(getter_AddRefs(weakRef));
  webBrowser->AddWebBrowserListener(weakRef,
				    NS_GET_IID (nsIWebProgressListener));

  // set ourselves as the parent uri content listener
  nsCOMPtr<nsIURIContentListener> uriListener;
  uriListener = do_QueryInterface(mContentListenerGuard);
  webBrowser->SetParentURIContentListener(uriListener);

  // save the window id of the newly created window
  nsCOMPtr<nsIWidget> mozWidget;
  mWindow->mBaseWindow->GetMainWidget(getter_AddRefs(mozWidget));
  // get the native drawing area
  GdkWindow *tmp_window =
    static_cast<GdkWindow *>
               (mozWidget->GetNativeData(NS_NATIVE_WINDOW));
  // and, thanks to superwin we actually need the parent of that.
  // FIXME is this true on gtk2 widget?
  tmp_window = gdk_window_get_parent(tmp_window);
  // save the widget ID - it should be the mozarea of the window.
  gpointer data = nsnull;
  gdk_window_get_user_data(tmp_window, &data);
  mMozWindowWidget = static_cast<GtkWidget *>(data);

  // Apply the current chrome mask
  ApplyChromeMask();

  return NS_OK;
}

void
GeckoBrowser::Unrealize(void)
{
  // reparent to our offscreen window
  GeckoSingle::ReparentToOffscreen(mMozWindowWidget);
}

void
GeckoBrowser::Show(void)
{
  // Get the nsIWebBrowser object for our embedded window.
  nsCOMPtr<nsIWebBrowser> webBrowser;
  mWindow->GetWebBrowser (getter_AddRefs (webBrowser));

  // and set the visibility on the thing
  nsCOMPtr<nsIBaseWindow> baseWindow (do_QueryInterface (webBrowser));
  baseWindow->SetVisibility(PR_TRUE);
}

void
GeckoBrowser::Hide(void)
{
  // Get the nsIWebBrowser object for our embedded window.
  nsCOMPtr<nsIWebBrowser> webBrowser;
  mWindow->GetWebBrowser (getter_AddRefs (webBrowser));

  // and set the visibility on the thing
  nsCOMPtr<nsIBaseWindow> baseWindow (do_QueryInterface (webBrowser));
  baseWindow->SetVisibility (PR_FALSE);
}

void
GeckoBrowser::Resize(PRUint32 aWidth, PRUint32 aHeight)
{
  mWindow->SetDimensions(nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION |
			 nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_INNER,
			 0, 0, aWidth, aHeight);
}

void
GeckoBrowser::Destroy(void)
{
  // This flag might have been set from
  // EmbedWindow::DestroyBrowserWindow() as well if someone used a
  // window.close() or something or some other script action to close
  // the window.  No harm setting it again.
  mIsDestroyed = PR_TRUE;

  // Get the nsIWebBrowser object for our embedded window.
  nsCOMPtr<nsIWebBrowser> webBrowser;
  mWindow->GetWebBrowser(getter_AddRefs(webBrowser));

  // Release our progress listener
  nsCOMPtr<nsISupportsWeakReference> supportsWeak
          (do_QueryInterface(mProgressGuard));
  nsCOMPtr<nsIWeakReference> weakRef;
  supportsWeak->GetWeakReference(getter_AddRefs(weakRef));
  webBrowser->RemoveWebBrowserListener(weakRef,
				       NS_GET_IID (nsIWebProgressListener));
  weakRef = nsnull;
  supportsWeak = nsnull;

  // Release our content listener
  webBrowser->SetParentURIContentListener(nsnull);
  mContentListenerGuard = nsnull;
  mContentListener = nsnull;

  // Now that we have removed the listener, release our progress
  // object
  mProgressGuard = nsnull;
  mProgress = nsnull;

  // detach our event listeners and release the event receiver
  DetachListeners();
 
  mEventTarget = nsnull;

  // destroy our child window
  mWindow->ReleaseChildren();

  // release navigation
  mNavigation = nsnull;

  // release session history
  mSessionHistory = nsnull;

  mOwningWidget = nsnull;

  mMozWindowWidget = 0;
}

void
GeckoBrowser::Reload(PRUint32 reloadFlags)
{
  /* Use the session history if it is available, this
   * allows framesets to reload correctly */
  nsCOMPtr<nsIWebNavigation> wn (do_QueryInterface(mSessionHistory));

  if (!wn)
    wn = mNavigation;

  NS_ENSURE_TRUE (wn, );

  wn->Reload(reloadFlags);
}

void
GeckoBrowser::ApplyChromeMask()
{
   if (mWindow) {
      nsCOMPtr<nsIWebBrowser> webBrowser;
      mWindow->GetWebBrowser(getter_AddRefs(webBrowser));

      nsCOMPtr<nsIDOMWindow> domWindow;
      webBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
      if (domWindow) {

         nsCOMPtr<nsIDOMBarProp> scrollbars;
         domWindow->GetScrollbars(getter_AddRefs(scrollbars));
         if (scrollbars) {

            scrollbars->SetVisible
               (mChromeMask & nsIWebBrowserChrome::CHROME_SCROLLBARS ? 
                PR_TRUE : PR_FALSE);
         }
      }
   }
}


void
GeckoBrowser::SetChromeMask(PRUint32 aChromeMask)
{
   mChromeMask = aChromeMask;

   ApplyChromeMask();
}

void
GeckoBrowser::SetURI(const char *aURI)
{
  mURI = aURI;
}

void
GeckoBrowser::LoadCurrentURI(void)
{
  if (mURI.Length()) {
    nsCOMPtr<nsPIDOMWindow> piWin;
    GetPIDOMWindow(getter_AddRefs(piWin));
    nsAutoPopupStatePusher popupStatePusher(piWin, openAllowed);

    nsEmbedString uri;
    NS_CStringToUTF16(mURI, NS_CSTRING_ENCODING_UTF8, uri);
    mNavigation->LoadURI(uri.get(),                         // URI string
                         nsIWebNavigation::LOAD_FLAGS_NONE, // Load flags
                         nsnull,                            // Referring URI
                         nsnull,                            // Post data
                         nsnull);                           // extra headers
  }
}

#if 0
nsresult
GeckoBrowser::OpenStream(const char *aBaseURI, const char *aContentType)
{
  nsCOMPtr<nsIWebBrowser> webBrowser;
  mWindow->GetWebBrowser(getter_AddRefs(webBrowser));

  nsCOMPtr<nsIWebBrowserStream> wbStream = do_QueryInterface(webBrowser);
  if (!wbStream) return NS_ERROR_FAILURE;

  return wbStream->OpenStream(aBaseURI, aContentType);
}

nsresult
GeckoBrowser::AppendToStream(const char *aData, PRInt32 aLen)
{
  // Attach listeners to this document since in some cases we don't
  // get updates for content added this way.
  ContentStateChange();

  nsCOMPtr<nsIWebBrowser> webBrowser;
  mWindow->GetWebBrowser(getter_AddRefs(webBrowser));

  nsCOMPtr<nsIWebBrowserStream> wbStream = do_QueryInterface(webBrowser); 
  if (!wbStream) return NS_ERROR_FAILURE;

  return wbStream->AppendToStream(aData, aLen);
}

nsresult
GeckoBrowser::CloseStream(void)
{
  nsCOMPtr<nsIWebBrowser> webBrowser;
  mWindow->GetWebBrowser(getter_AddRefs(webBrowser));

  nsCOMPtr<nsIWebBrowserStream> wbStream = do_QueryInterface(webBrowser);
  if (!wbStream) return NS_ERROR_FAILURE;

  return wbStream->CloseStream();
}
#endif

void
GeckoBrowser::ContentStateChange(void)
{

  // we don't attach listeners to chrome
  if (mListenersAttached && !mIsChrome)
    return;

  GetListener();

  if (!mEventTarget)
    return;
  
  AttachListeners();

}

void
GeckoBrowser::ContentFinishedLoading(void)
{
  if (mIsChrome) {
    // We're done loading.
    mChromeLoaded = PR_TRUE;

    // get the web browser
    nsCOMPtr<nsIWebBrowser> webBrowser;
    mWindow->GetWebBrowser(getter_AddRefs(webBrowser));
    
    // get the content DOM window for that web browser
    nsCOMPtr<nsIDOMWindow> domWindow;
    webBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
    if (!domWindow) {
      NS_WARNING("no dom window in content finished loading\n");
      return;
    }
    
    // resize the content
    domWindow->SizeToContent();

    // and since we're done loading show the window, assuming that the
    // visibility flag has been set.
    PRBool visibility;
    mWindow->GetVisibility(&visibility);
    if (visibility)
      mWindow->SetVisibility(PR_TRUE);
  }
}

void
GeckoBrowser::ChildFocusIn(void)
{
  if (mIsDestroyed)
    return;

  nsresult rv;
  nsCOMPtr<nsIWebBrowser> webBrowser;
  rv = mWindow->GetWebBrowser(getter_AddRefs(webBrowser));
  if (NS_FAILED(rv))
    return;

  nsCOMPtr<nsIWebBrowserFocus> webBrowserFocus(do_QueryInterface(webBrowser));
  if (!webBrowserFocus)
    return;

  webBrowserFocus->Activate();
}

void
GeckoBrowser::ChildFocusOut(void)
{
  if (mIsDestroyed)
    return;

  nsresult rv;
  nsCOMPtr<nsIWebBrowser> webBrowser;
  rv = mWindow->GetWebBrowser(getter_AddRefs(webBrowser));
  if (NS_FAILED(rv))
    return;

  nsCOMPtr<nsIWebBrowserFocus> webBrowserFocus(do_QueryInterface(webBrowser));
  if (!webBrowserFocus)
    return;

  webBrowserFocus->Deactivate();
}

// Get the event listener for the chrome event handler.

void
GeckoBrowser::GetListener(void)
{
  if (mEventTarget)
    return;

  nsCOMPtr<nsPIDOMWindow> piWin;
  GetPIDOMWindow(getter_AddRefs(piWin));

  if (!piWin)
    return;

  mEventTarget = do_QueryInterface(piWin->GetChromeEventHandler());
}

// attach key and mouse event listeners

void
GeckoBrowser::AttachListeners(void)
{
  if (!mEventTarget || mListenersAttached)
    return;

  nsIDOMEventListener *eventListener =
    static_cast<nsIDOMEventListener *>
               (static_cast<nsIDOMKeyListener *>(mEventListener));

  // add the key listener
  nsresult rv;
  rv = mEventTarget->AddEventListenerByIID(eventListener,
					     NS_GET_IID(nsIDOMKeyListener));
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to add key listener\n");
    return;
  }

  rv = mEventTarget->AddEventListenerByIID(eventListener,
					     NS_GET_IID(nsIDOMMouseListener));
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to add mouse listener\n");
    return;
  }

  rv = mEventTarget->AddEventListenerByIID(eventListener,
                                             NS_GET_IID(nsIDOMUIListener));
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to add UI listener\n");
    return;
  }

  rv = mEventTarget->AddEventListenerByIID(eventListener,
                                             NS_GET_IID(nsIDOMContextMenuListener));
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to add context menu listener\n");
    return;
  }

  // ok, all set.
  mListenersAttached = PR_TRUE;
}

void
GeckoBrowser::DetachListeners(void)
{
  if (!mListenersAttached || !mEventTarget)
    return;

  nsIDOMEventListener *eventListener =
    static_cast<nsIDOMEventListener *>
               (static_cast<nsIDOMKeyListener *>(mEventListener));

  nsresult rv;
  rv = mEventTarget->RemoveEventListenerByIID(eventListener,
						NS_GET_IID(nsIDOMKeyListener));
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to remove key listener\n");
    return;
  }

  rv =
    mEventTarget->RemoveEventListenerByIID(eventListener,
					     NS_GET_IID(nsIDOMMouseListener));
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to remove mouse listener\n");
    return;
  }

  rv = mEventTarget->RemoveEventListenerByIID(eventListener,
						NS_GET_IID(nsIDOMUIListener));
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to remove UI listener\n");
    return;
  }

  rv = mEventTarget->RemoveEventListenerByIID(eventListener,
						NS_GET_IID(nsIDOMContextMenuListener));
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to remove context menu listener\n");
    return;
  }

  mListenersAttached = PR_FALSE;
}

nsresult
GeckoBrowser::GetPIDOMWindow(nsPIDOMWindow **aPIWin)
{
  *aPIWin = nsnull;

  // get the web browser
  nsCOMPtr<nsIWebBrowser> webBrowser;
  mWindow->GetWebBrowser(getter_AddRefs(webBrowser));

  // get the content DOM window for that web browser
  nsCOMPtr<nsIDOMWindow> domWindow;
  webBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
  if (!domWindow)
    return NS_ERROR_FAILURE;

  // get the private DOM window
  nsCOMPtr<nsPIDOMWindow> domWindowPrivate = do_QueryInterface(domWindow);
  // and the root window for that DOM window
  *aPIWin = domWindowPrivate->GetPrivateRoot();
  
  if (*aPIWin) {
    NS_ADDREF(*aPIWin);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

#ifdef MOZ_ACCESSIBILITY_ATK
// FIXME does this REALLY work with frames?
void *
GeckoBrowser::GetAtkObjectForCurrentDocument()
{
  if (!mNavigation)
    return nsnull;

  nsCOMPtr<nsIAccessibilityService> accService =
    do_GetService("@mozilla.org/accessibilityService;1");
  if (accService) {
    //get current document
    nsCOMPtr<nsIDOMDocument> domDoc;
    mNavigation->GetDocument(getter_AddRefs(domDoc));
    NS_ENSURE_TRUE(domDoc, nsnull);

    nsCOMPtr<nsIDOMNode> domNode(do_QueryInterface(domDoc));
    NS_ENSURE_TRUE(domNode, nsnull);

    nsCOMPtr<nsIAccessible> acc;
    accService->GetAccessibleFor(domNode, getter_AddRefs(acc));
    NS_ENSURE_TRUE(acc, nsnull);

    void *atkObj = nsnull;
    if (NS_SUCCEEDED(acc->GetNativeInterface(&atkObj)))
      return atkObj;
  }
  return nsnull;
}
#endif /* MOZ_ACCESSIBILITY_ATK */
