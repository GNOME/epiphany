/*
 *  GtkNSSClientAuthDialogs.cpp
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_MOZILLA_PSM

#include "MozillaPrivate.h"

#include "nsIServiceManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"

#include <gtk/gtkdialog.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkstock.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtktextbuffer.h>
#include <gtk/gtktextview.h>
#include <gtk/gtkprogressbar.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkexpander.h>
#include <glib/gi18n.h>

#include "GtkNSSClientAuthDialogs.h"

#include "ephy-state.h"

GtkNSSClientAuthDialogs::GtkNSSClientAuthDialogs()
{
}


GtkNSSClientAuthDialogs::~GtkNSSClientAuthDialogs()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS1 (GtkNSSClientAuthDialogs, 
			       nsIClientAuthDialogs)

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


static void
option_menu_changed_cb (GtkOptionMenu *optionmenu, GtkTextView *textview)
{
	GtkWidget *menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (optionmenu));;
	GtkWidget *item = gtk_menu_get_active (GTK_MENU (menu));
	GtkTextBuffer *buffer = gtk_text_view_get_buffer (textview);
	PRUnichar *details;

	if (item == 0)
	{
		gtk_text_buffer_set_text (buffer, "", -1);
		return;
	}

	details = (PRUnichar*)g_object_get_data (G_OBJECT (item), "details");
	g_return_if_fail (details);

	const nsACString &certnick = NS_ConvertUCS2toUTF8(details);
	gtk_text_buffer_set_text (buffer, PromiseFlatCString(certnick).get(), -1);
}

NS_IMETHODIMP
GtkNSSClientAuthDialogs::ChooseCertificate (nsIInterfaceRequestor *ctx,
					    const PRUnichar *cn, 
					    const PRUnichar *organization, 
					    const PRUnichar *issuer, 
					    const PRUnichar **certNickList,
					    const PRUnichar **certDetailsList,
					    PRUint32 count, PRInt32 *selectedIndex,
					    PRBool *canceled)
{
	GtkWidget *dialog, *label, *vbox, *optionmenu, *textview, *menu;
	GtkWidget *details, *expander, *hbox, *image;
	char *msg, *tt_cn, *markup_text;
	PRUint32 i;
	gboolean showDetails;

	nsCOMPtr<nsIDOMWindow> parent = do_GetInterface (ctx);
	GtkWidget *gparent = MozillaFindGtkParent (parent);

	dialog = gtk_dialog_new_with_buttons ("",
					      GTK_WINDOW (gparent),
					      GTK_DIALOG_DESTROY_WITH_PARENT,
					      GTK_STOCK_CANCEL,
					      GTK_RESPONSE_CANCEL,
					      _("_Select Certificate"),
					      GTK_RESPONSE_OK,
					      NULL);
	
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX(GTK_DIALOG (dialog)->action_area), 6);	
	gtk_box_set_spacing (GTK_BOX(GTK_DIALOG (dialog)->vbox), 12);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	
	hbox = gtk_hbox_new (FALSE, 12);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);

	image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_WARNING,
					  GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment (GTK_MISC (image), 0.5, 0.0);
	gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
	gtk_widget_show (image);

	vbox = gtk_vbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
	gtk_widget_show (vbox);

	label = gtk_label_new (NULL);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_label_set_selectable (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
	

	const nsACString &utf8_cn = NS_ConvertUCS2toUTF8(cn);
	tt_cn = g_strdup_printf (_("\"<tt>%s</tt>\""), 
				 PromiseFlatCString(utf8_cn).get());

	msg = g_strdup_printf (_("Choose a certificate to present as identification to %s."),
			       tt_cn);
	markup_text = g_strdup_printf ("<span weight=\"bold\" size=\"larger\">%s</span>\n\n%s",
				       _("Select a certificate to identify yourself."),
				       msg);
	gtk_label_set_markup (GTK_LABEL (label), markup_text);
	g_free (msg);
	g_free (tt_cn);
	g_free (markup_text);

        /* Create and populate the option menu */
	optionmenu = gtk_option_menu_new ();
	menu = gtk_menu_new ();
	gtk_option_menu_set_menu (GTK_OPTION_MENU (optionmenu), menu);
	gtk_box_pack_start (GTK_BOX (vbox), optionmenu, FALSE, TRUE, 0);
	gtk_widget_show (menu);
	gtk_widget_show (optionmenu);

	for (i = 0 ; i < count ; i++)
	{
		const nsACString &certnick = NS_ConvertUCS2toUTF8(certNickList[i]);
		GtkWidget *item = gtk_menu_item_new_with_label (PromiseFlatCString(certnick).get());
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		g_object_set_data (G_OBJECT (item), "details", (void*)certDetailsList[i]);
		gtk_widget_show (item);
	}


	expander = gtk_expander_new_with_mnemonic (_("Certificate _Details"));
	ephy_state_add_expander (GTK_WIDGET (expander), "client-auth-dialog-expander", FALSE);

	gtk_widget_show (expander);
	gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), expander, FALSE, FALSE, 0);

	/* Create the text box */
	textview = gtk_text_view_new ();
	gtk_text_view_set_editable (GTK_TEXT_VIEW (textview), FALSE);
	gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (textview), FALSE);
	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (textview), GTK_WRAP_WORD);
	gtk_widget_set_size_request (GTK_WIDGET (textview), -1, 100);
	gtk_widget_show (textview);

	details = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (details), GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (details), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (details), textview);
	gtk_widget_show (details);

	details = higgy_indent_widget (details);
	gtk_container_set_border_width (GTK_CONTAINER (details), 5);
	gtk_widget_show (details);

	gtk_container_add (GTK_CONTAINER (expander), details);

	g_signal_connect (G_OBJECT (optionmenu), "changed",
			  G_CALLBACK (option_menu_changed_cb),
			  textview);

	gtk_option_menu_set_history (GTK_OPTION_MENU (optionmenu), 0);

	/* run the dialog */
	int res = gtk_dialog_run (GTK_DIALOG (dialog));
	if (res == GTK_RESPONSE_OK)
	{
		*canceled = PR_FALSE;
		*selectedIndex = gtk_option_menu_get_history (GTK_OPTION_MENU (optionmenu));
	} 
	else
	{
		*canceled = PR_TRUE;
	}

	gtk_widget_destroy (dialog);
	return NS_OK;
}



#endif
