/*
 *  GtkNSSDialogs.cpp
 *
 *  Copyright (C) 2003 Crispin Flowerday <gnome@flowerday.cx>
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

/*
 * This file provides Gtk implementations of the mozilla Certificate dialogs
 * such as the ones displayed when connecting to a site with a self-signed
 * or expired certificate.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_MOZILLA_PSM

#include "MozillaPrivate.h"

#include "nsIX509Cert.h"
#include "nsICertificateDialogs.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIX509CertValidity.h"

#include <gtk/gtkdialog.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkalignment.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkimage.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkeditable.h>
#include <gtk/gtktable.h>

#include <libgnome/gnome-i18n.h>

#include "GtkNSSDialogs.h"

#include <time.h>

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

NS_IMPL_ISUPPORTS1 (GtkNSSDialogs, nsIBadCertListener)

/**
 *  Call the mozilla service to display a certificate
 */
static void
view_certificate (nsIInterfaceRequestor *ctx, nsIX509Cert *cert)
{
	nsresult rv;
	nsCOMPtr<nsICertificateDialogs> certDialogs =
		do_GetService (NS_CERTIFICATEDIALOGS_CONTRACTID, &rv);
	g_return_if_fail (NS_SUCCEEDED (rv));

	certDialogs->ViewCert (ctx, cert);
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
        GtkWidget *dialog, *hbox, *label, *image, *checkbox;
	int res;

	nsCOMPtr<nsIDOMWindow> parent = do_GetInterface (ctx);
	GtkWidget *gparent = MozillaFindGtkParent (parent);

        g_return_val_if_fail (GTK_IS_WINDOW (gparent), GTK_RESPONSE_CANCEL);
        g_return_val_if_fail (markup_text, GTK_RESPONSE_CANCEL);
        g_return_val_if_fail (!checkbox_text || checkbox_value, GTK_RESPONSE_CANCEL);

	dialog = gtk_dialog_new_with_buttons ("",
					      GTK_WINDOW (gparent),
					      GTK_DIALOG_NO_SEPARATOR,
					      NULL);

	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 6);

	/* Add the buttons */
        gtk_dialog_add_button (GTK_DIALOG (dialog), _("_View Certificate"),
                               NSSDIALOG_RESPONSE_VIEW_CERT);

        gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CANCEL,
                               GTK_RESPONSE_CANCEL);

	if (affirmative_text == NULL)
	{
		affirmative_text = _("_Accept");
	}

        gtk_dialog_add_button (GTK_DIALOG (dialog), 
			       affirmative_text,
                               GTK_RESPONSE_ACCEPT);

        /* Create the actual widgets that go in the display part of the dialog */
        hbox = gtk_hbox_new (FALSE, 12);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 6);
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), hbox);

        image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_WARNING,
                                          GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment (GTK_MISC (image), 0.5, 0.0);
	gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);

        label = gtk_label_new (NULL);
        gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
        gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);

        if (checkbox_text)
	{
		GtkWidget *vbox = gtk_vbox_new (FALSE, 12);
		gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
		gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
		
		checkbox = gtk_check_button_new_with_mnemonic (checkbox_text);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbox), 
					      *checkbox_value);
		gtk_box_pack_start (GTK_BOX (vbox), checkbox, TRUE, TRUE, 0);
	}
	else
	{
		checkbox = 0;
		gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
	}

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


NS_IMETHODIMP
GtkNSSDialogs::ConfirmMismatchDomain (nsIInterfaceRequestor *ctx,
                                      const nsACString &targetURL,
                                      nsIX509Cert *cert, PRBool *_retval)
{
	nsAutoString commonName;
	char *ttTargetUrl, *ttCommonName, *first, *second, *msg;
	int res;

	cert->GetCommonName (commonName);

 	ttTargetUrl = g_strdup_printf ("\"<tt>%s</tt>\"", 
                                        PromiseFlatCString(targetURL).get());

	ttCommonName = g_strdup_printf ("\"<tt>%s</tt>\"", 
                                         NS_ConvertUCS2toUTF8(commonName).get());

        first = g_strdup_printf (_("The site %s returned security information for "
				   "%s. It is possible that someone is intercepting "
				   "your communication to obtain your confidential "
				   "information."),
				 ttTargetUrl, ttCommonName);

        second = g_strdup_printf (_("You should only accept the security information if you "
                                    "trust %s and %s."),
                                  ttTargetUrl, ttCommonName);

	msg = g_strdup_printf ("<span weight=\"bold\" size=\"larger\">%s</span>\n\n%s\n\n%s",
			       _("Accept incorrect security information?"),
			       first, second);

        res = display_cert_warning_box (ctx, cert, msg, NULL, NULL, NULL);

	g_free (ttTargetUrl);
	g_free (ttCommonName);
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
	nsAutoString commonName;
	char *ttCommonName, *secondary, *tertiary, *msg;
	int res;

	cert->GetCommonName (commonName);

	ttCommonName = g_strdup_printf ("\"<tt>%s</tt>\"", 
					NS_ConvertUCS2toUTF8(commonName).get());

	secondary = g_strdup_printf
		           (_("Your browser was unable to trust %s. "
			      "It is possible that someone is intercepting your "
			      "communication to obtain your confidential information."),
			      ttCommonName);

        tertiary = g_strdup_printf
		           (_("You should only connect to the site if you are certain "
			      "you are connected to %s."),
			    ttCommonName);

	msg = g_strdup_printf ("<span weight=\"bold\" size=\"larger\">%s</span>\n\n%s\n\n%s",
			       _("Connect to untrusted site?"),
			       secondary, tertiary);

	res = display_cert_warning_box (ctx, cert, msg, 
					_("_Don't show this message again for this site"),
					&accept_perm, _("Co_nnect"));
	g_free (ttCommonName);
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
	nsAutoString commonName;
	time_t t;
	struct tm tm;
	char formattedDate[32];
	const char *primary, *text;
	char *ttCommonName, *secondary, *msg;

	*_retval = PR_FALSE;
	
	nsCOMPtr<nsIX509CertValidity> validity;
	rv = cert->GetValidity (getter_AddRefs(validity));
	if (NS_FAILED(rv)) return rv;
	
	rv = validity->GetNotAfter (&notAfter);
	if (NS_FAILED(rv)) return rv;
	
	rv = validity->GetNotBefore (&notBefore);
	if (NS_FAILED(rv)) return rv;
	
	if (LL_CMP(now, >, notAfter)) {
		primary = _("Accept expired security information?");
		text    = _("The security information for %s "
			    "expired on %s.");
		timeToUse = notAfter;
	} else {
		primary = _("Accept not yet valid security information?");
		text    = _("The security information for %s isn't valid until %s.");
		timeToUse = notBefore;
	}
	
	cert->GetCommonName (commonName);

	LL_DIV (normalizedTime, timeToUse, PR_USEC_PER_SEC);
	LL_L2UI (t, normalizedTime);
	strftime (formattedDate, sizeof(formattedDate), _("%a %-d %b %Y"), 
		  localtime_r (&t, &tm));

	ttCommonName = g_strdup_printf ("\"<tt>%s</tt>\"", 
                                        NS_ConvertUCS2toUTF8(commonName).get());

	secondary = g_strdup_printf (text, ttCommonName, formattedDate);

	msg = g_strdup_printf ("<span weight=\"bold\" size=\"larger\">%s</span>\n\n%s\n\n%s",
			       primary, secondary, 
			       _("You should ensure that your computer's time is correct."));

        int res = display_cert_warning_box (ctx, cert, msg, NULL, NULL, NULL);

	g_free (msg);
	g_free (secondary);
	g_free (ttCommonName);

	*_retval = (res == GTK_RESPONSE_ACCEPT);
	
	return NS_OK;
}

/* void notifyCrlNextupdate (in nsIInterfaceRequestor socketInfo, 
   in AUTF8String targetURL, in nsIX509Cert cert); */
NS_IMETHODIMP 
GtkNSSDialogs::NotifyCrlNextupdate (nsIInterfaceRequestor *ctx,
                                    const nsACString & targetURL, nsIX509Cert *cert)
{
	GtkWidget *dialog, *image, *hbox, *label;
        char *ttCommonName, *ttTargetUrl, *primary, *secondary, *msg;
        nsAutoString commonName;

	nsCOMPtr<nsIDOMWindow> parent = do_GetInterface (ctx);
	GtkWidget *gparent = MozillaFindGtkParent (parent);

	dialog = gtk_dialog_new_with_buttons ("",
					      GTK_WINDOW (gparent),
					      GTK_DIALOG_NO_SEPARATOR,
					      GTK_STOCK_OK,
					      GTK_RESPONSE_OK,
					      NULL);

	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 6);

        hbox = gtk_hbox_new (FALSE, 12);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 6);
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), hbox);

        image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_ERROR, 
                                          GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment (GTK_MISC (image), 0.5, 0);
	gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);

        label = gtk_label_new (NULL);
        gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
        gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (image), 0.0, 0.0);
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);

	cert->GetCommonName (commonName);

	ttCommonName = g_strdup_printf ("\"<tt>%s</tt>\"", 
                                        NS_ConvertUCS2toUTF8(commonName).get());

	ttTargetUrl = g_strdup_printf ("\"<tt>%s</tt>\"", 
                                       PromiseFlatCString(targetURL).get());

	primary = g_strdup_printf (_("Cannot establish connection to %s"),
                                   ttTargetUrl);

	secondary = g_strdup_printf (_("The certificate revocation list (CRL) from %s "
                                       "needs to be updated."),
                                     ttCommonName);
	msg = g_strdup_printf ("<span weight=\"bold\" size=\"larger\">%s</span>\n\n%s\n\n%s",
                               primary, secondary,
                               _("Please ask your system administrator for assistance."));
	
	gtk_label_set_markup (GTK_LABEL (label), msg);
	
	g_free (msg);
	g_free (primary);
	g_free (secondary);
	g_free (ttCommonName);
	g_free (ttTargetUrl);

	gtk_widget_show_all (dialog);

	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
	return NS_OK;
}

#endif
