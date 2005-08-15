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
#include <nsIWebBrowserFocus.h>
#include <nsIDOMNode.h>
#include <nsIDOMElement.h>
#include <nsIDOMDocument.h>
#include <nsIDOMDocumentView.h>
#include <nsIDOMAbstractView.h>
#include <nsIDOMDocumentEvent.h>
#include <nsIDOMEvent.h>
#include <nsIDOMKeyEvent.h>
#include <nsIDOMEventTarget.h>
#include <nsIDOMHTMLAnchorElement.h>

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

static const PRUnichar kKeyEvents[] = { 'K', 'e', 'y', 'E', 'v', 'e', 'n', 't', 's', '\0' };
static const PRUnichar kKeyPress[] = { 'k', 'e', 'y', 'p', 'r', 'e', 's', 's', '\0' };

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
  mWebBrowser = nsnull;

  rv = NS_ERROR_FAILURE;
  gtk_moz_embed_get_nsIWebBrowser (GTK_MOZ_EMBED (aEmbed),
				   getter_AddRefs (mWebBrowser));
  NS_ENSURE_TRUE (mWebBrowser, rv);

#ifdef HAVE_TYPEAHEADFIND
  nsCOMPtr<nsIDocShell> docShell (do_GetInterface (mWebBrowser, &rv));
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

  mFinder = do_GetInterface (mWebBrowser, &rv);
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

PRBool
EphyFind::ActivateLink (GdkModifierType aMask)
{
	nsresult rv;
	nsCOMPtr<nsIDOMElement> link;
#if defined(HAVE_TYPEAHEADFIND) && defined(HAVE_GECKO_1_8)
	rv = mFinder->GetFoundLink (getter_AddRefs (link));
	NS_ENSURE_TRUE (NS_SUCCEEDED (rv) && link, FALSE);
#else
	nsCOMPtr<nsIWebBrowserFocus> focus (do_QueryInterface (mWebBrowser));
	NS_ENSURE_TRUE (focus, FALSE);

	rv = focus->GetFocusedElement (getter_AddRefs (link));
	NS_ENSURE_TRUE (NS_SUCCEEDED (rv) && link, FALSE);

	/* ensure this is really a link so we don't accidentally submit if we're on a button or so! */
	nsCOMPtr<nsIDOMHTMLAnchorElement> anchor (do_QueryInterface (link));
	if (!anchor) return FALSE;
#endif /* HAVE_TYPEAHEADFIND && HAVE_GECKO_1_8 */

	nsCOMPtr<nsIDOMDocument> doc;
	rv = link->GetOwnerDocument (getter_AddRefs (doc));
	NS_ENSURE_TRUE (doc, FALSE);

	nsCOMPtr<nsIDOMDocumentView> docView (do_QueryInterface (doc));
	NS_ENSURE_TRUE (docView, FALSE);

	nsCOMPtr<nsIDOMAbstractView> abstractView;
	docView->GetDefaultView (getter_AddRefs (abstractView));
	NS_ENSURE_TRUE (abstractView, FALSE);

	nsCOMPtr<nsIDOMDocumentEvent> docEvent (do_QueryInterface (doc));
	NS_ENSURE_TRUE (docEvent, FALSE);

	nsCOMPtr<nsIDOMEvent> event;
	rv = docEvent->CreateEvent (nsEmbedString(kKeyEvents), getter_AddRefs (event));
	NS_ENSURE_SUCCESS (rv, FALSE);

	nsCOMPtr<nsIDOMKeyEvent> keyEvent (do_QueryInterface (event));
	NS_ENSURE_TRUE (keyEvent, FALSE);

	rv = keyEvent->InitKeyEvent (nsEmbedString (kKeyPress),
				     PR_TRUE /* bubble */,
				     PR_TRUE /* cancelable */,
				     abstractView,
				     (aMask & GDK_CONTROL_MASK) != 0,
				     (aMask & GDK_MOD1_MASK) != 0 /* Alt */,
				     (aMask & GDK_SHIFT_MASK) != 0,
				     PR_FALSE /* Meta */,
				     nsIDOMKeyEvent::DOM_VK_RETURN,
				     0);
	NS_ENSURE_SUCCESS (rv, FALSE);

	nsCOMPtr<nsIDOMEventTarget> target (do_QueryInterface (link));
	NS_ENSURE_TRUE (target, FALSE);

	PRBool defaultPrevented = PR_FALSE;
	rv = target->DispatchEvent (event, &defaultPrevented);

	return NS_SUCCEEDED (rv) && defaultPrevented;
}
