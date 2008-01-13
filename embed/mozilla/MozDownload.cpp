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
 * Portions created by the Initial Developer are Copyright © 2002
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

#include <stdlib.h>

#include <glib/gi18n.h>
#include <gio/gdesktopappinfo.h>
#include <gio/gio.h>

#include <nsStringAPI.h>

#include <nsComponentManagerUtils.h>
#include <nsICancelable.h>
#include <nsIChannel.h>
#include <nsIDOMDocument.h>
#include <nsIFileURL.h>
#include <nsIIOService.h>
#include <nsILocalFile.h>
#include <nsIMIMEInfo.h>
#include <nsIMIMEService.h>
#include <nsIObserver.h>
#include <nsIRequest.h>
#include <nsIURI.h>
#include <nsIWritablePropertyBag2.h>
#include <nsIWebBrowserPersist.h>

#include <nsMemory.h>
#include <nsNetError.h>
#include <nsServiceManagerUtils.h>

#ifndef HAVE_GECKO_1_9
#include "EphyBadCertRejector.h"
#endif

#include "EphyUtils.h"

#include "eel-gconf-extensions.h"
#include "ephy-debug.h"
#include "ephy-file-helpers.h"
#include "ephy-prefs.h"
#include "mozilla-download.h"

#include "MozDownload.h"

/* Minimum time between progress updates */
#define PROGRESS_RATE 500000 /* microsec */

const char* const persistContractID = "@mozilla.org/embedding/browser/nsWebBrowserPersist;1";

MozDownload::MozDownload() :
	mTotalProgress(-1),
	mCurrentProgress(0),
	mMaxSize(-1),
	mAddToRecent(PR_TRUE),
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

NS_IMPL_ISUPPORTS4 (MozDownload,
		    nsIWebProgressListener,
		    nsIWebProgressListener2,
		    nsITransfer,
		    nsIInterfaceRequestor)

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

/* void init (in nsIURI aSource, in nsIURI aTarget, in AString aDisplayName, in nsIMIMEInfo aMIMEInfo, in PRTime startTime, in nsILocalFile aTempFile, in nsICancelable aCancelable); */
NS_IMETHODIMP
MozDownload::Init (nsIURI *aSource,
		   nsIURI *aTarget,
		   const nsAString &aDisplayName,
		   nsIMIMEInfo *aMIMEInfo,
		   PRTime aStartTime,
		   nsILocalFile *aTempFile,
		   nsICancelable *aCancelable)
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
	mInterval = PROGRESS_RATE;
	mLastUpdate = mStartTime;
	mMIMEInfo = aMIMEInfo;
	mAddToRecent = addToView;

	/* This will create a refcount cycle, which needs to be broken in ::OnStateChange */
	mCancelable = aCancelable;

	if (addToView)
	{ 
		DownloaderView *dview;
        EphyDownload **cache_ptr;
		dview = EPHY_DOWNLOADER_VIEW
			(ephy_embed_shell_get_downloader_view (embed_shell));
		mEphyDownload = mozilla_download_new (this);
        cache_ptr = &mEphyDownload;
		g_object_add_weak_pointer (G_OBJECT (mEphyDownload),
					   (gpointer *) cache_ptr);
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
MozDownload::GetPercentComplete(PRInt32 *aPercentComplete)
{
	NS_ENSURE_ARG_POINTER(aPercentComplete);
	*aPercentComplete = mPercentComplete;

 	return NS_OK;
}

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
	nsCString spec;
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

		/* break refcount cycle */
		mCancelable = nsnull;
        
		nsCString destSpec;
		nsCString mimeType;

		mDestination->GetSpec (destSpec);

		if (NS_SUCCEEDED (aStatus) && mMIMEInfo)
		{
			rv = mMIMEInfo->GetMIMEType (mimeType);
			NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);
		}

		if (mAddToRecent)
		{
			ephy_file_add_recent_item (destSpec.get(), mimeType.get());
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
			/* see http://bugzilla.gnome.org/show_bug.cgi?id=456945 */
#ifdef HAVE_GECKO_1_9
			return NS_OK;
#else
			GDesktopAppInfo *helperApp;
			NS_ENSURE_TRUE (mMIMEInfo, NS_ERROR_FAILURE);

			nsString description;
			mMIMEInfo->GetApplicationDescription (description);

			nsCString cDesc;
			NS_UTF16ToCString (description, NS_CSTRING_ENCODING_UTF8, cDesc);

			/* HACK we use the application description to decide
			   if we have to open the saved file */
			if (g_str_has_prefix (cDesc.get(), "gnome-default:"))
			{
				/* Format gnome-default:<usertime>:<helperapp id> */
				char **str = g_strsplit (cDesc.get(), ":", -1);
				g_return_val_if_fail (g_strv_length (str) == 3, NS_ERROR_FAILURE);

				char *end;
				guint32 user_time = strtoul (str[1], &end, 0);

				helperApp = g_desktop_app_info_new (str[2]);
				if (!helperApp) return NS_ERROR_FAILURE;

				nsCString aDest;
				rv = mDestination->GetSpec (aDest);
				NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);
                
                GFile* file;
                GList* list = NULL;

                file = g_file_new_for_uri (destSpec.get ());
                list = g_list_append (list, file);
				ephy_file_launch_application (G_APP_INFO (helperApp), list, user_time, NULL);

                g_list_free (list);
                g_object_unref (file);
				g_strfreev (str);
			}
			else if (g_str_has_prefix (cDesc.get(), "gnome-browse-to-file:"))
			{
				/* Format gnome-browse-to-file:<usertime> */
				char **str = g_strsplit (cDesc.get(), ":", -1);
				g_return_val_if_fail (g_strv_length (str) == 2, NS_ERROR_FAILURE);

				char *end;
				guint32 user_time = strtoul (str[1], &end, 0);

				nsCString aDest;
				rv = mDestination->GetSpec (aDest);
				NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

				ephy_file_browse_to (aDest.get (), user_time);

				g_strfreev (str);
			}
#endif /* HAVE_GECKO_1_9 */
		}
	}
        
	return NS_OK; 
}

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
	    (aMaxTotalProgress == -1 || aCurTotalProgress < aMaxTotalProgress))
		return NS_OK;

	mLastUpdate = now;

	if (aMaxTotalProgress <= 0)
	{
            mPercentComplete = -1;
	}
	else
	{
		/* Make sure not to round up, so we don't display 100% unless
		 * it's really finished! 
		 */
	        mPercentComplete = (PRInt32)(((float)aCurTotalProgress / (float)aMaxTotalProgress) * 100.0);
	}

	mTotalProgress = aMaxTotalProgress;
	mCurrentProgress = aCurTotalProgress;

	if (mEphyDownload)
	{
		g_signal_emit_by_name (mEphyDownload, "changed");
	}

	return NS_OK;
}

#ifdef HAVE_GECKO_1_9
/* boolean onRefreshAttempted (in nsIWebProgress aWebProgress, in nsIURI aRefreshURI, in long aDelay, in boolean aSameURI); */
NS_IMETHODIMP
MozDownload::OnRefreshAttempted(nsIWebProgress *aWebProgress,
                                nsIURI *aUri,
                                PRInt32 aDelay,
                                PRBool aSameUri,
                                PRBool *allowRefresh)
{
        *allowRefresh = PR_TRUE;
        return NS_OK;
}
#endif

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

/* void getInterface (in nsIIDRef uuid, [iid_is (uuid), retval] out nsQIResult result); */
NS_IMETHODIMP
MozDownload::GetInterface(const nsIID & uuid, void * *result)
{
#ifndef HAVE_GECKO_1_9
	if (uuid.Equals (NS_GET_IID (nsIBadCertListener)) &&
	    mEmbedPersist)
	{
		EphyEmbedPersistFlags flags;

		g_object_get (mEmbedPersist, "flags", &flags, (char *) NULL);

		if (flags & EPHY_EMBED_PERSIST_NO_CERTDIALOGS)
		{
			nsIBadCertListener *badCertRejector = new EphyBadCertRejector ();
			if (!badCertRejector) return NS_ERROR_OUT_OF_MEMORY;

			*result = badCertRejector;
			NS_ADDREF (badCertRejector);

			return NS_OK;
		}
	}
#endif
	return NS_ERROR_NO_INTERFACE;
}

void
MozDownload::Cancel()
{
	if (mDownloadState != EPHY_DOWNLOAD_DOWNLOADING &&
	    mDownloadState != EPHY_DOWNLOAD_PAUSED)
	{
		return;
	}

	if (mCancelable)
	{
		/* FIXME: error code? */
		mCancelable->Cancel (NS_BINDING_ABORTED);
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
				  PRInt64 aMaxSize)
{
	nsresult rv = NS_OK;

	EphyEmbedPersistFlags ephy_flags;
	ephy_flags = ephy_embed_persist_get_flags (EPHY_EMBED_PERSIST (embedPersist));

	if (!ephy_embed_persist_get_dest (EPHY_EMBED_PERSIST (embedPersist)))
	{
		nsCString cPath;
		inDestFile->GetNativePath (cPath);

		ephy_embed_persist_set_dest (EPHY_EMBED_PERSIST (embedPersist),
				cPath.get());
	}
    
	nsCOMPtr<nsIMIMEService> mimeService (do_GetService ("@mozilla.org/mime;1"));
	nsCOMPtr<nsIMIMEInfo> mimeInfo;
	if (mimeService)
	{
		mimeService->GetFromTypeAndExtension (nsCString(contentType),
						      nsCString(),
						      getter_AddRefs (mimeInfo));
	}

	PRBool isHTML = (contentType &&
			(strcmp (contentType, "text/html") == 0 ||
			 strcmp (contentType, "text/xml") == 0 ||
			 strcmp (contentType, "application/xhtml+xml") == 0));

	nsCOMPtr<nsIWebBrowserPersist> webPersist (do_CreateInstance(persistContractID, &rv));
	NS_ENSURE_SUCCESS (rv, rv);
  
	PRInt64 timeNow = PR_Now();
  
	nsString fileDisplayName;
	inDestFile->GetLeafName(fileDisplayName);

	nsCOMPtr<nsIIOService> ioService;
	rv = EphyUtils::GetIOService (getter_AddRefs (ioService));
	NS_ENSURE_SUCCESS (rv, rv);

	nsCOMPtr<nsIURI> destURI;
	ioService->NewFileURI (inDestFile, getter_AddRefs(destURI));

	MozDownload *downloader = new MozDownload ();
	/* dlListener attaches to its progress dialog here, which gains ownership */
	/* FIXME is that still true? */
	rv = downloader->InitForEmbed (inOriginalURI, destURI, fileDisplayName,
				       mimeInfo, timeNow, nsnull, webPersist, embedPersist, aMaxSize);
	NS_ENSURE_SUCCESS (rv, rv);

	rv = webPersist->SetProgressListener (downloader);
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

	/* Create a new tagged channel if we need to block cookies from server */
	if (ephy_flags & EPHY_EMBED_PERSIST_NO_COOKIES)
	{
		nsCOMPtr<nsIChannel> tmpChannel;
		rv = ioService->NewChannelFromURI (sourceURI, getter_AddRefs (tmpChannel));
		NS_ENSURE_SUCCESS (rv, rv);

		nsCOMPtr<nsIWritablePropertyBag2> props = do_QueryInterface(tmpChannel);
		rv = props->SetPropertyAsBool (NS_LITERAL_STRING("epiphany-blocking-cookies"), PR_TRUE);
		NS_ENSURE_SUCCESS (rv, rv);
	
		rv = webPersist->SaveChannel (tmpChannel, inDestFile);
	}
	else if (!domDocument || !isHTML || ephy_flags & EPHY_EMBED_PERSIST_COPY_PAGE)
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

		nsCString cPath;
		inDestFile->GetNativePath (cPath);

		char *basename = g_path_get_basename (cPath.get());
		char *dirname = g_path_get_dirname (cPath.get());
		char *dot_pos = strchr (basename, '.');
		if (dot_pos)
		{
			*dot_pos = 0;
		}
		/* translators: this is the directory name to store auxilary files when saving html files */
		char *new_basename = g_strdup_printf (_("%s Files"), basename);
		char *new_path = g_build_filename (dirname, new_basename, NULL);
		g_free (new_basename);
		g_free (basename);
		g_free (dirname);
      
		filesFolder = do_CreateInstance ("@mozilla.org/file/local;1");
		filesFolder->InitWithNativePath (nsCString(new_path));

		g_free (new_path);

		rv = webPersist->SaveDocument (domDocument, inDestFile, filesFolder,
					       contentType, encodingFlags, 80);
	}
  
	return rv;
}

static char*
GetFilePath (const char *filename)
{
	const char *home_dir;
	char *download_dir, *path;

	download_dir = ephy_file_get_downloads_dir ();

	if (ephy_ensure_dir_exists (download_dir, NULL))
	{
		path = g_build_filename (download_dir, filename, (char *) NULL);
	}
	else
	{
		home_dir = g_get_home_dir ();
		path = g_build_filename (home_dir ? home_dir : "/", filename, (char *) NULL);
	}
	g_free (download_dir);
	
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

	destFile->InitWithNativePath (nsCString (path));
	g_free (path);

	NS_IF_ADDREF (*_retval = destFile);
	return NS_OK;
}
