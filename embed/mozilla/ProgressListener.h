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
 */

#ifndef PROGRESSLISTENER2_H__
#define PROGRESSLISTENER2_H__

#include "downloader-view.h"
#include "ephy-embed-persist.h"
#include "ephy-embed-shell.h"

#include <gtk/gtkwidget.h>
#include "nsIWebProgressListener.h"
#include "nsIHelperAppLauncherDialog.h"
#include "nsIExternalHelperAppService.h"
#include "nsCExternalHandlerService.h"
#include "nsIWebBrowserPersist.h"
#include "nsWeakReference.h"
#include "nsIURI.h"
#include "nsILocalFile.h"
#include "nsIDOMWindow.h"
#include "nsIRequest.h"
#include "nsIDownload.h"
#include "nsIObserver.h"
#include "nsIProgressDialog.h"
#include "nsIMIMEInfo.h"

#include "ContentHandler.h"

#define G_PROGRESSDIALOG_CID                \
{ /* d2a2f743-f126-4f1f-1234-d4e50490f112 */         \
    0xd2a2f743,                                      \
    0xf126,                                          \
    0x4f1f,                                          \
    {0x12, 0x34, 0xd4, 0xe5, 0x04, 0x90, 0xf1, 0x12} \
}

#define G_PROGRESSDIALOG_CLASSNAME "Ephy's Download Progress Dialog"
#define G_PROGRESSDIALOG_CONTRACTID "@mozilla.org/progressdialog;1"

class GProgressListener : public nsIProgressDialog,
			  public nsIWebProgressListener,
 			  public nsSupportsWeakReference
{
 public:
 	NS_DECL_ISUPPORTS
	NS_DECL_NSIWEBPROGRESSLISTENER
	NS_DECL_NSIPROGRESSDIALOG
	NS_DECL_NSIDOWNLOAD

	GProgressListener ();
	virtual ~GProgressListener ();

	NS_METHOD InitForPersist (nsIWebBrowserPersist *aPersist,
				  nsIDOMWindow *aParent, nsIURI *aURI,
				  nsIFile *aFile,
				  DownloadAction aAction,
				  EphyEmbedPersist *ephyPersist,
				  PRBool Dialog,
				  PRInt64 aTimeDownloadStarted = 0);
	nsresult Pause (void);
	nsresult Resume (void);
	nsresult Abort (void);

	GTimer *mTimer;

 private:
	NS_METHOD PrivateInit (void);
	NS_METHOD LaunchHelperApp (void);

	NS_METHOD LaunchHandler (PersistHandlerInfo *handler);
	
	nsCOMPtr<nsIHelperAppLauncher> mLauncher;
	nsCOMPtr<nsIWebBrowserPersist> mPersist;
	nsCOMPtr<GContentHandler> mHandler;
	nsCOMPtr<nsIDOMWindow> mParent;
	nsCOMPtr<nsIRequest> mRequest;
	
	EphyEmbedPersist *mEphyPersist;
	
	nsCOMPtr<nsIURI> mUri;
	PRInt64 mTimeDownloadStarted;
	nsCOMPtr<nsIFile> mFile;
	
	PRInt64 mStartTime;
	PRInt64 mElapsed;
	
	PRInt64 mLastUpdate;
	PRInt32 mInterval;

	PRFloat64 mPriorKRate;
	PRInt32 mRateChanges;
	PRInt32 mRateChangeLimit;

	PRBool mIsPaused;
	PRBool mAbort;
	gboolean mDialog;

	DownloadAction mAction;
	
	DownloaderView *mDownloaderView;

	EphyEmbedShell *ephy_shell;
	
	guint mTimeoutFunc;

	nsCOMPtr<nsIObserver> mObserver;

	nsCOMPtr<nsIMIMEInfo> mMIMEInfo;

	PRInt32 mPercentComplete;
};

extern nsresult NS_NewProgressListenerFactory(nsIFactory** aFactory);

#endif // PROGRESSLISTENER2_H__

