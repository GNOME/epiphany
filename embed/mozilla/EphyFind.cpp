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

#include "mozilla-config.h"

#include "config.h"

#include "EphyFind.h"

#include "ephy-debug.h"

#undef MOZILLA_INTERNAL_API
#include <nsEmbedString.h>
#define MOZILLA_INTERNAL_API 1

#include <gtkmozembed.h>
#include <gtkmozembed_internal.h>
#include <nsCOMPtr.h>
#include <nsIServiceManager.h>
#include <nsIInterfaceRequestorUtils.h>
#include <nsIDOMWindow.h>
#include <nsIWebBrowser.h>

#ifdef HAVE_TYPEAHEADFIND
#include <nsIDocShell.h>
#include <nsITypeAheadFind.h>
#else
#include <nsIWebBrowserFind.h>
#include <nsMemory.h>
#endif

#include <glib.h>

#ifdef HAVE_TYPEAHEADFIND
#define NS_TYPEAHEADFIND_CONTRACTID "@mozilla.org/typeaheadfind;1"
#endif /* HAVE_TYPEAHEADFIND */

EphyFind::EphyFind ()
: mCurrentEmbed(nsnull)
{
  LOG ("EphyFind ctor [%p]", this);
}

EphyFind::~EphyFind ()
{
  LOG ("EphyFind dtor [%p]", this);
}

nsresult
EphyFind::SetEmbed (EphyEmbed *aEmbed)
{
  nsresult rv = NS_OK;
  if (aEmbed == mCurrentEmbed) return rv;

  mCurrentEmbed = nsnull;

  rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsIWebBrowser> webBrowser;
  gtk_moz_embed_get_nsIWebBrowser (GTK_MOZ_EMBED (aEmbed),
				   getter_AddRefs (webBrowser));
  NS_ENSURE_TRUE (webBrowser, rv);

#ifdef HAVE_TYPEAHEADFIND
  nsCOMPtr<nsIDocShell> docShell (do_GetInterface (webBrowser, &rv));
  NS_ENSURE_SUCCESS (rv, rv);

  if (!mFinder) {
    mFinder = do_CreateInstance (NS_TYPEAHEADFIND_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS (rv, rv);

    rv = mFinder->Init (docShell);
    mFinder->SetFocusLinks (PR_TRUE);
  } else {
    rv = mFinder->SetDocShell (docShell);
  }
  NS_ENSURE_SUCCESS (rv, rv);
#else
  PRUnichar *string = nsnull;
  if (mFinder) {
    mFinder->GetSearchString (&string);
  }

  mFinder = do_GetInterface (webBrowser, &rv);
  NS_ENSURE_SUCCESS (rv, rv);

  mFinder->SetWrapFind (PR_TRUE);

  if (string) {
    mFinder->SetSearchString (string);
    nsMemory::Free (string);    
  }
#endif /* HAVE_TYPEAHEADFIND */

  mCurrentEmbed = aEmbed;

  return rv;
}

void
EphyFind::SetFindProperties (const char *aSearchString,
			     PRBool aCaseSensitive)
{
  if (!mFinder) return;

#ifdef HAVE_TYPEAHEADFIND
  mFinder->SetCaseSensitive (aCaseSensitive);
  /* search string is set on ::Find */
#else 
  mFinder->SetMatchCase (aCaseSensitive);
  
  nsEmbedString uSearchString;
  NS_CStringToUTF16 (nsEmbedCString (aSearchString ? aSearchString : ""),
		     NS_CSTRING_ENCODING_UTF8, uSearchString);
  
  mFinder->SetSearchString (uSearchString.get ());
#endif /* TYPEAHEADFIND */
}

PRBool
EphyFind::Find (const char *aSearchString,
                PRBool aLinksOnly)
{
  if (!mFinder) return PR_FALSE;

  nsEmbedString uSearchString;
  NS_CStringToUTF16 (nsEmbedCString (aSearchString ? aSearchString : ""),
  		     NS_CSTRING_ENCODING_UTF8, uSearchString);

#ifdef HAVE_TYPEAHEADFIND
  nsresult rv;
  PRUint16 found = nsITypeAheadFind::FIND_NOTFOUND;
  rv = mFinder->Find (uSearchString, aLinksOnly, &found);

  return NS_SUCCEEDED (rv) && found != nsITypeAheadFind::FIND_NOTFOUND;
#else
  mFinder->SetSearchString (uSearchString.get ());
  mFinder->SetFindBackwards (PR_FALSE);

  nsresult rv;
  PRBool didFind = PR_FALSE;
  rv = mFinder->FindNext (&didFind);
        
  return NS_SUCCEEDED (rv) && didFind;
#endif /* HAVE_TYPEAHEADFIND */
}

PRBool
EphyFind::FindAgain (PRBool aForward)
{
  if (!mFinder) return PR_FALSE;

#ifdef HAVE_TYPEAHEADFIND
  nsresult rv;
  PRUint16 found = nsITypeAheadFind::FIND_NOTFOUND;
  if (aForward) {
    rv = mFinder->FindNext (&found);
  } else {
    rv = mFinder->FindPrevious (&found);
  }

  return NS_SUCCEEDED (rv) && found != nsITypeAheadFind::FIND_NOTFOUND;
#else
  mFinder->SetFindBackwards (!aForward);
        
  nsresult rv;
  PRBool didFind = PR_FALSE;
  rv = mFinder->FindNext (&didFind);
        
  return NS_SUCCEEDED (rv) && didFind;
#endif /* HAVE_TYPEAHEADFIND */
}
