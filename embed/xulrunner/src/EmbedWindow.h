/*
 *  Copyright © Christopher Blizzard
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

#ifndef __EmbedWindow_h
#define __EmbedWindow_h

#include <nsEmbedString.h>
#include <nsIWebBrowserChrome.h>
#include <nsIWebBrowserChromeFocus.h>
#include <nsIEmbeddingSiteWindow.h>
#include <nsITooltipListener.h>
#include <nsISupports.h>
#include <nsIWebBrowser.h>
#include <nsIBaseWindow.h>
#include <nsIInterfaceRequestor.h>
#include <nsCOMPtr.h>

#include <gtk/gtk.h>

class GeckoBrowser;

class EmbedWindow : public nsIWebBrowserChrome,
		    public nsIWebBrowserChromeFocus,
                    public nsIEmbeddingSiteWindow,
                    public nsITooltipListener,
		    public nsIInterfaceRequestor
{

 public:

  EmbedWindow();
  virtual ~EmbedWindow();

  nsresult Init            (GeckoBrowser *aOwner);
  nsresult CreateWindow    (void);
  void     ReleaseChildren (void);

  NS_DECL_ISUPPORTS

  NS_DECL_NSIWEBBROWSERCHROME

  NS_DECL_NSIWEBBROWSERCHROMEFOCUS

  NS_DECL_NSIEMBEDDINGSITEWINDOW

  NS_DECL_NSITOOLTIPLISTENER

  NS_DECL_NSIINTERFACEREQUESTOR

  nsEmbedString            mTitle;
  nsEmbedString            mJSStatus;
  nsEmbedString            mLinkMessage;

  nsCOMPtr<nsIBaseWindow>  mBaseWindow; // [OWNER]

private:

  GeckoBrowser            *mOwner;
  nsCOMPtr<nsIWebBrowser>  mWebBrowser; // [OWNER]
  static GtkWidget        *sTipWindow;
  PRBool                   mVisibility;
  PRBool                   mIsModal;

};

#endif /* __EmbedWindow_h */
