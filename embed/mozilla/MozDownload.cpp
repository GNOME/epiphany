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
 * ***** END LICENSE BLOCK ***** */

#include "MozDownload.h"
#include "mozilla-download.h"

#include "nsIExternalHelperAppService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsIRequest.h"
#include "netCore.h"
#include "nsIObserver.h"

//*****************************************************************************
// MozDownload
//*****************************************************************************

MozDownload::MozDownload() :
    mGotFirstStateChange(false), mIsNetworkTransfer(false),
    mUserCanceled(false),
    mStatus(NS_OK),
    mEmbedPersist(nsnull),
    mDownloadState(EPHY_DOWNLOAD_DOWNLOADING)
{
}

MozDownload::~MozDownload()
{
}

NS_IMPL_ISUPPORTS2(MozDownload, nsIDownload, nsIWebProgressListener)

NS_IMETHODIMP
MozDownload::InitForEmbed (nsIURI *aSource, nsILocalFile *aTarget, const PRUnichar *aDisplayName,
		           nsIMIMEInfo *aMIMEInfo, PRInt64 startTime, nsIWebBrowserPersist *aPersist,
		           MozillaEmbedPersist *aEmbedPersist)
{
	mEmbedPersist = aEmbedPersist;
	return Init (aSource, aTarget, aDisplayName, aMIMEInfo, startTime, aPersist);
}

/* void init (in nsIURI aSource, in nsILocalFile aTarget, in wstring aDisplayName, in nsIMIMEInfo aMIMEInfo, in long long startTime, in nsIWebBrowserPersist aPersist); */
NS_IMETHODIMP
MozDownload::Init(nsIURI *aSource, nsILocalFile *aTarget, const PRUnichar *aDisplayName,
		   nsIMIMEInfo *aMIMEInfo, PRInt64 startTime, nsIWebBrowserPersist *aPersist)
{
	PRBool addToView = PR_TRUE;

	if (mEmbedPersist)
	{
		EmbedPersistFlags flags;

		ephy_embed_persist_get_flags (EPHY_EMBED_PERSIST (mEmbedPersist), &flags);

		addToView = !(flags & EMBED_PERSIST_NO_VIEW);
	}

	mSource = aSource;
	mDestination = aTarget;
	mStartTime = startTime;
	mPercentComplete = 0;
	mInterval = 4000; // in ms
	mLastUpdate = mStartTime;

	if (aPersist)
	{
		mWebPersist = aPersist;
		aPersist->SetProgressListener(this);
        }

	if (addToView)
	{ 
		DownloaderView *dview;
		dview = EPHY_DOWNLOADER_VIEW (ephy_embed_shell_get_downloader_view
				             (embed_shell));
		mEphyDownload = mozilla_download_new ();
		MOZILLA_DOWNLOAD (mEphyDownload)->moz_download = this;
		downloader_view_add_download (dview, mEphyDownload);
	}
	else
	{
		mEphyDownload = nsnull;
	}

	return NS_OK;
}

/* readonly attribute nsIURI source; */
NS_IMETHODIMP
MozDownload::GetSource(nsIURI * *aSource)
{
    NS_ENSURE_ARG_POINTER(aSource);
    NS_IF_ADDREF(*aSource = mSource);
    return NS_OK;
}

/* readonly attribute nsILocalFile target; */
NS_IMETHODIMP
MozDownload::GetTarget(nsILocalFile * *aTarget)
{
    NS_ENSURE_ARG_POINTER(aTarget);
    NS_IF_ADDREF(*aTarget = mDestination);
    return NS_OK;
}

/* readonly attribute nsIWebBrowserPersist persist; */
NS_IMETHODIMP
MozDownload::GetPersist(nsIWebBrowserPersist * *aPersist)
{
    NS_ENSURE_ARG_POINTER(aPersist);
    NS_IF_ADDREF(*aPersist = mWebPersist);
    return NS_OK;
}

/* readonly attribute PRInt32 percentComplete; */
NS_IMETHODIMP
MozDownload::GetPercentComplete(PRInt32 *aPercentComplete)
{
    NS_ENSURE_ARG_POINTER(aPercentComplete);
    *aPercentComplete = mPercentComplete;
    return NS_OK;
}

NS_IMETHODIMP
MozDownload::GetTotalProgress(PRInt32 *aTotalProgress)
{
    NS_ENSURE_ARG_POINTER(aTotalProgress);
    *aTotalProgress = mTotalProgress;
    return NS_OK;
}

NS_IMETHODIMP
MozDownload::GetCurrentProgress(PRInt32 *aCurrentProgress)
{
    NS_ENSURE_ARG_POINTER(aCurrentProgress);
    *aCurrentProgress = mCurrentProgress;
    return NS_OK;
}

NS_IMETHODIMP
MozDownload::GetState(EphyDownloadState *aDownloadState)
{
    NS_ENSURE_ARG_POINTER(aDownloadState);
    *aDownloadState = mDownloadState;
    return NS_OK;
}

/* attribute wstring displayName; */
NS_IMETHODIMP
MozDownload::GetDisplayName(PRUnichar * *aDisplayName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MozDownload::SetDisplayName(const PRUnichar * aDisplayName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute long long startTime; */
NS_IMETHODIMP
MozDownload::GetStartTime(PRInt64 *aStartTime)
{
    NS_ENSURE_ARG_POINTER(aStartTime);
    *aStartTime = mStartTime;
    return NS_OK;
}

NS_IMETHODIMP
MozDownload::GetElapsedTime(PRInt64 *aElapsedTime)
{
    NS_ENSURE_ARG_POINTER(aElapsedTime);
    *aElapsedTime = PR_Now() - mStartTime;
    return NS_OK;
}

/* readonly attribute nsIMIMEInfo MIMEInfo; */
NS_IMETHODIMP
MozDownload::GetMIMEInfo(nsIMIMEInfo * *aMIMEInfo)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute nsIWebProgressListener listener; */
NS_IMETHODIMP
MozDownload::GetListener(nsIWebProgressListener * *aListener)
{
    NS_ENSURE_ARG_POINTER(aListener);
    NS_IF_ADDREF(*aListener = (nsIWebProgressListener *)this);
    return NS_OK;
}

NS_IMETHODIMP
MozDownload::SetListener(nsIWebProgressListener * aListener)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute nsIObserver observer; */
NS_IMETHODIMP
MozDownload::GetObserver(nsIObserver * *aObserver)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MozDownload::SetObserver(nsIObserver * aObserver)
{
    if (aObserver)
        aObserver->QueryInterface(NS_GET_IID(nsIHelperAppLauncher), getter_AddRefs(mHelperAppLauncher));
    return NS_OK;
}

/* void onStateChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long aStateFlags, in nsresult aStatus); */
NS_IMETHODIMP 
MozDownload::OnStateChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest,
			    PRUint32 aStateFlags, nsresult aStatus)
{
    // For a file download via the external helper app service, we will never get a start
    // notification. The helper app service has gotten that notification before it created us.
    if (!mGotFirstStateChange) {
        mIsNetworkTransfer = ((aStateFlags & STATE_IS_NETWORK) != 0);
        mGotFirstStateChange = PR_TRUE;
    }

    if (NS_FAILED(aStatus) && NS_SUCCEEDED(mStatus))
        mStatus = aStatus;
  
    // We will get this even in the event of a cancel,
    if ((aStateFlags & STATE_STOP) && (!mIsNetworkTransfer || (aStateFlags & STATE_IS_NETWORK))) {
	/* Keep us alive */
	nsCOMPtr<nsIDownload> kungFuDeathGrip(this);

	mDownloadState = NS_SUCCEEDED (aStatus) ? EPHY_DOWNLOAD_COMPLETED : EPHY_DOWNLOAD_FAILED;
	if (mEphyDownload)
	{
		g_signal_emit_by_name (mEphyDownload, "changed");
	}

        if (mWebPersist)
	{
            mWebPersist->SetProgressListener(nsnull);
            mWebPersist = nsnull;
        }
        mHelperAppLauncher = nsnull;

	if (mEmbedPersist)
	{
		if (NS_SUCCEEDED (aStatus))
		{
			mozilla_embed_persist_completed (mEmbedPersist);
		}
		else
		{
			mozilla_embed_persist_cancelled (mEmbedPersist);
		}
	}
    }
        
    return NS_OK; 
}

/* void onProgressChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aCurSelfProgress, in long aMaxSelfProgress, in long aCurTotalProgress, in long aMaxTotalProgress); */
NS_IMETHODIMP 
MozDownload::OnProgressChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest,
			       PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress,
			       PRInt32 aCurTotalProgress, PRInt32 aMaxTotalProgress)
{
    PRInt64 now = PR_Now ();

    if ((now - mLastUpdate < mInterval) &&
	 (aMaxTotalProgress != -1) &&
	 (aCurTotalProgress < aMaxTotalProgress))
	return NS_OK;

    mLastUpdate = now;
    
    if (mUserCanceled) {
        if (mHelperAppLauncher)
            mHelperAppLauncher->Cancel();
        else if (aRequest)
            aRequest->Cancel(NS_BINDING_ABORTED);
        mUserCanceled = false;
    }
    if (aMaxTotalProgress == -1)
        mPercentComplete = -1;
    else
        mPercentComplete = (PRInt32)(((float)aCurTotalProgress / (float)aMaxTotalProgress) * 100.0 + 0.5);

    mTotalProgress = aMaxTotalProgress;
    mCurrentProgress = aCurTotalProgress;

    if (mEphyDownload)
    {
      g_signal_emit_by_name (mEphyDownload, "changed");
    }

    return NS_OK;
}

/* void onLocationChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsIURI location); */
NS_IMETHODIMP
MozDownload::OnLocationChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsIURI *location)
{
    return NS_OK;
}

/* void onStatusChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsresult aStatus, in wstring aMessage); */
NS_IMETHODIMP 
MozDownload::OnStatusChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest,
			     nsresult aStatus, const PRUnichar *aMessage)
{
	return NS_OK;
}

/* void onSecurityChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long state); */
NS_IMETHODIMP 
MozDownload::OnSecurityChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 state)
{
    return NS_OK;
}

void
MozDownload::Cancel()
{
    mUserCanceled = true;
    // nsWebBrowserPersist does the right thing: After canceling, next time through
    // OnStateChange(), aStatus != NS_OK. This isn't the case with nsExternalHelperAppService.
    if (!mWebPersist)
        mStatus = NS_ERROR_ABORT;
}

void
MozDownload::Pause()
{
}

void
MozDownload::Resume()
{
}
