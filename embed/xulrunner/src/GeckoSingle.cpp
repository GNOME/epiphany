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
#include <config.h>

#include <stdlib.h>
#include "nsIDocShell.h"
#include "nsIWebProgress.h"
#include "nsIWebBrowserStream.h"
#include "nsIWidget.h"

// all of our local includes
#include "GeckoSingle.h"
#include "EmbedWindow.h"
#include "gecko-init.h"
#include "gecko-init-private.h"

GSList      *GeckoSingle::sWindowList  = nsnull;
PRUint32     GeckoSingle::sWidgetCount = 0;

GeckoSingle::GeckoSingle()
{
}

GeckoSingle::~GeckoSingle()
{
}

/* static */
GeckoBrowser *
GeckoSingle::FindPrivateForBrowser(nsIWebBrowserChrome *aBrowser)
{
  // This function doesn't get called very often at all ( only when
  // creating a new window ) so it's OK to walk the list of open
  // windows.
  for (GSList *l = sWindowList; l != NULL; l = l->next) {
    GeckoBrowser *tmpPrivate = static_cast<GeckoBrowser *>(l->data);
    // get the browser object for that window
    nsIWebBrowserChrome *chrome = static_cast<nsIWebBrowserChrome *>
                                             (tmpPrivate->mWindow);
    if (chrome == aBrowser)
      return tmpPrivate;
  }

  return nsnull;
}

/* static */
void
GeckoSingle::ReparentToOffscreen (GtkWidget* aWidget)
{
  gecko_reparent_to_offscreen (aWidget);
}

/* static */
void
GeckoSingle::AddBrowser(GeckoBrowser *aBrowser)
{
  PushStartup();
  sWindowList = g_slist_prepend (sWindowList, aBrowser);
}

/* static */
void
GeckoSingle::RemoveBrowser(GeckoBrowser *aBrowser)
{
  sWindowList = g_slist_remove (sWindowList, aBrowser);
  PopStartup();
}

/* static */
void
GeckoSingle::PushStartup()
{
  GeckoSingle::sWidgetCount++;
}

/* static */
void
GeckoSingle::PopStartup()
{
  GeckoSingle::sWidgetCount--;
  if (GeckoSingle::sWidgetCount == 0) {
    gecko_shutdown();
#ifdef XPCOM_GLUE
    XPCOMGlueShutdown();
#endif
  }
}
