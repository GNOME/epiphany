/*
 *  Copyright © 2001 Philip Langdale
 *  Copyright © 2003 Marco Pesenti Gritti
 *  Copyright © 2003 Xan Lopez
 *  Copyright © 2004 Christian Persch
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

#include "mozilla-config.h"
#include "config.h"

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkstock.h>

#include <nsStringAPI.h>

#include <nsAutoPtr.h>
#include <nsCExternalHandlerService.h>
#include <nsComponentManagerUtils.h>
#include <nsIDOMWindow.h>
#include <nsIInterfaceRequestorUtils.h>
#include <nsILocalFile.h>
#include <nsIMIMEInfo.h>
#include <nsIURL.h>
#include <nsMemory.h>
#include <nsNetError.h>
#include <nsServiceManagerUtils.h>

#include "eel-gconf-extensions.h"
#include "ephy-debug.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-single.h"
#include "ephy-file-chooser.h"
#include "ephy-file-helpers.h"
#include "ephy-gui.h"
#include "ephy-prefs.h"
#include "ephy-stock-icons.h"

#include "AutoModalDialog.h"
#include "EphyUtils.h"
#include "MozDownload.h"

#include "ContentHandler.h"

/* FIXME: we don't generally have a timestamp for the user action which initiated this
 * content handler.
 */
GContentHandler::GContentHandler()
: mHelperApp(NULL),
  mPermission(EPHY_MIME_PERMISSION_UNKNOWN),
  mUserTime(0),
  mAction(CONTENT_ACTION_NONE)
{
	LOG ("GContentHandler ctor (%p)", this);
}

GContentHandler::~GContentHandler()
{
	LOG ("GContentHandler dtor (%p)", this);
}

NS_IMPL_ISUPPORTS1(GContentHandler, nsIHelperAppLauncherDialog)

/* void show (in nsIHelperAppLauncher aLauncher, in nsISupports aContext, in unsigned long aReason); */
NS_IMETHODIMP
GContentHandler::Show (nsIHelperAppLauncher *aLauncher,
		       nsISupports *aContext,
		       PRUint32 aReason)
{
	nsRefPtr<GContentHandler> kungFuDeathGrip(this);

	nsresult rv;
	EphyEmbedSingle *single;
	gboolean handled = FALSE;

	/* FIXME: handle aForced / aReason argument in some way? */

	mContext = aContext;

	/* Check for a potential veto */
	nsCOMPtr<nsIDOMWindow> window (do_GetInterface (aContext));
	GtkWidget *embed = EphyUtils::FindEmbed (window);
	if (EPHY_IS_EMBED (embed))
	{
		if (g_object_get_data (G_OBJECT (embed), "content-handler-deny"))
		{
			return NS_OK;
		}
	}

	mLauncher = aLauncher;
	rv = Init ();
	NS_ENSURE_SUCCESS (rv, rv);

	single = EPHY_EMBED_SINGLE (ephy_embed_shell_get_embed_single (embed_shell));
	g_signal_emit_by_name (single, "handle_content", mMimeType.get(),
			       mUrl.get(), &handled);

	if (!handled)
	{
		MIMEInitiateAction ();
	}
	else
	{
		mLauncher->Cancel (NS_BINDING_ABORTED);
	}

	return NS_OK;
}

/* nsILocalFile promptForSaveToFile (in nsISupports aWindowContext, in wstring aDefaultFile, in wstring aSuggestedFileExtension); */
NS_IMETHODIMP GContentHandler::PromptForSaveToFile(
				    nsIHelperAppLauncher *aLauncher,
				    nsISupports *aWindowContext,
				    const PRUnichar *aDefaultFile,
				    const PRUnichar *aSuggestedFileExtension,
#ifdef HAVE_GECKO_1_9
				    PRBool aForcePrompt,
#endif
				    nsILocalFile **_retval)
{
	EphyFileChooser *dialog;
	int response;
	char *filename = NULL;
	nsCString defaultFile;

	NS_UTF16ToCString (nsString (aDefaultFile),
			   NS_CSTRING_ENCODING_UTF8, defaultFile);

	if (mAction != CONTENT_ACTION_SAVEAS)
	{
		return BuildDownloadPath (defaultFile.get(), _retval);
	}
	nsCOMPtr<nsIDOMWindow> parentDOMWindow (do_GetInterface (aWindowContext));

	AutoModalDialog modalDialog (parentDOMWindow, PR_FALSE);
	if (!modalDialog.ShouldShow ())
		return NS_ERROR_FAILURE;

	GtkWindow *parentWindow = modalDialog.GetParent ();

	dialog = ephy_file_chooser_new (_("Save"), GTK_WIDGET (parentWindow),
					GTK_FILE_CHOOSER_ACTION_SAVE,
					CONF_STATE_SAVE_DIR,
					EPHY_FILE_FILTER_ALL);
	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
	/* Remove leading dots */
	const char *fname = defaultFile.get();
	while (*fname == '.') fname++;

	gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), fname);

	if (parentWindow)
	{
		gtk_window_group_add_window (ephy_gui_ensure_window_group (GTK_WINDOW (parentWindow)),
					     GTK_WINDOW (dialog));
	}

	/* FIXME: this will only be the real user time if we came from ::Show */
	ephy_gui_window_update_user_time (GTK_WIDGET (dialog), (guint32) mUserTime);

	/* FIXME: modal -- mozilla sucks! */
	do
	{
		g_free (filename);
		response = modalDialog.Run (GTK_DIALOG (dialog));
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
	} while (response == GTK_RESPONSE_ACCEPT
		 && !ephy_gui_check_location_writable (GTK_WIDGET (dialog), filename));

	if (response == GTK_RESPONSE_ACCEPT)
	{
		nsCOMPtr <nsILocalFile> destFile (do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));
		NS_ENSURE_TRUE (destFile, NS_ERROR_FAILURE);

		destFile->InitWithNativePath (nsCString (filename));
		g_free (filename);

		NS_IF_ADDREF (*_retval = destFile);

		gtk_widget_destroy (GTK_WIDGET (dialog));

		return NS_OK;
	}
	else
	{
		gtk_widget_destroy (GTK_WIDGET (dialog));
		g_free (filename);

		return NS_ERROR_FAILURE;
	}
}

NS_METHOD GContentHandler::Init ()
{
	nsresult rv;

	NS_ENSURE_TRUE (mLauncher, NS_ERROR_FAILURE);

	nsCOMPtr<nsIMIMEInfo> MIMEInfo;
	mLauncher->GetMIMEInfo (getter_AddRefs(MIMEInfo));
	NS_ENSURE_TRUE (MIMEInfo, NS_ERROR_FAILURE);

	rv = MIMEInfo->GetMIMEType (mMimeType);

	nsCOMPtr<nsIURI> uri;
	mLauncher->GetSource (getter_AddRefs(uri));
	NS_ENSURE_TRUE (uri, NS_ERROR_FAILURE);
	
	uri->GetSpec (mUrl);

	return NS_OK;
}

/* static */ void
GContentHandler::ResponseCallback (GtkWidget *dialog,
                                   int response,
                                   GContentHandler *self)
{
	gtk_widget_destroy (dialog);

	if (response > 0)
	{
		self->mAction = (ContentAction) response;
	}
	else
	{
		self->mAction = CONTENT_ACTION_NONE;
	}

	self->MIMEDoAction ();
}

static void
release_cb (GContentHandler *data)
{
	NS_RELEASE (data);
}

NS_METHOD GContentHandler::MIMEConfirmAction ()
{
	GtkWidget *dialog, *button, *image;
	const char *action_label;
	char *mime_description;
	nsCString file_name;
			
	nsCOMPtr<nsIDOMWindow> parentDOMWindow = do_GetInterface (mContext);
	GtkWindow *parentWindow = GTK_WINDOW (EphyUtils::FindGtkParent(parentDOMWindow));

	action_label =  (mAction == CONTENT_ACTION_OPEN) ||
			(mAction == CONTENT_ACTION_OPEN_TMP) ?
			GTK_STOCK_OPEN : STOCK_DOWNLOAD;

	mime_description = g_content_type_get_description (mMimeType.get());
	if (mime_description == NULL)
	{
		/* Translators: The text before the "|" is context to help you decide on
		 * the correct translation. You MUST OMIT it in the translated string. */
		mime_description = g_strdup (Q_("File Type:|Unknown"));
	}

	/* We have one tiny, minor issue, the filename can be empty (""),
	   is that severe enough to be completely fixed ? */
	nsString suggested;
		
	mLauncher->GetSuggestedFileName (suggested);
	NS_UTF16ToCString (suggested,
			   NS_CSTRING_ENCODING_UTF8, file_name);

	if (mPermission != EPHY_MIME_PERMISSION_SAFE && mHelperApp)
	{
		dialog = gtk_message_dialog_new
			(parentWindow, GTK_DIALOG_DESTROY_WITH_PARENT,
			 GTK_MESSAGE_WARNING, GTK_BUTTONS_NONE,
			 _("Download this potentially unsafe file?"));
			   
		gtk_message_dialog_format_secondary_text
			(GTK_MESSAGE_DIALOG (dialog),
			/* translators: First %s is the file type description,
			   Second %s is the file name */
			_("File Type: “%s”.\n\nIt is unsafe to open “%s” as "
			  "it could potentially damage your documents or "
			  "invade your privacy. You can download it instead."),
			  mime_description, file_name.get());		
	}
	else if (mAction == CONTENT_ACTION_OPEN_TMP && mHelperApp)
	{
		dialog = gtk_message_dialog_new
			(parentWindow, GTK_DIALOG_DESTROY_WITH_PARENT,
			 GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
			 _("Open this file?"));
			   
		gtk_message_dialog_format_secondary_text
			(GTK_MESSAGE_DIALOG (dialog),
			/* translators: First %s is the file type description,
			   Second %s is the file name,
			   Third %s is the application used to open the file */
			_("File Type: “%s”.\n\nYou can open “%s” using “%s” or save it."),
			   mime_description, file_name.get(), g_app_info_get_name (mHelperApp));		 
	}
	else
	{
		dialog = gtk_message_dialog_new
			(parentWindow, GTK_DIALOG_DESTROY_WITH_PARENT,
			 GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
			 _("Download this file?"));
		
		gtk_message_dialog_format_secondary_text
			(GTK_MESSAGE_DIALOG (dialog),
			/* translators: First %s is the file type description,
			   Second %s is the file name */
			_("File Type: “%s”.\n\nYou have no application able to open “%s”. "
			   "You can download it instead."),
			   mime_description, file_name.get());			 
	}
	
	g_free (mime_description);
	
	button = gtk_button_new_with_label (_("_Save As..."));
	image = gtk_image_new_from_stock (GTK_STOCK_SAVE_AS, GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image (GTK_BUTTON (button), image);
	/* don't show the image! see bug #307818 */
	gtk_widget_show (button);
	gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, CONTENT_ACTION_SAVEAS);

	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       action_label, mAction);

	gtk_window_set_icon_name (GTK_WINDOW (dialog), EPHY_STOCK_EPHY);
 
	int defaultResponse = mAction == CONTENT_ACTION_NONE
				? (int) GTK_RESPONSE_CANCEL
				: (int) mAction;
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), defaultResponse);

	NS_ADDREF_THIS();
	g_signal_connect_data (dialog, "response",
			       G_CALLBACK (ResponseCallback), this,
			       (GClosureNotify) release_cb, (GConnectFlags) 0);

	/* FIXME: should find a way to get the user time of the user action which
	 * initiated this content handler
	 */
	gtk_window_present (GTK_WINDOW (dialog));

	return NS_OK;
}

NS_METHOD GContentHandler::MIMEInitiateAction (void)
{
	gboolean auto_downloads;

	if (eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_SAVE_TO_DISK)) return NS_OK;

	auto_downloads = eel_gconf_get_boolean (CONF_AUTO_DOWNLOADS);

	mHelperApp = g_app_info_get_default_for_type (mMimeType.get(), FALSE);
	mPermission = ephy_file_check_mime (mMimeType.get());

	/* HACK! Check that this 'helper application' isn't Epiphany itself,
	 * see bug #310023.
	 */
	if (mHelperApp)
	{
		const char *id = g_app_info_get_id (mHelperApp);

		/* FIXME! menu editing can make this check fail!!!! */
		if (id && strcmp (id, "epiphany.desktop") == 0)
		{
			mHelperApp = nsnull;
		}
	}

#ifndef HAVE_GECKO_1_9
	if (auto_downloads)
	{
		mAction = CONTENT_ACTION_OPEN;
	}
	else
#endif /* !HAVE_GECKO_1_9 */
	{
		mAction = CONTENT_ACTION_OPEN_TMP;
	}

	if (!mHelperApp || mPermission != EPHY_MIME_PERMISSION_SAFE)
	{
		mAction = CONTENT_ACTION_DOWNLOAD;
	}

	if (!auto_downloads || mAction == CONTENT_ACTION_DOWNLOAD)
	{
		MIMEConfirmAction ();
	}
	else
	{
		MIMEDoAction ();
	}

	return NS_OK;
}

NS_METHOD GContentHandler::MIMEDoAction (void)
{
	/* This is okay, since we either clicked on a button, or we get 0 */
	mUserTime = (PRUint32) gtk_get_current_event_time ();

	nsCOMPtr<nsIMIMEInfo> mimeInfo;
	mLauncher->GetMIMEInfo(getter_AddRefs(mimeInfo));
	NS_ENSURE_TRUE (mimeInfo, NS_ERROR_FAILURE);

#ifndef HAVE_GECKO_1_9
	char *info = NULL;

	if (mAction == CONTENT_ACTION_OPEN)
	{
		g_return_val_if_fail (mHelperApp, NS_ERROR_FAILURE);

		const char *id;
		id = g_app_info_get_id (mHelperApp);
		
		/* The current time is fine here as the user has just clicked
		 * a button (it is used as the time for the application opening)
		 */
		info = g_strdup_printf ("gnome-default:%d:%s", gtk_get_current_event_time(), id);
	}
	else if (mAction == CONTENT_ACTION_DOWNLOAD)
	{
		info = g_strdup_printf ("gnome-browse-to-file:%d", gtk_get_current_event_time());
	}

	if (info != NULL)
	{
		nsString desc;
		NS_CStringToUTF16 (nsCString (info),
			           NS_CSTRING_ENCODING_UTF8, desc);
		g_free (info);

		/* HACK we use the application description to ask
		   MozDownload to open the file when download
		   is finished */
		mimeInfo->SetApplicationDescription (desc);
	}
	else
	{
		mimeInfo->SetApplicationDescription (nsString ());
	}
#endif /* HAVE_GECKO_1_9 */

	nsRefPtr<GContentHandler> kungFuDeathGrip(this);

	if (mAction == CONTENT_ACTION_OPEN)
	{
		mLauncher->SaveToDisk (nsnull, PR_FALSE);
	}
	else if (mAction == CONTENT_ACTION_OPEN_TMP)
	{
		mLauncher->LaunchWithApplication (nsnull, PR_FALSE);
	}
	else if (mAction == CONTENT_ACTION_NONE)
	{
		mLauncher->Cancel (NS_BINDING_ABORTED);
	}
	else
	{
		mLauncher->SaveToDisk (nsnull, PR_FALSE);
	}

#ifdef HAVE_GECKO_1_9
	/* We have to do this work down here because the external helper app 
	 * modifies the value after calling SaveToDisk.
	 */
	nsHandlerInfoAction action;
	if (mAction == CONTENT_ACTION_DOWNLOAD) {
		action = EPHY_ACTION_BROWSE_TO_FILE;

		/* This won't be able to transport the activation time so we 
		 * cannot do startup notification, but it's the best that was 
		 * available
		 */
		mimeInfo->SetPreferredAction (action);
	}
#endif

	return NS_OK;
}
