/*
 *  Copyright (C) 2001 Philip Langdale
 *  Copyright (C) 2003 Marco Pesenti Gritti
 *  Copyright (C) 2003 Xan Lopez
 *  Copyright (C) 2004 Christian Persch
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ContentHandler.h"
#include "MozillaPrivate.h"
#include "MozDownload.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIURL.h"
#include "nsILocalFile.h"
#include "nsIMIMEInfo.h"

#include "nsIWebNavigation.h" // Needed to create the LoadType flag

#include "ephy-prefs.h"
#include "eel-gconf-extensions.h"
#include "ephy-embed-single.h"
#include "ephy-embed-shell.h"
#include "ephy-file-chooser.h"
#include "ephy-debug.h"

#include <gtk/gtkimage.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkstock.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkdialog.h>
#include <libgnomevfs/gnome-vfs-mime.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <glib/gi18n.h>

class GContentHandler;

NS_IMPL_ISUPPORTS1(GContentHandler, nsIHelperAppLauncherDialog)

GContentHandler::GContentHandler() : mMimeType(nsnull)
{
	LOG ("GContentHandler ctor")
}

GContentHandler::~GContentHandler()
{
	LOG ("GContentHandler dtor")

	g_free (mUri);
	g_free (mMimeType);
}

////////////////////////////////////////////////////////////////////////////////
// begin nsIHelperAppLauncher impl
////////////////////////////////////////////////////////////////////////////////

#if MOZILLA_SNAPSHOT > 9
/* void show (in nsIHelperAppLauncher aLauncher, in nsISupports aContext); */
NS_IMETHODIMP GContentHandler::Show(nsIHelperAppLauncher *aLauncher,
				    nsISupports *aContext,
				    PRBool aForced)
#else
NS_IMETHODIMP GContentHandler::Show(nsIHelperAppLauncher *aLauncher,
				    nsISupports *aContext)
#endif
{
	nsresult rv;
	EphyEmbedSingle *single;
	gboolean handled = FALSE;

	mContext = aContext;
	mLauncher = aLauncher;
	rv = Init ();
	NS_ENSURE_SUCCESS (rv, rv);

	single = EPHY_EMBED_SINGLE (ephy_embed_shell_get_embed_single (embed_shell));
	g_signal_emit_by_name (single, "handle_content", mMimeType,
			       mUrl.get(), &handled);
	
	if (!handled)
	{
		MIMEDoAction ();
	}
	else
	{
		mLauncher->Cancel ();
	}

	return NS_OK;
}

/* nsILocalFile promptForSaveToFile (in nsISupports aWindowContext, in wstring aDefaultFile, in wstring aSuggestedFileExtension); */
NS_IMETHODIMP GContentHandler::PromptForSaveToFile(
#if MOZILLA_SNAPSHOT > 10
				    nsIHelperAppLauncher *aLauncher,
#endif				    
				    nsISupports *aWindowContext,
				    const PRUnichar *aDefaultFile,
				    const PRUnichar *aSuggestedFileExtension,
				    nsILocalFile **_retval)
{
	EphyFileChooser *dialog;
	gint response;
	char *filename;

	if (mAction != CONTENT_ACTION_SAVEAS)
	{
		return BuildDownloadPath (NS_ConvertUCS2toUTF8 (aDefaultFile).get(),
					  _retval);
	}

	nsCOMPtr<nsIDOMWindow> parentDOMWindow = do_QueryInterface (aWindowContext);
	GtkWidget *parentWindow = GTK_WIDGET (MozillaFindGtkParent (parentDOMWindow));

	dialog = ephy_file_chooser_new (_("Save"), parentWindow,
					GTK_FILE_CHOOSER_ACTION_SAVE,
					CONF_STATE_SAVE_DIR);
	gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog),
					   NS_ConvertUCS2toUTF8 (aDefaultFile).get());
	response = gtk_dialog_run (GTK_DIALOG (dialog));

	if (response == EPHY_RESPONSE_SAVE)
	{
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

		nsCOMPtr <nsILocalFile> destFile (do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));
		destFile->InitWithNativePath (nsDependentCString (filename));
		g_free (filename);

		NS_IF_ADDREF (*_retval = destFile);

		gtk_widget_destroy (GTK_WIDGET (dialog));

		return NS_OK;
	}
	else
	{
		gtk_widget_destroy (GTK_WIDGET (dialog));

		return NS_ERROR_FAILURE;
	}
}

#if MOZILLA_SNAPSHOT < 10
/* void showProgressDialog (in nsIHelperAppLauncher aLauncher, in nsISupports aContext); */
NS_METHOD GContentHandler::ShowProgressDialog(nsIHelperAppLauncher *aLauncher,
					      nsISupports *aContext)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}
#endif

NS_METHOD GContentHandler::LaunchHelperApp (void)
{
	nsresult rv;
	nsCOMPtr<nsIExternalHelperAppService> helperService;

	helperService = do_GetService (NS_EXTERNALHELPERAPPSERVICE_CONTRACTID);
	NS_ENSURE_TRUE (helperService, NS_ERROR_FAILURE);

	nsCOMPtr<nsPIExternalAppLauncher> appLauncher = do_QueryInterface (helperService);
	NS_ENSURE_TRUE (appLauncher, NS_ERROR_FAILURE);
	appLauncher->DeleteTemporaryFileOnExit(mTempFile);

	GList *params = NULL;
	char *param;
	
	param = gnome_vfs_make_uri_canonical (mUrl.get());
	params = g_list_append (params, param);
	gnome_vfs_mime_application_launch (mHelperApp, params);
	g_free (param);
	g_list_free (params);

	mLauncher->Cancel();

	return NS_OK;
}

NS_METHOD GContentHandler::CheckAppSupportScheme (void)
{
	GList *l;

	mAppSupportScheme = PR_FALSE;	

	if (!mHelperApp) return NS_OK;

	if (mHelperApp->expects_uris != GNOME_VFS_MIME_APPLICATION_ARGUMENT_TYPE_URIS)
		return NS_OK;
	
	for (l = mHelperApp->supported_uri_schemes; l != NULL; l = l->next)
	{
		char *uri_scheme = (char *)l->data;

		if (mScheme.Equals (uri_scheme))
		{
			mAppSupportScheme = PR_TRUE;
		}
	}

	return NS_OK;
}

NS_METHOD GContentHandler::Init (void)
{
	nsresult rv;

	NS_ENSURE_TRUE (mLauncher, NS_ERROR_FAILURE);

	nsCOMPtr<nsIMIMEInfo> MIMEInfo;
	mLauncher->GetMIMEInfo (getter_AddRefs(MIMEInfo));
	NS_ENSURE_TRUE (MIMEInfo, NS_ERROR_FAILURE);

	rv = MIMEInfo->GetMIMEType (&mMimeType);

#if MOZILLA_SNAPSHOT > 11
	mLauncher->GetTargetFile (getter_AddRefs(mTempFile));

	mLauncher->GetSource (getter_AddRefs(mUri));
	NS_ENSURE_TRUE (mUri, NS_ERROR_FAILURE);
#else
	PRInt64 TimeDownloadStarted;
	rv = mLauncher->GetDownloadInfo (getter_AddRefs(mUri),
					&TimeDownloadStarted,
					getter_AddRefs(mTempFile));
#endif
	
	rv = mUri->GetSpec (mUrl);
	rv = mUri->GetScheme (mScheme);

	return NS_OK;
}

NS_METHOD GContentHandler::MIMEConfirmAction ()
{
	GtkWidget *dialog;
	GtkWidget *hbox, *vbox, *label, *image;
	char *text;
	int response;

	nsCOMPtr<nsIDOMWindow> parentDOMWindow = do_QueryInterface (mContext);
	GtkWindow *parentWindow = GTK_WINDOW (MozillaFindGtkParent(parentDOMWindow));

	dialog = gtk_dialog_new_with_buttons
		("", parentWindow, GTK_DIALOG_NO_SEPARATOR,
		 _("_Save As..."), CONTENT_ACTION_SAVEAS,
		 GTK_STOCK_CANCEL, CONTENT_ACTION_NONE,
		 mAction == CONTENT_ACTION_OPEN ?
		 _("_Open") : _("_Download"), mAction,
		 NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), (guint)mAction);
	
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 14);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox,
			    TRUE, TRUE, 0);

	if (mPermission != EPHY_MIME_PERMISSION_SAFE)
	{
		text = g_strdup_printf ("<span weight=\"bold\" size=\"larger\">%s</span>\n\n%s",
					_("Download the unsafe file?"),
					_("This type of file could potentially damage your documents "
					  "or invade your privacy. "
					  "It's not safe to open it directly. You "
					  "can save it instead."));
	}
	else if (mAction == CONTENT_ACTION_OPEN)
	{
		text = g_strdup_printf ("<span weight=\"bold\" size=\"larger\">%s</span>\n\n%s",
					_("Open the file in another application?"),
					_("It's not possible to view this file type directly "
					  "in the browser. You can open it with another "
					  "application or save it."));
	}
	else
	{
		text = g_strdup_printf ("<span weight=\"bold\" size=\"larger\">%s</span>\n\n%s",
					_("Download the file?"),
					_("It's not possible to view this file because there is no "
					  "application installed that can open it."
					  "You can save it instead."));
	}

	image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_WARNING,
					  GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment (GTK_MISC (image), 0.5, 0.0);
	gtk_widget_show (image);
	gtk_box_pack_start (GTK_BOX (hbox), image, TRUE, TRUE, 0);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox);
	gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

	label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_label_set_markup (GTK_LABEL (label), text);
	g_free (text);

	gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
	gtk_widget_show (label);

	mAction = (ContentAction) gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);

	return NS_OK;
}

NS_METHOD GContentHandler::MIMEDoAction (void)
{
	nsresult rv;
	gboolean auto_downloads;

	if (eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_SAVE_TO_DISK)) return NS_OK;

	auto_downloads = eel_gconf_get_boolean (CONF_AUTO_DOWNLOADS);

	mHelperApp = gnome_vfs_mime_get_default_application (mMimeType); 
	CheckAppSupportScheme ();
	mPermission = ephy_embed_shell_check_mime (embed_shell, mMimeType);

	mAction = CONTENT_ACTION_OPEN;

	if (mPermission != EPHY_MIME_PERMISSION_SAFE)
	{
		mAction = CONTENT_ACTION_DOWNLOAD;
	}

	if (!auto_downloads)
	{
		MIMEConfirmAction ();
	}

	nsCOMPtr<nsIMIMEInfo> mimeInfo;
	mLauncher->GetMIMEInfo(getter_AddRefs(mimeInfo));
	NS_ENSURE_TRUE (mimeInfo, NS_ERROR_FAILURE);

	if (mAction == CONTENT_ACTION_OPEN)
	{
		/* HACK we use the application description to ask
		   MozDownload to open the file when download
		   is finished */
		mimeInfo->SetApplicationDescription
			(NS_LITERAL_STRING ("gnome-default").get());
	}
	else
	{
		mimeInfo->SetApplicationDescription (nsnull);
	}

	if (mAction == CONTENT_ACTION_OPEN && mAppSupportScheme)
	{
		LaunchHelperApp ();
	}
	else if (mAction == CONTENT_ACTION_NONE)
	{
		mLauncher->Cancel ();
	}
	else
	{
		mLauncher->SaveToDisk (nsnull,PR_FALSE);
	}

	return NS_OK;
}
