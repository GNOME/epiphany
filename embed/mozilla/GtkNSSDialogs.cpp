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

#include "EphyUtils.h"
#include "AutoJSContextStack.h"

#include <nsCOMPtr.h>
#include <nsMemory.h>
#include <nsIServiceManager.h>
#include <nsIInterfaceRequestor.h>
#include <nsIInterfaceRequestorUtils.h>
#include <nsIX509Cert.h>
#include <nsIX509CertValidity.h>
#include <nsIX509CertDB.h>
#include <nsIASN1Object.h>
#include <nsIASN1Sequence.h>
#include <nsICRLInfo.h>
#include <nsISimpleEnumerator.h>
#include <nsIArray.h>
#include <nsIDOMWindow.h>
#undef MOZILLA_INTERNAL_API
#include <nsEmbedString.h>
#define MOZILLA_INTERNAL_API 1

#include <gconf/gconf-client.h>
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
#include <gtk/gtktreestore.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtktextbuffer.h>
#include <gtk/gtktextview.h>
#include <gtk/gtkprogressbar.h>
#include <glib/gi18n.h>
#include <time.h>

#include "GtkNSSDialogs.h"
#include "ephy-file-helpers.h"
#include "ephy-glade.h"
#include "ephy-gui.h"

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

NS_IMPL_ISUPPORTS2 (GtkNSSDialogs, 
		    nsICertificateDialogs,
		    nsIBadCertListener)

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

	label = gtk_label_new ("");
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
	gtk_box_set_spacing (GTK_BOX(dialog->action_area), 6);	
	gtk_box_set_spacing (GTK_BOX(dialog->vbox), 12);

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

	nsresult rv;
	AutoJSContextStack stack;
	rv = stack.Init ();
	if (NS_FAILED (rv)) return rv;

	/* NOTE: Due to a mozilla bug [https://bugzilla.mozilla.org/show_bug.cgi?id=306288],
	 * we will always end up without a parent!
	 */
	nsCOMPtr<nsIDOMWindow> parent = do_GetInterface (ctx);
	GtkWindow *gparent = GTK_WINDOW (EphyUtils::FindGtkParent (parent));

        g_return_val_if_fail (markup_text, GTK_RESPONSE_CANCEL);
        g_return_val_if_fail (!checkbox_text || checkbox_value, GTK_RESPONSE_CANCEL);
	
	dialog = gtk_dialog_new_with_buttons ("", gparent,
					      GTK_DIALOG_DESTROY_WITH_PARENT,
					      NULL);
	if (gparent)
	{
		gtk_window_group_add_window (ephy_gui_ensure_window_group (gparent),
					     GTK_WINDOW (dialog));
	}

	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), "web-browser");

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


NS_IMETHODIMP
GtkNSSDialogs::ConfirmMismatchDomain (nsIInterfaceRequestor *ctx,
                                      const nsACString &targetURL,
                                      nsIX509Cert *cert, PRBool *_retval)
{
	char *first, *second, *msg;
	int res;

	nsEmbedString commonName;
	cert->GetCommonName (commonName);

	nsEmbedCString cCommonName;
	NS_UTF16ToCString (commonName,
			   NS_CSTRING_ENCODING_UTF8, cCommonName);

	nsEmbedCString cTargetUrl (targetURL);

        first = g_markup_printf_escaped (_("The site \"%s\" returned security information for "
					   "\"%s\". It is possible that someone is intercepting "
					   "your communication to obtain your confidential "
					   "information."),
					 cTargetUrl.get(), cCommonName.get());

        second = g_markup_printf_escaped (_("You should only accept the security information if you "
					    "trust \"%s\" and \"%s\"."),
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

	nsEmbedString commonName;
	cert->GetCommonName (commonName);

	nsEmbedCString cCommonName;
	NS_UTF16ToCString (commonName,
			   NS_CSTRING_ENCODING_UTF8, cCommonName);

	secondary = g_markup_printf_escaped
		           (_("It was not possible to automatically trust \"%s\". "
			      "It is possible that someone is intercepting your "
			      "communication to obtain your confidential information."),
			      cCommonName.get());

        tertiary = g_markup_printf_escaped
		           (_("You should only connect to the site if you are certain "
			      "you are connected to \"%s\"."),
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
		text    = _("The security information for \"%s\" "
			    "expired on %s.");
		timeToUse = notAfter;
	} 
	else
	{
		primary = _("Accept not yet valid security information?");
		text    = _("The security information for \"%s\" isn't valid until %s.");
		timeToUse = notBefore;
	}
	
	nsEmbedString commonName;
	cert->GetCommonName (commonName);

	nsEmbedCString cCommonName;
	NS_UTF16ToCString (commonName,
			   NS_CSTRING_ENCODING_UTF8, cCommonName);

	LL_DIV (normalizedTime, timeToUse, PR_USEC_PER_SEC);
	LL_L2UI (t, normalizedTime);
	/* To translators: this a time format that is used while displaying the
	 * expiry or start date of an SSL certificate, for the format see 
	 * strftime(3) */
	strftime (formattedDate, sizeof(formattedDate), _("%a %d %b %Y"), 
		  localtime_r (&t, &tm));
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
   in AUTF8String targetURL, in nsIX509Cert cert); */
NS_IMETHODIMP 
GtkNSSDialogs::NotifyCrlNextupdate (nsIInterfaceRequestor *ctx,
				    const nsACString & targetURL, nsIX509Cert *cert)
{
	GtkWidget *dialog, *label;
	char *msg, *primary, *secondary;

	nsCOMPtr<nsIDOMWindow> parent = do_GetInterface (ctx);
	GtkWidget *gparent = EphyUtils::FindGtkParent (parent);

	dialog = gtk_dialog_new_with_buttons ("",
					      GTK_WINDOW (gparent),
					      GTK_DIALOG_DESTROY_WITH_PARENT,
					      GTK_STOCK_OK,
					      GTK_RESPONSE_OK,
					      NULL);

	gtk_window_set_icon_name (GTK_WINDOW (dialog), "web-browser");

	higgy_setup_dialog (GTK_DIALOG (dialog), GTK_STOCK_DIALOG_ERROR,
			    &label, NULL);

	nsEmbedString commonName;
	cert->GetCommonName (commonName);

	nsEmbedCString cCommonName;
	NS_UTF16ToCString (commonName,
			   NS_CSTRING_ENCODING_UTF8, cCommonName);

	nsEmbedCString cTargetUrl (targetURL);

	primary = g_markup_printf_escaped (_("Cannot establish connection to \"%s\"."),
					   cTargetUrl.get());

	secondary = g_markup_printf_escaped (_("The certificate revocation list (CRL) from \"%s\" "
					       "needs to be updated."),
					     cCommonName.get());
	msg = g_strdup_printf ("<span weight=\"bold\" size=\"larger\">%s</span>\n\n%s\n\n%s",
			       primary, secondary,
			       _("Please ask your system administrator for assistance."));
	
	gtk_label_set_markup (GTK_LABEL (label), msg);

	g_free (primary);
	g_free (secondary);
	g_free (msg);

	gtk_widget_show_all (dialog);

	g_signal_connect (G_OBJECT (dialog),
			  "response",
			  (GCallback)gtk_widget_destroy, NULL);

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

	nsCOMPtr<nsIDOMWindow> parent = do_GetInterface (ctx);
	GtkWindow *gparent = GTK_WINDOW (EphyUtils::FindGtkParent (parent));

	dialog = gtk_dialog_new_with_buttons (_("Trust new Certificate Authority?"), gparent,
					      GTK_DIALOG_DESTROY_WITH_PARENT,
					      _("_View Certificate"),
					      NSSDIALOG_RESPONSE_VIEW_CERT,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      _("_Trust CA"),	GTK_RESPONSE_ACCEPT,
					      NULL);

	if (gparent)
	{
		gtk_window_group_add_window (ephy_gui_ensure_window_group (gparent),
					     GTK_WINDOW (dialog));
	}

	gtk_window_set_icon_name (GTK_WINDOW (dialog), "web-browser");

	higgy_setup_dialog (GTK_DIALOG (dialog), GTK_STOCK_DIALOG_WARNING,
			    &label, NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);

	nsEmbedString commonName;
	cert->GetCommonName (commonName);

	nsEmbedCString cCommonName;
	NS_UTF16ToCString (commonName,
			   NS_CSTRING_ENCODING_UTF8, cCommonName);

	primary = g_markup_printf_escaped (_("Trust new Certificate Authority \"%s\" to identify web sites?"),
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
					      NULL);

	if (gparent)
	{
		gtk_window_group_add_window (ephy_gui_ensure_window_group (gparent),
					     GTK_WINDOW (dialog));
	}

	gtk_window_set_icon_name (GTK_WINDOW (dialog), "web-browser");

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

struct SetPasswordCallback
{
	GtkWidget *entry1;
	GtkWidget *entry2;
	GtkWidget *widget;
};


static void
set_password_entry_changed_cb (GtkEditable *editable, void * _data)
{
	SetPasswordCallback * data = (SetPasswordCallback*)_data;
	gchar * text1 = gtk_editable_get_chars 
		                    (GTK_EDITABLE(data->entry1), 0, -1);
	gchar * text2 = gtk_editable_get_chars 
		                    (GTK_EDITABLE(data->entry2), 0, -1);

	if (strcmp (text1, text2) == 0)
	{
		gtk_widget_set_sensitive (data->widget, TRUE);
	}
	else
	{
		gtk_widget_set_sensitive (data->widget, FALSE);
	}

	g_free (text1);
	g_free (text2);
}


/**
 *  Calculate the quality of a password. The algorithm used is taken
 *  directly from mozilla in:
 *     $MOZSRC/security/manager/pki/resources/content/password.js
 */
static void
password_quality_meter_cb (GtkEditable *editable, GtkWidget *progress)
{
	gchar * text = gtk_editable_get_chars (editable, 0, -1);

	/* Get the length */
	glong length = g_utf8_strlen (text, -1);

	/* Count the number of number, symbols and uppercase chars */
	gint uppercase = 0;
	gint symbols = 0;
	gint numbers = 0;
	for( const gchar * p = text; *p; p = g_utf8_find_next_char (p, NULL) )
	{
		gunichar uc = g_utf8_get_char(p);
		if (g_unichar_isdigit (uc))
		{
			numbers++;
		}
		else if (g_unichar_isupper (uc))
		{
			uppercase++;
		}
		else if (g_unichar_islower (uc))
		{
			/* Not counted */
		}
		else if (g_unichar_isgraph (uc))
		{
			symbols++;
		}
	}

	if (length    > 5) length = 5;
	if (numbers   > 3) numbers = 3;
	if (symbols   > 3) symbols = 3;
	if (uppercase > 3) uppercase = 3;
	
	gint strength = ((length*10)-20) + (numbers*10) + (symbols*15) + (uppercase*10);
	if (strength < 0)   strength = 0;
	if (strength > 100) strength = 100;

	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (progress), (strength/100.0));

	g_free (text);
}


NS_IMETHODIMP 
GtkNSSDialogs::SetPKCS12FilePassword(nsIInterfaceRequestor *ctx, 
				    nsAString &_password,
				    PRBool *_retval)
{
	GtkWidget *dialog, *table, *entry1, *entry2, *button, *label, *vbox;
	GtkWidget *progress;
	char *msg;

	nsresult rv;
	AutoJSContextStack stack;
	rv = stack.Init ();
	if (NS_FAILED (rv)) return rv;

	nsCOMPtr<nsIDOMWindow> parent = do_GetInterface (ctx);
	GtkWindow *gparent = GTK_WINDOW (EphyUtils::FindGtkParent (parent));

	dialog = gtk_dialog_new_with_buttons ("", gparent,
					      GTK_DIALOG_DESTROY_WITH_PARENT,
					      GTK_STOCK_CANCEL,
					      GTK_RESPONSE_CANCEL,
					      NULL);

	if (gparent)
	{
		gtk_window_group_add_window (ephy_gui_ensure_window_group (gparent),
					     GTK_WINDOW (dialog));
	}

	gtk_window_set_icon_name (GTK_WINDOW (dialog), "web-browser");

	higgy_setup_dialog (GTK_DIALOG (dialog), GTK_STOCK_DIALOG_QUESTION,
			    &label, &vbox);

	button = gtk_button_new_with_mnemonic (_("_Backup Certificate"));
	gtk_widget_show (button);
	gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_OK);
	GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	msg = g_strdup_printf ("<span weight=\"bold\" size=\"larger\">%s</span>\n\n%s",
			       _("Select password."),
			       _("Select a password to protect this certificate."));
	gtk_label_set_markup (GTK_LABEL (label), msg);
	g_free (msg);

	table = gtk_table_new (3, 3, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (table), 6);
	gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);

	label = gtk_label_new (NULL);
	entry1 = gtk_entry_new ();
	entry2 = gtk_entry_new ();
	gtk_label_set_markup_with_mnemonic (GTK_LABEL (label), _("_Password:"));
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry1);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_entry_set_visibility (GTK_ENTRY (entry1), FALSE);
	g_signal_connect_swapped (entry1, "activate",
				  (GCallback)gtk_widget_grab_focus,
				  entry2);

	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1, 
			  GTK_FILL, GTK_FILL, 0, 0 );
	gtk_table_attach (GTK_TABLE (table), entry1, 1, 2, 0, 1,
			  GTK_FILL, GTK_FILL, 0, 0 );

	label = gtk_label_new (NULL);
	gtk_label_set_markup_with_mnemonic (GTK_LABEL (label), _("Con_firm password:"));
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry2);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_entry_set_visibility (GTK_ENTRY (entry2), FALSE);
	gtk_entry_set_activates_default (GTK_ENTRY (entry2), TRUE);

	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2, 
			  GTK_FILL, GTK_FILL, 0, 0 );
	gtk_table_attach (GTK_TABLE (table), entry2, 1, 2, 1, 2,
			  GTK_FILL, GTK_FILL, 0, 0 );

	/* TODO: We need a better password quality meter */
	label = gtk_label_new (_("Password quality:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	progress = gtk_progress_bar_new ();
	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (progress), 0.0);

	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3, 
			  GTK_FILL, GTK_FILL, 0, 0 );
	gtk_table_attach (GTK_TABLE (table), progress, 1, 2, 2, 3,
			  GTK_FILL, GTK_FILL, 0, 0 );

	SetPasswordCallback callback_data = { entry1, entry2, button };
	g_signal_connect (entry1, "changed",
			  (GCallback)set_password_entry_changed_cb,
			  &callback_data);

	g_signal_connect (entry1, "changed",
			  (GCallback)password_quality_meter_cb,
			  progress);

	g_signal_connect (entry2, "changed",
			  (GCallback)set_password_entry_changed_cb,
			  &callback_data);


	gtk_widget_show_all (dialog);
	int ret = gtk_dialog_run (GTK_DIALOG (dialog));

	if (ret != GTK_RESPONSE_OK)
	{
		*_retval = PR_FALSE;
	}
	else
	{
		gchar *text = gtk_editable_get_chars (GTK_EDITABLE (entry1), 0, -1);
		NS_CStringToUTF16 (nsEmbedCString (text),
			           NS_CSTRING_ENCODING_UTF8, _password);
		g_free (text);
		*_retval = PR_TRUE;
	}
	gtk_widget_destroy (dialog);
	return NS_OK;
}

NS_IMETHODIMP 
GtkNSSDialogs::GetPKCS12FilePassword(nsIInterfaceRequestor *ctx, 
				     nsAString &_password,
				     PRBool *_retval)
{
	GtkWidget *dialog, *hbox, *label, *entry, *vbox;
	char *msg;

	nsresult rv;
	AutoJSContextStack stack;
	rv = stack.Init ();
	if (NS_FAILED (rv)) return rv;

	nsCOMPtr<nsIDOMWindow> parent = do_GetInterface (ctx);
	GtkWindow *gparent = GTK_WINDOW (EphyUtils::FindGtkParent (parent));

	dialog = gtk_dialog_new_with_buttons ("", gparent,
					      GTK_DIALOG_DESTROY_WITH_PARENT,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      _("I_mport Certificate"), GTK_RESPONSE_OK,
					      NULL);

	if (gparent)
	{
		gtk_window_group_add_window (ephy_gui_ensure_window_group (gparent),
					     GTK_WINDOW (dialog));
	}

	gtk_window_set_icon_name (GTK_WINDOW (dialog), "web-browser");

	higgy_setup_dialog (GTK_DIALOG (dialog), GTK_STOCK_DIALOG_QUESTION,
			    &label, &vbox);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	msg = g_strdup_printf ("<span weight=\"bold\" size=\"larger\">%s</span>\n\n%s",
			       _("Password required."),
			       _("Enter the password for this certificate."));
	gtk_label_set_markup (GTK_LABEL (label), msg);
	g_free (msg);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	
	label = gtk_label_new (NULL);
	entry = gtk_entry_new ();

	gtk_label_set_markup_with_mnemonic (GTK_LABEL (label), _("_Password:"));
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
	gtk_entry_set_visibility (GTK_ENTRY (entry), FALSE);
	gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);

	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), entry, FALSE, FALSE, 0);

	gtk_widget_show_all (dialog);
	int ret = gtk_dialog_run (GTK_DIALOG (dialog));

	if (ret != GTK_RESPONSE_OK)
	{
		*_retval = PR_FALSE;
	}
	else
	{
		gchar * text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
		NS_CStringToUTF16 (nsEmbedCString (text),
			           NS_CSTRING_ENCODING_UTF8, _password);
		g_free (text);
		*_retval = PR_TRUE;
	}
	gtk_widget_destroy (dialog);

	return NS_OK;
}


static void
set_table_row (GtkWidget *table, int row, const char *title, GtkWidget *label)
{
	GtkWidget *header;
	char buf[64];

	g_snprintf (buf, sizeof(buf), "<b>%s</b>", title);
	header = gtk_label_new (buf);
	gtk_label_set_use_markup (GTK_LABEL(header), TRUE);
	gtk_misc_set_alignment (GTK_MISC(header), 0, 0);
	gtk_widget_show (header);
	gtk_table_attach (GTK_TABLE (table), header, 0, 1, row, row+1,
			  GTK_FILL, GTK_FILL, 0, 0);
	
	gtk_misc_set_alignment (GTK_MISC(label), 0, 0);
	gtk_widget_show (label);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 1, 2, row, row+1);
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
					      NULL);

	gtk_window_set_icon_name (GTK_WINDOW (dialog), "web-browser");

	higgy_setup_dialog (GTK_DIALOG (dialog), GTK_STOCK_DIALOG_INFO,
			    &label, &vbox);

	msg = g_strdup_printf ("<span weight=\"bold\" size=\"larger\">%s</span>\n\n%s",
			       _("Certificate Revocation list successfully imported."),
			       _("Certificate Revocation list (CRL) imported:"));
	gtk_label_set_markup (GTK_LABEL (label), msg);
	g_free (msg);

	table = gtk_table_new (2, 3, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (table), 6);

	nsEmbedString org, orgUnit, nextUpdate;
	rv = crl->GetOrganization (org);
	if (NS_FAILED(rv)) return rv;

	rv = crl->GetOrganizationalUnit (orgUnit);
	if (NS_FAILED(rv)) return rv;

	rv = crl->GetNextUpdateLocale (nextUpdate);
	if (NS_FAILED(rv)) return rv;

	nsEmbedCString cOrg;
	NS_UTF16ToCString (org, NS_CSTRING_ENCODING_UTF8, cOrg);
	label = gtk_label_new (cOrg.get());
	set_table_row (table, 0, _("Organization:"), label);

	nsEmbedCString cOrgUnit;
	NS_UTF16ToCString (orgUnit, NS_CSTRING_ENCODING_UTF8, cOrgUnit);
	label = gtk_label_new (cOrgUnit.get());
	set_table_row (table, 1, _("Unit:"), label);

	nsEmbedCString cNextUpdate;
	NS_UTF16ToCString (nextUpdate, NS_CSTRING_ENCODING_UTF8, cNextUpdate);
	label = gtk_label_new (cNextUpdate.get());
	set_table_row (table, 2, _("Next Update:"), label);

	gtk_box_pack_start (GTK_BOX (vbox), higgy_indent_widget (table), FALSE, FALSE, 0);

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
		nsEmbedCString cValue;
		NS_UTF16ToCString (value, NS_CSTRING_ENCODING_UTF8, cValue);
		gtk_label_set_use_markup (GTK_LABEL (label), FALSE);
		gtk_label_set_text (GTK_LABEL (label), cValue.get());
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

		nsEmbedString value;
		rv = nsCert->GetCommonName (value);
		if (NS_FAILED(rv)) return FALSE;

		nsEmbedCString cValue;
		NS_UTF16ToCString (value, NS_CSTRING_ENCODING_UTF8, cValue);

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
	nsEmbedString dispNameU;
	object->GetDisplayName(dispNameU);

	nsEmbedCString cDispNameU;
	NS_UTF16ToCString (dispNameU, NS_CSTRING_ENCODING_UTF8, cDispNameU);

	GtkTreeIter iter;
	gtk_tree_store_append (GTK_TREE_STORE (model), &iter, parent);

	gtk_tree_store_set (GTK_TREE_STORE(model), &iter,
			    0, cDispNameU.get(),
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

		nsEmbedString dispValU;
		object->GetDisplayValue(dispValU);

		nsEmbedCString cDispValU;
		NS_UTF16ToCString (dispValU, NS_CSTRING_ENCODING_UTF8, cDispValU);

		gtk_text_buffer_set_text (text_buffer, cDispValU.get(), -1);
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
						     NULL);

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
						     NULL);

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
	nsEmbedString value;
	PRUint32 verifystate, count;
	PRUnichar ** usage;
	GtkSizeGroup * sizegroup;

	nsresult rv;
	AutoJSContextStack stack;
	rv = stack.Init ();
	if (NS_FAILED (rv)) return rv;

	gxml = ephy_glade_widget_new (ephy_file ("certificate-dialogs.glade"),
				      "viewcert_dialog",
				      &dialog, NULL, NULL);

	nsCOMPtr<nsIDOMWindow> parent = do_GetInterface (ctx);
	GtkWindow *gparent = GTK_WINDOW (EphyUtils::FindGtkParent (parent));
	if (gparent)
	{
		gtk_window_set_transient_for (GTK_WINDOW(dialog), GTK_WINDOW(gparent));
		gtk_window_group_add_window (ephy_gui_ensure_window_group (GTK_WINDOW (gparent)),
					     GTK_WINDOW (dialog));
		gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
	}

	gtk_window_set_icon_name (GTK_WINDOW (dialog), "web-browser");

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
			nsEmbedCString msg;
			NS_UTF16ToCString (nsEmbedString(usage[i]),
					   NS_CSTRING_ENCODING_UTF8, msg);

			GtkWidget *label = gtk_label_new(msg.get());
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
