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
 * Adapted for epiphany by Marco Pesenti Gritti <marco@gnome.org>
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

#include "MozDownload.h"
#include "mozilla-download.h"
#include "eel-gconf-extensions.h"
#include "ephy-prefs.h"
#include <libgnomevfs/gnome-vfs-utils.h>

#include "nsIExternalHelperAppService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsIRequest.h"
#include "netCore.h"

const char* const persistContractID = "@mozilla.org/embedding/browser/nsWebBrowserPersist;1";

MozDownload::MozDownload() :
	mGotFirstStateChange(false), mIsNetworkTransfer(false),
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

		flags = ephy_embed_persist_get_flags (EPHY_EMBED_PERSIST (mEmbedPersist));

		addToView = !(flags & EMBED_PERSIST_NO_VIEW);
	}

	mSource = aSource;
	mDestination = aTarget;
	mStartTime = startTime;
	mTotalProgress = 0;
	mCurrentProgress = 0;
	mPercentComplete = 0;
	mInterval = 4000; /* in ms */
	mLastUpdate = mStartTime;

	if (aPersist)
	{
		mWebPersist = aPersist;
		aPersist->SetProgressListener(this);
        }

	if (addToView)
	{ 
		DownloaderView *dview;
		dview = EPHY_DOWNLOADER_VIEW
			(ephy_embed_shell_get_downloader_view (embed_shell));
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

NS_IMETHODIMP
MozDownload::GetSource(nsIURI **aSource)
{
	NS_ENSURE_ARG_POINTER(aSource);
	NS_IF_ADDREF(*aSource = mSource);

	return NS_OK;
}

NS_IMETHODIMP
MozDownload::GetTarget(nsILocalFile **aTarget)
{
	NS_ENSURE_ARG_POINTER(aTarget);
	NS_IF_ADDREF(*aTarget = mDestination);

	return NS_OK;
}

NS_IMETHODIMP
MozDownload::GetPersist(nsIWebBrowserPersist **aPersist)
{
	NS_ENSURE_ARG_POINTER(aPersist);
	NS_IF_ADDREF(*aPersist = mWebPersist);

	return NS_OK;
}

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

NS_IMETHODIMP
MozDownload::GetMIMEInfo(nsIMIMEInfo **aMIMEInfo)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MozDownload::GetListener(nsIWebProgressListener **aListener)
{
	NS_ENSURE_ARG_POINTER(aListener);
	NS_IF_ADDREF(*aListener = (nsIWebProgressListener *)this);

	return NS_OK;
}

NS_IMETHODIMP
MozDownload::SetListener(nsIWebProgressListener *aListener)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MozDownload::GetObserver(nsIObserver **aObserver)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MozDownload::SetObserver(nsIObserver *aObserver)
{
	mObserver = aObserver;

	return NS_OK;
}

NS_IMETHODIMP 
MozDownload::OnStateChange (nsIWebProgress *aWebProgress, nsIRequest *aRequest,
			    PRUint32 aStateFlags, nsresult aStatus)
{
	/* For a file download via the external helper app service, we will never get a start
           notification. The helper app service has gotten that notification before it created us. */
	if (!mGotFirstStateChange)
	{
        	mIsNetworkTransfer = ((aStateFlags & STATE_IS_NETWORK) != 0);
	        mGotFirstStateChange = PR_TRUE;
	}

	if (NS_FAILED(aStatus) && NS_SUCCEEDED(mStatus))
        	mStatus = aStatus;
  
	/* We will get this even in the event of a cancel */
	if ((aStateFlags & STATE_STOP) && (!mIsNetworkTransfer || (aStateFlags & STATE_IS_NETWORK)))
	{
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

NS_IMETHODIMP 
MozDownload::OnProgressChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest,
			      PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress,
			      PRInt32 aCurTotalProgress, PRInt32 aMaxTotalProgress)
{
	if (!mRequest)
		mRequest = aRequest;

	PRInt64 now = PR_Now ();

	if ((now - mLastUpdate < mInterval) &&
	    (aMaxTotalProgress != -1) &&
	    (aCurTotalProgress < aMaxTotalProgress))
		return NS_OK;

	mLastUpdate = now;

	if (aMaxTotalProgress == -1)
	{
            mPercentComplete = -1;
	}
	else
	{
	        mPercentComplete = (PRInt32)(((float)aCurTotalProgress / (float)aMaxTotalProgress) * 100.0 + 0.5);
	}

	mTotalProgress = aMaxTotalProgress;
	mCurrentProgress = aCurTotalProgress;

	if (mEphyDownload)
	{
		g_signal_emit_by_name (mEphyDownload, "changed");
	}

	return NS_OK;
}

NS_IMETHODIMP
MozDownload::OnLocationChange (nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsIURI *location)
{
	return NS_OK;
}

NS_IMETHODIMP 
MozDownload::OnStatusChange (nsIWebProgress *aWebProgress, nsIRequest *aRequest,
			     nsresult aStatus, const PRUnichar *aMessage)
{
	return NS_OK;
}

NS_IMETHODIMP 
MozDownload::OnSecurityChange (nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 state)
{
	return NS_OK;
}

void
MozDownload::Cancel()
{
	if (mWebPersist)
	{
		mWebPersist->CancelSave ();
	}

	if (mObserver)
	{
		mObserver->Observe (nsnull, "oncancel", nsnull);
	}	
}

void
MozDownload::Pause()
{
	if (mRequest)
	{
		mRequest->Suspend ();
		mDownloadState = EPHY_DOWNLOAD_PAUSED;
	}
}

void
MozDownload::Resume()
{
	if (mRequest)
	{
		mRequest->Resume ();
		mDownloadState = EPHY_DOWNLOAD_DOWNLOADING;
	}
}

nsresult InitiateMozillaDownload (nsIDOMDocument *domDocument, nsIURI *sourceURI,
				  nsILocalFile* inDestFile, const char *contentType,
				  nsIURI* inOriginalURI, MozillaEmbedPersist *embedPersist,
				  nsIInputStream *postData, nsISupports *aCacheKey)
{
	nsresult rv = NS_OK;

	EmbedPersistFlags ephy_flags;
	ephy_flags = ephy_embed_persist_get_flags (EPHY_EMBED_PERSIST (embedPersist));

	PRBool isHTML = (domDocument && contentType &&
			(strcmp (contentType, "text/html") == 0 ||
			 strcmp (contentType, "text/xml") == 0 ||
			 strcmp (contentType, "application/xhtml+xml") == 0));

	nsCOMPtr<nsIWebBrowserPersist> webPersist = do_CreateInstance(persistContractID, &rv);
	if (NS_FAILED(rv)) return rv;
  
	PRInt64 timeNow = PR_Now();
  
	nsAutoString fileDisplayName;
	inDestFile->GetLeafName(fileDisplayName);

	MozDownload *downloader = new MozDownload ();
	/* dlListener attaches to its progress dialog here, which gains ownership */
	rv = downloader->InitForEmbed (inOriginalURI, inDestFile, fileDisplayName.get(),
				       nsnull, timeNow, webPersist, embedPersist);
	if (NS_FAILED(rv)) return rv;

	PRInt32 flags = nsIWebBrowserPersist::PERSIST_FLAGS_REPLACE_EXISTING_FILES;
	if (ephy_flags & EMBED_PERSIST_COPY_PAGE)
	{
		flags |= nsIWebBrowserPersist::PERSIST_FLAGS_FROM_CACHE;
	}
	webPersist->SetPersistFlags(flags);

	if (!isHTML || ephy_flags & EMBED_PERSIST_COPY_PAGE)
	{
		rv = webPersist->SaveURI (sourceURI, aCacheKey, nsnull,
					  postData, nsnull, inDestFile);
	}
	else
	{
		if (!domDocument) return rv;  /* should never happen */
    
		PRInt32 encodingFlags = 0;
		nsCOMPtr<nsILocalFile> filesFolder;

    		if (contentType && strcmp (contentType, "text/plain") == 0)
		{
			/* Create a local directory in the same dir as our file.  It
			   will hold our associated files. */

			filesFolder = do_CreateInstance("@mozilla.org/file/local;1");
			nsAutoString unicodePath;
			inDestFile->GetPath(unicodePath);
			filesFolder->InitWithPath(unicodePath);
      
			nsAutoString leafName;
			filesFolder->GetLeafName(leafName);
			nsAutoString nameMinusExt(leafName);
			PRInt32 index = nameMinusExt.RFind(".");
			if (index >= 0)
			{
				nameMinusExt.Left(nameMinusExt, index);
			}

			nameMinusExt += NS_LITERAL_STRING(" Files");
			filesFolder->SetLeafName(nameMinusExt);
			PRBool exists = PR_FALSE;
			filesFolder->Exists(&exists);
			if (!exists)
			{
				rv = filesFolder->Create(nsILocalFile::DIRECTORY_TYPE, 0755);
				if (NS_FAILED(rv)) return rv;
			}
		}
		else
		{
			encodingFlags |= nsIWebBrowserPersist::ENCODE_FLAGS_FORMATTED |
					 nsIWebBrowserPersist::ENCODE_FLAGS_ABSOLUTE_LINKS |
					 nsIWebBrowserPersist::ENCODE_FLAGS_NOFRAMES_CONTENT;
		}

		rv = webPersist->SaveDocument (domDocument, inDestFile, filesFolder,
					       contentType, encodingFlags, 80);
	}
  
	return rv;
}

static char*
GetFilePath (const char *filename)
{
	char *path, *download_dir;

	download_dir = eel_gconf_get_string (CONF_STATE_DOWNLOAD_DIR);
	if (!download_dir)
	{
		/* Emergency download destination */
		download_dir = g_strdup (g_get_home_dir ());
	}

	if (!strcmp (download_dir, "Desktop"))
	{
		if (eel_gconf_get_boolean (CONF_DESKTOP_IS_HOME_DIR))
		{
			path = g_build_filename 
				(g_get_home_dir (),
				 filename,
				 NULL);
		}
		else
		{
			path = g_build_filename 
				(g_get_home_dir (), "Desktop",
				 filename,
				 NULL);
		}
	}
	else
	{
		char *expanded;

		expanded = gnome_vfs_expand_initial_tilde (download_dir);
		path = g_build_filename (expanded, filename, NULL);
		g_free (expanded);
	}
	g_free (download_dir);

	return path;
}

nsresult BuildDownloadPath (const char *defaultFileName, nsILocalFile **_retval)
{
	char *path;
	int i = 0;

	path = GetFilePath (defaultFileName);
	
	while (g_file_test (path, G_FILE_TEST_EXISTS))
	{
		g_free (path);

		char *tmp_path;
		tmp_path = g_strdup_printf ("%s.%d", defaultFileName,  ++i);
		path = GetFilePath (tmp_path);
		g_free (tmp_path);
	}

	nsCOMPtr <nsILocalFile> destFile (do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));
	destFile->InitWithNativePath (nsDependentCString (path));
	g_free (path);

	NS_IF_ADDREF (*_retval = destFile);
	return NS_OK;
}

