/*
 *  Copyright (C) 2000-2003 Marco Pesenti Gritti, Xan Lopez
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

#ifndef __ContentHandler_h
#define __ContentHandler_h

#include "ephy-embed-shell.h"

#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include "nsIHelperAppLauncherDialog.h"
#include "nsIExternalHelperAppService.h"
#include "nsCExternalHandlerService.h"
#include "nsIWebProgressListener.h"

#include "nsString.h"
#include "nsIURI.h"
#include "nsILocalFile.h"

#include "nsCRT.h"
#include "nsWeakReference.h"

#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsError.h"

#define G_CONTENTHANDLER_CID			     \
{ /* 16072c4a-23a6-4996-9beb-9335c06bbeae */         \
    0x16072c4a,                                      \
    0x23a6,                                          \
    0x4996,                                          \
    {0x9b, 0xeb, 0x93, 0x35, 0xc0, 0x6b, 0xbe, 0xae} \
}

class nsIFactory;

class GContentHandler : public nsIHelperAppLauncherDialog
{
  public:
	NS_DECL_ISUPPORTS
	NS_DECL_NSIHELPERAPPLAUNCHERDIALOG

	GContentHandler();
	virtual ~GContentHandler();

	NS_METHOD FindHelperApp (void);
	NS_METHOD LaunchHelperApp (void);

	NS_METHOD GetLauncher (nsIHelperAppLauncher * *_retval);
	NS_METHOD SetHelperApp(GnomeVFSMimeApplication *mHelperApp,
			       PRBool alwaysUse);
	NS_METHOD SynchroniseMIMEInfo (void);
	NS_METHOD MIMEAskAction (void);
  private:
	/* additional members */
	NS_METHOD Init (void);
	NS_METHOD ProcessMimeInfo (void);
	
	nsCOMPtr<nsIHelperAppLauncher> mLauncher;

	nsCOMPtr<nsIURI> mUri;
	nsCOMPtr<nsIFile> mTempFile;

	char *mMimeType;	
	PRBool mUrlHelper;
	GnomeVFSMimeApplication *mHelperApp;
	
	nsCString mUrl;
	nsCString mScheme;
};

#endif
