/*
 *  Copyright (C) 2000-2003 Marco Pesenti Gritti
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

#ifndef EPHY_WRAPPER_H
#define EPHY_WRAPPER_H

#include "ephy-encodings.h"

#include "nsIDocShell.h"
//#include "ProgressListener.h"
#include "nsIWebNavigation.h"
#include "nsIWebPageDescriptor.h"
#include "nsISHistory.h"
#include "nsIWebBrowser.h"
#include "nsIWebProgressListener.h"
#include "nsCOMPtr.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMDocument.h"
#include "nsPIDOMWindow.h"
#include <gtkmozembed.h>

#include "nsIPrintSettings.h"

class EphyEventListener;

class EphyWrapper
{
public:
	EphyWrapper();
	~EphyWrapper();

	nsresult Init (GtkMozEmbed *mozembed);
	nsresult Destroy (void);

	nsresult SetZoom (float aTextZoom, PRBool reflow);
	nsresult GetZoom (float *aTextZoom);

	nsresult Print (nsIPrintSettings *options, PRBool preview);
	nsresult GetPrintSettings (nsIPrintSettings * *options);
	nsresult PrintPreviewClose (void);
	nsresult PrintPreviewNumPages (int *numPages);
	nsresult PrintPreviewNavigate(PRInt16 navType, PRInt32 pageNum);

	nsresult FindSetProperties (const PRUnichar *search_string,
			            PRBool case_sensitive,
				    PRBool wrap_around);
	nsresult Find (PRBool bacwards,
		       PRBool *didFind);

	nsresult GetMainDocumentUrl (nsCString &url);
	nsresult GetDocumentUrl (nsCString &url);

	nsresult LoadDocument(nsISupports *aPageDescriptor, PRUint32 aDisplayType);
	nsresult GetPageDescriptor(nsISupports **aPageDescriptor);

	nsresult GetSHInfo (PRInt32 *count, PRInt32 *index);
	nsresult GetSHTitleAtIndex (PRInt32 index, PRUnichar **title);
	nsresult GetSHUrlAtIndex (PRInt32 index, nsCString &url);

	nsresult CopyHistoryTo (EphyWrapper *embed);

	nsresult GoToHistoryIndex (PRInt16 index);

	nsresult ForceEncoding (const char *encoding);

	nsresult GetEncodingInfo (EphyEncodingInfo **infoptr);

	nsresult CanCutSelection(PRBool *result);

	nsresult CanCopySelection(PRBool *result);

	nsresult CanPaste(PRBool *result);

	nsresult CutSelection(void);

	nsresult CopySelection(void);

	nsresult Paste(void);

	nsresult GetMainDOMDocument (nsIDOMDocument **aDOMDocument);

	nsresult SelectAll (void);

	nsresult PushTargetDocument (nsIDOMDocument *domDoc);
	nsresult PopTargetDocument ();

	nsresult GetDOMDocument (nsIDOMDocument **aDOMDocument);
	nsresult GetDOMWindow (nsIDOMWindow **aDOMWindow);

	nsCOMPtr<nsIWebBrowser>           mWebBrowser;

	nsCOMPtr<nsIWebNavigation>        mChromeNav;

	GtkMozEmbed *mGtkMozEmbed;
private:
	nsCOMPtr<nsIDOMDocument> mTargetDocument;
	nsCOMPtr<nsIWebProgressListener> mProgress;
	nsCOMPtr<nsIDOMEventReceiver> mEventReceiver;
	EphyEventListener *mEventListener;

	nsresult GetListener (void);
	nsresult AttachListeners (void);
	nsresult DetachListeners (void);
	nsresult SetZoomOnDocshell (float aZoom, nsIDocShell *DocShell);
	nsresult GetDocShell (nsIDocShell **aDocShell);
	nsresult GetCSSBackground (nsIDOMNode *node, nsAutoString& url);
	nsresult GetFocusedDOMWindow (nsIDOMWindow **aDOMWindow);
	nsresult GetSHistory (nsISHistory **aSHistory);
	nsresult GetPIDOMWindow(nsPIDOMWindow **aPIWin);
	nsresult GetWebNavigation(nsIWebNavigation **aWebNavigation);
};

#endif
