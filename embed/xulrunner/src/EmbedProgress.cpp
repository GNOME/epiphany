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

#include <mozilla-config.h>
#include "config.h"

#include "EmbedProgress.h"

#include <nsIChannel.h>
#include <nsIWebProgress.h>
#include <nsIDOMWindow.h>

#include "nsIURI.h"
#include "nsMemory.h"

#include "gecko-embed-types.h"

#include "gecko-embed-signals.h"

EmbedProgress::EmbedProgress()
: mOwner(nsnull)
{
}

EmbedProgress::~EmbedProgress()
{
}

/* FIXME implement nsIWebProgressListener2 */
NS_IMPL_ISUPPORTS2(EmbedProgress,
		   nsIWebProgressListener,
		   nsISupportsWeakReference)

nsresult
EmbedProgress::Init(GeckoBrowser *aOwner)
{
  mOwner = aOwner;
  return NS_OK;
}

NS_IMETHODIMP
EmbedProgress::OnStateChange(nsIWebProgress *aWebProgress,
			     nsIRequest     *aRequest,
			     PRUint32        aStateFlags,
			     nsresult        aStatus)
{
  // give the widget a chance to attach any listeners
  mOwner->ContentStateChange();
  // if we've got the start flag, emit the signal
  if ((aStateFlags & GECKO_EMBED_FLAG_IS_NETWORK) && 
      (aStateFlags & GECKO_EMBED_FLAG_START))
  {
    g_signal_emit (mOwner->mOwningWidget, gecko_embed_signals[NET_START], 0);
  }

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
  if (!channel) return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIURI> requestURI;
  channel->GetURI(getter_AddRefs(requestURI));
  if (!requestURI) return NS_ERROR_FAILURE;

  if (IsCurrentURI(requestURI))
  {
    // for people who know what they are doing
    g_signal_emit (mOwner->mOwningWidget,
                   gecko_embed_signals[NET_STATE], 0,
                   aStateFlags, aStatus);
  }

  nsEmbedCString uriString;
  requestURI->GetSpec(uriString);
  g_signal_emit (mOwner->mOwningWidget, gecko_embed_signals[NET_STATE_ALL], 0,
		  uriString.get(), (gint)aStateFlags, (gint)aStatus);
  // and for stop, too
  if ((aStateFlags & GECKO_EMBED_FLAG_IS_NETWORK) && 
      (aStateFlags & GECKO_EMBED_FLAG_STOP))
  {
    g_signal_emit (mOwner->mOwningWidget, gecko_embed_signals[NET_STOP], 0);
    // let our owner know that the load finished
    mOwner->ContentFinishedLoading();
  }

  return NS_OK;
}

NS_IMETHODIMP
EmbedProgress::OnProgressChange(nsIWebProgress *aWebProgress,
				nsIRequest     *aRequest,
				PRInt32         aCurSelfProgress,
				PRInt32         aMaxSelfProgress,
				PRInt32         aCurTotalProgress,
				PRInt32         aMaxTotalProgress)
{
  nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
  if (!channel) return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIURI> requestURI;
  channel->GetURI(getter_AddRefs(requestURI));
  if (!requestURI) return NS_ERROR_FAILURE;

  // is it the same as the current uri?
  if (IsCurrentURI(requestURI)) {
    g_signal_emit (mOwner->mOwningWidget,
                   gecko_embed_signals[PROGRESS], 0,
                   aCurTotalProgress, aMaxTotalProgress);
  }

  nsEmbedCString uriString;
  requestURI->GetSpec(uriString);
  g_signal_emit (mOwner->mOwningWidget,
		 gecko_embed_signals[PROGRESS_ALL], 0,
		 uriString.get(),
		 aCurTotalProgress, aMaxTotalProgress);
  return NS_OK;
}

NS_IMETHODIMP
EmbedProgress::OnLocationChange(nsIWebProgress *aWebProgress,
				nsIRequest     *aRequest,
				nsIURI         *aLocation)
{
  nsEmbedCString newURI;
  NS_ENSURE_ARG_POINTER(aLocation);
  aLocation->GetSpec(newURI);

  // Make sure that this is the primary frame change and not
  // just a subframe.
  PRBool isSubFrameLoad = PR_FALSE;
  if (aWebProgress) {
    nsCOMPtr<nsIDOMWindow> domWindow;
    nsCOMPtr<nsIDOMWindow> topDomWindow;

    aWebProgress->GetDOMWindow(getter_AddRefs(domWindow));

    // get the root dom window
    if (domWindow)
      domWindow->GetTop(getter_AddRefs(topDomWindow));

    if (domWindow != topDomWindow)
      isSubFrameLoad = PR_TRUE;
  }

  if (!isSubFrameLoad) {
    mOwner->SetURI(newURI.get());
    g_signal_emit (mOwner->mOwningWidget, gecko_embed_signals[LOCATION], 0);
  }

  return NS_OK;
}

NS_IMETHODIMP
EmbedProgress::OnStatusChange(nsIWebProgress  *aWebProgress,
			      nsIRequest      *aRequest,
			      nsresult         aStatus,
			      const PRUnichar *aMessage)
{
  // need to make a copy so we can safely cast to a void *
  nsEmbedString message(aMessage);
  PRUnichar *tmpString = NS_StringCloneData(message);

  g_signal_emit (mOwner->mOwningWidget,
		 gecko_embed_signals[STATUS_CHANGE], 0,
		 static_cast<void *>(aRequest),
		 static_cast<int>(aStatus),
		 static_cast<void *>(tmpString));

  nsMemory::Free(tmpString);

  return NS_OK;
}

NS_IMETHODIMP
EmbedProgress::OnSecurityChange(nsIWebProgress *aWebProgress,
				nsIRequest     *aRequest,
				PRUint32         aState)
{
  g_signal_emit (mOwner->mOwningWidget,
		 gecko_embed_signals[SECURITY_CHANGE], 0,
		 static_cast<void *>(aRequest),
		 aState);
  return NS_OK;
}

PRBool
EmbedProgress::IsCurrentURI(nsIURI *aURI)
{
  nsEmbedCString spec;
  aURI->GetSpec(spec);

  return strcmp(mOwner->mURI.get(), spec.get()) == 0;
}
