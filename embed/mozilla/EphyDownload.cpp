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

#include "EphyDownload.h"

#include "nsIExternalHelperAppService.h"
//#include "nsILocalFIleMac.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsIRequest.h"
#include "netCore.h"
#include "nsIObserver.h"

//#include "UDownloadDisplay.h"
//#include "UMacUnicode.h"

//#include "UNavServicesDialogs.h"


//*****************************************************************************
// EphyDownload
//*****************************************************************************
#pragma mark [EphyDownload]

//ADownloadProgressView *EphyDownload::sProgressView;

EphyDownload::EphyDownload() :
    mGotFirstStateChange(false), mIsNetworkTransfer(false),
    mUserCanceled(false),
    mStatus(NS_OK)
{
}

EphyDownload::~EphyDownload()
{
}

NS_IMPL_ISUPPORTS2(EphyDownload, nsIDownload, nsIWebProgressListener)

#pragma mark -
#pragma mark [EphyDownload::nsIDownload]

/* void init (in nsIURI aSource, in nsILocalFile aTarget, in wstring aDisplayName, in nsIMIMEInfo aMIMEInfo, in long long startTime, in nsIWebBrowserPersist aPersist); */
NS_IMETHODIMP
EphyDownload::Init(nsIURI *aSource, nsILocalFile *aTarget, const PRUnichar *aDisplayName,
		   nsIMIMEInfo *aMIMEInfo, PRInt64 startTime, nsIWebBrowserPersist *aPersist)
{
    try {
        mSource = aSource;
        mDestination = aTarget;
        mStartTime = startTime;
        mPercentComplete = 0;
	mInterval = 400000; // ms
	mPriorKRate = 0;
	mRateChanges = 0;
	mRateChangeLimit = 2;
	mIsPaused = PR_FALSE;
	mStartTime = PR_Now();
	mLastUpdate = mStartTime;
        if (aPersist) {
            mWebPersist = aPersist;
            // We have to break this circular ref when the download is done -
            // until nsIWebBrowserPersist supports weak refs - bug #163889.
            aPersist->SetProgressListener(this);
        }
	// UI Rumba
	mDownloaderView = EPHY_DOWNLOADER_VIEW (ephy_embed_shell_get_downloader_view
			(embed_shell));
	downloader_view_add_download (mDownloaderView, "A", "B", "C", (gpointer)this);
  //      EnsureProgressView();
  //      sProgressView->AddDownloadItem(this);
    }
    catch (...) {
        return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

/* readonly attribute nsIURI source; */
NS_IMETHODIMP
EphyDownload::GetSource(nsIURI * *aSource)
{
    NS_ENSURE_ARG_POINTER(aSource);
    NS_IF_ADDREF(*aSource = mSource);
    return NS_OK;
}

/* readonly attribute nsILocalFile target; */
NS_IMETHODIMP
EphyDownload::GetTarget(nsILocalFile * *aTarget)
{
    NS_ENSURE_ARG_POINTER(aTarget);
    NS_IF_ADDREF(*aTarget = mDestination);
    return NS_OK;
}

/* readonly attribute nsIWebBrowserPersist persist; */
NS_IMETHODIMP
EphyDownload::GetPersist(nsIWebBrowserPersist * *aPersist)
{
    NS_ENSURE_ARG_POINTER(aPersist);
    NS_IF_ADDREF(*aPersist = mWebPersist);
    return NS_OK;
}

/* readonly attribute PRInt32 percentComplete; */
NS_IMETHODIMP
EphyDownload::GetPercentComplete(PRInt32 *aPercentComplete)
{
    NS_ENSURE_ARG_POINTER(aPercentComplete);
    *aPercentComplete = mPercentComplete;
    return NS_OK;
}

/* attribute wstring displayName; */
NS_IMETHODIMP
EphyDownload::GetDisplayName(PRUnichar * *aDisplayName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EphyDownload::SetDisplayName(const PRUnichar * aDisplayName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute long long startTime; */
NS_IMETHODIMP
EphyDownload::GetStartTime(PRInt64 *aStartTime)
{
    NS_ENSURE_ARG_POINTER(aStartTime);
    *aStartTime = mStartTime;
    return NS_OK;
}

/* readonly attribute nsIMIMEInfo MIMEInfo; */
NS_IMETHODIMP
EphyDownload::GetMIMEInfo(nsIMIMEInfo * *aMIMEInfo)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute nsIWebProgressListener listener; */
NS_IMETHODIMP
EphyDownload::GetListener(nsIWebProgressListener * *aListener)
{
    NS_ENSURE_ARG_POINTER(aListener);
    NS_IF_ADDREF(*aListener = (nsIWebProgressListener *)this);
    return NS_OK;
}

NS_IMETHODIMP
EphyDownload::SetListener(nsIWebProgressListener * aListener)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute nsIObserver observer; */
NS_IMETHODIMP
EphyDownload::GetObserver(nsIObserver * *aObserver)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EphyDownload::SetObserver(nsIObserver * aObserver)
{
    if (aObserver)
        aObserver->QueryInterface(NS_GET_IID(nsIHelperAppLauncher), getter_AddRefs(mHelperAppLauncher));
    return NS_OK;
}

#pragma mark -
#pragma mark [EphyDownload::nsIWebProgressListener]

/* void onStateChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long aStateFlags, in nsresult aStatus); */
NS_IMETHODIMP 
EphyDownload::OnStateChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest,
			    PRUint32 aStateFlags, nsresult aStatus)
{
    // For a file download via the external helper app service, we will never get a start
    // notification. The helper app service has gotten that notification before it created us.
    if (!mGotFirstStateChange) {
        mIsNetworkTransfer = ((aStateFlags & STATE_IS_NETWORK) != 0);
        mGotFirstStateChange = PR_TRUE;
        //BroadcastMessage(msg_OnDLStart, this);
    }

    if (NS_FAILED(aStatus) && NS_SUCCEEDED(mStatus))
        mStatus = aStatus;
  
    // We will get this even in the event of a cancel,
    if ((aStateFlags & STATE_STOP) && (!mIsNetworkTransfer || (aStateFlags & STATE_IS_NETWORK))) {
        if (mWebPersist) {
            mWebPersist->SetProgressListener(nsnull);
            mWebPersist = nsnull;
        }
        mHelperAppLauncher = nsnull;
        //BroadcastMessage(msg_OnDLComplete, this);
    }
        
    return NS_OK; 
}

/* void onProgressChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aCurSelfProgress, in long aMaxSelfProgress, in long aCurTotalProgress, in long aMaxTotalProgress); */
NS_IMETHODIMP 
EphyDownload::OnProgressChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest,
			       PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress,
			       PRInt32 aCurTotalProgress, PRInt32 aMaxTotalProgress)
{
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
    
    //MsgOnDLProgressChangeInfo info(this, aCurTotalProgress, aMaxTotalProgress);
    // From ProgressListener
    PRInt64 now = PR_Now();
    mElapsed = now - mStartTime;

    if ((now - mLastUpdate < mInterval) && 
	    (aMaxTotalProgress != -1) &&  
	    (aCurTotalProgress < aMaxTotalProgress))
	{
		return NS_OK;
	}
    mLastUpdate = now;


    gfloat progress = -1;
	if (aMaxTotalProgress > 0)
	{
		progress = (gfloat)aCurTotalProgress /
			(gfloat)aMaxTotalProgress;
	}

	/* compute download rate */
	gfloat speed = -1;
	PRInt64 currentRate;
	if (mElapsed)
	{
		currentRate = ((PRInt64)(aCurTotalProgress)) * 1000000 / mElapsed;
	}
	else
	{
		currentRate = 0;
	}		

	if (!mIsPaused && currentRate)
	{
		PRFloat64 currentKRate = ((PRFloat64)currentRate)/1024;
		if (currentKRate != mPriorKRate)
		{
			if (mRateChanges++ == mRateChangeLimit)
			{
				mPriorKRate = currentKRate;
				mRateChanges = 0;
			}
			else
			{
				currentKRate = mPriorKRate;
			}
		}
		else
		{
			mRateChanges = 0;
		}

		speed = currentKRate;
	}
	
	/* compute time remaining */
	gint remaining = -1;
	if (currentRate && (aMaxTotalProgress > 0))
	{
		 remaining = (gint)((aMaxTotalProgress - aCurTotalProgress)
				 /currentRate + 0.5);
	}

    //BroadcastMessage(msg_OnDLProgressChange, &info);
    // UI Rumba
    downloader_view_set_download_progress (mDownloaderView,
		    			   mElapsed / 1000000,
					   remaining,
					   speed,
					   aMaxTotalProgress / 1024.0 + 0.5,
					   aCurTotalProgress / 1024.0 + 0.5,
					   progress,
					   (gpointer)this);

    return NS_OK;
}

/* void onLocationChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsIURI location); */
NS_IMETHODIMP
EphyDownload::OnLocationChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsIURI *location)
{
    return NS_OK;
}

/* void onStatusChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsresult aStatus, in wstring aMessage); */
NS_IMETHODIMP 
EphyDownload::OnStatusChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest,
			     nsresult aStatus, const PRUnichar *aMessage)
{
    return NS_OK;
}

/* void onSecurityChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long state); */
NS_IMETHODIMP 
EphyDownload::OnSecurityChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 state)
{
    return NS_OK;
}

#pragma mark -
#pragma mark [EphyDownload Internal Methods]

void
EphyDownload::Cancel()
{
    mUserCanceled = true;
    // nsWebBrowserPersist does the right thing: After canceling, next time through
    // OnStateChange(), aStatus != NS_OK. This isn't the case with nsExternalHelperAppService.
    if (!mWebPersist)
        mStatus = NS_ERROR_ABORT;
}

void
EphyDownload::Pause()
{
}

void
EphyDownload::Resume()
{
}

/*void
EphyDownload::CreateProgressView()
{
    sProgressView = new CMultiDownloadProgress;
    ThrowIfNil_(sProgressView);
}*/


//*****************************************************************************
// CHelperAppLauncherDialog
//*****************************************************************************   
/*#pragma mark -
#pragma mark [CHelperAppLauncherDialog]

CHelperAppLauncherDialog::CHelperAppLauncherDialog()
{
}

CHelperAppLauncherDialog::~CHelperAppLauncherDialog()
{
}

NS_IMPL_ISUPPORTS1(CHelperAppLauncherDialog, nsIHelperAppLauncherDialog)*/

/* void show (in nsIHelperAppLauncher aLauncher, in nsISupports aContext, in boolean aForced); */
/*NS_IMETHODIMP CHelperAppLauncherDialog::Show(nsIHelperAppLauncher *aLauncher, nsISupports *aContext, PRBool aForced)
{
    return aLauncher->SaveToDisk(nsnull, PR_FALSE);
}*/

/* nsILocalFile promptForSaveToFile (in nsIHelperAppLauncher aLauncher, in nsISupports aWindowContext, in wstring aDefaultFile, in wstring aSuggestedFileExtension); */
/*NS_IMETHODIMP CHelperAppLauncherDialog::PromptForSaveToFile(nsIHelperAppLauncher* aLauncher, 
                                                            nsISupports *aWindowContext, 
                                                            const PRUnichar *aDefaultFile, 
                                                            const PRUnichar *aSuggestedFileExtension, 
                                                            nsILocalFile **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    *_retval = nsnull;
 
    static bool sFirstTime = true;   
	UNavServicesDialogs::LFileDesignator	designator;

    if (sFirstTime) {
        // Get the default download folder and point Nav Sevices to it.
        nsCOMPtr<nsIFile> defaultDownloadDir;
        NS_GetSpecialDirectory(NS_MAC_DEFAULT_DOWNLOAD_DIR, getter_AddRefs(defaultDownloadDir));
        if (defaultDownloadDir) {
            nsCOMPtr<nsILocalFileMac> macDir(do_QueryInterface(defaultDownloadDir));
            FSSpec defaultDownloadSpec;
            if (NS_SUCCEEDED(macDir->GetFSSpec(&defaultDownloadSpec)))
                designator.SetDefaultLocation(defaultDownloadSpec, true);
        }
        sFirstTime = false;
    }
	
	Str255  defaultName;
	CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsDependentString(aDefaultFile), defaultName);
    bool result = designator.AskDesignateFile(defaultName);
    
    // After the dialog is dismissed, process all activation an update events right away.
    // The save dialog code calls UDesktop::Activate after dismissing the dialog. All that
    // does is activate the now frontmost LWindow which was behind the dialog. It does not
    // remove the activate event from the queue. If that event is not processed and removed
    // before we show the progress window, bad things happen. Specifically, the progress
    // dialog will show in front and then, shortly thereafter, the window which was behind this save
    // dialog will be moved to the front.
    
    if (LEventDispatcher::GetCurrentEventDispatcher()) { // Can this ever be NULL?
        EventRecord theEvent;
        while (::WaitNextEvent(updateMask | activMask, &theEvent, 0, nil))
            LEventDispatcher::GetCurrentEventDispatcher()->DispatchEvent(theEvent);
    }
        
    if (result) {
        FSSpec destSpec;
        designator.GetFileSpec(destSpec);
        nsCOMPtr<nsILocalFileMac> destFile;
        NS_NewLocalFileWithFSSpec(&destSpec, PR_TRUE, getter_AddRefs(destFile));
        if (!destFile)
            return NS_ERROR_FAILURE;
        *_retval = destFile;
        NS_ADDREF(*_retval);
        return NS_OK;
    }
    else
        return NS_ERROR_ABORT;
}*/

