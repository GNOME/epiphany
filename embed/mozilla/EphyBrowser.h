/*
 *  Copyright (C) 2000-2004 Marco Pesenti Gritti
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

#ifndef EPHY_BROWSER_H
#define EPHY_BROWSER_H

#include "config.h"

#include "ephy-embed.h"
#include <gtk/gtkwidget.h>

#include <gtkmozembed.h>
#include <nsCOMPtr.h>
#include <nsIDOMEventListener.h>
#include <nsIDOMContextMenuListener.h>
#include <nsIDOMEventTarget.h>
#include <nsIWebNavigation.h>
#include <nsISHistory.h>
#include <nsIWebBrowser.h>
#include <nsIWebBrowserFocus.h>
#include <nsIDOMDocument.h>
#include <nsIDOMWindow.h>
#include <nsIPrintSettings.h>
#include <nsIRequest.h>

#ifdef ALLOW_PRIVATE_API
#include <nsIContentViewer.h>
#endif

#ifdef HAVE_MOZILLA_PSM
#include <nsISecureBrowserUI.h>
#endif

class EphyBrowser;

class EphyEventListener : public nsIDOMEventListener
{
public:
	EphyEventListener();
	virtual ~EphyEventListener();

	nsresult Init (EphyBrowser *aOwner);

	NS_DECL_ISUPPORTS

	// nsIDOMEventListener

	NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent) = 0;

protected:
	EphyBrowser *mOwner;
};

class EphyFaviconEventListener : public EphyEventListener
{
public:
	NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);
};

class EphyPopupBlockEventListener : public EphyEventListener
{
public:
	NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);
};

class EphyModalAlertEventListener : public EphyEventListener
{
public:
	NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);
};

class EphyContextMenuListener : public nsIDOMContextMenuListener
{
public:
        NS_DECL_ISUPPORTS

        EphyContextMenuListener();   
        virtual ~EphyContextMenuListener();

        nsresult Init(EphyBrowser *aOwner);

	// nsIDOMContextMenuListener

        NS_IMETHOD ContextMenu(nsIDOMEvent *aEvent);
        NS_IMETHOD HandleEvent(nsIDOMEvent *aEvent);

protected:
        EphyBrowser *mOwner;
};

class EphyBrowser
{
friend class EphyEventListener;
friend class EphyFaviconEventListener;
friend class EphyPopupBlockEventListener;
friend class EphyModalAlertEventListener;
friend class EphyContextMenuListener;
public:
	EphyBrowser();
	~EphyBrowser();

	nsresult Init (GtkMozEmbed *mozembed);
	nsresult Destroy (void);

	nsresult DoCommand (const char *command);
	nsresult GetCommandState (const char *command, PRBool *enabled);

	nsresult SetZoom (float aTextZoom);
	nsresult GetZoom (float *aTextZoom);

	nsresult Print ();
	nsresult SetPrintPreviewMode (PRBool previewMode);
	nsresult PrintPreviewNumPages (int *numPages);
	nsresult PrintPreviewNavigate(PRInt16 navType, PRInt32 pageNum);

	nsresult FindSetProperties (const PRUnichar *search_string,
			            PRBool case_sensitive,
				    PRBool wrap_around);
	nsresult Find (PRBool bacwards,
		       PRBool *didFind);

	nsresult GetPageDescriptor(nsISupports **aPageDescriptor);

	nsresult GetSHInfo (PRInt32 *count, PRInt32 *index);
	nsresult GetSHTitleAtIndex (PRInt32 index, PRUnichar **title);
	nsresult GetSHUrlAtIndex (PRInt32 index, nsACString &url);
	nsresult GoToHistoryIndex (PRInt16 index);

	nsresult ForceEncoding (const char *encoding);
	nsresult GetEncoding (nsACString &encoding);
	nsresult GetForcedEncoding (nsACString &encoding);

	nsresult PushTargetDocument (nsIDOMDocument *domDoc);
	nsresult PopTargetDocument ();

	nsresult GetDocument (nsIDOMDocument **aDOMDocument);
	nsresult GetTargetDocument (nsIDOMDocument **aDOMDocument);
	nsresult GetDocumentURI (nsIURI **aURI);
	nsresult GetTargetDocumentURI (nsIURI **aURI);
	nsresult GetDOMWindow (nsIDOMWindow **window);

	nsresult GetHasModifiedForms (PRBool *modified);

	nsresult GetSecurityInfo (PRUint32 *aState, nsACString &aDescription);
	nsresult ShowCertificate ();

        EphyEmbedDocumentType GetDocumentType ();

#ifdef GTKMOZEMBED_BROKEN_FOCUS
	nsresult FocusActivate ();
	nsresult FocusDeactivate ();
#endif

	nsCOMPtr<nsIWebBrowser> mWebBrowser;
private:
	GtkWidget *mEmbed;

	nsCOMPtr<nsIWebBrowserFocus> mWebBrowserFocus;
	nsCOMPtr<nsIDOMDocument> mTargetDocument;
	nsCOMPtr<nsIDOMEventTarget> mEventTarget;
	nsCOMPtr<nsIDOMWindow> mDOMWindow;
	EphyFaviconEventListener *mFaviconEventListener;
	EphyPopupBlockEventListener *mPopupBlockEventListener;
	EphyModalAlertEventListener *mModalAlertListener;
	EphyContextMenuListener *mContextMenuListener;
	PRBool mInitialized;
#ifdef HAVE_MOZILLA_PSM
	nsCOMPtr<nsISecureBrowserUI> mSecurityInfo;
#endif

	nsresult GetListener (void);
	nsresult AttachListeners (void);
	nsresult DetachListeners (void);
	nsresult GetSHistory (nsISHistory **aSHistory);
	nsresult GetContentViewer (nsIContentViewer **aViewer);
	nsresult GetDocumentHasModifiedForms (nsIDOMDocument *aDomDoc, PRUint32 *aNumTextFields, PRBool *aHasTextArea);
	PRBool   CompareFormsText (nsAString &aDefaultText, nsAString &aUserText);
};

#endif /* !EPHY_BROWSER_H */
