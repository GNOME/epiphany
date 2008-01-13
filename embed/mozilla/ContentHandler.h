/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti,
 *  Copyright © 2003 Xan Lopez
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  $Id$
 */

#ifndef CONTENT_HANDLER_H
#define CONTENT_HANDLER_H

#include <gio/gio.h>

#include <nsCOMPtr.h>
#include <nsIFile.h>
#include <nsIHelperAppLauncherDialog.h>
#include <nsIURI.h>

#include "ephy-file-helpers.h"


typedef enum
{
	CONTENT_ACTION_OPEN = 1,
	CONTENT_ACTION_OPEN_TMP,
	CONTENT_ACTION_DOWNLOAD,
	CONTENT_ACTION_SAVEAS,
	CONTENT_ACTION_NONE
} ContentAction;

#define G_CONTENTHANDLER_CID			     \
{ /* 16072c4a-23a6-4996-9beb-9335c06bbeae */         \
    0x16072c4a,                                      \
    0x23a6,                                          \
    0x4996,                                          \
    {0x9b, 0xeb, 0x93, 0x35, 0xc0, 0x6b, 0xbe, 0xae} \
}

class GContentHandler : public nsIHelperAppLauncherDialog
{
  public:
	NS_DECL_ISUPPORTS
	NS_DECL_NSIHELPERAPPLAUNCHERDIALOG

	GContentHandler();
	virtual ~GContentHandler();

	NS_METHOD MIMEDoAction ();
	ContentAction mAction;
  private:

	NS_METHOD Init ();

	NS_METHOD MIMEInitiateAction ();
	NS_METHOD MIMEConfirmAction ();

	nsCOMPtr<nsIHelperAppLauncher> mLauncher;
	nsCOMPtr<nsISupports> mContext;

	GAppInfo *mHelperApp;
	EphyMimePermission mPermission;

	nsCString mUrl;
	nsCString mMimeType;
	PRUint32 mUserTime;
};

#endif /* CONTENT_HANDLER_H */
