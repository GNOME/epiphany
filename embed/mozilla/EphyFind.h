/*
 *  Copyright (C) 2005 Christian Persch
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

#ifndef TYPEAHEADFIND_H
#define TYPEAHEADFIND_H

#include <gdk/gdktypes.h>

class nsITypeAheadFind;
class nsIWebBrowser;
class nsIWebBrowserFind;

#include <nsCOMPtr.h>

#include "ephy-embed.h"
#include "ephy-embed-find.h"

class EphyFind
{
  public:
    EphyFind ();
    ~EphyFind ();

    nsresult SetEmbed (EphyEmbed *aEmbed);
    void SetFindProperties (const char *aSearchString,
			    PRBool aCaseSensitive);
    void SetSelectionAttention (PRBool aAttention);
    EphyEmbedFindResult Find (const char *aSearchString,
			      PRBool aLinksOnly);
    EphyEmbedFindResult FindAgain (PRBool aForward);
    PRBool ActivateLink (GdkModifierType aMask);

  private:
    EphyEmbed *mCurrentEmbed;

    nsCOMPtr<nsIWebBrowser> mWebBrowser;

    nsCOMPtr<nsITypeAheadFind> mFinder;
    PRPackedBool mAttention;
    PRPackedBool mHasFocus;
};

#endif /* !TYPEAHEADFIND_H */
