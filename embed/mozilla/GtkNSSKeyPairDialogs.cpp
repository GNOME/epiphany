/*
 *  GtkNSSKeyPairDialogs.cpp
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
 * This file provides Gtk implementations of the mozilla Generating Key Pair
 * dialogs.
 *
 * This implementation takes some liberties with the mozilla API. Although the
 * API requires a nsIDomWindowInternal, it only actually calls the Close()
 * function on that class. Therefore we provide a dummy class that only 
 * implements that function (it just sets a flag). 
 *
 * Periodically we check to see whether the dialog should have been closed. If
 * it should be closed, then the key generation has finished, so close the dialog
 * (using gtk_dialog_response), and return.
 *
 */

#include "mozilla-config.h"
#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkimage.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkprogressbar.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkvbox.h>

#include <nsIDOMWindow.h>
#include <nsIInterfaceRequestor.h>
#include <nsIInterfaceRequestorUtils.h>
#include <nsIKeygenThread.h>
#include <nsIObserver.h>
#include <nsIServiceManager.h>

#include "ephy-debug.h"
#include "ephy-gui.h"

#include "AutoJSContextStack.h"
#include "EphyUtils.h"

#include "GtkNSSKeyPairDialogs.h"

GtkNSSKeyPairDialogs::GtkNSSKeyPairDialogs ()
{
	LOG ("GtkNSSKeyPairDialogs ctor (%p)", this);
}

GtkNSSKeyPairDialogs::~GtkNSSKeyPairDialogs ()
{
	LOG ("GtkNSSKeyPairDialogs dtor (%p)", this);
}

NS_IMPL_THREADSAFE_ISUPPORTS1 (GtkNSSKeyPairDialogs,
			       nsIGeneratingKeypairInfoDialogs)

class KeyPairObserver : public nsIObserver
{
public:
       NS_DECL_NSIOBSERVER
       NS_DECL_ISUPPORTS

       KeyPairObserver() : close_called (FALSE) {};
       virtual ~KeyPairObserver() {};

       gboolean close_called;
};

NS_IMPL_ISUPPORTS1 (KeyPairObserver, nsIObserver);

NS_IMETHODIMP KeyPairObserver::Observe (nsISupports *aSubject, const char *aTopic,
                                       const PRUnichar *aData)
{
       close_called = TRUE;
       return NS_OK;
}

/* ------------------------------------------------------------ */
static void
begin_busy (GtkWidget *widget)
{
	static GdkCursor *cursor = NULL;

	if (cursor == NULL) cursor = gdk_cursor_new (GDK_WATCH);

	if (!GTK_WIDGET_REALIZED (widget)) gtk_widget_realize (GTK_WIDGET(widget));

	gdk_window_set_cursor (GTK_WIDGET (widget)->window, cursor);
	while (gtk_events_pending ()) gtk_main_iteration ();
}

static void
end_busy (GtkWidget *widget)
{
	gdk_window_set_cursor (GTK_WIDGET(widget)->window, NULL);
}


struct KeyPairInfo
{
	GtkWidget *progress;
	GtkWidget *dialog;
	KeyPairObserver *helper;
};


static gboolean
generating_timeout_cb (KeyPairInfo *info)
{
	gtk_progress_bar_pulse (GTK_PROGRESS_BAR (info->progress));

	if (info->helper->close_called)
	{
		gtk_dialog_response (GTK_DIALOG (info->dialog), GTK_RESPONSE_OK);
	}
	return TRUE;
}


/* void displayGeneratingKeypairInfo (in nsIInterfaceRequestor ctx, in nsIKeygenTh
read runnable); */
NS_IMETHODIMP
GtkNSSKeyPairDialogs::DisplayGeneratingKeypairInfo (nsIInterfaceRequestor *ctx,
						    nsIKeygenThread *runnable)
{
	GtkWidget *dialog, *progress, *label, *vbox;
	gint timeout_id;

	nsresult rv;
	AutoJSContextStack stack;
	rv = stack.Init ();
	if (NS_FAILED (rv)) return rv;

	nsCOMPtr<nsIDOMWindow> parent = do_GetInterface (ctx);
	GtkWindow *gparent = GTK_WINDOW (EphyUtils::FindGtkParent (parent));

	dialog = gtk_dialog_new_with_buttons ("", gparent,
					      GTK_DIALOG_DESTROY_WITH_PARENT, NULL);

	if (gparent)
	{
		gtk_window_group_add_window (ephy_gui_ensure_window_group (gparent),
					     GTK_WINDOW (dialog));
	}

	gtk_window_set_icon_name (GTK_WINDOW (dialog), "web-browser");

	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);

	vbox = gtk_vbox_new (FALSE, 12);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
	gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), vbox, TRUE, TRUE, 0);

	label = gtk_label_new (NULL);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
	
	char *msg = g_strdup_printf ("<span weight=\"bold\" size=\"larger\">%s</span>\n\n%s",
				     _("Generating Private Key."),
				     _("Please wait while a new private key is "
				       "generated. This process could take a few minutes." ));
	gtk_label_set_markup (GTK_LABEL(label), msg);
	g_free (msg);

	progress = gtk_progress_bar_new ();
	gtk_box_pack_start (GTK_BOX (vbox), progress, TRUE, TRUE, 0);

	/* Create a helper class that just waits for close events
	 * from the other thread */
       nsCOMPtr<KeyPairObserver> helper = new KeyPairObserver;

	KeyPairInfo callback_data = { progress, dialog, helper };
	timeout_id = g_timeout_add (100, (GSourceFunc)generating_timeout_cb, &callback_data);

	gtk_widget_show_all (dialog);
	gtk_widget_hide (GTK_DIALOG (dialog)->action_area);

	begin_busy (dialog);
	runnable->StartKeyGeneration (helper);
	int res = gtk_dialog_run (GTK_DIALOG (dialog));
	if (res != GTK_RESPONSE_OK && helper->close_called == FALSE)
	{
		/* Ignore the already_closed flag, our nsIDOMWindowInterna::Close
		 * function just sets a flag, it doesn't close the window, so we
		 * dont have a race condition */
		PRBool already_closed = FALSE;
		runnable->UserCanceled (&already_closed);
	}

	g_source_remove (timeout_id);
	end_busy (dialog);
	gtk_widget_destroy (dialog);
	return NS_OK;
}
