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

#ifndef __GeckoSingle_h
#define __GeckoSingle_h

#include <nsCOMPtr.h>
#include <nsEmbedString.h>
#include <nsIWebNavigation.h>
#include <nsISHistory.h>
// for our one function that gets the GeckoSingle via the chrome
// object.
#include <nsIWebBrowserChrome.h>
#include <nsIAppShell.h>
// app component registration
#include <nsIGenericFactory.h>
#include <nsIComponentRegistrar.h>

#include "gecko-embed-single.h"
#include "gecko-embed.h"

#include "GeckoBrowser.h"

class EmbedProfile;
class EmbedProgress;
class EmbedWindow;
class EmbedContentListener;
class EmbedEventListener;

class nsPIDOMWindow;
class nsIDirectoryServiceProvider;
class nsProfileDirServiceProvider;
  
class GeckoSingle
{
 friend class GeckoBrowser;

 public:

  GeckoSingle();
  ~GeckoSingle();

  static void PushStartup ();
  static void PopStartup ();

 // static GeckoSingle* GetInstance();
 
  // This function will find the specific GeckoBrowser object for a
  // given nsIWebBrowserChrome.
  static GeckoBrowser *FindPrivateForBrowser(nsIWebBrowserChrome *aBrowser);

  // the number of widgets that have been created
  static PRUint32                sWidgetCount;
  // the list of application-specific components to register
  static const nsModuleComponentInfo  *sAppComps;
  static int                     sNumAppComps;
  // the list of all open windows
  static GSList                 *sWindowList;

  static void ReparentToOffscreen (GtkWidget *aWidget);
  static void AddBrowser (GeckoBrowser *aBrowser);
  static void RemoveBrowser (GeckoBrowser *aBrowser);
};

#endif /* __GeckoSingle_h */
