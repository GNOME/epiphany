/*
 *  Copyright © 2006 Christian Persch
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2.1, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#include "mozilla-config.h"
#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <glade/glade-xml.h>

#include <nsStringAPI.h>

#include <nsCOMPtr.h>
#include <nsIDOMWindow.h>
#include <nsIInterfaceRequestor.h>
#include <nsIInterfaceRequestorUtils.h>

#include "eel-gconf-extensions.h"
#include "ephy-debug.h"
#include "ephy-embed-shell.h"
#include "ephy-file-helpers.h"
#include "ephy-prefs.h"

#include "AutoJSContextStack.h"
#include "AutoWindowModalState.h"
#include "EphyUtils.h"

#include "GeckoFormSigningDialog.h"

#define LITERAL(s) NS_REINTERPRET_CAST(const nsAString::char_type*, NS_L(s))

NS_IMPL_ISUPPORTS1 (GeckoFormSigningDialog,
		    nsIFormSigningDialog)

GeckoFormSigningDialog::GeckoFormSigningDialog()
{
  LOG ("GeckoFormSigningDialog ctor [%p]", this);
}

GeckoFormSigningDialog::~GeckoFormSigningDialog()
{
  LOG ("GeckoFormSigningDialog dtor [%p]", this);
}

/* nsIFormSigningDialog implementation */

/* boolean confirmSignText (in nsIInterfaceRequestor ctxt,
			    in AString host,
			    in AString signText,
			    [array, size_is (count)] in wstring certNickList,
			    [array, size_is (count)] in wstring certDetailsList,
			    in PRUint32 count,
			    out PRInt32 selectedIndex,
			    out AString password); */
NS_IMETHODIMP
GeckoFormSigningDialog::ConfirmSignText (nsIInterfaceRequestor *ctx,
					 const nsAString & host,
					 const nsAString & signText,
					 const PRUnichar **certNickList,
					 const PRUnichar **certDetailsList,
					 PRUint32 count,
					 PRInt32 *selectedIndex,
					 nsAString &_password,
					 PRBool *_cancelled)
{
  /* FIXME: limit |signText| to a sensitlbe length (maybe 100k)? */

  nsresult rv;
  AutoJSContextStack stack;
  rv = stack.Init ();
  if (NS_FAILED (rv)) return rv;

  nsCOMPtr<nsIDOMWindow> parent (do_GetInterface (ctx));
  if (!parent) {
    parent = EphyJSUtils::GetDOMWindowFromCallContext ();
    g_print ("Fallback window %p\n", (void*)parent.get());
  }
  GtkWidget *gparent = EphyUtils::FindGtkParent (parent);

  AutoWindowModalState modalState (parent);

  GladeXML *gxml = glade_xml_new (ephy_file ("form-signing-dialog.glade"),
				  "form_signing_dialog", NULL);
  g_return_val_if_fail (gxml, NS_ERROR_FAILURE);

  GtkWidget *dialog = glade_xml_get_widget (gxml, "form_signing_dialog");
  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (gparent));

  GtkLabel *primary_label = GTK_LABEL (glade_xml_get_widget (gxml, "primary_label"));
  char *primary = g_strdup_printf (_("The web site “%s” requests that you sign the following text:"),
				   NS_ConvertUTF16toUTF8 (host).get ());
  gtk_label_set_text (primary_label, primary);
  g_free (primary);

  GtkTextView *textview = GTK_TEXT_VIEW (glade_xml_get_widget (gxml, "textview"));
  NS_ConvertUTF16toUTF8 text (signText);
  gtk_text_buffer_set_text (gtk_text_view_get_buffer (textview),
			    text.get (), text.Length ());

  GtkTable *table = GTK_TABLE (glade_xml_get_widget (gxml, "table"));
  GtkComboBox *combo = GTK_COMBO_BOX (gtk_combo_box_new_text ());
  for (PRUint32 i = 0; i < count; ++i) {
    gtk_combo_box_append_text (combo, NS_ConvertUTF16toUTF8 (certNickList[i]).get ());
  }

  gtk_combo_box_set_active (combo, 0);
  gtk_table_attach (table, GTK_WIDGET (combo), 1, 2, 0, 1,
		    GtkAttachOptions (0), GtkAttachOptions (0), 0, 0);
  gtk_widget_show (GTK_WIDGET (combo));

  /* FIXME: Add "View Certificate" button */

  GtkEntry *password_entry = GTK_ENTRY (glade_xml_get_widget (gxml, "password_entry"));

  GtkWidget *button = gtk_dialog_add_button (GTK_DIALOG (dialog),
					     GTK_STOCK_CANCEL,
					     GTK_RESPONSE_CANCEL);
  gtk_dialog_add_button (GTK_DIALOG (dialog),
			 _("_Sign text"),
			 GTK_RESPONSE_ACCEPT);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_REJECT);
  gtk_widget_grab_focus (button);

  /* FIXME: make Sign insensitive for some time (proportional to text length, with maximum?) */

  g_object_unref (gxml);

  int response = gtk_dialog_run (GTK_DIALOG (dialog));

  *_cancelled = response != GTK_RESPONSE_ACCEPT;

  if (response == GTK_RESPONSE_ACCEPT) {
    _password = NS_ConvertUTF8toUTF16 (gtk_entry_get_text (password_entry));
    *selectedIndex = gtk_combo_box_get_active (combo);
  }

  gtk_widget_destroy (dialog);

  return NS_OK;
}
