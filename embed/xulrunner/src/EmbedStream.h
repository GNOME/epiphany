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

#include <nsISupports.h>
#include <nsCOMPtr.h>
#include <nsIOutputStream.h>
#include <nsIInputStream.h>
#include <nsILoadGroup.h>
#include <nsIChannel.h>
#include <nsIStreamListener.h>

class EmbedPrivate;
  
class EmbedStream : public nsIInputStream 
{
 public:

  EmbedStream();
  virtual ~EmbedStream();

  void      InitOwner      (EmbedPrivate *aOwner);
  NS_METHOD Init           (void);

  NS_METHOD OpenStream     (const char *aBaseURI, const char *aContentType);
  NS_METHOD AppendToStream (const char *aData, PRInt32 aLen);
  NS_METHOD CloseStream    (void);

  NS_METHOD Append         (const char *aData, PRUint32 aLen);

  // nsISupports
  NS_DECL_ISUPPORTS
  // nsIInputStream
  NS_DECL_NSIINPUTSTREAM

 private:
  nsCOMPtr<nsIOutputStream>   mOutputStream;
  nsCOMPtr<nsIInputStream>    mInputStream;

  nsCOMPtr<nsILoadGroup>      mLoadGroup;
  nsCOMPtr<nsIChannel>        mChannel;
  nsCOMPtr<nsIStreamListener> mStreamListener;

  PRUint32                    mOffset;
  PRBool                      mDoingStream;

  EmbedPrivate *mOwner;

};
