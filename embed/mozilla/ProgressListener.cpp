/*
 *  Copyright (C) 2001 Philip Langdale, Matthew Aubury
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ProgressListener.h"
#include "ephy-file-helpers.h"
#include "downloader-view.h"
#include "mozilla-embed-persist.h"
#include "nsXPIDLString.h"
#include "nsCOMPtr.h"

static void
download_remove_cb (DownloaderView *dv, GProgressListener *Changed, GProgressListener *Progress);
static void
download_resume_cb (DownloaderView *dv, GProgressListener *Changed, GProgressListener *Progress);
static void
download_pause_cb (DownloaderView *dv, GProgressListener *Changed, GProgressListener *Progress);

NS_IMPL_ISUPPORTS4 (GProgressListener, nsIDownload, nsIWebProgressListener,
		    nsIProgressDialog, nsISupportsWeakReference)

//---------------------------------------------------------------------------

//---------------------------------------------------------------------------

GProgressListener::GProgressListener () : mLauncher(nsnull),
					  mPersist(nsnull),
				          mHandler(nsnull),
					  mObserver(nsnull),
					  mMIMEInfo(nsnull),
					  mPercentComplete(0)
{
	NS_INIT_ISUPPORTS ();
}

GProgressListener::~GProgressListener ()
{
	/* destructor code */
}

NS_METHOD GProgressListener::InitForPersist (nsIWebBrowserPersist *aPersist,
					     nsIDOMWindow *aParent, 
					     nsIURI *aURI,
					     nsIFile *aFile,
					     DownloadAction aAction,
					     EphyEmbedPersist *ephyPersist,
					     PRBool Dialog,
					     PRInt64 aTimeDownloadStarted)
{
	nsresult rv;
	/* fill in download details */
	mAction = aAction;
	mParent = aParent;
	mDialog = Dialog;
	mUri = aURI;
	mFile = aFile;
	mPersist = aPersist;
	mTimeDownloadStarted = aTimeDownloadStarted;
	mEphyPersist = ephyPersist;

	/* do remaining init */
	rv = PrivateInit ();

	/* pick up progress messages */
	mPersist->SetProgressListener (this);

	return rv;
}

NS_METHOD GProgressListener::PrivateInit (void)
{
	mInterval            = 500000;     /* in microsecs, 0.5s */
	mPriorKRate          = 0;
	mRateChanges         = 0;
	mRateChangeLimit     = 2;          /* only update rate every second */
	mIsPaused            = PR_FALSE;
	mAbort               = PR_FALSE;
	mStartTime           = PR_Now ();
	mLastUpdate          = mStartTime;
	mElapsed             = 0;

	if (mDialog)
	{
		gchar *filename, *source, *dest;
		nsAutoString uTmp;
		nsCAutoString cTmp;
		nsresult rv;

		rv = mFile->GetLeafName (uTmp);
		if (NS_FAILED (rv)) return NS_ERROR_FAILURE;
		filename = g_strdup (NS_ConvertUCS2toUTF8(uTmp).get());

		rv = mFile->GetPath (uTmp);
		if (NS_FAILED (rv)) return NS_ERROR_FAILURE;
		dest = g_strdup (NS_ConvertUCS2toUTF8(uTmp).get());

		rv = mUri->GetSpec (cTmp);
		if (NS_FAILED (rv)) return NS_ERROR_FAILURE;
		source = g_strdup (cTmp.get());

		mDownloaderView = EPHY_DOWNLOADER_VIEW
			(ephy_embed_shell_get_downloader_view (embed_shell));
		downloader_view_add_download (mDownloaderView, filename, source,
					      dest, (gpointer)this);
		g_free (source);
		g_free (dest);
		g_free (filename);

		g_signal_connect (G_OBJECT (mDownloaderView), 
				  "download_remove",
				  G_CALLBACK (download_remove_cb),
				  this);
		g_signal_connect (G_OBJECT (mDownloaderView), 
				  "download_pause",
				  G_CALLBACK (download_pause_cb),
				  this);
		g_signal_connect (G_OBJECT (mDownloaderView), 
				  "download_resume",
				  G_CALLBACK (download_resume_cb),
				  this);
	}

	return NS_OK;
}

NS_IMETHODIMP GProgressListener::Init(nsIURI *aSource,
                                      nsILocalFile *aTarget,
                                      const PRUnichar *aDisplayName,
				      nsIMIMEInfo *aMIMEInfo,
                                      PRInt64 aStartTime,
                                      nsIWebBrowserPersist *aPersist)
{
        mUri = aSource;
        mFile = aTarget;
        mTimeDownloadStarted = aStartTime;
        mStartTime = aStartTime;
        mPersist = aPersist;
	mMIMEInfo = aMIMEInfo;
        mAction = ACTION_NONE;
	if(mMIMEInfo)
	{
		nsMIMEInfoHandleAction mimeAction;
		if(NS_SUCCEEDED(mMIMEInfo->GetPreferredAction(&mimeAction)))
		{
			mAction = (mimeAction == nsIMIMEInfo::useHelperApp) ?
				  ACTION_SAVEFORHELPER : ACTION_NONE;
		}
	}
        mDialog = TRUE;

        return PrivateInit();
}

NS_IMETHODIMP GProgressListener::Open(nsIDOMWindow *aParent)
{
        mParent = aParent;
        mDialog = TRUE;

        return NS_OK;
}

/* attribute long long startTime; */
NS_IMETHODIMP GProgressListener::GetStartTime(PRInt64 *aStartTime)
{
        *aStartTime = mStartTime;
        return NS_OK;
}

/* attribute nsIURI source; */
NS_IMETHODIMP GProgressListener::GetSource(nsIURI * *aSource)
{
        NS_IF_ADDREF(*aSource = mUri);
        return NS_OK;
}

/* attribute nsILocalFile target; */
NS_IMETHODIMP GProgressListener::GetTarget(nsILocalFile * *aTarget)
{
        nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(mFile);
        NS_IF_ADDREF(*aTarget = localFile);
        return NS_OK;
}

NS_IMETHODIMP GProgressListener::GetMIMEInfo(nsIMIMEInfo * *aMIMEInfo)
{
	NS_IF_ADDREF(*aMIMEInfo = mMIMEInfo);
	return NS_OK;
}

/* attribute nsIObserver observer; */
NS_IMETHODIMP GProgressListener::GetObserver(nsIObserver * *aObserver)
{
        NS_IF_ADDREF(*aObserver = mObserver);
        return NS_OK;
}
NS_IMETHODIMP GProgressListener::SetObserver(nsIObserver * aObserver)
{
        mObserver = aObserver;
        return NS_OK;
}

/* attribute nsIWebProgressListener listener; */
NS_IMETHODIMP GProgressListener::GetListener(nsIWebProgressListener * *aListener)
{
        *aListener = nsnull;
        return NS_OK;
}
NS_IMETHODIMP GProgressListener::SetListener(nsIWebProgressListener * aListener)
{
        return NS_OK;
}

/* readonly attribute PRInt32 percentComplete; */
NS_IMETHODIMP GProgressListener::GetPercentComplete(PRInt32 *aPercentComplete)
{
        return *aPercentComplete = mPercentComplete;
}

/* attribute wstring displayName; */
NS_IMETHODIMP GProgressListener::GetDisplayName(PRUnichar * *aDisplayName)
{
        *aDisplayName = nsnull;
        return NS_OK;
}
NS_IMETHODIMP GProgressListener::SetDisplayName(const PRUnichar * aDisplayName)
{
        return NS_OK;
}

NS_IMETHODIMP GProgressListener::GetPersist(nsIWebBrowserPersist * *aPersist)
{
        NS_IF_ADDREF(*aPersist = mPersist);
        return NS_OK;
}

NS_IMETHODIMP GProgressListener::SetDialog(nsIDOMWindow *aDialog)
{
        return NS_OK;
}

NS_IMETHODIMP GProgressListener::GetDialog(nsIDOMWindow * *aDialog)
{
        *aDialog = nsnull;
        return NS_OK;
}

/* attribute PRBool cancelDownloadOnClose; */
NS_IMETHODIMP  GProgressListener::GetCancelDownloadOnClose(PRBool *aCancelDownloadOnClose)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP  GProgressListener::SetCancelDownloadOnClose(PRBool aCancelDownloadOnClose)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GProgressListener::LaunchHandler (PersistHandlerInfo *handler)
{
	nsresult rv;
	nsCOMPtr<nsIExternalHelperAppService> helperService =
		do_GetService (NS_EXTERNALHELPERAPPSERVICE_CONTRACTID);

	nsCOMPtr<nsPIExternalAppLauncher> appLauncher =
		do_QueryInterface (helperService, &rv);
	if (NS_SUCCEEDED(rv))
	{
		appLauncher->DeleteTemporaryFileOnExit(mFile);
	}

	nsAutoString uFileName;
        
	mFile->GetPath(uFileName);
        const nsACString &cFileName = NS_ConvertUCS2toUTF8(uFileName);
	
	char *fname = g_strdup(PromiseFlatCString(cFileName).get());
	ephy_file_launch_application (handler->command,
				      fname,
				      handler->need_terminal);
	g_free (fname);

	return NS_OK;
}

/*
 * void onStateChange (in nsIWebProgress aWebProgress, 
 *                     in nsIRequest aRequest, 
 *		       in long aStateFlags, 
 *		       in unsigned long aStatus);
 */
NS_IMETHODIMP GProgressListener::OnStateChange (nsIWebProgress *aWebProgress,
						nsIRequest *aRequest,
						PRUint32 aStateFlags,
						PRUint32 aStatus)
{
	if (mAbort) return NS_ERROR_FAILURE;

	if (aStateFlags & nsIWebProgressListener::STATE_STOP)
	{

		if (mDialog)
		{
			downloader_view_set_download_status (mDownloaderView, 
							     DOWNLOAD_STATUS_COMPLETED, 
							     (gpointer)this);
		}

		switch (mAction)
                {
		case ACTION_SAVEFORHELPER:
			LaunchHelperApp();
			break;
			
		case ACTION_NONE:
			if (mLauncher)
			{
				mLauncher->CloseProgressWindow ();
			}
			break;
		case ACTION_OBJECT_NOTIFY:

			g_return_val_if_fail (EPHY_IS_EMBED_PERSIST (mEphyPersist),
					      NS_ERROR_FAILURE);
			
			PersistHandlerInfo *handler;
			
			g_object_get (mEphyPersist,
		      		      "handler", &handler,        
		      		      NULL);

			if (handler)
			{
				LaunchHandler (handler);
			}
		
			mozilla_embed_persist_completed
				(MOZILLA_EMBED_PERSIST (mEphyPersist));
		}		
	}

	return NS_OK;
}

/*
 * void onProgressChange (in nsIWebProgress aWebProgress, 
 *                        in nsIRequest aRequest, 
 *                        in long aCurSelfProgress, 
 *                        in long aMaxSelfProgress, 
 *                        in long aCurTotalProgress, 
 *                        in long aMaxTotalProgress); 
 */
NS_IMETHODIMP GProgressListener::
			OnProgressChange (nsIWebProgress *aWebProgress,
					  nsIRequest *aRequest,
					  PRInt32 aCurSelfProgress,
					  PRInt32 aMaxSelfProgress,
					  PRInt32 aCurTotalProgress,
					  PRInt32 aMaxTotalProgress)
{
	if (mAbort) return NS_ERROR_FAILURE;

	/* FIXME maxsize check here */

	if (!mDialog) return NS_OK;

	mRequest = aRequest;

	PRInt64 now = PR_Now ();

	/* get out if we're updating too quickly */
	if ((now - mLastUpdate < mInterval) && 
	    (aMaxTotalProgress != -1) &&  
	    (aCurTotalProgress < aMaxTotalProgress))
	{
		return NS_OK;
	}
	mLastUpdate = now;

	/* compute elapsed time */
	mElapsed = now - mStartTime;

	/* compute size done */
	PRInt32 currentKBytes = (PRInt32)(aCurTotalProgress / 1024.0 + 0.5);

	/* compute total size */
	PRInt32 totalKBytes = (PRInt32)(aMaxTotalProgress / 1024.0 + 0.5);

	/* compute progress value */
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

	downloader_view_set_download_progress (mDownloaderView,
					       mElapsed / 1000000,
					       remaining,
					       speed,
					       totalKBytes,
					       currentKBytes,
					       progress,
					       (gpointer)this);

	return NS_OK;
}

/* void onLocationChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsIURI location); */
NS_IMETHODIMP GProgressListener::
			OnLocationChange(nsIWebProgress *aWebProgress,
					 nsIRequest *aRequest, nsIURI *location)
{
	return NS_OK;
}

/* void onStatusChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsresult aStatus, in wstring aMessage); */
NS_IMETHODIMP GProgressListener::
			OnStatusChange(nsIWebProgress *aWebProgress,
				       nsIRequest *aRequest, nsresult aStatus,
				       const PRUnichar *aMessage)
{
	return NS_OK;
}

/* void onSecurityChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long state); */
NS_IMETHODIMP GProgressListener::
			OnSecurityChange(nsIWebProgress *aWebProgress,
					 nsIRequest *aRequest,
					 PRUint32 state)
{
	return NS_OK;
}

//---------------------------------------------------------------------------

NS_METHOD GProgressListener::LaunchHelperApp (void)
{
	if (!mMIMEInfo)
		return NS_ERROR_FAILURE;

	nsresult rv;

	nsCOMPtr<nsIFile> helperFile;
	rv = mMIMEInfo->GetPreferredApplicationHandler(getter_AddRefs(helperFile));
	if(NS_FAILED(rv)) return NS_ERROR_FAILURE;

	nsCAutoString helperFileName;
	rv = helperFile->GetNativePath(helperFileName);
	if(NS_FAILED(rv)) return NS_ERROR_FAILURE;

	nsMIMEInfoHandleAction mimeAction;
	rv = mMIMEInfo->GetPreferredAction(&mimeAction);
	if(NS_FAILED(rv)) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIExternalHelperAppService> helperService =
		do_GetService (NS_EXTERNALHELPERAPPSERVICE_CONTRACTID, &rv);
	if (NS_SUCCEEDED(rv))
	{
		nsCOMPtr<nsPIExternalAppLauncher> appLauncher =
			do_QueryInterface (helperService, &rv);
		if (NS_SUCCEEDED(rv))
		{
			appLauncher->DeleteTemporaryFileOnExit(mFile);
		}
	}

	nsCAutoString cFileName;
	mFile->GetNativePath(cFileName);
	if(NS_FAILED(rv)) return NS_ERROR_FAILURE;

	nsXPIDLString helperDesc;
	rv = mMIMEInfo->GetApplicationDescription(getter_Copies(helperDesc));
	if(NS_FAILED(rv)) return NS_ERROR_FAILURE;

	gboolean terminalHelper = 
		helperDesc.Equals(NS_LITERAL_STRING("runInTerminal")) ?
		TRUE : FALSE;

	ephy_file_launch_application (helperFileName.get(),
				      cFileName.get(),
				      terminalHelper);

	return NS_OK;
}

nsresult GProgressListener::Pause (void)
{
        nsresult rv;

        if (!mIsPaused)
        {
                rv = mRequest->Suspend ();
                if (NS_SUCCEEDED (rv))
                {
                        mIsPaused = PR_TRUE;
                }
        }
        else
        {
                rv = NS_ERROR_FAILURE;
        }

        return rv;
}

nsresult GProgressListener::Resume (void)
{
        nsresult rv;

        if (mIsPaused)
        {
                rv = mRequest->Resume ();
                if (NS_SUCCEEDED (rv))
                {
                        mIsPaused = PR_FALSE;
                }
        }
        else
        {
                rv = NS_ERROR_FAILURE;
        }

        return rv;
}

nsresult GProgressListener::Abort (void)
{
	PRBool notify;
	notify = (mAction == ACTION_OBJECT_NOTIFY);
        mAction = ACTION_NONE;
        mAbort = PR_TRUE;

        if (mObserver)
        {
                mObserver->Observe(NS_ISUPPORTS_CAST(nsIProgressDialog*, this),
                                             "oncancel", nsnull);
                OnStateChange(nsnull, nsnull,
                              nsIWebProgressListener::STATE_STOP, 0);
        }

        if (mPersist)
        {
                mPersist->CancelSave ();
        }
        else if (mLauncher)
        {
                mLauncher->Cancel ();
        }
        else
        {
                return NS_ERROR_FAILURE;
        }

	if (notify)
	{	
		mozilla_embed_persist_cancelled
			(MOZILLA_EMBED_PERSIST (mEphyPersist));	
	}

	return NS_OK;
}

static void
download_remove_cb (DownloaderView *dv, GProgressListener *Changed, GProgressListener *Progress)
{
	if (Changed == Progress){
		Progress->Abort();
	}
}

static void
download_resume_cb (DownloaderView *dv, GProgressListener *Changed, GProgressListener *Progress)
{
	if (Changed == Progress) {
		Progress->Resume();
	}
}

static void
download_pause_cb (DownloaderView *dv, GProgressListener *Changed, GProgressListener *Progress)
{
	if (Changed == Progress) {
		Progress->Pause();
	}
}
