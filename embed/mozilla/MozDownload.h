/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Conrad Carlen <ccarlen@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK *****
 *
 * $Id$
 */

#ifndef MozDownload_h__
#define MozDownload_h__

#include <libgnomevfs/gnome-vfs-mime-handlers.h>

#include <nsCOMPtr.h>
#include <nsIInterfaceRequestor.h>
#include <nsITransfer.h>
#include <nsIWebProgressListener.h>

#include "mozilla-embed-persist.h"
#include "downloader-view.h"
#include "ephy-download.h"
#include "ephy-embed-shell.h"

class nsICancelable;
class nsIDOMDocument;
class nsIInputStream;
class nsILocalFile;
class nsIMIMEInfo;
class nsIObserver;
class nsIRequest;
class nsIURI;
class nsIWebBrowserPersist;

/* MozDownload
   Holds information used to display a single download in the UI. This object is 
   created in one of two ways:
   (1) By nsExternalHelperAppHandler when Gecko encounters a  MIME type which
       it doesn't itself handle. In this case, the notifications sent to
       nsIDownload are controlled by nsExternalHelperAppHandler.
   (2) By the embedding app's file saving code when saving a web page or a link
       target. See CHeaderSniffer.cpp. In this case, the notifications sent to
       nsIDownload are controlled by the implementation of nsIWebBrowserPersist.
*/

#define MOZ_DOWNLOAD_CID                \
{ /* d2a2f743-f126-4f1f-1234-d4e50490f112 */         \
	0xd2a2f743,                                      \
	0xf126,                                          \
	0x4f1f,                                          \
	{0x12, 0x34, 0xd4, 0xe5, 0x04, 0x90, 0xf1, 0x12} \
}

#define MOZ_DOWNLOAD_CLASSNAME "Ephy's Download Progress Dialog"

nsresult InitiateMozillaDownload (nsIDOMDocument *domDocument, nsIURI *sourceUri,
				  nsILocalFile* inDestFile, const char *contentType,
				  nsIURI* inOriginalURI, MozillaEmbedPersist *embedPersist,
				  nsIInputStream *postData, nsISupports *aCacheKey,
				  PRInt64 aMaxSize);
nsresult BuildDownloadPath (const char *defaultFileName, nsILocalFile **_retval);

class MozDownload : public nsITransfer,
		    public nsIInterfaceRequestor
{
public:
        MozDownload();
	virtual ~MozDownload();
    
	NS_DECL_ISUPPORTS
	NS_DECL_NSIWEBPROGRESSLISTENER
	NS_DECL_NSIWEBPROGRESSLISTENER2
	NS_DECL_NSITRANSFER
	NS_DECL_NSIINTERFACEREQUESTOR

	nsresult GetMIMEInfo (nsIMIMEInfo **aMIMEInfo);
	nsresult GetTargetFile (nsILocalFile **aFile);
	nsresult GetSource(nsIURI * *aSource);
	nsresult GetPercentComplete(PRInt32 *aPercentComplete);

	virtual void Cancel();
	virtual void Pause();
	virtual void Resume();

	nsresult GetState           (EphyDownloadState *aDownloadState);
	nsresult GetCurrentProgress (PRInt64 *aCurrentProgress);
	nsresult GetTotalProgress   (PRInt64 *aTProgress);
 	nsresult GetElapsedTime     (PRInt64 *aTProgress);

	nsresult InitForEmbed       (nsIURI *aSource, nsIURI *aTarget,
				     const nsAString &aDisplayName, nsIMIMEInfo *aMIMEInfo,
				     PRTime aStartTime, nsILocalFile *aTempFile,
				     nsICancelable *aCancelable, MozillaEmbedPersist *aEmbedPersist,
				     PRInt64 aMaxSize);

protected:
	nsCOMPtr<nsIURI>        mSource;
	nsCOMPtr<nsIURI>        mDestination;

	nsCOMPtr<nsIMIMEInfo>   mMIMEInfo;
	PRTime                  mStartTime;
	PRTime		    	mLastUpdate;
	PRInt64		    	mElapsed;
	PRInt32		    	mInterval;
	PRInt32                 mPercentComplete;
	PRInt64                 mTotalProgress;
	PRInt64                 mCurrentProgress;
	PRInt64			mMaxSize;

	nsresult                mStatus;

	nsCOMPtr<nsICancelable>		mCancelable;
	nsCOMPtr<nsIRequest>		mRequest;
	EphyDownload                   *mEphyDownload;
	DownloaderView                 *mDownloaderView;
	MozillaEmbedPersist            *mEmbedPersist;
	EphyDownloadState               mDownloadState;
};

#endif // MozDownload_h__
