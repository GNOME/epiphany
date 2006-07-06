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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Terry Hayes <thayes@netscape.com>
 *   Javier Delgadillo <javi@netscape.com>
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
 *  Copyright (C) 2005 Christian Persch
 *
 *  $Id$
 */

#include "mozilla-config.h"

#include "config.h"

#include "GtkNSSSecurityWarningDialogs.h"
#include "EphyUtils.h"
#include "AutoJSContextStack.h"
#include "AutoWindowModalState.h"

#include <nsCOMPtr.h>
#include <nsIPrefBranch.h>
#include <nsIPrefService.h>
#include <nsIServiceManager.h>
#include <nsIInterfaceRequestor.h>
#include <nsIInterfaceRequestorUtils.h>
#include <nsIDOMWindow.h>

#include <glib/gi18n.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtklabel.h>

NS_IMPL_THREADSAFE_ISUPPORTS1 (GtkNSSSecurityWarningDialogs, nsISecurityWarningDialogs)

#define ENTER_SITE_PREF      "security.warn_entering_secure"
#define WEAK_SITE_PREF       "security.warn_entering_weak"
#define MIXEDCONTENT_PREF    "security.warn_viewing_mixed"
#define INSECURE_SUBMIT_PREF "security.warn_submit_insecure"

GtkNSSSecurityWarningDialogs::GtkNSSSecurityWarningDialogs()
{
}

GtkNSSSecurityWarningDialogs::~GtkNSSSecurityWarningDialogs()
{
}

NS_IMETHODIMP 
GtkNSSSecurityWarningDialogs::ConfirmEnteringSecure (nsIInterfaceRequestor *aContext,
						     PRBool *_retval)
{
	DoDialog (aContext,
		  ENTER_SITE_PREF,
		  GTK_MESSAGE_INFO,
		  GTK_BUTTONS_OK,
		  GTK_RESPONSE_OK,
		  _("Security Notice"),
		  _("This page is loaded over a secure connection"),
		  _("The padlock icon in the statusbar indicates whether a page is secure."),
		  nsnull, _retval);

	*_retval = PR_TRUE;
	return NS_OK;
}

NS_IMETHODIMP 
GtkNSSSecurityWarningDialogs::ConfirmEnteringWeak (nsIInterfaceRequestor *aContext,
						   PRBool *_retval)
{
	DoDialog (aContext,
		  WEAK_SITE_PREF,
		  GTK_MESSAGE_WARNING,
		  GTK_BUTTONS_OK,
		  GTK_RESPONSE_OK,
		  _("Security Warning"),
		  _("This page is loaded over a low security connection"),
		  _("Any information you see or enter on this page could "
		    "easily be intercepted by a third party."),
		  nsnull, _retval);

	*_retval = PR_TRUE;
	return NS_OK;
}

NS_IMETHODIMP 
GtkNSSSecurityWarningDialogs::ConfirmLeavingSecure (nsIInterfaceRequestor *aContext,
						    PRBool *_retval)
{
	/* don't prompt */
	*_retval = PR_TRUE;
	return NS_OK;
}

NS_IMETHODIMP 
GtkNSSSecurityWarningDialogs::ConfirmMixedMode (nsIInterfaceRequestor *aContext,
						PRBool *_retval)
{
	DoDialog (aContext,
		  MIXEDCONTENT_PREF,
		  GTK_MESSAGE_WARNING,
		  GTK_BUTTONS_OK,
		  GTK_RESPONSE_OK,
		  _("Security Warning"),
		  _("Some parts of this page are loaded over an insecure connection"),
		  _("Some information you see or enter will be sent over an insecure "
		    "connection, and could easily be intercepted by a third party."),
		  nsnull, _retval);

	*_retval = PR_TRUE;
	return NS_OK;
}

NS_IMETHODIMP 
GtkNSSSecurityWarningDialogs::ConfirmPostToInsecure (nsIInterfaceRequestor *aContext,
						     PRBool* _retval)
{
	DoDialog (aContext,
		  INSECURE_SUBMIT_PREF,
		  GTK_MESSAGE_WARNING,
		  GTK_BUTTONS_CANCEL,
		  GTK_RESPONSE_ACCEPT,
		  _("Security Warning"),
		  _("Send this information over an insecure connection?"),
		  _("The information you have entered will be sent over an "
		    "insecure connection, and could easily be intercepted "
		    "by a third party."),
		  _("_Send"),
		  _retval);

	return NS_OK;
}

NS_IMETHODIMP 
GtkNSSSecurityWarningDialogs::ConfirmPostToInsecureFromSecure (nsIInterfaceRequestor *aContext,
							       PRBool* _retval)
{
	DoDialog (aContext,
		  nsnull, /* No preference for this one - it's too important */
		  GTK_MESSAGE_WARNING,
		  GTK_BUTTONS_CANCEL,
		  GTK_RESPONSE_CANCEL,
		  _("Security Warning"),
		  _("Send this information over an insecure connection?"),
		  _("Although this page was loaded over a secure connection, "
		    "the information you have entered will be sent over an "
		    "insecure connection, and could easily be intercepted by "
		    "a third party."),
		  _("_Send"),
		  _retval);

	return NS_OK;
}

void
GtkNSSSecurityWarningDialogs::DoDialog (nsIInterfaceRequestor *aContext,
					const char *aPrefName,
					GtkMessageType aType,
					GtkButtonsType aButtons,
					int aDefaultResponse,
					const char *aTitle,
					const char *aPrimary,
					const char *aSecondary,
					const char *aButtonText,
					PRBool *_retval)
{
	*_retval = PR_FALSE;

	nsresult rv;
	PRBool show = PR_TRUE;
	nsCOMPtr<nsIPrefBranch> prefBranch
		(do_GetService (NS_PREFSERVICE_CONTRACTID));
	if (prefBranch && aPrefName)
	{
		rv = prefBranch->GetBoolPref (aPrefName, &show);
		if (NS_FAILED(rv)) show = PR_TRUE;
	}

	char *showOncePref = NULL;
	PRBool showOnce = PR_FALSE;
	if (!show && prefBranch && aPrefName)
	{
		showOncePref = g_strconcat (aPrefName, ".show_once", (char *) NULL);
		rv = prefBranch->GetBoolPref (showOncePref, &showOnce);
		if (NS_FAILED (rv)) showOnce = PR_FALSE;
	}

	if (!show && !showOnce)
	{
		g_free (showOncePref);
		*_retval = PR_TRUE;
		return;
	}

	/* On 1.8.0, domWin will be always nsnull, because of 
	 * https://bugzilla.mozilla.org/show_bug.cgi?id=277587
	 */
	nsCOMPtr<nsIDOMWindow> domWin (do_GetInterface (aContext));
	GtkWidget *parent = EphyUtils::FindGtkParent (domWin);

	AutoJSContextStack stack;
	rv = stack.Init ();
	if (NS_FAILED (rv)) return;

	AutoWindowModalState modalState (domWin);

	GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW (parent),
						    GTK_DIALOG_MODAL, aType,
						    aButtons, aPrimary);

	if (parent && GTK_WINDOW (parent)->group)
	{
		gtk_window_group_add_window (GTK_WINDOW (parent)->group,
					     GTK_WINDOW (dialog));
	}

	if (aSecondary)
	{
		gtk_message_dialog_format_secondary_text
			(GTK_MESSAGE_DIALOG (dialog), aSecondary);
	}

	if (aButtonText)
	{
		gtk_dialog_add_button (GTK_DIALOG (dialog), aButtonText,
				       GTK_RESPONSE_ACCEPT);
	}

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), aDefaultResponse);

	gtk_window_set_title (GTK_WINDOW (dialog), aTitle);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), "web-browser");

	int response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	*_retval = (response == GTK_RESPONSE_ACCEPT || response == GTK_RESPONSE_OK);

	if (prefBranch && showOncePref && showOnce && *_retval)
	{
		prefBranch->SetBoolPref (showOncePref, PR_FALSE);
	}

	g_free (showOncePref);
}
