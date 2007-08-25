/*
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
 *  The Original Code is mozilla.org code.
 *
 *  The Initial Developer of the Original Code is
 *  Netscape Communications Corporation.
 *  Portions created by the Initial Developer are Copyright © 2003
 *  the Initial Developer. All Rights Reserved.
 *
 *  Contributor(s):
 *   Brian Ryner <bryner@brianryner.com>
 *
 *  $Id$
 */

#include <mozilla-config.h>
#include "config.h"

#include "GeckoUtils.h"

#include "gecko-embed.h"

#include <nsCOMPtr.h>
#include <nsIDOMWindow.h>
#include <nsIWindowWatcher.h>
#include <nsIWebBrowserChrome.h>
#include <nsIEmbeddingSiteWindow.h>
#include <nsIServiceManager.h>
#include <nsServiceManagerUtils.h>

GtkWidget *
GeckoUtils::GetGeckoEmbedForDOMWindow (nsIDOMWindow * aDOMWindow)
{
  if (!aDOMWindow)
    return NULL;

  /* Get the toplevel DOM window, in case this window is a frame */
  nsCOMPtr<nsIDOMWindow> domWin;
  aDOMWindow->GetTop (getter_AddRefs (domWin));
  if (!domWin)
    return NULL;

  nsCOMPtr< nsIWindowWatcher> wwatch
    (do_GetService ("@mozilla.org/embedcomp/window-watcher;1"));
  NS_ENSURE_TRUE (wwatch, NULL);

  nsCOMPtr<nsIWebBrowserChrome> chrome;
  wwatch->GetChromeForWindow (domWin, getter_AddRefs (chrome));

  nsCOMPtr <nsIEmbeddingSiteWindow> siteWindow (do_QueryInterface (chrome));
  if (!siteWindow)
    return NULL;

  GtkWidget *widget;
  siteWindow->GetSiteWindow ((void **) &widget);
  if (!widget || !GECKO_IS_EMBED (widget))
    return NULL;

  return widget;
}

GtkWidget *
GeckoUtils::GetGtkWindowForDOMWindow (nsIDOMWindow * aDOMWindow)
{
  GtkWidget *embed = GeckoUtils::GetGeckoEmbedForDOMWindow (aDOMWindow);
  if (!embed)
    return NULL;

  GtkWidget *gtkWin = gtk_widget_get_toplevel (embed);
  if (!GTK_WIDGET_TOPLEVEL (gtkWin))
    return NULL;

  return gtkWin;
}
