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

#ifndef __GeckoBrowser_h
#define __GeckoBrowser_h

#include <nsCOMPtr.h>
#include <nsEmbedString.h>
#include <nsIWebNavigation.h>
#include <nsISHistory.h>
// for our one function that gets the GeckoBrowser via the chrome
// object.
#include <nsIWebBrowserChrome.h>
#include <nsIAppShell.h>
#include <nsPIDOMEventTarget.h>
// app component registration
#include <nsIGenericFactory.h>
#include <nsIComponentRegistrar.h>

#include "gecko-embed.h"

class EmbedProfile;
class EmbedProgress;
class EmbedWindow;
class EmbedContentListener;
class EmbedEventListener;

class nsPIDOMWindow;
class nsIDirectoryServiceProvider;
class nsProfileDirServiceProvider;

class GeckoBrowser {

 public:

  GeckoBrowser();
  ~GeckoBrowser();

  nsresult    Init            (GeckoEmbed *aOwningWidget);
  nsresult    Realize         (PRBool *aAlreadRealized);
  void        Unrealize       (void);
  void        Show            (void);
  void        Hide            (void);
  void        Resize          (PRUint32 aWidth, PRUint32 aHeight);
  void        Destroy         (void);
  void        SetURI          (const char *aURI);
  void        LoadCurrentURI  (void);
  void        Reload          (PRUint32 reloadFlags);

  void        SetChromeMask   (PRUint32 chromeMask);
  void        ApplyChromeMask ();

  nsresult OpenStream         (const char *aBaseURI, const char *aContentType);
  nsresult AppendToStream     (const char *aData, PRInt32 aLen);
  nsresult CloseStream        (void);

  // This is an upcall that will come from the progress listener
  // whenever there is a content state change.  We need this so we can
  // attach event listeners.
  void        ContentStateChange    (void);

  // This is an upcall from the progress listener when content is
  // finished loading.  We have this so that if it's chrome content
  // that we can size to content properly and show ourselves if
  // visibility is set.
  void        ContentFinishedLoading(void);

  // these let the widget code know when the toplevel window gets and
  // looses focus.
  void        TopLevelFocusIn (void);
  void        TopLevelFocusOut(void);

  // these are when the widget itself gets focus in and focus out
  // events
  void        ChildFocusIn (void);
  void        ChildFocusOut(void);

#ifdef MOZ_ACCESSIBILITY_ATK
  void *GetAtkObjectForCurrentDocument();
#endif

  GeckoEmbed                   *mOwningWidget;

  // all of the objects that we own
  EmbedWindow                   *mWindow;
  nsCOMPtr<nsISupports>          mWindowGuard;
  EmbedProgress                 *mProgress;
  nsCOMPtr<nsISupports>          mProgressGuard;
  EmbedContentListener          *mContentListener;
  nsCOMPtr<nsISupports>          mContentListenerGuard;
  EmbedEventListener            *mEventListener;
  nsCOMPtr<nsISupports>          mEventListenerGuard;

  nsCOMPtr<nsIWebNavigation>     mNavigation;
  nsCOMPtr<nsISHistory>          mSessionHistory;

  // our event receiver
  nsCOMPtr<nsPIDOMEventTarget>  mEventTarget;

  // the currently loaded uri
  nsEmbedCString                 mURI;

  // chrome mask
  PRUint32                       mChromeMask;
  // is this a chrome window?
  PRBool                         mIsChrome;
  // has the chrome finished loading?
  PRBool                         mChromeLoaded;
  // saved window ID for reparenting later
  GtkWidget                     *mMozWindowWidget;
  // has someone called Destroy() on us?
  PRBool                         mIsDestroyed;

 private:

  // is the chrome listener attached yet?
  PRBool                         mListenersAttached;

  void GetListener     (void);
  void AttachListeners (void);
  void DetachListeners (void);

  // this will get the PIDOMWindow for this widget
  nsresult        GetPIDOMWindow   (nsPIDOMWindow **aPIWin);
};

#endif /* __GeckoBrowser_h */
