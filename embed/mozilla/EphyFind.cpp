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

#include <glib.h>

#include <nsStringAPI.h>

#include <gtkmozembed.h>
#include <gtkmozembed_internal.h>
#include <nsComponentManagerUtils.h>
#include <nsCOMPtr.h>
#include <nsIDocShell.h>
#include <nsIDocShellTreeItem.h>
#include <nsIDOMAbstractView.h>
#include <nsIDOMDocumentEvent.h>
#include <nsIDOMDocument.h>
#include <nsIDOMDocumentView.h>
#include <nsIDOMElement.h>
#include <nsIDOMEvent.h>
#include <nsIDOMEventTarget.h>
#include <nsIDOMHTMLAnchorElement.h>
#include <nsIDOMKeyEvent.h>
#include <nsIDOMNode.h>
#include <nsIDOMWindow.h>
#include <nsIInterfaceRequestorUtils.h>
#include <nsISelectionController.h>
#include <nsISelectionDisplay.h>
#include <nsISimpleEnumerator.h>
#include <nsITypeAheadFind.h>
#include <nsIWebBrowserFocus.h>
#include <nsIWebBrowser.h>
#include <nsServiceManagerUtils.h>

#include "ephy-debug.h"

#include "EphyFind.h"

#define NS_TYPEAHEADFIND_CONTRACTID "@mozilla.org/typeaheadfind;1"

static const PRUnichar kKeyEvents[] = { 'K', 'e', 'y', 'E', 'v', 'e', 'n', 't', 's', '\0' };
static const PRUnichar kKeyPress[] = { 'k', 'e', 'y', 'p', 'r', 'e', 's', 's', '\0' };

EphyFind::EphyFind ()
: mCurrentEmbed(nsnull)
, mAttention(PR_FALSE)
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

  SetSelectionAttention (PR_FALSE);

  mCurrentEmbed = nsnull;
  mWebBrowser = nsnull;

  rv = NS_ERROR_FAILURE;
  gtk_moz_embed_get_nsIWebBrowser (GTK_MOZ_EMBED (aEmbed),
				   getter_AddRefs (mWebBrowser));
  NS_ENSURE_TRUE (mWebBrowser, rv);

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

  mCurrentEmbed = aEmbed;

  return rv;
}

void
EphyFind::SetFindProperties (const char *aSearchString,
			     PRBool aCaseSensitive)
{
  if (!mFinder) return;

  mFinder->SetCaseSensitive (aCaseSensitive);
  /* search string is set on ::Find */
}

void
EphyFind::SetSelectionAttention (PRBool aAttention)
{
  if (aAttention == mAttention) return;

  mAttention = aAttention;

  nsresult rv;
  nsCOMPtr<nsIDocShell> shell (do_GetInterface (mWebBrowser, &rv));
  /* It's okay for this to fail, if the tab is closing, or if
   * we weren't attached to any tab yet
   */
  if (NS_FAILED (rv) || !shell) return;

  nsCOMPtr<nsISimpleEnumerator> enumerator;
  rv = shell->GetDocShellEnumerator (nsIDocShellTreeItem::typeContent,
				     nsIDocShell::ENUMERATE_FORWARDS,
				     getter_AddRefs (enumerator));
  NS_ENSURE_SUCCESS (rv, );

  PRInt16 display;
  if (aAttention) {
    display = nsISelectionController::SELECTION_ATTENTION;
  } else {
    display = nsISelectionController::SELECTION_ON;
  }

  PRBool hasMore = PR_FALSE;
  while (NS_SUCCEEDED (enumerator->HasMoreElements (&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> element;
    enumerator->GetNext (getter_AddRefs (element));
    if (!element) continue;
	 
    nsCOMPtr<nsISelectionDisplay> sd (do_GetInterface (element));
    if (!sd) continue;
  
    nsCOMPtr<nsISelectionController> controller (do_QueryInterface (sd));
    if (!controller) continue;

    controller->SetDisplaySelection (display);
  }
}

EphyEmbedFindResult
EphyFind::Find (const char *aSearchString,
                PRBool aLinksOnly)
{
  if (!mFinder) return EPHY_EMBED_FIND_NOTFOUND;

  nsString uSearchString;
  NS_CStringToUTF16 (nsCString (aSearchString ? aSearchString : ""),
  		     NS_CSTRING_ENCODING_UTF8, uSearchString);

  SetSelectionAttention (PR_TRUE);

  nsresult rv;
  PRUint16 found = nsITypeAheadFind::FIND_NOTFOUND;
  rv = mFinder->Find (uSearchString, aLinksOnly, &found);

  return (EphyEmbedFindResult) found;
}

EphyEmbedFindResult
EphyFind::FindAgain (PRBool aForward)
{
  if (!mFinder) return EPHY_EMBED_FIND_NOTFOUND;

  SetSelectionAttention (PR_TRUE);

  nsresult rv;
  PRUint16 found = nsITypeAheadFind::FIND_NOTFOUND;
  if (aForward) {
    rv = mFinder->FindNext (&found);
  } else {
    rv = mFinder->FindPrevious (&found);
  }

  return (EphyEmbedFindResult) found;
}

PRBool
EphyFind::ActivateLink (GdkModifierType aMask)
{
	nsresult rv;
	nsCOMPtr<nsIDOMElement> link;
	rv = mFinder->GetFoundLink (getter_AddRefs (link));
	if (NS_FAILED (rv) || !link) return FALSE;

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
	rv = docEvent->CreateEvent (nsString(kKeyEvents), getter_AddRefs (event));
	NS_ENSURE_SUCCESS (rv, FALSE);

	nsCOMPtr<nsIDOMKeyEvent> keyEvent (do_QueryInterface (event));
	NS_ENSURE_TRUE (keyEvent, FALSE);

	rv = keyEvent->InitKeyEvent (nsString (kKeyPress),
				     PR_TRUE /* bubble */,
				     PR_TRUE /* cancelable */,
				     abstractView,
				     (aMask & GDK_CONTROL_MASK) != 0,
				     (aMask & GDK_MOD1_MASK) != 0 /* Alt */,
				     (aMask & GDK_SHIFT_MASK) != 0,
				     /* FIXME when we upgrade to gtk 2.10 */
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
