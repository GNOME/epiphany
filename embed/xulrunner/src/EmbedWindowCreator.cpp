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

#include <xpcom-config.h>
#include "config.h"

#include "EmbedWindowCreator.h"
#include "GeckoBrowser.h"
#include "GeckoSingle.h"
#include "EmbedWindow.h"

#include "gecko-embed-private.h"
#include "gecko-embed-single-private.h"
#include "gecko-embed-signals.h"

EmbedWindowCreator::EmbedWindowCreator(void)
{
}

EmbedWindowCreator::~EmbedWindowCreator()
{
}

NS_IMPL_ISUPPORTS1(EmbedWindowCreator, nsIWindowCreator)

NS_IMETHODIMP
EmbedWindowCreator::CreateChromeWindow(nsIWebBrowserChrome *aParent,
				       PRUint32 aChromeFlags,
				       nsIWebBrowserChrome **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  GeckoEmbed *newEmbed = nsnull;

  // No parent?  Ask via the singleton object instead.
  if (!aParent) {
    gecko_embed_single_create_window(&newEmbed,
				       (guint)aChromeFlags);
  }
  else {
    // Find the GeckoBrowser object for this web browser chrome object.
    GeckoBrowser *browser = GeckoSingle::FindPrivateForBrowser(aParent);
    
    if (!browser)
      return NS_ERROR_FAILURE;
    
    g_signal_emit (browser->mOwningWidget, gecko_embed_signals[NEW_WINDOW], 0,
		   &newEmbed, (guint) aChromeFlags);
    
  }

  // check to make sure that we made a new window
  if (!newEmbed)
    return NS_ERROR_FAILURE;

  // The window _must_ be realized before we pass it back to the
  // function that created it. Functions that create new windows
  // will do things like GetDocShell() and the widget has to be
  // realized before that can happen.
  gtk_widget_realize(GTK_WIDGET(newEmbed));
  
  GeckoBrowser *newGeckoBrowser = gecko_embed_get_GeckoBrowser (newEmbed);

  // set the chrome flag on the new window if it's a chrome open
  if (aChromeFlags & nsIWebBrowserChrome::CHROME_OPENAS_CHROME)
    newGeckoBrowser->mIsChrome = PR_TRUE;

  *_retval = static_cast<nsIWebBrowserChrome *>
                        ((newGeckoBrowser->mWindow));
  
  if (*_retval) {
    NS_ADDREF(*_retval);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}
