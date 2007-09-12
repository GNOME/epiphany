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

#include <strings.h>

#include "nsIURI.h"
#include <nsMemory.h>

#include "EmbedContentListener.h"
#include "GeckoBrowser.h"
#include "gecko-embed-signals.h"

#include "nsServiceManagerUtils.h"
#include "nsIWebNavigationInfo.h"
#include "nsDocShellCID.h"

EmbedContentListener::EmbedContentListener(void)
{
  mOwner = nsnull;
}

EmbedContentListener::~EmbedContentListener()
{
}

NS_IMPL_ISUPPORTS2(EmbedContentListener,
                   nsIURIContentListener,
                   nsISupportsWeakReference)

nsresult
EmbedContentListener::Init(GeckoBrowser *aOwner)
{
  mOwner = aOwner;
  return NS_OK;
}

NS_IMETHODIMP
EmbedContentListener::OnStartURIOpen(nsIURI     *aURI,
                                     PRBool     *aAbortOpen)
{
  nsresult rv;

  nsEmbedCString specString;
  rv = aURI->GetSpec(specString);

  if (NS_FAILED(rv))
    return rv;

  gboolean retval = FALSE;
  g_signal_emit (mOwner->mOwningWidget,
                 gecko_embed_signals[OPEN_URI], 0,
                 specString.get(), &retval);

  *aAbortOpen = retval;

  return NS_OK;
}

NS_IMETHODIMP
EmbedContentListener::DoContent(const char         *aContentType,
                                PRBool             aIsContentPreferred,
                                nsIRequest         *aRequest,
                                nsIStreamListener **aContentHandler,
                                PRBool             *aAbortProcess)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EmbedContentListener::IsPreferred(const char        *aContentType,
                                  char             **aDesiredContentType,
                                  PRBool            *aCanHandleContent)
{
  return CanHandleContent(aContentType, PR_TRUE, aDesiredContentType,
                          aCanHandleContent);
}

NS_IMETHODIMP
EmbedContentListener::CanHandleContent(const char        *aContentType,
                                       PRBool           aIsContentPreferred,
                                       char             **aDesiredContentType,
                                       PRBool            *_retval)
{
  *_retval = PR_FALSE;
  *aDesiredContentType = nsnull;
  
  if (aContentType) {
    nsCOMPtr<nsIWebNavigationInfo> webNavInfo(
        do_GetService(NS_WEBNAVIGATION_INFO_CONTRACTID));
    if (webNavInfo) {
      PRUint32 canHandle;
      nsresult rv =
        webNavInfo->IsTypeSupported(nsDependentCString(aContentType),
            mOwner ? mOwner->mNavigation.get() : nsnull,
            &canHandle);
      NS_ENSURE_SUCCESS(rv, rv);
      *_retval = (canHandle != nsIWebNavigationInfo::UNSUPPORTED);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
EmbedContentListener::GetLoadCookie(nsISupports **aLoadCookie)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EmbedContentListener::SetLoadCookie(nsISupports *aLoadCookie)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EmbedContentListener::GetParentContentListener(nsIURIContentListener **aParent)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EmbedContentListener::SetParentContentListener(nsIURIContentListener *aParent)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
