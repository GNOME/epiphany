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
 *
 *  $Id$
 */

#include "mozilla-config.h"
#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtkcelllayout.h>
#include <gtk/gtkcellrenderer.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkcombobox.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkexpander.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkimage.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkprogressbar.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkstock.h>
#include <gtk/gtktextbuffer.h>
#include <gtk/gtktextview.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtkvbox.h>

#include <nsStringAPI.h>

#include <nsIDOMWindow.h>
#include <nsIInterfaceRequestor.h>
#include <nsIInterfaceRequestorUtils.h>
#include <nsIServiceManager.h>

#include "ephy-debug.h"
#include "ephy-gui.h"
#include "ephy-state.h"

#include "AutoJSContextStack.h"
#include "AutoWindowModalState.h"
#include "EphyUtils.h"

#include "GtkNSSClientAuthDialogs.h"

GtkNSSClientAuthDialogs::GtkNSSClientAuthDialogs()
{
	LOG ("GtkNSSClientAuthDialogs ctor (%p)", this);
}


GtkNSSClientAuthDialogs::~GtkNSSClientAuthDialogs()
{
	LOG ("GtkNSSClientAuthDialogs dtor (%p)", this);
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
combo_changed_cb (GtkComboBox *combo, GtkTextView *textview)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTextBuffer *buffer;
	int index;

	model = gtk_combo_box_get_model (combo);
	index = gtk_combo_box_get_active (combo);
	buffer = gtk_text_view_get_buffer (textview);

	if (gtk_tree_model_iter_nth_child (model, &iter, NULL, index))
	{
		char *text;

		gtk_tree_model_get (model, &iter, 1, &text, -1);

		gtk_text_buffer_set_text (buffer, text, -1);

		g_free (text);
	}
	else
	{
		gtk_text_buffer_set_text (buffer, "", -1);
	}
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
	GtkWidget *dialog, *label, *vbox, *textview;
	GtkWidget *details, *expander, *hbox, *image;
	GtkWidget *combo;
	GtkListStore *store;
	GtkTreeIter iter;
	GtkCellRenderer *renderer;
	char *msg, *markup_text;
	PRUint32 i;

	nsresult rv;
	AutoJSContextStack stack;
	rv = stack.Init ();
	if (NS_FAILED (rv)) return rv;

	nsCOMPtr<nsIDOMWindow> parent (do_GetInterface (ctx));
	GtkWindow *gparent = GTK_WINDOW (EphyUtils::FindGtkParent (parent));

	AutoWindowModalState modalState (parent);

	dialog = gtk_dialog_new_with_buttons ("",
					      GTK_WINDOW (gparent),
					      GTK_DIALOG_DESTROY_WITH_PARENT,
					      GTK_STOCK_CANCEL,
					      GTK_RESPONSE_CANCEL,
					      _("_Select Certificate"),
					      GTK_RESPONSE_OK,
					      (char *) NULL);

	if (gparent)
	{
		gtk_window_group_add_window (ephy_gui_ensure_window_group (gparent),
					     GTK_WINDOW (dialog));
	}

	gtk_window_set_icon_name (GTK_WINDOW (dialog), "web-browser");

	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX(GTK_DIALOG (dialog)->vbox), 14); /* 24 = 2 * 5 + 14 */
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

	msg = g_markup_printf_escaped (_("Choose a certificate to present as identification to “%s”."),
				       NS_ConvertUTF16toUTF8 (cn).get());
	markup_text = g_strdup_printf ("<span weight=\"bold\" size=\"larger\">%s</span>\n\n%s",
				       _("Select a certificate to identify yourself."),
				       msg);
	gtk_label_set_markup (GTK_LABEL (label), markup_text);
	g_free (msg);
	g_free (markup_text);

        /* Create and populate the combo */
	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
	for (i = 0; i < count; i++)
	{
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    0, NS_ConvertUTF16toUTF8 (certNickList[i]).get(),
				    1, NS_ConvertUTF16toUTF8 (certDetailsList[i]).get(),
				    -1);
	}

	combo = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
	g_object_unref (store);

        renderer = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer,
                                        "text", 0,
                                        (char *) NULL);

	gtk_widget_show (combo);
	gtk_box_pack_start (GTK_BOX (vbox), combo, FALSE, TRUE, 0);

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

	g_signal_connect (G_OBJECT (combo), "changed",
			  G_CALLBACK (combo_changed_cb),
			  textview);

	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);

	/* run the dialog */
	int res = gtk_dialog_run (GTK_DIALOG (dialog));
	if (res == GTK_RESPONSE_OK)
	{
		*canceled = PR_FALSE;
		*selectedIndex = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));
	} 
	else
	{
		*canceled = PR_TRUE;
	}

	gtk_widget_destroy (dialog);
	return NS_OK;
}
