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
#include <config.h>

#include <nsIPipe.h>
#include <nsIInputStream.h>
#include <nsIOutputStream.h>
#include <nsIContentViewerContainer.h>
#include <nsIDocumentLoaderFactory.h>
#include <nsNetUtil.h>
#include <prmem.h>

#include "nsXPCOMCID.h"
#include "nsICategoryManager.h"

#include "nsIContentViewer.h"

#include "EmbedStream.h"
#include "EmbedPrivate.h"
#include "EmbedWindow.h"
#include "nsReadableUtils.h"

// nsIInputStream interface

NS_IMPL_ISUPPORTS1(EmbedStream, nsIInputStream)

EmbedStream::EmbedStream()
{
  mOwner       = nsnull;
  mOffset      = 0;
  mDoingStream = PR_FALSE;
}

EmbedStream::~EmbedStream()
{
}

void
EmbedStream::InitOwner(EmbedPrivate *aOwner)
{
  mOwner = aOwner;
}

NS_METHOD
EmbedStream::Init(void)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIInputStream> bufInStream;
  nsCOMPtr<nsIOutputStream> bufOutStream;
  
  rv = NS_NewPipe(getter_AddRefs(bufInStream),
		  getter_AddRefs(bufOutStream));

  if (NS_FAILED(rv)) return rv;
  
  mInputStream  = bufInStream;
  mOutputStream = bufOutStream;
  return NS_OK;
}

NS_METHOD
EmbedStream::OpenStream(const char *aBaseURI, const char *aContentType)
{
  NS_ENSURE_ARG_POINTER(aBaseURI);
  NS_ENSURE_ARG_POINTER(aContentType);

  nsresult rv = NS_OK;

  // if we're already doing a stream then close the current one
  if (mDoingStream)
    CloseStream();

  // set our state
  mDoingStream = PR_TRUE;

  // initialize our streams
  rv = Init();
  if (NS_FAILED(rv))
    return rv;

  // get the content area of our web browser
  nsCOMPtr<nsIWebBrowser> browser;
  mOwner->mWindow->GetWebBrowser(getter_AddRefs(browser));

  // get the viewer container
  nsCOMPtr<nsIContentViewerContainer> viewerContainer;
  viewerContainer = do_GetInterface(browser);

  // create a new uri object
  nsCOMPtr<nsIURI> uri;
  nsCAutoString spec(aBaseURI);
  rv = NS_NewURI(getter_AddRefs(uri), spec.get());
  
  if (NS_FAILED(rv))
    return rv;

  // create a new load group
  rv = NS_NewLoadGroup(getter_AddRefs(mLoadGroup), nsnull);
  if (NS_FAILED(rv))
    return rv;

  // create a new input stream channel
  rv = NS_NewInputStreamChannel(getter_AddRefs(mChannel), uri,
				static_cast<nsIInputStream *>(this),
				nsDependentCString(aContentType),
				EmptyCString());
  if (NS_FAILED(rv))
    return rv;

  // set the channel's load group
  rv = mChannel->SetLoadGroup(mLoadGroup);
  if (NS_FAILED(rv))
    return rv;

  // find a document loader for this content type

  nsXPIDLCString docLoaderContractID;
  nsCOMPtr<nsICategoryManager> catMan(do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return rv;
  rv = catMan->GetCategoryEntry("Gecko-Content-Viewers", aContentType,
                                getter_Copies(docLoaderContractID));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIDocumentLoaderFactory> docLoaderFactory;  
  docLoaderFactory = do_GetService(docLoaderContractID.get(), &rv);
  if (NS_FAILED(rv))
    return rv;

  // ok, create an instance of the content viewer for that command and
  // mime type
  nsCOMPtr<nsIContentViewer> contentViewer;
  rv = docLoaderFactory->CreateInstance("view", mChannel, mLoadGroup,
					aContentType, viewerContainer,
					nsnull,
					getter_AddRefs(mStreamListener),
					getter_AddRefs(contentViewer));
  if (NS_FAILED(rv))
    return rv;

  // set the container viewer container for this content view
  rv = contentViewer->SetContainer(viewerContainer);
  if (NS_FAILED(rv))
    return rv;

  // embed this sucker
  rv = viewerContainer->Embed(contentViewer, "view", nsnull);
  if (NS_FAILED(rv))
    return rv;

  // start our request
  nsCOMPtr<nsIRequest> request = do_QueryInterface(mChannel); 
  rv = mStreamListener->OnStartRequest(request, NULL);
  if (NS_FAILED(rv))
    return rv;
  
  return NS_OK;
}

NS_METHOD
EmbedStream::AppendToStream(const char *aData, gint32 aLen)
{
  nsresult rv;

  // append the data
  rv = Append(aData, aLen);
  if (NS_FAILED(rv))
    return rv;

  // notify our listeners
  nsCOMPtr<nsIRequest> request = do_QueryInterface(mChannel); 
  rv = mStreamListener->OnDataAvailable(request,
					NULL,
					static_cast<nsIInputStream *>(this),
					mOffset, /* offset */
					aLen); /* len */
  // move our counter
  mOffset += aLen;
  if (NS_FAILED(rv))
    return rv;

  return NS_OK;
}

NS_METHOD
EmbedStream::CloseStream(void)
{
  nsresult rv = NS_OK;

  NS_ENSURE_STATE(mDoingStream);
  mDoingStream = PR_FALSE;

  nsCOMPtr<nsIRequest> request = do_QueryInterface(mChannel, &rv); 
  if (NS_FAILED(rv))
    goto loser;
  
  rv = mStreamListener->OnStopRequest(request, NULL, NS_OK);
  if (NS_FAILED(rv))
    return rv;

 loser:
  mLoadGroup = nsnull;
  mChannel = nsnull;
  mStreamListener = nsnull;
  mOffset = 0;

  return rv;
}

NS_METHOD
EmbedStream::Append(const char *aData, PRUint32 aLen)
{
  nsresult rv = NS_OK;
  PRUint32 bytesWritten = 0;
  rv = mOutputStream->Write(aData, aLen, &bytesWritten);
  if (NS_FAILED(rv))
    return rv;
  
  NS_ASSERTION(bytesWritten == aLen,
	       "underlying byffer couldn't handle the write");
  return rv;
}

NS_IMETHODIMP
EmbedStream::Available(PRUint32 *_retval)
{
  return mInputStream->Available(_retval);
}

NS_IMETHODIMP
EmbedStream::Read(char * aBuf, PRUint32 aCount, PRUint32 *_retval)
{
  return mInputStream->Read(aBuf, aCount, _retval);
}

NS_IMETHODIMP EmbedStream::Close(void)
{
  return mInputStream->Close();
}

NS_IMETHODIMP
EmbedStream::ReadSegments(nsWriteSegmentFun aWriter, void * aClosure,
			  PRUint32 aCount, PRUint32 *_retval)
{
  char *readBuf = (char *)nsMemory::Alloc(aCount);
  PRUint32 nBytes;

  if (!readBuf)
    return NS_ERROR_OUT_OF_MEMORY;
  
  nsresult rv = mInputStream->Read(readBuf, aCount, &nBytes);

  *_retval = 0;

  if (NS_SUCCEEDED(rv)) {
    PRUint32 writeCount = 0;
    rv = aWriter(this, aClosure, readBuf, 0, nBytes, &writeCount);

    // XXX writeCount may be less than nBytes!!  This is the wrong
    // way to synthesize ReadSegments.
    NS_ASSERTION(writeCount == nBytes, "data loss");

    // errors returned from the writer end here!
    rv = NS_OK;
  }

  nsMemory::Free(readBuf);

  return rv;
}

NS_IMETHODIMP
EmbedStream::IsNonBlocking(PRBool *aNonBlocking)
{
    return mInputStream->IsNonBlocking(aNonBlocking);
}
