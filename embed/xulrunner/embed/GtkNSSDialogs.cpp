/*
 *  Copyright © 2003 Crispin Flowerday <gnome@flowerday.cx>
 *  Copyright © 2006 Christian Persch
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

/*
 * This file provides Gtk implementations of the mozilla Certificate dialogs
 * such as the ones displayed when connecting to a site with a self-signed
 * or expired certificate.
 */

#include "mozilla-config.h"
#include "config.h"

#include <time.h>

#include <glib/gi18n.h>
#include <gtk/gtkalignment.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkeditable.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkimage.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkprogressbar.h>
#include <gtk/gtksizegroup.h>
#include <gtk/gtkstock.h>
#include <gtk/gtktable.h>
#include <gtk/gtktextbuffer.h>
#include <gtk/gtktextview.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtktreestore.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkcombobox.h>
#include <gconf/gconf-client.h>
#include <glade/glade-xml.h>

#include <nsStringAPI.h>

#include <nsCOMPtr.h>
#include <nsIArray.h>
#include <nsIASN1Object.h>
#include <nsIASN1Sequence.h>
#include <nsICRLInfo.h>
#include <nsIDOMWindow.h>
#include <nsIInterfaceRequestor.h>
#include <nsIInterfaceRequestorUtils.h>
#include <nsIMutableArray.h>
#include <nsIPKCS11ModuleDB.h>
#include <nsIPKCS11Slot.h>
#include <nsIPK11Token.h>
#include <nsIPK11TokenDB.h>
#include <nsIServiceManager.h>
#include <nsISimpleEnumerator.h>
#include <nsIX509CertDB.h>
#include <nsIX509Cert.h>
#include <nsIX509CertValidity.h>
#include <nsMemory.h>
#include <nsServiceManagerUtils.h>

#ifdef HAVE_NSIMUTABLEARRAY_H
#include <nsIMutableArray.h>
#endif

#include "ephy-file-helpers.h"
#include "ephy-gui.h"
#include "ephy-password-dialog.h"
#include "ephy-stock-icons.h"

#include "AutoJSContextStack.h"
#include "AutoWindowModalState.h"
#include "EphyUtils.h"

#include "GtkNSSDialogs.h"

NS_DEFINE_CID (kX509CertCID, NS_IX509CERT_IID);
NS_DEFINE_CID (kASN1ObjectCID, NS_IASN1OBJECT_IID);

enum
{
	NSSDIALOG_RESPONSE_VIEW_CERT = 10
};

GtkNSSDialogs::GtkNSSDialogs ()
{
}

GtkNSSDialogs::~GtkNSSDialogs ()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS5 (GtkNSSDialogs, 
			       nsICertificateDialogs,
			       nsIBadCertListener,
			       nsITokenPasswordDialogs,
			       nsITokenDialogs,
			       nsIDOMCryptoDialogs)

/* There's also nsICertPickDialogs which is implemented in mozilla
 * but has no callers. So we don't implement it.
 * Same for nsIUserCertPicker which is only used in mailnews.
 */

/**
 *  Call the mozilla service to display a certificate
 */
static void
view_certificate (nsIInterfaceRequestor *ctx, nsIX509Cert *cert)
{
	nsresult rv;
	nsCOMPtr<nsICertificateDialogs> certDialogs =
		do_GetService (NS_CERTIFICATEDIALOGS_CONTRACTID, &rv);
	NS_ENSURE_SUCCESS (rv, );

	certDialogs->ViewCert (ctx, cert);
}

/** 
 *  Indent a widget according the HIG
 * 
 *  @returns: The new indented widget
 */
static GtkWidget*
higgy_indent_widget (GtkWidget *widget)
{
	GtkWidget *hbox;
	GtkWidget *label;

	hbox = gtk_hbox_new (FALSE, 6);

	label = gtk_label_new (NULL);
	gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, TRUE, 6);
	gtk_widget_show (label);

	gtk_box_pack_start (GTK_BOX(hbox), widget, TRUE, TRUE, 0);

	return hbox;
}

/**
 *  Setup up a dialog with the correct HIG'gy spacings, adding the content_widget
 */
static void
higgy_setup_dialog (GtkDialog *dialog, const gchar *stock_icon, 
		    GtkWidget **content_label,
		    GtkWidget **content_vbox)
{
	GtkWidget *hbox, *label, *image, *vbox;

	g_return_if_fail (GTK_IS_DIALOG (dialog));
	g_return_if_fail (content_label);

	gtk_dialog_set_has_separator (dialog, FALSE);
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	
	hbox = gtk_hbox_new (FALSE, 12);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), hbox);

	image = gtk_image_new_from_stock (stock_icon, GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment (GTK_MISC (image), 0.5, 0.0);
	gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);

	vbox = gtk_vbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

	label = gtk_label_new (NULL);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_label_set_selectable (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);

	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
	
	gtk_widget_show (image);
	gtk_widget_show (vbox);
	gtk_widget_show (hbox);
	gtk_widget_show (label);

	/* Set up the spacing for the dialog internal widgets */
	gtk_box_set_spacing (GTK_BOX(dialog->vbox), 14); /* 24 = 2 * 5 + 14 */

	*content_label = label;
	if (content_vbox)
	{
		*content_vbox = vbox;
	}
}


/**
 *  Display a dialog box, showing 'View Certificate', 'Cancel',
 *  and 'Accept' buttons. Optionally a checkbox can be shown,
 *  or the text can be NULL to avoid it being displayed
 * 
 *  @returns: GTK_RESPONSE_ACCEPT if the user clicked Accept
 */
static gint
display_cert_warning_box (nsIInterfaceRequestor *ctx, 
                          nsIX509Cert *cert,
			  const char *markup_text,
                          const char *checkbox_text,
                          gboolean *checkbox_value,
			  const char *affirmative_text)
{
	GtkWidget *dialog, *label, *checkbox, *vbox, *button;
	int res;

	g_return_val_if_fail (markup_text, GTK_RESPONSE_CANCEL);
	g_return_val_if_fail (!checkbox_text || checkbox_value, GTK_RESPONSE_CANCEL);

	nsresult rv;
	AutoJSContextStack stack;
	rv = stack.Init ();
	if (NS_FAILED (rv)) return rv;

	/* NOTE: Due to a mozilla bug [https://bugzilla.mozilla.org/show_bug.cgi?id=306288],
	 * we will always end up without a parent!
	 */
	nsCOMPtr<nsIDOMWindow> parent (do_GetInterface (ctx));
	GtkWindow *gparent = GTK_WINDOW (EphyUtils::FindGtkParent (parent));

	AutoWindowModalState modalState (parent);

	dialog = gtk_dialog_new_with_buttons ("", gparent,
					      GTK_DIALOG_DESTROY_WITH_PARENT,
					      (char *) NULL);
	if (gparent)
	{
		gtk_window_group_add_window (ephy_gui_ensure_window_group (gparent),
					     GTK_WINDOW (dialog));
	}

	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), EPHY_STOCK_EPHY);

	higgy_setup_dialog (GTK_DIALOG (dialog), 
			    GTK_STOCK_DIALOG_WARNING, &label, &vbox);

	/* Add the buttons */
	gtk_dialog_add_button (GTK_DIALOG (dialog), _("_View Certificate"),
			       NSSDIALOG_RESPONSE_VIEW_CERT);

	gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CANCEL,
			       GTK_RESPONSE_CANCEL);

	if (affirmative_text == NULL)
	{
		affirmative_text = _("_Accept");
	}

	button = gtk_dialog_add_button (GTK_DIALOG (dialog), 
					affirmative_text,
					GTK_RESPONSE_ACCEPT);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);

        if (checkbox_text)
	{
		checkbox = gtk_check_button_new_with_mnemonic (checkbox_text);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbox), 
					      *checkbox_value);

		gtk_box_pack_start (GTK_BOX (vbox), checkbox, TRUE, TRUE, 0);
	}
	else
	{
		checkbox = 0;
	}

	/* We don't want focus on the checkbox */
	gtk_widget_grab_focus (button);

	gtk_label_set_markup (GTK_LABEL (label), markup_text);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
	gtk_widget_show_all (dialog);

	while (1)
	{
		res = gtk_dialog_run (GTK_DIALOG (dialog));
		if (res == NSSDIALOG_RESPONSE_VIEW_CERT)
		{
                      view_certificate (ctx, cert);
		      continue;
		}
	
		break;
	}

	if (res == GTK_RESPONSE_ACCEPT && checkbox)
	{
		*checkbox_value = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbox));
	}

	gtk_widget_destroy (dialog);
        return res;
}


/* Helper functions */

nsresult
GtkNSSDialogs::GetTokenAndSlotFromName (const PRUnichar *aName,
					nsIPK11Token **aToken,
					nsIPKCS11Slot **aSlot)
{
	nsresult rv = NS_ERROR_FAILURE;
	*aToken = nsnull;
	*aSlot = nsnull;

	nsCOMPtr<nsIPK11TokenDB> tokenDB = do_GetService("@mozilla.org/security/pk11tokendb;1");
	nsCOMPtr<nsIPKCS11ModuleDB> pkcs11DB = do_GetService("@mozilla.org/security/pkcs11moduledb;1");
	if (!tokenDB || !pkcs11DB) return rv;

	rv = tokenDB->FindTokenByName (aName, aToken);
	NS_ENSURE_TRUE (NS_SUCCEEDED (rv) && *aToken, rv);

	pkcs11DB->FindSlotByName (aName, aSlot);

	NS_ENSURE_TRUE (*aSlot, NS_ERROR_FAILURE);

#ifdef GNOME_ENABLE_DEBUG
	/* Dump some info about this token */
	nsIPK11Token *token = *aToken;
	PRUnichar *tName, *tLabel, *tManID, *tHWVersion, *tFWVersion, *tSN;
	PRInt32 minPwdLen;
	PRBool needsInit, isHW, needsLogin, isFriendly;

	token->GetTokenName(&tName);
	token->GetTokenLabel(&tLabel);
	token->GetTokenManID(&tManID);
	token->GetTokenHWVersion(&tHWVersion);
	token->GetTokenFWVersion(&tFWVersion);
	token->GetTokenSerialNumber(&tSN);
	token->GetMinimumPasswordLength(&minPwdLen);
	token->GetNeedsUserInit(&needsInit);
	token->IsHardwareToken(&isHW);
	token->NeedsLogin(&needsLogin);
	token->IsFriendly(&isFriendly);

	g_print ("Token '%s' has \nName: %s\nLabel: %s\nManID: %s\nHWversion: %s\nFWVersion: %s\nSN: %s\n"
			"MinPwdLen: %d\nNeedsUserInit: %d\nIsHWToken: %d\nNeedsLogin: %d\nIsFriendly: %d\n\n",
		NS_ConvertUTF16toUTF8(aName).get(),

		NS_ConvertUTF16toUTF8(tName).get(),
		NS_ConvertUTF16toUTF8(tLabel).get(),
		NS_ConvertUTF16toUTF8(tManID).get(),
		NS_ConvertUTF16toUTF8(tHWVersion).get(),
		NS_ConvertUTF16toUTF8(tFWVersion).get(),
		NS_ConvertUTF16toUTF8(tSN).get(),
		minPwdLen,
		needsInit,
		isHW,
		needsLogin,
		isFriendly);

	nsIPKCS11Slot *slot = *aSlot;
	PRUnichar*slDesc;
	slot->GetDesc(&slDesc);
	g_print ("Slot description: %s\n", NS_ConvertUTF16toUTF8 (slDesc).get());
#endif

	return NS_OK;
}
  
/* nsICertificateDialogs */

NS_IMETHODIMP
GtkNSSDialogs::ConfirmMismatchDomain (nsIInterfaceRequestor *ctx,
                                      const nsACString &targetURL,
                                      nsIX509Cert *cert, PRBool *_retval)
{
	char *first, *second, *msg;
	int res;

	nsString commonName;
	cert->GetCommonName (commonName);

	NS_ConvertUTF16toUTF8 cCommonName (commonName);

	nsCString cTargetUrl (targetURL);

	first = g_markup_printf_escaped (_("The site “%s” returned security information for "
					   "“%s”. It is possible that someone is intercepting "
					   "your communication to obtain your confidential "
					   "information."),
					 cTargetUrl.get(), cCommonName.get());

        second = g_markup_printf_escaped (_("You should only accept the security information if you "
					    "trust “%s” and “%s”."),
					  cTargetUrl.get(), cCommonName.get());
	
	msg = g_strdup_printf ("<span weight=\"bold\" size=\"larger\">%s</span>\n\n%s\n\n%s",
			       _("Accept incorrect security information?"),
			       first, second);
 
	res = display_cert_warning_box (ctx, cert, msg, NULL, NULL, NULL);
 
        g_free (second);
        g_free (first);
        g_free (msg);

	*_retval = (res == GTK_RESPONSE_ACCEPT);
	return NS_OK;
}


NS_IMETHODIMP
GtkNSSDialogs::ConfirmUnknownIssuer (nsIInterfaceRequestor *ctx,
                                     nsIX509Cert *cert, PRInt16 *outAddType,
                                     PRBool *_retval)
{
	gboolean accept_perm = FALSE;
	char *secondary, *tertiary, *msg;
	int res;

	nsString commonName;
	cert->GetCommonName (commonName);

	NS_ConvertUTF16toUTF8 cCommonName (commonName);

	secondary = g_markup_printf_escaped
		           (_("It was not possible to automatically trust “%s”. "
			      "It is possible that someone is intercepting your "
			      "communication to obtain your confidential information."),
			      cCommonName.get());

        tertiary = g_markup_printf_escaped
		           (_("You should only connect to the site if you are certain "
			      "you are connected to “%s”."),
			    cCommonName.get());

	msg = g_strdup_printf ("<span weight=\"bold\" size=\"larger\">%s</span>\n\n%s\n\n%s",
			       _("Connect to untrusted site?"),
			       secondary, tertiary);
 
	res = display_cert_warning_box (ctx, cert, msg, 
 					_("_Trust this security information from now on"),
 					&accept_perm, _("Co_nnect"));
        g_free (tertiary);
        g_free (secondary);
        g_free (msg);

        if (res != GTK_RESPONSE_ACCEPT)
	{
		*_retval = PR_FALSE;
		*outAddType = UNINIT_ADD_FLAG;
	}
	else
	{
		if (accept_perm)
		{
			*_retval    = PR_TRUE;
			*outAddType = ADD_TRUSTED_PERMANENTLY;
		}
		else
		{
			*_retval    = PR_TRUE;
			*outAddType = ADD_TRUSTED_FOR_SESSION;
		}
	}

 	return NS_OK;
}


/* boolean confirmCertExpired (in nsIInterfaceRequestor socketInfo, 
   in nsIX509Cert cert); */
NS_IMETHODIMP 
GtkNSSDialogs::ConfirmCertExpired (nsIInterfaceRequestor *ctx,
                                   nsIX509Cert *cert, PRBool *_retval)
{
	nsresult rv;
	PRTime now = PR_Now();
	PRTime notAfter, notBefore, timeToUse;
	PRInt64 normalizedTime;
	time_t t;
	struct tm tm;
	char formattedDate[128];
	char *fdate;
	const char *primary, *text;
	char *secondary, *msg;

	*_retval = PR_FALSE;
	
	nsCOMPtr<nsIX509CertValidity> validity;
	rv = cert->GetValidity (getter_AddRefs(validity));
	if (NS_FAILED(rv)) return rv;
	
	rv = validity->GetNotAfter (&notAfter);
	if (NS_FAILED(rv)) return rv;
	
	rv = validity->GetNotBefore (&notBefore);
	if (NS_FAILED(rv)) return rv;
	
	if (LL_CMP(now, >, notAfter))
	{
		primary = _("Accept expired security information?");
		/* Translators: first %s is a hostname, second %s is a time/date */
		text    = _("The security information for “%s” "
			    "expired on %s.");
		timeToUse = notAfter;
	} 
	else
	{
		primary = _("Accept not yet valid security information?");
		/* Translators: first %s is a hostname, second %s is a time/date */
		text    = _("The security information for “%s” isn't valid until %s.");
		timeToUse = notBefore;
	}
	
	nsString commonName;
	cert->GetCommonName (commonName);

	NS_ConvertUTF16toUTF8 cCommonName (commonName);

	LL_DIV (normalizedTime, timeToUse, PR_USEC_PER_SEC);
	LL_L2UI (t, normalizedTime);
	/* To translators: this a time format that is used while displaying the
	 * expiry or start date of an SSL certificate, for the format see 
	 * strftime(3) */
	strftime (formattedDate, sizeof(formattedDate), _("%a %d %b %Y"), 
		  localtime_r (&t, &tm));
	/* FIXME! this isn't actually correct, LC_CTIME codeset could be different than locale codeset! */
	fdate = g_locale_to_utf8 (formattedDate, -1, NULL, NULL, NULL);

	secondary = g_markup_printf_escaped (text, cCommonName.get(), fdate);

	msg = g_strdup_printf ("<span weight=\"bold\" size=\"larger\">%s</span>\n\n%s\n\n%s",
			       primary, secondary, 
			       _("You should ensure that your computer's time is correct."));

	int res = display_cert_warning_box (ctx, cert, msg, NULL, NULL, NULL);

	g_free (fdate);
	g_free (msg);
	g_free (secondary);

	*_retval = (res == GTK_RESPONSE_ACCEPT);
	
	return NS_OK;
}

/* void notifyCrlNextupdate (in nsIInterfaceRequestor socketInfo, 
   			     in AUTF8String targetURL,
 			     in nsIX509Cert cert); */
NS_IMETHODIMP 
GtkNSSDialogs::NotifyCrlNextupdate (nsIInterfaceRequestor *ctx,
				    const nsACString & targetURL,
				    nsIX509Cert *cert)
{
	nsCOMPtr<nsIDOMWindow> parent = do_GetInterface (ctx);
	GtkWidget *gparent = EphyUtils::FindGtkParent (parent);

	nsCString cTargetUrl (targetURL);

	nsString commonName;
	cert->GetCommonName (commonName);

	GtkWidget *dialog = gtk_message_dialog_new
				(GTK_WINDOW (gparent),
				 GTK_DIALOG_DESTROY_WITH_PARENT,
				 GTK_MESSAGE_ERROR,
				 GTK_BUTTONS_OK,
				 _("Cannot establish connection to “%s”"),
				 cTargetUrl.get ());

	gtk_message_dialog_format_secondary_text
			(GTK_MESSAGE_DIALOG (dialog),
			 _("The certificate revocation list (CRL) from “%s” "
			   "needs to be updated.\n\n"
			   "Please ask your system administrator for assistance."),
			 NS_ConvertUTF16toUTF8 (commonName).get ());
	gtk_window_set_icon_name (GTK_WINDOW (dialog), EPHY_STOCK_EPHY);

	g_signal_connect (dialog, "response",
			  (GCallback) gtk_widget_destroy, NULL);

	gtk_widget_show_all (dialog);
	return NS_OK;
}

NS_IMETHODIMP 
GtkNSSDialogs::ConfirmDownloadCACert(nsIInterfaceRequestor *ctx, 
				    nsIX509Cert *cert,
				    PRUint32 *_trust,
				    PRBool *_retval)
{
	GtkWidget *dialog, *label;
	char *msg, *primary;

	nsresult rv;
	AutoJSContextStack stack;
	rv = stack.Init ();
	if (NS_FAILED (rv)) return rv;

	nsCOMPtr<nsIDOMWindow> parent (do_GetInterface (ctx));
	GtkWindow *gparent = GTK_WINDOW (EphyUtils::FindGtkParent (parent));

	AutoWindowModalState modalState (parent);

	dialog = gtk_dialog_new_with_buttons (_("Trust new Certificate Authority?"), gparent,
					      GTK_DIALOG_DESTROY_WITH_PARENT,
					      _("_View Certificate"),
					      NSSDIALOG_RESPONSE_VIEW_CERT,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      _("_Trust CA"),	GTK_RESPONSE_ACCEPT,
					      (char *) NULL);

	if (gparent)
	{
		gtk_window_group_add_window (ephy_gui_ensure_window_group (gparent),
					     GTK_WINDOW (dialog));
	}

	gtk_window_set_icon_name (GTK_WINDOW (dialog), EPHY_STOCK_EPHY);

	higgy_setup_dialog (GTK_DIALOG (dialog), GTK_STOCK_DIALOG_WARNING,
			    &label, NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);

	nsString commonName;
	cert->GetCommonName (commonName);

	NS_ConvertUTF16toUTF8 cCommonName (commonName);

	primary = g_markup_printf_escaped (_("Trust new Certificate Authority “%s” to identify web sites?"),
					   cCommonName.get());

	msg = g_strdup_printf ("<span weight=\"bold\" size=\"larger\">%s</span>\n\n%s",
			       primary,
			       _("Before trusting a Certificate Authority (CA) you should "
				 "verify the certificate is authentic."));
	gtk_label_set_markup (GTK_LABEL (label), msg);
	g_free (primary);
	g_free (msg);

	gtk_widget_show_all (dialog);
	int ret;

	while (1)
	{
		ret = gtk_dialog_run (GTK_DIALOG (dialog));
		if (ret == NSSDIALOG_RESPONSE_VIEW_CERT)
		{
		      view_certificate (ctx, cert);
		      continue;
		}
	
		break;
	}

	if (ret != GTK_RESPONSE_ACCEPT)
	{
		*_retval = PR_FALSE;
	}
	else
	{
		if (ret == GTK_RESPONSE_ACCEPT)
		{
			*_trust |= nsIX509CertDB::TRUSTED_SSL;
		}
		else
		{
			*_trust = nsIX509CertDB::UNTRUSTED;
		}

		*_retval = PR_TRUE;
	}
	gtk_widget_destroy (dialog);

	return NS_OK;
}


NS_IMETHODIMP 
GtkNSSDialogs::NotifyCACertExists (nsIInterfaceRequestor *ctx)
{
	GtkWidget *dialog, *label;
	char * msg;

	nsCOMPtr<nsIDOMWindow> parent = do_GetInterface (ctx);
	GtkWindow *gparent = GTK_WINDOW (EphyUtils::FindGtkParent (parent));

	dialog = gtk_dialog_new_with_buttons ("", gparent,
					      GTK_DIALOG_DESTROY_WITH_PARENT,
					      GTK_STOCK_OK,
					      GTK_RESPONSE_OK,
					      (char *) NULL);

	if (gparent)
	{
		gtk_window_group_add_window (ephy_gui_ensure_window_group (gparent),
					     GTK_WINDOW (dialog));
	}

	gtk_window_set_icon_name (GTK_WINDOW (dialog), EPHY_STOCK_EPHY);

	higgy_setup_dialog (GTK_DIALOG (dialog), GTK_STOCK_DIALOG_ERROR,
			    &label, NULL);

	msg = g_strdup_printf ("<span weight=\"bold\" size=\"larger\">%s</span>\n\n%s",
				_("Certificate already exists."),
			       _("The certificate has already been imported."));
	gtk_label_set_markup (GTK_LABEL (label), msg);
	g_free (msg);

	g_signal_connect (G_OBJECT (dialog),
			  "response",
			  (GCallback)gtk_widget_destroy, NULL);

	gtk_widget_show_all (dialog);
	return NS_OK;
}

/* FIXME: This interface sucks! There is way to know the name of the certificate! */
NS_IMETHODIMP 
GtkNSSDialogs::SetPKCS12FilePassword(nsIInterfaceRequestor *ctx, 
				    nsAString &_password,
				    PRBool *_retval)
{
	GtkWidget *dialog;
	char *msg;

	nsresult rv;
	AutoJSContextStack stack;
	rv = stack.Init ();
	if (NS_FAILED (rv)) return rv;

	nsCOMPtr<nsIDOMWindow> parent (do_GetInterface (ctx));
	GtkWidget *gparent = EphyUtils::FindGtkParent (parent);

	AutoWindowModalState modalState (parent);

	dialog = ephy_password_dialog_new (gparent,
					   _("Select Password"),
					   EphyPasswordDialogFlags(EPHY_PASSWORD_DIALOG_FLAGS_SHOW_NEW_PASSWORD |
								   EPHY_PASSWORD_DIALOG_FLAGS_SHOW_QUALITY_METER));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);

	/* FIXME: set accept button text to (_("_Back Up Certificate") ?
	 * That's not actually correct, since this function is also called from other places!
	 */

	msg = g_markup_printf_escaped (_("Select a password to protect this certificate"));
	gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog), msg);
	g_free (msg);

	int response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_hide (dialog);
	
	if (response == GTK_RESPONSE_ACCEPT)
	{
		const char *text = ephy_password_dialog_get_new_password (EPHY_PASSWORD_DIALOG (dialog));
		g_return_val_if_fail (text != NULL, NS_ERROR_FAILURE);
		NS_CStringToUTF16 (nsDependentCString (text),
			           NS_CSTRING_ENCODING_UTF8, _password);
	}

	*_retval = response == GTK_RESPONSE_ACCEPT;

	gtk_widget_destroy (dialog);

	return NS_OK;
}

NS_IMETHODIMP 
GtkNSSDialogs::GetPKCS12FilePassword(nsIInterfaceRequestor *ctx, 
				     nsAString &_password,
				     PRBool *_retval)
{
	g_print ("GtkNSSDialogs::GetPKCS12FilePassword\n");

	nsresult rv;
	AutoJSContextStack stack;
	rv = stack.Init ();
	if (NS_FAILED (rv)) return rv;

	nsCOMPtr<nsIDOMWindow> parent (do_GetInterface (ctx));
	GtkWidget *gparent = EphyUtils::FindGtkParent (parent);

	AutoWindowModalState modalState (parent);

	GtkWidget *dialog = ephy_password_dialog_new
				(gparent,
				 "",
				 EphyPasswordDialogFlags (EPHY_PASSWORD_DIALOG_FLAGS_SHOW_PASSWORD));
	EphyPasswordDialog *password_dialog = EPHY_PASSWORD_DIALOG (dialog);
	/* FIXME: set accept button text to _("I_mport Certificate") ? */

	gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);

	/* FIXME: mozilla sucks, no way to get the name of the certificate / cert file! */
	char *msg = g_markup_printf_escaped (_("Enter the password for this certificate"));
	gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog), msg);
	g_free (msg);
			
	int response = gtk_dialog_run (GTK_DIALOG (dialog));

	if (response == GTK_RESPONSE_ACCEPT)
	{
		const char *pwd = ephy_password_dialog_get_password (password_dialog);
		NS_CStringToUTF16 (nsDependentCString (pwd),
				   NS_CSTRING_ENCODING_UTF8, _password);
	}

	*_retval = response == GTK_RESPONSE_ACCEPT;

	gtk_widget_destroy (dialog);

	return NS_OK;
}


static void
set_table_row (GtkWidget *table,
	       int& row,
	       const char *title,
	       const char *text)
{
	GtkWidget *header, *label;
	char *bold;

	if (text == NULL || text[0] == 0) return;

	bold = g_markup_printf_escaped ("<b>%s</b>", title);
	header = gtk_label_new (bold);
	g_free (bold);

	gtk_label_set_use_markup (GTK_LABEL (header), TRUE);
	gtk_misc_set_alignment (GTK_MISC (header), 0, 0);
	gtk_widget_show (header);
	gtk_table_attach (GTK_TABLE (table), header, 0, 1, row, row+1,
			  GTK_FILL, GTK_FILL, 0, 0);

	label = gtk_label_new (text);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
	gtk_label_set_selectable (GTK_LABEL (label), TRUE);
	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
	gtk_label_set_max_width_chars (GTK_LABEL (label), 48);
	gtk_widget_show (label);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 1, 2, row, row+1);

	row++;
}

NS_IMETHODIMP 
GtkNSSDialogs::CrlImportStatusDialog(nsIInterfaceRequestor *ctx, nsICRLInfo *crl)
{

	GtkWidget *dialog, *label, *table, *vbox;
	nsresult rv;
	char *msg;

	nsCOMPtr<nsIDOMWindow> parent = do_GetInterface (ctx);
	GtkWidget *gparent = EphyUtils::FindGtkParent (parent);

	dialog = gtk_dialog_new_with_buttons ("",
					      GTK_WINDOW (gparent),
					      GTK_DIALOG_DESTROY_WITH_PARENT,
					      GTK_STOCK_OK, GTK_RESPONSE_OK,
					      (char *) NULL);

	gtk_window_set_icon_name (GTK_WINDOW (dialog), EPHY_STOCK_EPHY);
	gtk_window_set_title (GTK_WINDOW (dialog), _("Certificate Revocation List Imported"));

	/* Needed because gparent == NULL always because of mozilla sucks */
	gtk_window_set_skip_pager_hint (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_skip_taskbar_hint (GTK_WINDOW (dialog), TRUE);

	higgy_setup_dialog (GTK_DIALOG (dialog), GTK_STOCK_DIALOG_INFO,
			    &label, &vbox);

	msg = g_strdup_printf ("<span weight=\"bold\" size=\"larger\">%s</span>",
			       _("Certificate Revocation List (CRL) successfully imported"));
	gtk_label_set_markup (GTK_LABEL (label), msg);
	g_free (msg);

	table = gtk_table_new (2, 3, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (table), 12);

	nsString org, orgUnit, nextUpdate;
	rv = crl->GetOrganization (org);
	if (NS_FAILED(rv)) return rv;

	rv = crl->GetOrganizationalUnit (orgUnit);
	if (NS_FAILED(rv)) return rv;

	rv = crl->GetNextUpdateLocale (nextUpdate);
	if (NS_FAILED(rv)) return rv;

	int row = 0;
	set_table_row (table, row, _("Organization:"), NS_ConvertUTF16toUTF8 (org).get ());

	set_table_row (table, row, _("Unit:"), NS_ConvertUTF16toUTF8 (orgUnit).get ());

	set_table_row (table, row, _("Next Update:"), NS_ConvertUTF16toUTF8 (nextUpdate).get ());

	gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);

	gtk_widget_show_all (dialog);
	g_signal_connect (G_OBJECT (dialog),
			  "response",
			  (GCallback)gtk_widget_destroy, NULL);

	gtk_widget_show_all (dialog);
	return NS_OK;
}

/** 
 *  Help function to fill in the labels on the General tab
 */
static void
set_label_cert_attribute (GladeXML* gxml, const char* label_id, nsAString &value)
{
	GtkWidget *label;
	label = glade_xml_get_widget (gxml, label_id);

	g_return_if_fail (GTK_IS_LABEL (label));

	if (!value.Length()) {
		gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
		char *msg = g_strdup_printf ("<i>&lt;%s&gt;</i>",
					     _("Not part of certificate"));
		gtk_label_set_markup (GTK_LABEL (label), msg);
		g_free (msg);
	}
	else
	{
		gtk_label_set_use_markup (GTK_LABEL (label), FALSE);
		gtk_label_set_text (GTK_LABEL (label), NS_ConvertUTF16toUTF8 (value).get());
	}
}


/**
 *  Do that actual filling in of the certificate tree
 */
static gboolean
fill_cert_chain_tree (GtkTreeView *treeview, nsIArray *certChain)
{
	nsresult rv;
	GtkTreeModel * model = gtk_tree_view_get_model (treeview);

	GtkTreeIter parent;
	PRUint32 numCerts;
	rv =  certChain->GetLength (&numCerts);
	if (NS_FAILED(rv) || numCerts < 1) return FALSE;

	for (int i = (int)numCerts-1 ; i >= 0; i--)
	{
		nsCOMPtr<nsIX509Cert> nsCert;
		rv = certChain->QueryElementAt (i, kX509CertCID,
						getter_AddRefs(nsCert));
		if (NS_FAILED(rv)) return FALSE;

		GtkTreeIter iter;
		gtk_tree_store_append (GTK_TREE_STORE (model), &iter, 
				       (i == (int)numCerts-1) ? NULL : &parent);

		nsString value;
		rv = nsCert->GetCommonName (value);
		if (NS_FAILED(rv)) return FALSE;

		NS_ConvertUTF16toUTF8 cValue (value);

		nsIX509Cert *nsCertP = nsCert;
		if (value.Length())
		{
			gtk_tree_store_set (GTK_TREE_STORE(model), &iter,
					    0, cValue.get(),
					    1, nsCertP,
					    -1);
		}
		else
		{
			char * title;
			rv = nsCert->GetWindowTitle (&title);
			if (NS_FAILED(rv)) return FALSE;
			
			gtk_tree_store_set (GTK_TREE_STORE(model),
					    &iter, 0, title, 1, nsCertP, -1);
			nsMemory::Free (title);
		}
		parent = iter;
	}
	gtk_tree_view_expand_all (GTK_TREE_VIEW (treeview));

	/* And select the last entry, and scroll the view so it's visible */
	GtkTreeSelection *select = gtk_tree_view_get_selection (treeview);
	GtkTreePath *path = gtk_tree_model_get_path (model, &parent);
	gtk_tree_selection_select_path (select, path);
	gtk_tree_view_scroll_to_cell (treeview, path, NULL, TRUE, 0.5, 0.0);
	gtk_tree_path_free (path);

	return TRUE; 
}

/**
 *  Add an ASN object to the treeview, recursing if the object was a
 *  sequence
 */
static void
add_asn1_object_to_tree(GtkTreeModel *model, nsIASN1Object *object, GtkTreeIter *parent)
{
	nsString dispNameU;
	object->GetDisplayName(dispNameU);

	GtkTreeIter iter;
	gtk_tree_store_append (GTK_TREE_STORE (model), &iter, parent);

	gtk_tree_store_set (GTK_TREE_STORE(model), &iter,
			    0, NS_ConvertUTF16toUTF8 (dispNameU).get(),
			    1, object,
			    -1);

	nsCOMPtr<nsIASN1Sequence> sequence(do_QueryInterface(object));
	if (!sequence) return;

	nsCOMPtr<nsIMutableArray> asn1Objects;
	sequence->GetASN1Objects(getter_AddRefs(asn1Objects));

	PRUint32 numObjects;
	asn1Objects->GetLength(&numObjects);
	if (!asn1Objects) return;

	for (PRUint32 i = 0; i < numObjects ; i++)
	{
		nsCOMPtr<nsIASN1Object> currObject;
		asn1Objects->QueryElementAt (i, kASN1ObjectCID,
					     getter_AddRefs (currObject));
		add_asn1_object_to_tree (model, currObject, &iter);
	}
}


/**
 *  Update the "Certificate Fields" treeview when a different cert
 *  is selected in the hierarchy text view
 */
static void
cert_chain_tree_view_selection_changed_cb (GtkTreeSelection *selection, 
					   GtkWidget* tree_view)
{
	GtkTreeIter iter;
	nsIX509Cert *nsCert;
	nsresult rv;
	GtkTreeModel * model;
		
	if (gtk_tree_selection_get_selected (selection, &model, &iter))
	{
		gtk_tree_model_get (model, &iter, 1, &nsCert, -1);

		nsCOMPtr<nsIASN1Object> object;
		rv = nsCert->GetASN1Structure (getter_AddRefs(object));
		if (NS_FAILED(rv)) return;

		model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));
		gtk_tree_store_clear (GTK_TREE_STORE (model));
		add_asn1_object_to_tree (model, object, NULL);

		gtk_tree_view_expand_all (GTK_TREE_VIEW (tree_view));
	}
}

/**
 *  When the "Certificate Field" treeview is changed, update the 
 *  text_view to display the value of the currently selected field
 */
static void
field_tree_view_selection_changed_cb (GtkTreeSelection *selection, 
				      GtkWidget* text_view)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTextBuffer * text_buffer = 
		gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));

	if (gtk_tree_selection_get_selected (selection, &model, &iter))
	{
		nsIASN1Object *object;

		gtk_tree_model_get (model, &iter, 1, &object, -1);

		nsString dispValU;
		object->GetDisplayValue(dispValU);

		gtk_text_buffer_set_text (text_buffer, NS_ConvertUTF16toUTF8 (dispValU).get(), -1);
	}
	else
	{
		gtk_text_buffer_set_text (text_buffer, "", 0);	
	}
}

/**
 *  Setup the various treeviews, the textview, and fill the treeviews
 */
static gboolean
setup_view_cert_tree (GtkWidget *dialog, GladeXML*gxml, nsIArray *certChain)
{
	GtkCellRenderer *renderer;
	GtkWidget *chain_tree_view, *field_tree_view, *text_view;
	PangoFontDescription *monospace_font_desc;
	GConfClient *conf_client;
	char *monospace_font;

	chain_tree_view = glade_xml_get_widget (gxml, "treeview_cert_chain");
	field_tree_view = glade_xml_get_widget (gxml, "treeview_cert_info");
	text_view	= glade_xml_get_widget (gxml, "textview_field_value");

	/* Setup the certificate chain view */
	GtkTreeStore *store = gtk_tree_store_new (2, 
						  G_TYPE_STRING,
						  G_TYPE_POINTER);
	gtk_tree_view_set_model (GTK_TREE_VIEW (chain_tree_view), GTK_TREE_MODEL (store));
	g_object_unref (store);


	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (chain_tree_view),
						     0, "Certificate",
						     renderer,
						     "text", 0,
						     (char *) NULL);

	GtkTreeSelection *select = gtk_tree_view_get_selection (GTK_TREE_VIEW (chain_tree_view));
	gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);

	g_signal_connect (G_OBJECT (select), "changed",
			  G_CALLBACK (cert_chain_tree_view_selection_changed_cb),
			  field_tree_view);

	/* Setup the certificate field view */
	store = gtk_tree_store_new (2, 
				    G_TYPE_STRING,
				    G_TYPE_POINTER);
	gtk_tree_view_set_model (GTK_TREE_VIEW (field_tree_view), GTK_TREE_MODEL (store));
	g_object_unref (store);


	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (field_tree_view),
						     0, "Certificate Field",
						     renderer,
						     "text", 0,
						     (char *) NULL);

	select = gtk_tree_view_get_selection (GTK_TREE_VIEW (field_tree_view));
	gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);

	g_signal_connect (G_OBJECT (select), "changed",
			  G_CALLBACK (field_tree_view_selection_changed_cb),
			  text_view);

	/* Get the text_view displaying a propertional font
	 *
	 * Pick up the monospace font from desktop preferences */
	conf_client = gconf_client_get_default ();
	monospace_font = gconf_client_get_string (conf_client, 
			     "/desktop/gnome/interface/monospace_font_name", NULL);
	if (monospace_font)
	{
		monospace_font_desc = pango_font_description_from_string (monospace_font);
		gtk_widget_modify_font (text_view, monospace_font_desc);
		pango_font_description_free (monospace_font_desc);
	}
	g_object_unref (conf_client);	   
	
	/* And fill the certificate chain tree */
	return fill_cert_chain_tree (GTK_TREE_VIEW (chain_tree_view), certChain);
}

/* void viewCert (in nsIX509Cert cert); */
NS_IMETHODIMP 
GtkNSSDialogs::ViewCert(nsIInterfaceRequestor *ctx, 
		       nsIX509Cert *cert)
{
	GtkWidget *dialog, *widget;
	GladeXML *gxml;
	nsString value;
	PRUint32 verifystate, count;
	PRUnichar ** usage;
	GtkSizeGroup * sizegroup;

	nsresult rv;
	AutoJSContextStack stack;
	rv = stack.Init ();
	if (NS_FAILED (rv)) return rv;

	gxml = glade_xml_new (ephy_file ("certificate-dialogs.glade"),
			      "viewcert_dialog", NULL);
	g_return_val_if_fail (gxml != NULL, NS_ERROR_FAILURE);

	dialog = glade_xml_get_widget (gxml, "viewcert_dialog");
	g_return_val_if_fail (dialog != NULL, NS_ERROR_FAILURE);

	nsCOMPtr<nsIDOMWindow> parent (do_GetInterface (ctx));
	GtkWindow *gparent = GTK_WINDOW (EphyUtils::FindGtkParent (parent));

	AutoWindowModalState modalState (parent);

	if (gparent)
	{
		gtk_window_set_transient_for (GTK_WINDOW(dialog), GTK_WINDOW(gparent));
		gtk_window_group_add_window (ephy_gui_ensure_window_group (GTK_WINDOW (gparent)),
					     GTK_WINDOW (dialog));
		gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
	}

	gtk_window_set_icon_name (GTK_WINDOW (dialog), EPHY_STOCK_EPHY);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Certificate Properties"));
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CLOSE);

	/* Set up the GtkSizeGroup so that the columns line up */
	sizegroup = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	widget = glade_xml_get_widget (gxml, "label_size1");
	gtk_size_group_add_widget (sizegroup, widget);
	widget = glade_xml_get_widget (gxml, "label_size2");
	gtk_size_group_add_widget (sizegroup, widget);
	widget = glade_xml_get_widget (gxml, "label_size3");
	gtk_size_group_add_widget (sizegroup, widget);
	widget = glade_xml_get_widget (gxml, "label_size4");
	gtk_size_group_add_widget (sizegroup, widget);
	g_object_unref (sizegroup);

	rv = cert->GetUsagesArray (FALSE, &verifystate, &count, &usage);
	if (NS_FAILED(rv)) return rv;

	const char * text;
	switch (verifystate)
	{
	case nsIX509Cert::VERIFIED_OK:
		text = _("This certificate has been verified for the following uses:");
		break;
	case nsIX509Cert::CERT_REVOKED:
		text = _("Could not verify this certificate because it has been revoked.");
		break;
	case nsIX509Cert::CERT_EXPIRED:
		text = _("Could not verify this certificate because it has expired.");
		break;
	case nsIX509Cert::CERT_NOT_TRUSTED:
		text = _("Could not verify this certificate because it is not trusted.");
		break;
	case nsIX509Cert::ISSUER_NOT_TRUSTED:
		text = _("Could not verify this certificate because the issuer is not trusted.");
		break;
	case nsIX509Cert::ISSUER_UNKNOWN:
		text = _("Could not verify this certificate because the issuer is unknown.");
		break;
	case nsIX509Cert::INVALID_CA:
		text = _("Could not verify this certificate because the CA certificate is invalid.");
		break;
	case nsIX509Cert::NOT_VERIFIED_UNKNOWN:
	case nsIX509Cert::USAGE_NOT_ALLOWED:
	default:
		text = _("Could not verify this certificate for unknown reasons.");
	}
	
	char *vmsg = g_strdup_printf ("<b>%s</b>", text);
	widget = glade_xml_get_widget (gxml, "label_verify_text");
	g_return_val_if_fail (GTK_IS_LABEL (widget), NS_ERROR_FAILURE);
	gtk_label_set_markup (GTK_LABEL (widget), vmsg);
	g_free (vmsg);

	if (count > 0)
	{
		GtkWidget *vbox = gtk_vbox_new (FALSE, 3);
		GtkWidget *indent;
		for (PRUint32 i = 0 ; i < count ; i++)
		{
			GtkWidget *label = gtk_label_new (NS_ConvertUTF16toUTF8 (usage[i]).get());
			gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
			gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
			nsMemory::Free (usage[i]);
		}
		nsMemory::Free (usage);
		indent = higgy_indent_widget (vbox);
		widget = glade_xml_get_widget (gxml, "vbox_validity");
		g_return_val_if_fail (GTK_IS_BOX (widget), NS_ERROR_FAILURE);
	
		gtk_box_pack_start (GTK_BOX (widget), indent, FALSE, FALSE, 0);
	}

	cert->GetCommonName (value);
	set_label_cert_attribute (gxml, "label_cn", value);

	cert->GetOrganization (value);
	set_label_cert_attribute (gxml, "label_o", value);

	cert->GetOrganizationalUnit (value);
	set_label_cert_attribute (gxml, "label_ou", value);

	cert->GetSerialNumber (value);
	set_label_cert_attribute (gxml, "label_serial", value);

	rv = cert->GetIssuerCommonName (value);
	if (NS_FAILED(rv)) return rv;
	set_label_cert_attribute (gxml, "label_issuer_cn", value);

	cert->GetIssuerOrganization (value);
	set_label_cert_attribute (gxml, "label_issuer_o", value);

	cert->GetIssuerOrganizationUnit (value);
	set_label_cert_attribute (gxml, "label_issuer_ou", value);

	nsCOMPtr<nsIX509CertValidity> validity;
	rv = cert->GetValidity (getter_AddRefs(validity));
	if (NS_FAILED(rv)) return rv;
	
	rv = validity->GetNotAfterLocalDay (value);
	if (NS_FAILED(rv)) return rv;
	set_label_cert_attribute (gxml, "label_notafter", value);
	
	rv = validity->GetNotBeforeLocalDay (value);
	if (NS_FAILED(rv)) return rv;
	set_label_cert_attribute (gxml, "label_notbefore", value);

	cert->GetSha1Fingerprint (value);
	set_label_cert_attribute (gxml, "label_sha_print", value);

	cert->GetMd5Fingerprint (value);
	set_label_cert_attribute (gxml, "label_md5_print", value);

	/* Hold a reference to each certificate in the chain while the
	 * dialog is displayed, this holds the reference for the ASN
	 * objects as well */

	nsCOMPtr<nsIArray> certChain;
	rv = cert->GetChain (getter_AddRefs(certChain));
	if (NS_FAILED(rv)) return rv;

	gboolean ret = setup_view_cert_tree (dialog, gxml, certChain);
	if (ret == FALSE) return NS_ERROR_FAILURE;

	g_object_unref (gxml);

	gtk_widget_show_all (dialog);

	int res;
	while (1)
	{
		res = gtk_dialog_run (GTK_DIALOG (dialog));
		if (res == GTK_RESPONSE_HELP)
		{
			ephy_gui_help (GTK_WINDOW (dialog), "epiphany", "using-certificate-viewer");
			continue;
		}  
		break;
	}

	gtk_widget_destroy (dialog);
	return NS_OK;
}

/* nsITokenPasswordDialogs */

/* NOTE: This interface totally sucks, see https://bugzilla.mozilla.org/show_bug.cgi?id=306993 */

/* void setPassword (in nsIInterfaceRequestor ctx, in wstring tokenName, out boolean canceled); */
NS_IMETHODIMP
GtkNSSDialogs::SetPassword(nsIInterfaceRequestor *aCtx,
			   const PRUnichar *aTokenName,
			   PRBool *aCancelled)
{
	NS_ENSURE_ARG_POINTER(aCancelled);

	nsresult rv;
	nsCOMPtr<nsIPK11Token> token;
	nsCOMPtr<nsIPKCS11Slot> slot;
	rv = GetTokenAndSlotFromName (aTokenName, getter_AddRefs (token),
				      getter_AddRefs (slot));
	NS_ENSURE_SUCCESS (rv, rv);
	NS_ENSURE_TRUE (token && slot, NS_ERROR_FAILURE);

	AutoJSContextStack stack;
	rv = stack.Init ();
	if (NS_FAILED (rv)) return rv;

	PRUint32 status = nsIPKCS11Slot::SLOT_UNINITIALIZED;
	slot->GetStatus (&status);

	nsCOMPtr<nsIDOMWindow> parent (do_GetInterface (aCtx));
	GtkWidget *gparent = EphyUtils::FindGtkParent (parent);

	AutoWindowModalState modalState (parent);

	EphyPasswordDialogFlags flags =
		EphyPasswordDialogFlags (EPHY_PASSWORD_DIALOG_FLAGS_SHOW_NEW_PASSWORD |
					 EPHY_PASSWORD_DIALOG_FLAGS_SHOW_QUALITY_METER);
	if (status != nsIPKCS11Slot::SLOT_UNINITIALIZED)
		flags = EphyPasswordDialogFlags (flags | EPHY_PASSWORD_DIALOG_FLAGS_SHOW_PASSWORD);

	GtkWidget *dialog = ephy_password_dialog_new
				(gparent,
				 _("Change Token Password"),
				 flags);
	EphyPasswordDialog *password_dialog = EPHY_PASSWORD_DIALOG (dialog);

	char *message;
	if (status == nsIPKCS11Slot::SLOT_UNINITIALIZED) {
		message = g_markup_printf_escaped (_("Choose a password for the “%s” token"),
						   NS_ConvertUTF16toUTF8 (aTokenName).get ());
	} else {
		message = g_markup_printf_escaped (_("Change the password for the “%s” token"),
						   NS_ConvertUTF16toUTF8 (aTokenName).get ());
	}

	gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog),
				       message);
	g_free (message);

	int response;
	nsString oldPassword;
	PRBool pwdOk, needsLogin;
	do {
		response = gtk_dialog_run (GTK_DIALOG (dialog));

		if (status != nsIPKCS11Slot::SLOT_UNINITIALIZED)
		{
			const char *pwd = ephy_password_dialog_get_password (password_dialog);
			oldPassword = NS_ConvertUTF8toUTF16 (pwd);
		}
	} while (response == GTK_RESPONSE_OK &&
		 status != nsIPKCS11Slot::SLOT_UNINITIALIZED &&
		 NS_SUCCEEDED (token->NeedsLogin (&needsLogin)) && needsLogin &&
		 NS_SUCCEEDED (token->CheckPassword (oldPassword.get (), &pwdOk) &&
		 !pwdOk));

	if (response == GTK_RESPONSE_ACCEPT)
	{
		const char *pwd = ephy_password_dialog_get_new_password (password_dialog);
			
		NS_ConvertUTF8toUTF16 newPassword (pwd);

		if (status == nsIPKCS11Slot::SLOT_UNINITIALIZED)
		{
			rv = token->InitPassword (newPassword.get ());
		}
		else
		{
			rv = token->ChangePassword (oldPassword.get (),
						    newPassword.get ());
		}
	}
	else
	{
		rv = NS_OK;
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));

	*aCancelled = response != GTK_RESPONSE_ACCEPT;

	return rv;
}

/* void getPassword (in nsIInterfaceRequestor ctx, in wstring tokenName, out wstring password, out boolean canceled); */
NS_IMETHODIMP
GtkNSSDialogs::GetPassword(nsIInterfaceRequestor *aCtx,
			   const PRUnichar *aTokenName,
			   PRUnichar **aPassword,
			   PRBool *aCancelled)
{
	NS_ENSURE_ARG_POINTER(aCancelled);

	nsresult rv;
	nsCOMPtr<nsIPK11Token> token;
	nsCOMPtr<nsIPKCS11Slot> slot;
	rv = GetTokenAndSlotFromName (aTokenName, getter_AddRefs (token),
				      getter_AddRefs (slot));
	NS_ENSURE_SUCCESS (rv, rv);
	NS_ENSURE_TRUE (token && slot, NS_ERROR_FAILURE);

	AutoJSContextStack stack;
	rv = stack.Init ();
	if (NS_FAILED (rv)) return rv;

	nsCOMPtr<nsIDOMWindow> parent (do_GetInterface (aCtx));
	GtkWidget *gparent = EphyUtils::FindGtkParent (parent);

	AutoWindowModalState modalState (parent);

	EphyPasswordDialogFlags flags =
		EphyPasswordDialogFlags (EPHY_PASSWORD_DIALOG_FLAGS_SHOW_PASSWORD);

	GtkWidget *dialog = ephy_password_dialog_new
				(gparent,
				 _("Get Token Password"), /* FIXME */
				 flags);
	EphyPasswordDialog *password_dialog = EPHY_PASSWORD_DIALOG (dialog);

	/* Translators: A "token" is something that enables the user to authenticate himself or
         * prove his credentials. This can be either a hardware device (e.g. a smart-card), or
         * a data file (e.g. a cryptographic certificate).
         */
	char *message = g_markup_printf_escaped (_("Please enter the password for the “%s” token"),
						 NS_ConvertUTF16toUTF8 (aTokenName).get ());
	gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog),
				       message);
	g_free (message);

	int response = gtk_dialog_run (GTK_DIALOG (dialog));

	if (response == GTK_RESPONSE_ACCEPT)
	{
		const char *pwd = ephy_password_dialog_get_password (password_dialog);
		*aPassword = NS_StringCloneData (NS_ConvertUTF8toUTF16 (pwd));
	}
	
	gtk_widget_destroy (GTK_WIDGET (dialog));

	*aCancelled = response != GTK_RESPONSE_ACCEPT;

	return NS_OK;
}

/* nsITokenDialogs */

static void
SelectionChangedCallback (GtkComboBox *combo,
			  GtkDialog *dialog)
{
  int active = gtk_combo_box_get_active (combo);
  gtk_dialog_set_response_sensitive (dialog, GTK_RESPONSE_ACCEPT, active >= 0);
}

/* void ChooseToken (in nsIInterfaceRequestor ctx,
		     [array, size_is (count)] in wstring tokenNameList,
		     in unsigned long count,
		     out wstring tokenName,
		     out boolean canceled); */
NS_IMETHODIMP
GtkNSSDialogs::ChooseToken (nsIInterfaceRequestor *aContext,
			    const PRUnichar **tokenNameList,
			    PRUint32 count,
			    PRUnichar **_tokenName,
			    PRBool *_cancelled)
{
  NS_ENSURE_ARG (tokenNameList);
  NS_ENSURE_ARG (count);

  nsresult rv;
  AutoJSContextStack stack;
  rv = stack.Init ();
  if (NS_FAILED (rv)) return rv;

  /* Didn't you know it? MOZILLA SUCKS! ChooseToken is always called with |aContext| == NULL! See
   * http://bonsai.mozilla.org/cvsblame.cgi?file=mozilla/security/manager/ssl/src/nsKeygenHandler.cpp&rev=1.39&mark=346#346
   * Need to investigate if we it's always called directly from code called from JS, in which case we
   * can use EphyJSUtils::GetDOMWindowFromCallContext.
   */
  nsCOMPtr<nsIDOMWindow> parent (do_GetInterface (aContext));
  GtkWidget *gparent = EphyUtils::FindGtkParent (parent);

  AutoWindowModalState modalState (parent);

  GtkWidget *dialog = gtk_message_dialog_new
		  (GTK_WINDOW (gparent),
		   GTK_DIALOG_DESTROY_WITH_PARENT,
		   GTK_MESSAGE_OTHER,
		   GTK_BUTTONS_CANCEL,
		   _("Please select a token:"));

  gtk_window_set_icon_name (GTK_WINDOW (dialog), EPHY_STOCK_EPHY);

  GtkWidget *combo = gtk_combo_box_new_text ();
  for (PRUint32 i = 0; i < count; ++i) {
    gtk_combo_box_append_text (GTK_COMBO_BOX (combo),
			       NS_ConvertUTF16toUTF8 (tokenNameList[i]).get ());
  }
  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), -1);
  g_signal_connect (combo, "changed",
		    G_CALLBACK (SelectionChangedCallback), dialog);

  /* FIXME: View Cert button? */

  GtkWidget *vbox = GTK_MESSAGE_DIALOG (dialog)->label->parent;
  gtk_box_pack_start (GTK_BOX (vbox), combo, FALSE, FALSE, 0);

  gtk_dialog_add_button (GTK_DIALOG (dialog),
			 _("_Select"),
			 GTK_RESPONSE_ACCEPT);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_REJECT);

  int response = gtk_dialog_run (GTK_DIALOG (dialog));
  int selected = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));

  gtk_widget_destroy (dialog);
	
  *_cancelled = response != GTK_RESPONSE_ACCEPT;

  if (response == GTK_RESPONSE_ACCEPT) {
    NS_ENSURE_TRUE (selected >= 0 && selected < (int) count, NS_ERROR_FAILURE);
    *_tokenName = NS_StringCloneData (nsDependentString (tokenNameList[selected]));
  }

  return NS_OK;
}

/* nsIDOMCryptoDialogs */

/* Note: this interface sucks! See https://bugzilla.mozilla.org/show_bug.cgi?id=341914 */

/* boolean ConfirmKeyEscrow (in nsIX509Cert escrowAuthority); */
NS_IMETHODIMP
GtkNSSDialogs::ConfirmKeyEscrow (nsIX509Cert *aEscrowAuthority,
				 PRBool *_retval)
{
  NS_ENSURE_ARG (aEscrowAuthority);

  nsresult rv;
  AutoJSContextStack stack;
  rv = stack.Init ();
  if (NS_FAILED (rv)) return rv;

#if 0
  nsCOMPtr<nsIDOMWindow> parent (do_GetInterface (aCtx));
#endif
  nsCOMPtr<nsIDOMWindow> parent (EphyJSUtils::GetDOMWindowFromCallContext ());
  GtkWidget *gparent = EphyUtils::FindGtkParent (parent);

  AutoWindowModalState modalState (parent);

  /* FIXME: is that guaranteed to be non-empty? */
  nsString commonName;
  aEscrowAuthority->GetCommonName (commonName);

  GtkWidget *dialog = gtk_message_dialog_new
			(GTK_WINDOW (gparent),
			 GTK_DIALOG_DESTROY_WITH_PARENT,
			 GTK_MESSAGE_WARNING /* QUESTION really but it's also a strong warnings... */,
			 GTK_BUTTONS_NONE,
			 _("Escrow the secret key?"));

  /* FIXME: If I understand the documentation of generateCRMFRequest
   * correctly, key escrow is never used for signing keys (if it were,
   * we'd have to warn that the cert authority can forge your signature
   * too).
   */
  gtk_message_dialog_format_secondary_text
    (GTK_MESSAGE_DIALOG (dialog),
     _("The certificate authority “%s” requests that you give it a copy "
       "of the newly generated secret key.\n\n"
       "This will enable the certificate authority read any "
       "communications encrypted with this key "
       "without your knowledge or consent.\n\n"
       "It is strongly recommended not to allow it."),
     NS_ConvertUTF16toUTF8 (commonName).get ());

  gtk_window_set_icon_name (GTK_WINDOW (dialog), EPHY_STOCK_EPHY);

  GtkWidget *button = gtk_dialog_add_button (GTK_DIALOG (dialog),
					     _("_Reject"),
					     GTK_RESPONSE_REJECT);
  gtk_dialog_add_button (GTK_DIALOG (dialog),
			 _("_Allow"),
			 GTK_RESPONSE_ACCEPT);
  /* FIXME: View Cert button? */

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_REJECT);
  gtk_widget_grab_focus (button);

  int response = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
	
  *_retval = response == GTK_RESPONSE_ACCEPT;

  return NS_OK;
}
