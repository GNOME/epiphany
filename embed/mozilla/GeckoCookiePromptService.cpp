/*
 *  Copyright © 2003 Tommi Komulainen <tommi.komulainen@iki.fi>
 *  Copyright © 2004, 2007 Christian Persch
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2
 *  as published by the Free Software Foundation.
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

#include <xpcom-config.h>
#include "config.h"

#include <glib/gi18n.h>

#include <gtk/gtkbox.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkwindow.h>

#include <nsStringAPI.h>

#include "ephy-debug.h"
#include "ephy-gui.h"

#include "AutoJSContextStack.h"
#include "AutoWindowModalState.h"
#include "EphyUtils.h"

#include "GeckoCookiePromptService.h"

NS_IMPL_ISUPPORTS1 (GeckoCookiePromptService, nsICookiePromptService)

GeckoCookiePromptService::GeckoCookiePromptService()
{
  LOG ("GeckoCookiePromptService ctor [%p]", this);
}

GeckoCookiePromptService::~GeckoCookiePromptService()
{
  LOG ("GeckoCookiePromptService dtor [%p]", this);
}

/* boolean cookieDialog (in nsIDOMWindow parent, in nsICookie cookie, in ACString hostname, in long cookiesFromHost, in boolean changingCookie, inout boolean checkValue); */
NS_IMETHODIMP
GeckoCookiePromptService::CookieDialog (nsIDOMWindow *aParent,
                                        nsICookie *aCookie,
                                        const nsACString &aHostname,
                                        PRInt32 aCookiesFromHost,
                                        PRBool aChangingCookie,
                                        PRBool *_checkValue,
                                        PRBool *_retval)
{
  NS_ENSURE_ARG (aParent);
  NS_ENSURE_ARG (aCookie);
  NS_ENSURE_ARG_POINTER (_checkValue);
  NS_ENSURE_ARG_POINTER (_retval);

  // TODO short-circuit and accept session cookies as per preference
  // TODO until mozilla starts supporting it natively?

  GtkWidget *parent = EphyUtils::FindGtkParent (aParent);
  NS_ENSURE_TRUE(parent, NS_ERROR_INVALID_POINTER);

  nsresult rv;
  AutoJSContextStack stack;
  rv = stack.Init ();
  if (NS_FAILED (rv)) {
    return rv;
  }

  AutoWindowModalState modalState (aParent);

  nsCString host(aHostname);

  GtkWidget *dialog = gtk_message_dialog_new
                        (GTK_WINDOW (parent),
                         GTK_DIALOG_MODAL /* FIXME mozilla sucks! */,
                         GTK_MESSAGE_QUESTION,
                         GTK_BUTTONS_NONE,
                         _("Accept cookie from %s?"),
                         host.get());
  GtkWindow *window = GTK_WINDOW (dialog);
  GtkDialog *gdialog = GTK_DIALOG (dialog);
  GtkMessageDialog *message_dialog = GTK_MESSAGE_DIALOG (dialog);

  gtk_window_set_icon_name (window, "web-browser");
  gtk_window_set_title (window, _("Accept Cookie?"));

  if (aChangingCookie) {
    gtk_message_dialog_format_secondary_text
      (message_dialog,
       _("The site wants to modify an existing cookie."));
  } else if (aCookiesFromHost == 0) {
    gtk_message_dialog_format_secondary_text
      (message_dialog,
       _("The site wants to set a cookie."));
  } else if (aCookiesFromHost == 1) {
    gtk_message_dialog_format_secondary_text
      (message_dialog,
       _("The site wants to set a second cookie."));
  } else {
    char *num_text = g_strdup_printf
                      (ngettext ("You already have %d cookie from this site.",
                                 "You already have %d cookies from this site.",
                                 aCookiesFromHost),
                       aCookiesFromHost);

    gtk_message_dialog_format_secondary_text
      (message_dialog,
       "The site %s wants to set another cookie. %s",
       host.get(), num_text);
    g_free (num_text);
  }

  GtkWidget *checkbutton;
  checkbutton = gtk_check_button_new_with_mnemonic
                  (_("Apply this _decision to all cookies from this site"));
  gtk_widget_show (checkbutton);
  gtk_box_pack_start (GTK_BOX (ephy_gui_message_dialog_get_content_box (dialog)),
                      checkbutton, FALSE, FALSE, 0);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton), *_checkValue);

  gtk_dialog_add_button (gdialog,
                         _("_Reject"), GTK_RESPONSE_REJECT);
  gtk_dialog_add_button (gdialog,
                         _("_Accept"), GTK_RESPONSE_ACCEPT);
  gtk_dialog_set_default_response (gdialog, GTK_RESPONSE_ACCEPT);

  int response = gtk_dialog_run (gdialog);

  if (response == GTK_RESPONSE_ACCEPT || response == GTK_RESPONSE_REJECT) {
    *_retval = (response == GTK_RESPONSE_ACCEPT);
    *_checkValue = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbutton));
  } else {
    /* if the dialog was closed, but no button was pressed,
      * consider it as 'Reject' but ignore the checkbutton
      */
    *_retval = PR_FALSE;
    *_checkValue = PR_FALSE;
  }

  gtk_widget_destroy (dialog);

  return NS_OK;
}
