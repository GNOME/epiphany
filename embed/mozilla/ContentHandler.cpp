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

#include "nsIDocShell.h"
#include "nsIWebNavigation.h" // Needed to create the LoadType flag

#include "ephy-prefs.h"
#include "eel-gconf-extensions.h"
#include "ephy-embed-single.h"
#include "ephy-embed-shell.h"
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

GContentHandler::GContentHandler() : mMimeType(nsnull),
				     mUrlHelper(PR_FALSE)
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

/* This is copied verbatim from nsDocShell.h in Mozilla, we should keep it synced */
/* http://lxr.mozilla.org/mozilla/source/docshell/base/nsDocShell.h */

#define MAKE_LOAD_TYPE(type, flags) ((type) | (flags << 16))

enum LoadType {
    LOAD_NORMAL = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL, nsIWebNavigation::LOAD_FLAGS_NONE),
    LOAD_NORMAL_REPLACE = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL, nsIWebNavigation::LOAD_FLAGS_REPLACE_HISTORY),
    LOAD_HISTORY = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_HISTORY, nsIWebNavigation::LOAD_FLAGS_NONE),
    LOAD_RELOAD_NORMAL = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_RELOAD, nsIWebNavigation::LOAD_FLAGS_NONE),
    LOAD_RELOAD_BYPASS_CACHE = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_RELOAD, nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE),
    LOAD_RELOAD_BYPASS_PROXY = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_RELOAD, nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY),
    LOAD_RELOAD_BYPASS_PROXY_AND_CACHE = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_RELOAD, nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE | nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY),
    LOAD_LINK = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL, nsIWebNavigation::LOAD_FLAGS_IS_LINK),
    LOAD_REFRESH = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL, nsIWebNavigation::LOAD_FLAGS_IS_REFRESH),
    LOAD_RELOAD_CHARSET_CHANGE = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_RELOAD, nsIWebNavigation::LOAD_FLAGS_CHARSET_CHANGE),
    LOAD_BYPASS_HISTORY = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL, nsIWebNavigation::LOAD_FLAGS_BYPASS_HISTORY)
};

static GtkWidget*
ch_unrequested_dialog_construct (GtkWindow *parent, const char *title)
{
	GtkWidget *dialog;
	GtkWidget *hbox, *vbox, *label, *image;
	char *str, *tmp_str, *tmp_title;

	dialog = gtk_dialog_new_with_buttons ("",
					      GTK_WINDOW (parent),
					      GTK_DIALOG_NO_SEPARATOR,
					      GTK_STOCK_CANCEL,
					      GTK_RESPONSE_CANCEL,
					      GTK_STOCK_OK,
					      GTK_RESPONSE_OK,
					      NULL);
	
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 14);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox,
			    TRUE, TRUE, 0);

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
	tmp_title = g_strconcat ("<b>", title, "</b>", NULL);
	tmp_str = g_strdup_printf (_("An unrequested download (%s) has been started.\n Would you like to continue it and open the file?"),
			           tmp_title);
	str = g_strconcat ("<big>", tmp_str, "</big>", NULL);
	gtk_label_set_markup (GTK_LABEL (label), str);
	g_free (tmp_title);
	g_free (tmp_str);
	g_free (str);
	gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
	gtk_widget_show (label);

	return dialog;
}

static void
ch_unrequested_dialog_cb (GtkWidget *dialog,
			  int response_id,
			  gpointer user_data)
{
	switch (response_id) 
	{
		case GTK_RESPONSE_OK:
			((GContentHandler*)user_data)->MIMEAskAction ();
			break;
		case GTK_RESPONSE_CANCEL:
			nsCOMPtr<nsIHelperAppLauncher> launcher;
		
			((GContentHandler*)user_data)->GetLauncher (getter_AddRefs(launcher));
			launcher->Cancel ();
			break;
	}
	
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

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

	mLauncher = aLauncher;
	rv = Init ();
	NS_ENSURE_SUCCESS (rv, rv);

	single = EPHY_EMBED_SINGLE (ephy_embed_shell_get_embed_single (embed_shell));
	g_signal_emit_by_name (single, "handle_content", mMimeType,
			       mUrl.get(), &handled);

	nsCOMPtr<nsIDocShell> eDocShell = do_QueryInterface(aContext);
	PRUint32 eLoadType;
	eDocShell->GetLoadType (&eLoadType);
	
	/* We ask the user what to do if he has not explicitely started the download
	 * (LoadType == LOAD_LINK) */
	
	if (eLoadType != LOAD_LINK) {
		GtkWidget *dialog;
		dialog = ch_unrequested_dialog_construct (NULL, mUrl.get()); 
		g_signal_connect (G_OBJECT (dialog),
				  "response",
				  G_CALLBACK (ch_unrequested_dialog_cb),
				  this);
		gtk_widget_show (dialog);
	}
	else if (!handled)
	{
		MIMEAskAction ();
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
	return BuildDownloadPath (NS_ConvertUCS2toUTF8 (aDefaultFile).get(),
				  _retval);
}

#if MOZILLA_SNAPSHOT < 10
/* void showProgressDialog (in nsIHelperAppLauncher aLauncher, in nsISupports aContext); */
NS_METHOD GContentHandler::ShowProgressDialog(nsIHelperAppLauncher *aLauncher,
					      nsISupports *aContext)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}
#endif

NS_METHOD GContentHandler::FindHelperApp (void)
{
	if (mUrlHelper)
	{
		return LaunchHelperApp ();
	}

	nsresult rv;
	rv = SynchroniseMIMEInfo();
	if (NS_FAILED (rv)) return NS_ERROR_FAILURE;

	return mLauncher->LaunchWithApplication (nsnull, PR_FALSE);
}

NS_METHOD GContentHandler::LaunchHelperApp (void)
{
	if (mMimeType)
	{
		nsresult rv;
		nsCOMPtr<nsIExternalHelperAppService> helperService =
			do_GetService (NS_EXTERNALHELPERAPPSERVICE_CONTRACTID);
		NS_ENSURE_TRUE (helperService, NS_ERROR_FAILURE);

		nsCOMPtr<nsPIExternalAppLauncher> appLauncher =
			do_QueryInterface (helperService);
		if (appLauncher)
		{
			appLauncher->DeleteTemporaryFileOnExit(mTempFile);
		}

		nsString uFileName;
		mTempFile->GetPath(uFileName);
		const nsCString &aFileName = NS_ConvertUCS2toUTF8(uFileName);

		const nsCString &document = (mUrlHelper) ? mUrl : aFileName;

		char *param = g_strdup (document.get());
		GList *params = NULL;
		params = g_list_append (params, param);
		gnome_vfs_mime_application_launch (mHelperApp, params);
		g_free (param);
		g_list_free (params);

		if (mUrlHelper)
		{
			mLauncher->Cancel();
		}
	}
	else
	{
		mLauncher->Cancel ();
	}

	return NS_OK;
}

NS_METHOD GContentHandler::GetLauncher (nsIHelperAppLauncher * *_retval)
{
	NS_IF_ADDREF (*_retval = mLauncher);
	return NS_OK;
}

static gboolean 
application_support_scheme (GnomeVFSMimeApplication *app, const nsCString &aScheme)
{
	GList *l;

	g_return_val_if_fail (app != NULL, FALSE);
	g_return_val_if_fail (!aScheme.IsEmpty(), FALSE);
	
	if (app->expects_uris != GNOME_VFS_MIME_APPLICATION_ARGUMENT_TYPE_URIS)
		return FALSE;
	
	for (l = app->supported_uri_schemes; l != NULL; l = l->next)
	{
		char *uri_scheme = (char *)l->data;
		g_return_val_if_fail (uri_scheme != NULL, FALSE);
		if (aScheme.Equals (uri_scheme)) return TRUE;
	}

	return FALSE;
}

NS_METHOD GContentHandler::SetHelperApp(GnomeVFSMimeApplication *aHelperApp,
					PRBool alwaysUse)
{
	mHelperApp = aHelperApp;
	mUrlHelper = application_support_scheme (aHelperApp, mScheme);

	return NS_OK;
}

NS_METHOD GContentHandler::SynchroniseMIMEInfo (void)
{
	nsresult rv;
	char *command_with_path;

	NS_ENSURE_TRUE (mLauncher, NS_ERROR_FAILURE);

	nsCOMPtr<nsIMIMEInfo> mimeInfo;
	mLauncher->GetMIMEInfo(getter_AddRefs(mimeInfo));
	NS_ENSURE_TRUE (mimeInfo, NS_ERROR_FAILURE);

	command_with_path = g_find_program_in_path (mHelperApp->command);
	if (command_with_path == NULL) return NS_ERROR_FAILURE;

	nsCOMPtr<nsILocalFile> helperFile;
	NS_NewNativeLocalFile (nsDependentCString(command_with_path),
			       PR_TRUE,
			       getter_AddRefs(helperFile));
	NS_ENSURE_TRUE (helperFile, NS_ERROR_FAILURE);

	g_free (command_with_path);

	rv = mimeInfo->SetPreferredApplicationHandler(helperFile);
	NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

	nsMIMEInfoHandleAction mimeInfoAction;
	mimeInfoAction = nsIMIMEInfo::useHelperApp;

	if(mHelperApp->requires_terminal) //Information passing kludge!
	{
		rv = mimeInfo->SetApplicationDescription
				(NS_LITERAL_STRING("runInTerminal").get());
		NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);
	}

	rv = mimeInfo->SetPreferredAction(mimeInfoAction);
	NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

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

	ProcessMimeInfo ();

	return NS_OK;
}

NS_METHOD GContentHandler::ProcessMimeInfo (void)
{
	if (mMimeType == NULL ||
	    !nsCRT::strcmp(mMimeType, "application/octet-stream"))
	{
		/* FIXME: we leak the old value of mMimeType */
		
		nsresult rv;
		nsCOMPtr<nsIURL> url = do_QueryInterface(mUri, &rv);
		if (NS_SUCCEEDED(rv) && url)
		{
			nsCAutoString uriFileName;
			url->GetFileName(uriFileName);
			mMimeType = g_strdup
					(gnome_vfs_mime_type_from_name
						(uriFileName.get()));
		}
		else
		{
			mMimeType = g_strdup ("application/octet-stream");
		}
	}

	return NS_OK;
}

NS_METHOD GContentHandler::MIMEAskAction (void)
{
	nsresult rv;
	gboolean auto_open;

	auto_open = eel_gconf_get_boolean (CONF_AUTO_OPEN_DOWNLOADS);
	GnomeVFSMimeApplication *DefaultApp = gnome_vfs_mime_get_default_application(mMimeType);

	EphyMimePermission permission;
	permission = ephy_embed_shell_check_mime (embed_shell, mMimeType);
	
	if (!auto_open || !DefaultApp || permission != EPHY_MIME_PERMISSION_SAFE)
	{
		if (eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_SAVE_TO_DISK)) return NS_OK;

		nsCOMPtr<nsIHelperAppLauncher> launcher;
		GetLauncher (getter_AddRefs(launcher));
		NS_ENSURE_TRUE (launcher, NS_ERROR_FAILURE);

		launcher->SaveToDisk (nsnull,PR_FALSE);
	}
	else
	{
		rv = SetHelperApp (DefaultApp, FALSE);
		NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

		rv = FindHelperApp ();
		NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);
	}

	return NS_OK;
}
