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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "mozilla-version.h"

#include "mozilla-download.h"
#include "eel-gconf-extensions.h"
#include "ephy-prefs.h"
#include "ephy-file-helpers.h"
#include "ephy-debug.h"
#include "MozDownload.h"
#include "EphyUtils.h"

#include <libgnomevfs/gnome-vfs-utils.h>
#include <glib/gi18n.h>

#include <nsIFileURL.h>
#include <nsEmbedString.h>

const char* const persistContractID = "@mozilla.org/embedding/browser/nsWebBrowserPersist;1";

MozDownload::MozDownload() :
	mMaxSize(-1),
	mStatus(NS_OK),
	mEmbedPersist(nsnull),
	mDownloadState(EPHY_DOWNLOAD_DOWNLOADING)
{
	LOG ("MozDownload ctor (%p)", (void *) this)
}

MozDownload::~MozDownload()
{
	LOG ("MozDownload dtor (%p)", (void *) this)

	NS_ASSERTION (!mEphyDownload, "MozillaDownload still alive!");
}

NS_IMPL_ISUPPORTS3(MozDownload, nsIDownload, nsITransfer, nsIWebProgressListener)

NS_IMETHODIMP
MozDownload::InitForEmbed (nsIURI *aSource, nsIURI *aTarget, const PRUnichar *aDisplayName,
		           nsIMIMEInfo *aMIMEInfo, PRInt64 startTime, nsIWebBrowserPersist *aPersist,
		           MozillaEmbedPersist *aEmbedPersist, PRInt32 aMaxSize)
{
	mEmbedPersist = aEmbedPersist;
	mMaxSize = aMaxSize;
	return Init (aSource, aTarget, aDisplayName, aMIMEInfo, startTime, aPersist);
}

/* void init (in nsIURI aSource, in nsIURI aTarget, in wstring aDisplayName, in nsIMIMEInfo aMIMEInfo, in long long startTime, in nsIWebBrowserPersist aPersist); */
NS_IMETHODIMP
MozDownload::Init(nsIURI *aSource, nsIURI *aTarget, const PRUnichar *aDisplayName,
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
	mInterval = 200000; /* microsec */
	mLastUpdate = mStartTime;
	mMIMEInfo = aMIMEInfo;

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
		mEphyDownload = mozilla_download_new (this);
		g_object_add_weak_pointer (G_OBJECT (mEphyDownload),
					   (gpointer *) &mEphyDownload);
		downloader_view_add_download (dview, mEphyDownload);
		g_object_unref (mEphyDownload);
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
MozDownload::GetTarget(nsIURI **aTarget)
{
	NS_ENSURE_ARG_POINTER(aTarget);
	NS_IF_ADDREF(*aTarget = mDestination);

	return NS_OK;
}

NS_IMETHODIMP
MozDownload::GetTargetFile (nsILocalFile** aTargetFile)
{
	nsresult rv;
                                                                                                                              
	nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(mDestination, &rv);
	if (NS_FAILED(rv)) return rv;
                                                                                                                              
	nsCOMPtr<nsIFile> file;
	rv = fileURL->GetFile(getter_AddRefs(file));
	if (NS_SUCCEEDED(rv))
		rv = CallQueryInterface(file, aTargetFile);
	return rv;
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
	nsresult rv;

	if (NS_FAILED(aStatus) && NS_SUCCEEDED(mStatus))
        	mStatus = aStatus;

	/* We will get this even in the event of a cancel */
	if (aStateFlags & STATE_STOP)
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
		else if (NS_SUCCEEDED (aStatus))
		{
			GnomeVFSMimeApplication *helperApp;
#if MOZILLA_CHECK_VERSION4 (1, 8, MOZILLA_ALPHA, 1)
			nsEmbedCString mimeType;
			rv = mMIMEInfo->GetMIMEType (mimeType);
			NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

			helperApp = gnome_vfs_mime_get_default_application (mimeType.get()); 

			nsEmbedString description;
			mMIMEInfo->GetApplicationDescription (description);

			nsEmbedCString cDesc;
			NS_UTF16ToCString (description, NS_CSTRING_ENCODING_UTF8, cDesc);

			/* HACK we use the application description to decide
			   if we have to open the saved file */
			if ((strcmp (cDesc.get(), "gnome-default") == 0) &&
			    helperApp)
#else
			char *mimeType;
			rv = mMIMEInfo->GetMIMEType (&mimeType);
			NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

			helperApp = gnome_vfs_mime_get_default_application (mimeType); 

			PRUnichar *description;
			mMIMEInfo->GetApplicationDescription (&description);

			nsEmbedCString cDesc;
			NS_UTF16ToCString (nsEmbedString(description),
					   NS_CSTRING_ENCODING_UTF8, cDesc);

			/* HACK we use the application description to decide
			   if we have to open the saved file */
			if (strcmp (cDesc.get(), "gnome-default") == 0 &&
			    helperApp)
#endif
			{
				GList *params = NULL;
				char *param;
				nsEmbedCString aDest;

				mDestination->GetSpec (aDest);

				param = gnome_vfs_make_uri_canonical (aDest.get ());
				params = g_list_append (params, param);
				gnome_vfs_mime_application_launch (helperApp, params);
				g_free (param);

				g_list_free (params);
			}

			gnome_vfs_mime_application_free (helperApp);
		}
	}
        
	return NS_OK; 
}

NS_IMETHODIMP 
MozDownload::OnProgressChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest,
			      PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress,
			      PRInt32 aCurTotalProgress, PRInt32 aMaxTotalProgress)
{
	if (mMaxSize >= 0 &&
	    ((aMaxTotalProgress > 0 && mMaxSize < aMaxTotalProgress) ||
	     mMaxSize < aCurTotalProgress))
	{
		Cancel ();
	}

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
	if (mDownloadState != EPHY_DOWNLOAD_DOWNLOADING &&
	    mDownloadState != EPHY_DOWNLOAD_PAUSED)
	{
		return;
	}
	
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
				  nsIInputStream *postData, nsISupports *aCacheKey,
				  PRInt32 aMaxSize)
{
	nsresult rv = NS_OK;

	EmbedPersistFlags ephy_flags;
	ephy_flags = ephy_embed_persist_get_flags (EPHY_EMBED_PERSIST (embedPersist));

	PRBool isHTML = (contentType &&
			(strcmp (contentType, "text/html") == 0 ||
			 strcmp (contentType, "text/xml") == 0 ||
			 strcmp (contentType, "application/xhtml+xml") == 0));

	nsCOMPtr<nsIWebBrowserPersist> webPersist = do_CreateInstance(persistContractID, &rv);
	NS_ENSURE_SUCCESS (rv, rv);
  
	PRInt64 timeNow = PR_Now();
  
	nsEmbedString fileDisplayName;
	inDestFile->GetLeafName(fileDisplayName);

	nsCOMPtr<nsIIOService> ioService;
	rv = EphyUtils::GetIOService (getter_AddRefs (ioService));
	NS_ENSURE_SUCCESS (rv, rv);

	nsCOMPtr<nsIURI> destURI;
	ioService->NewFileURI (inDestFile, getter_AddRefs(destURI));

	MozDownload *downloader = new MozDownload ();
	/* dlListener attaches to its progress dialog here, which gains ownership */
	rv = downloader->InitForEmbed (inOriginalURI, destURI, fileDisplayName.get(),
				       nsnull, timeNow, webPersist, embedPersist, aMaxSize);
	NS_ENSURE_SUCCESS (rv, rv);

	PRInt32 flags = nsIWebBrowserPersist::PERSIST_FLAGS_REPLACE_EXISTING_FILES;

	if (!domDocument && !isHTML && !(ephy_flags & EMBED_PERSIST_COPY_PAGE))
	{
		flags |= nsIWebBrowserPersist::PERSIST_FLAGS_NO_CONVERSION;
	}
	if (ephy_flags & EMBED_PERSIST_COPY_PAGE)
	{
		flags |= nsIWebBrowserPersist::PERSIST_FLAGS_FROM_CACHE;
	}
	webPersist->SetPersistFlags(flags);

	if (!domDocument || !isHTML || ephy_flags & EMBED_PERSIST_COPY_PAGE)
	{
		rv = webPersist->SaveURI (sourceURI, aCacheKey, nsnull,
					  postData, nsnull, inDestFile);
	}
	else
	{
		PRInt32 encodingFlags = 0;
		nsCOMPtr<nsILocalFile> filesFolder;

		/**
		 * Construct a directory path to hold the associated files; mozilla
		 * will create the directory as needed.
		 */

		nsEmbedCString cPath;
		inDestFile->GetNativePath (cPath);

		GString *path = g_string_new (cPath.get());
		char *dot_pos = strchr (path->str, '.');
		if (dot_pos)
		{
			g_string_truncate (path, dot_pos - path->str);
		}
		g_string_append (path, " ");
		g_string_append (path, _("Files"));
      
		filesFolder = do_CreateInstance ("@mozilla.org/file/local;1");
		filesFolder->InitWithNativePath (nsEmbedCString(path->str));

		g_string_free (path, TRUE);

		rv = webPersist->SaveDocument (domDocument, inDestFile, filesFolder,
					       contentType, encodingFlags, 80);
	}
  
	return rv;
}

static char*
GetFilePath (const char *filename)
{
	char *path = NULL;
	char *download_dir, *converted_dp, *expanded;

	download_dir = eel_gconf_get_string (CONF_STATE_DOWNLOAD_DIR);

	if (!download_dir)
	{
		/* Emergency download destination */
		return g_build_filename (g_get_home_dir (), filename, NULL);
	}

	if (g_utf8_collate (download_dir, "Downloads") == 0)
	{
		const char *default_dir;

		g_free (download_dir);

		default_dir = ephy_file_downloads_dir ();
		ephy_ensure_dir_exists (default_dir);

		return g_build_filename (default_dir, filename, NULL);
	}

	converted_dp = g_filename_from_utf8 (download_dir, -1, NULL, NULL, NULL);
	g_free (download_dir);

	if (converted_dp)
	{
		expanded = gnome_vfs_expand_initial_tilde (converted_dp);
		ephy_ensure_dir_exists (expanded);
		path = g_build_filename (expanded, filename, NULL);

		g_free (expanded);
		g_free (converted_dp);
	}
	else
	{
		/* Fallback, see FIXME above too */
		path = g_build_filename (g_get_home_dir (), filename, NULL);
	}

	return path;
}

nsresult BuildDownloadPath (const char *defaultFileName, nsILocalFile **_retval)
{
	char *path;

	path = GetFilePath (defaultFileName);
	
	if (g_file_test (path, G_FILE_TEST_EXISTS))
	{
		int i = 1;
		char *dot_pos, *serial = NULL;
		GString *tmp_path;
		gssize position;

		dot_pos = g_strrstr (defaultFileName, ".");
		if (dot_pos)
		{
			position = dot_pos - defaultFileName;
		}
		else
		{
			position = strlen (defaultFileName);
		}
		tmp_path = g_string_new (NULL);

		do {
			g_free (path);
			g_string_assign (tmp_path, defaultFileName);
			serial = g_strdup_printf ("(%d)", i++);
			g_string_insert (tmp_path, position, serial);
			g_free (serial);
			path = GetFilePath (tmp_path->str);
			
		} while (g_file_test (path, G_FILE_TEST_EXISTS));
		
		g_string_free (tmp_path, TRUE);
	}

	nsCOMPtr <nsILocalFile> destFile (do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));
	destFile->InitWithNativePath (nsEmbedCString (path));
	g_free (path);

	NS_IF_ADDREF (*_retval = destFile);
	return NS_OK;
}

