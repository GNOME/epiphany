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

#include "mozilla-config.h"

#include "config.h"

#include "mozilla-download.h"
#include "eel-gconf-extensions.h"
#include "ephy-prefs.h"
#include "ephy-file-helpers.h"
#include "ephy-debug.h"
#include "MozDownload.h"
#include "EphyUtils.h"

#include <libgnomevfs/gnome-vfs-utils.h>
#include <glib/gi18n.h>

#include <nsIIOService.h>
#include <nsIURI.h>
#include <nsIDOMDocument.h>
#include <nsILocalFile.h>
#include <nsIWebBrowserPersist.h>
#include <nsIObserver.h>
#include <nsIRequest.h>
#include <nsIFileURL.h>
#undef MOZILLA_INTERNAL_API
#include <nsEmbedString.h>
#define MOZILLA_INTERNAL_API 1
#include <nsMemory.h>
#include <nsNetError.h>

#include <stdlib.h>

const char* const persistContractID = "@mozilla.org/embedding/browser/nsWebBrowserPersist;1";

MozDownload::MozDownload() :
	mTotalProgress(-1),
	mCurrentProgress(0),
	mMaxSize(-1),
	mStatus(NS_OK),
	mEmbedPersist(nsnull),
	mDownloadState(EPHY_DOWNLOAD_INITIALISING)
{
	LOG ("MozDownload ctor (%p)", (void *) this);
}

MozDownload::~MozDownload()
{
	LOG ("MozDownload dtor (%p)", (void *) this);

	NS_ASSERTION (!mEphyDownload, "MozillaDownload still alive!");
}

#ifdef HAVE_GECKO_1_8
NS_IMPL_ISUPPORTS3(MozDownload, nsIWebProgressListener, nsIWebProgressListener2, nsITransfer)
#else
NS_IMPL_ISUPPORTS3(MozDownload, nsIWebProgressListener, nsIDownload, nsITransfer)
#endif

#ifdef HAVE_GECKO_1_8
nsresult
MozDownload::InitForEmbed (nsIURI *aSource, nsIURI *aTarget, const nsAString &aDisplayName,
		           nsIMIMEInfo *aMIMEInfo, PRTime aStartTime, nsILocalFile *aTempFile,
			   nsICancelable *aCancelable, MozillaEmbedPersist *aEmbedPersist,
			   PRInt64 aMaxSize)
{
	mEmbedPersist = aEmbedPersist;
	mMaxSize = aMaxSize;
	return Init (aSource, aTarget, aDisplayName, aMIMEInfo, aStartTime, aTempFile, aCancelable);
}
#else
nsresult
MozDownload::InitForEmbed (nsIURI *aSource, nsIURI *aTarget, const PRUnichar *aDisplayName,
		           nsIMIMEInfo *aMIMEInfo, PRInt64 startTime, nsIWebBrowserPersist *aPersist,
		           MozillaEmbedPersist *aEmbedPersist, PRInt64 aMaxSize)
{
	mEmbedPersist = aEmbedPersist;
	mMaxSize = aMaxSize;
	return Init (aSource, aTarget, aDisplayName, aMIMEInfo, startTime, aPersist);
}
#endif

#ifdef HAVE_GECKO_1_8
/* void init (in nsIURI aSource, in nsIURI aTarget, in AString aDisplayName, in nsIMIMEInfo aMIMEInfo, in PRTime startTime, in nsILocalFile aTempFile, in nsICancelable aCancelable); */
NS_IMETHODIMP
MozDownload::Init (nsIURI *aSource,
		   nsIURI *aTarget,
		   const nsAString &aDisplayName,
		   nsIMIMEInfo *aMIMEInfo,
		   PRTime aStartTime,
		   nsILocalFile *aTempFile,
		   nsICancelable *aCancelable)
#else
/* void init (in nsIURI aSource, in nsIURI aTarget, in wstring aDisplayName, in nsIMIMEInfo aMIMEInfo, in long long startTime, in nsIWebBrowserPersist aPersist); */
NS_IMETHODIMP
MozDownload::Init (nsIURI *aSource,
		   nsIURI *aTarget,
		   const PRUnichar *aDisplayName,
		   nsIMIMEInfo *aMIMEInfo,
		   PRInt64 aStartTime,
		   nsIWebBrowserPersist *aPersist)
#endif
{
	PRBool addToView = PR_TRUE;

	if (mEmbedPersist)
	{
		EphyEmbedPersistFlags flags;

		flags = ephy_embed_persist_get_flags (EPHY_EMBED_PERSIST (mEmbedPersist));

		addToView = !(flags & EPHY_EMBED_PERSIST_NO_VIEW);
	}

	mSource = aSource;
	mDestination = aTarget;
	mStartTime = aStartTime;
	mTotalProgress = 0;
	mCurrentProgress = 0;
	mPercentComplete = 0;
	mInterval = 200000; /* microsec */
	mLastUpdate = mStartTime;
	mMIMEInfo = aMIMEInfo;

#ifdef HAVE_GECKO_1_8
	/* This will create a refcount cycle, which needs to be broken in ::OnStateChange */
	mCancelable = aCancelable;
#else
	if (aPersist)
	{
		mWebPersist = aPersist;
		aPersist->SetProgressListener(this);
        }
#endif

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

#ifndef HAVE_GECKO_1_8
NS_IMETHODIMP
MozDownload::GetTarget(nsIURI **aTarget)
{
	NS_ENSURE_ARG_POINTER(aTarget);
	NS_IF_ADDREF(*aTarget = mDestination);

	return NS_OK;
}
#endif

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

#ifndef HAVE_GECKO_1_8
NS_IMETHODIMP
MozDownload::GetPersist(nsIWebBrowserPersist **aPersist)
{
	NS_ENSURE_ARG_POINTER(aPersist);
	NS_IF_ADDREF(*aPersist = mWebPersist);

	return NS_OK;
}
#endif

NS_IMETHODIMP
MozDownload::GetPercentComplete(PRInt32 *aPercentComplete)
{
	NS_ENSURE_ARG_POINTER(aPercentComplete);
	*aPercentComplete = mPercentComplete;

 	return NS_OK;
}

#ifndef HAVE_GECKO_1_8
NS_IMETHODIMP
MozDownload::GetStartTime(PRInt64 *aStartTime)
{
	NS_ENSURE_ARG_POINTER(aStartTime);
	*aStartTime = mStartTime;

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
#endif /* !HAVE_GECKO_1_8 */

NS_IMETHODIMP
MozDownload::GetTotalProgress(PRInt64 *aTotalProgress)
{
	NS_ENSURE_ARG_POINTER(aTotalProgress);
	*aTotalProgress = mTotalProgress;

	return NS_OK;
}

NS_IMETHODIMP
MozDownload::GetCurrentProgress(PRInt64 *aCurrentProgress)
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
MozDownload::GetElapsedTime(PRInt64 *aElapsedTime)
{
	NS_ENSURE_ARG_POINTER(aElapsedTime);
	*aElapsedTime = PR_Now() - mStartTime;

	return NS_OK;
}

NS_IMETHODIMP
MozDownload::GetMIMEInfo(nsIMIMEInfo **aMIMEInfo)
{
        NS_ENSURE_ARG_POINTER(aMIMEInfo);
	NS_IF_ADDREF(*aMIMEInfo = mMIMEInfo);

	return NS_OK;
}

#ifndef HAVE_GECKO_1_8
NS_IMETHODIMP
MozDownload::GetListener(nsIWebProgressListener **aListener)
{
	NS_ENSURE_ARG_POINTER(aListener);

	return CallQueryInterface (this, aListener);
}

NS_IMETHODIMP
MozDownload::SetListener(nsIWebProgressListener *aListener)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MozDownload::GetObserver(nsIObserver **aObserver)
{
	*aObserver = mObserver;
	NS_IF_ADDREF (*aObserver);

	return NS_OK;
}

NS_IMETHODIMP
MozDownload::SetObserver(nsIObserver *aObserver)
{
	mObserver = aObserver;

	return NS_OK;
}
#endif

NS_IMETHODIMP 
MozDownload::OnStateChange (nsIWebProgress *aWebProgress, nsIRequest *aRequest,
			    PRUint32 aStateFlags, nsresult aStatus)
{
	nsresult rv;

	if (NS_FAILED(aStatus) && NS_SUCCEEDED(mStatus))
        	mStatus = aStatus;

	if (aStateFlags & STATE_START)
	{
		mDownloadState = EPHY_DOWNLOAD_DOWNLOADING;

		if (mEphyDownload)
		{
			g_signal_emit_by_name (mEphyDownload, "changed");
		}
	}

	/* We will get this even in the event of a cancel */
	/* Due to a mozilla bug [https://bugzilla.mozilla.org/show_bug.cgi?id=304353],
	 * we'll only get STATE_STOP if we're driven from external app handler; elsewhere
	 * we get STATE_STOP | STATE_IS_NETWORK | STATE_IS_REQUEST. So check first if 
	 * STATE_IS_REQUEST is set.
	 */
	/* Be careful that download is only completed when STATE_IS_NETWORK is set
 	 * and many lonely STOP events may be triggered before.
	 */
#ifdef GNOME_ENABLE_DEBUG
{
	nsEmbedCString spec;
	if (mSource) mSource->GetSpec(spec);

	LOG ("url %s, status %x, state %x (is-stop:%s, is-network:%s, is-request:%s)",
	     spec.get(), aStatus, aStateFlags,
	     aStateFlags & STATE_STOP ? "t" : "f",
	     aStateFlags & STATE_IS_NETWORK ? "t" : "f",
	     aStateFlags & STATE_IS_REQUEST ? "t" : "f");
}
#endif

	if (((aStateFlags & STATE_IS_REQUEST) &&
	     (aStateFlags & STATE_IS_NETWORK) &&
	     (aStateFlags & STATE_STOP)) ||
	    aStateFlags == STATE_STOP)
	{
		LOG ("STATE_STOP");

		/* Keep us alive */
		nsCOMPtr<nsITransfer> kungFuDeathGrip(this);

		mDownloadState = NS_SUCCEEDED (aStatus) ? EPHY_DOWNLOAD_COMPLETED : EPHY_DOWNLOAD_FAILED;
		if (mEphyDownload)
		{
			g_signal_emit_by_name (mEphyDownload, "changed");
		}

#ifdef HAVE_GECKO_1_8
		/* break refcount cycle */
		mCancelable = nsnull;
#else
	        if (mWebPersist)
		{
	            mWebPersist->SetProgressListener(nsnull);
	            mWebPersist = nsnull;
	        }
#endif

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
			nsEmbedCString mimeType;
#ifdef HAVE_GECKO_1_8
			rv = mMIMEInfo->GetMIMEType (mimeType);
			NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

			nsEmbedString description;
			mMIMEInfo->GetApplicationDescription (description);

			nsEmbedCString cDesc;
			NS_UTF16ToCString (description, NS_CSTRING_ENCODING_UTF8, cDesc);
#else
			char *mime = nsnull;
			rv = mMIMEInfo->GetMIMEType (&mime);
			NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

			if (mime)
			{
				mimeType.Assign (mime);
				nsMemory::Free (mime);
			}

			PRUnichar *description;
			mMIMEInfo->GetApplicationDescription (&description);
			NS_ENSURE_TRUE (description, NS_ERROR_FAILURE);

			nsEmbedCString cDesc;
			NS_UTF16ToCString (nsEmbedString(description),
					   NS_CSTRING_ENCODING_UTF8, cDesc);

			nsMemory::Free (description);
#endif

			/* HACK we use the application description to decide
			   if we have to open the saved file */
			if (g_str_has_prefix (cDesc.get(), "gnome-default:"))
			{
				/* Format gnome-default:<usertime>:<helperapp id> */
				char **str = g_strsplit (cDesc.get(), ":", -1);
				g_return_val_if_fail (g_strv_length (str) == 3, NS_ERROR_FAILURE);

				char *end;
				guint32 user_time = strtoul (str[1], &end, 0);

				helperApp = gnome_vfs_mime_application_new_from_desktop_id (str[2]);
				if (!helperApp) return NS_ERROR_FAILURE;

				nsEmbedCString aDest;
				rv = mDestination->GetSpec (aDest);
				NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

				ephy_file_launch_application (helperApp, aDest.get (), user_time);

				gnome_vfs_mime_application_free (helperApp);
				g_strfreev (str);
			}
		}
	}
        
	return NS_OK; 
}

#ifdef HAVE_GECKO_1_8
NS_IMETHODIMP
MozDownload::OnProgressChange (nsIWebProgress *aWebProgress,
			       nsIRequest *aRequest,
			       PRInt32 aCurSelfProgress,
			       PRInt32 aMaxSelfProgress,
			       PRInt32 aCurTotalProgress,
			       PRInt32 aMaxTotalProgress)
{
	return OnProgressChange64 (aWebProgress, aRequest,
				   aCurSelfProgress, aMaxSelfProgress,
				   aCurTotalProgress, aMaxTotalProgress);
}

/* void onProgressChange64 (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long long aCurSelfProgress, in long long aMaxSelfProgress, in long long aCurTotalProgress,
 in long long aMaxTotalProgress); */
NS_IMETHODIMP
MozDownload::OnProgressChange64 (nsIWebProgress *aWebProgress,
				 nsIRequest *aRequest,
				 PRInt64 aCurSelfProgress,
				 PRInt64 aMaxSelfProgress,
				 PRInt64 aCurTotalProgress,
				 PRInt64 aMaxTotalProgress)
#else /* !HAVE_GECKO_1_8 */
NS_IMETHODIMP
MozDownload::OnProgressChange (nsIWebProgress *aWebProgress,
			       nsIRequest *aRequest,
			       PRInt32 aCurSelfProgress,
			       PRInt32 aMaxSelfProgress,
			       PRInt32 aCurTotalProgress,
			       PRInt32 aMaxTotalProgress)
#endif /* HAVE_GECKO_1_8 */
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

#ifdef HAVE_GECKO_1_8
	if (mCancelable)
	{
		/* FIXME: error code? */
		mCancelable->Cancel (NS_BINDING_ABORTED);
	}
#else
	if (mWebPersist)
	{
		mWebPersist->CancelSave ();
	}

	if (mObserver)
	{
		mObserver->Observe (nsnull, "oncancel", nsnull);
	}
#endif
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
				  PRInt64 aMaxSize)
{
	nsresult rv = NS_OK;

	EphyEmbedPersistFlags ephy_flags;
	ephy_flags = ephy_embed_persist_get_flags (EPHY_EMBED_PERSIST (embedPersist));

	PRBool isHTML = (contentType &&
			(strcmp (contentType, "text/html") == 0 ||
			 strcmp (contentType, "text/xml") == 0 ||
			 strcmp (contentType, "application/xhtml+xml") == 0));

	nsCOMPtr<nsIWebBrowserPersist> webPersist (do_CreateInstance(persistContractID, &rv));
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
	/* FIXME is that still true? */
#ifdef HAVE_GECKO_1_8
	rv = downloader->InitForEmbed (inOriginalURI, destURI, fileDisplayName,
				       nsnull, timeNow, nsnull, webPersist, embedPersist, aMaxSize);
	NS_ENSURE_SUCCESS (rv, rv);

	rv = webPersist->SetProgressListener (downloader);
#else
	rv = downloader->InitForEmbed (inOriginalURI, destURI, fileDisplayName.get(),
				       nsnull, timeNow, webPersist, embedPersist, aMaxSize);
#endif
	NS_ENSURE_SUCCESS (rv, rv);

	PRInt32 flags = nsIWebBrowserPersist::PERSIST_FLAGS_REPLACE_EXISTING_FILES;

	if (!domDocument && !isHTML && !(ephy_flags & EPHY_EMBED_PERSIST_COPY_PAGE) &&
	    !(ephy_flags & EPHY_EMBED_PERSIST_DO_CONVERSION))
	{
		flags |= nsIWebBrowserPersist::PERSIST_FLAGS_NO_CONVERSION;
	}
	if (ephy_flags & EPHY_EMBED_PERSIST_COPY_PAGE)
	{
		flags |= nsIWebBrowserPersist::PERSIST_FLAGS_FROM_CACHE;
	}
	webPersist->SetPersistFlags(flags);

	if (!domDocument || !isHTML || ephy_flags & EPHY_EMBED_PERSIST_COPY_PAGE)
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
	char *path = NULL, *download_dir, *expanded;

	download_dir = eel_gconf_get_string (CONF_STATE_DOWNLOAD_DIR);

	if (download_dir && strcmp (download_dir, "Downloads") == 0)
	{
		g_free (download_dir);
		download_dir = ephy_file_downloads_dir ();
	}
  	else if (download_dir && strcmp (download_dir, "Desktop") == 0)
	{
		g_free (download_dir);
		download_dir = ephy_file_desktop_dir ();
	}  
	else if (download_dir)
	{
		char *converted_dp;

		converted_dp = g_filename_from_utf8 (download_dir, -1, NULL, NULL, NULL);
		g_free (download_dir);
		download_dir = converted_dp;
	}

	if (download_dir == NULL)
	{
		/* Emergency download destination */
		download_dir = g_strdup (g_get_home_dir ());
	}

	g_return_val_if_fail (download_dir != NULL, FALSE);

	expanded = gnome_vfs_expand_initial_tilde (download_dir);
	if (ephy_ensure_dir_exists (expanded))
	{
		path = g_build_filename (expanded, filename, NULL);
	}
	g_free (expanded);
	g_free (download_dir);
	
	if (path == NULL)
	{
		path = g_build_filename (g_get_home_dir (), filename, NULL);
	}

	return path;
}

static const char*
file_is_compressed (const char *filename)
{
        int i;
        static const char * const compression[] = {".gz", ".bz2", ".Z", ".lz", NULL};

        for (i = 0; compression[i] != NULL; i++)
        {
                if (g_str_has_suffix (filename, compression[i]))
                        return compression[i];
        }

        return NULL;
}

static const char*
parse_extension (const char *filename)
{
        const char *compression;

        compression = file_is_compressed (filename);

        /* If the file is compressed we might have a double extension */
        if (compression != NULL)
        {
                int i;
                static const char * const extensions[] = {"tar", "ps", "xcf", "dvi", "txt", "text", NULL};

                for (i = 0; extensions[i] != NULL; i++)
                {
                        char *suffix;
                        suffix = g_strdup_printf (".%s%s", extensions[i],
                                                  compression);

                        if (g_str_has_suffix (filename, suffix))
                        {
                                char *p;

                                p = g_strrstr (filename, suffix);
                                g_free (suffix);

                                return p;
                        }

                        g_free (suffix);
                }
        }
        
        /* default case */
        return g_strrstr (filename, ".");
}

nsresult BuildDownloadPath (const char *defaultFileName, nsILocalFile **_retval)
{
	char *path;

	path = GetFilePath (defaultFileName);
	
	if (g_file_test (path, G_FILE_TEST_EXISTS))
	{
		int i = 1;
		const char *dot_pos;
                char *serial = NULL;
		GString *tmp_path;
		gssize position;

                dot_pos = parse_extension (defaultFileName);
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
	NS_ENSURE_TRUE (destFile, NS_ERROR_FAILURE);

	destFile->InitWithNativePath (nsEmbedCString (path));
	g_free (path);

	NS_IF_ADDREF (*_retval = destFile);
	return NS_OK;
}
