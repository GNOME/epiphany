/*
 *  Copyright (C) 2001 Philip Langdale
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
 * The functioning of the download architecture, as described by Philip
 * on 28 May 2001 and updated on 28 June 2001:
 * 
 * When mozilla runs into a file it cannot render internally or that it
 * does not have a plugin for, it calls the
 * nsIExternalHelperAppService. This service will then either attempt to
 * save the file or run it with a helper app depending on what the
 * mozilla mime database returns.
 * 
 * nsIExternalHelperAppService then calls out to the nsIHelperAppDialog
 * interface which handles the UI for the service. This is the interface
 * which we have reimplemented. Therefore, with a major caveat, we have
 * put a GNOME/GTK frontend on an unmodified mozilla backend.
 * 
 * Now for the caveat. With respect to saving files to disk, the mozilla
 * backend works exactly the same as it does in
 * mozilla-the-browser. However, for dealing with helper apps, we do not
 * use the mozilla backend at all. This is because we want to use the
 * gnome-vfs database to retrieve helper-app info, rather than the
 * mozilla helper app database.
 * 
 * How it works:
 * 
 * a) The user clicks on a link or follows a redirect to a file of a type
 * that mozilla cannot handle. Mozilla passes the link to the
 * ExternalHelperAppService which in turn calls the Show() method of
 * nsIHelperAppDialog.
 * 
 * b) In our implementation of Show() we first compare the passed mime
 * type to epiphany's mime list. If the mime type is in the list, we then
 * lookup the Action associated with the mime type. Currently, the
 * possible mime-actions are:
 * 
 * Save to Disk
 * Run with Helper App
 * Ask User
 * 
 * The default action is Ask User, and if the mime-type is not in our
 * list, this is what will be assumed.
 * 
 * c) If Ask User is the chosen action, a dialog will be shown to the
 * user allowing the user to choose from the other two possible actions
 * as well as a checkbox to let the user set the default action to the
 * chosen action for the future.
 * 
 * d-1) The "Save to Disk" action. We first check epiphany preferences to
 * see if the user wants to use the built-in mozilla downloader, gtm or
 * a command-line executed downloader.
 *
 * d-2a) The built-in downloader.  This action is handled by the mozilla
 * backend. Our nsIHelperAppDialog does the same thing that the
 * mozilla-the-browser one does, which is to call the SaveToDisk() method
 * of nsIExternalHelperAppService. This in turn calls the
 * PromptForSaveToFile() method of nsIHelperAppDialog putting the ball
 * back in our court.
 * 
 * d-2b) Now, if epiphany is configured to always ask for a download
 * directory, it will pop up a file selector so that the user can select
 * the directory and filename to save the file to.  Otherwise, it will
 * use epiphany's default download directory and proceed without
 * interaction.
 * 
 * d-2c) When PromptForSaveToFile() returns, nsIExternalHelperAppService
 * will then call the ShowProgressDialog() method of
 * nsIHelperAppDialog. This progress dialog, obviously, tracks the
 * progress of the download. It is worth noting that mozilla starts the
 * actual download as soon as the user clicks on the link or follows the
 * redirect. While the user is deciding what action to take, the file is
 * downloading. Often, for small files, the file is already downloaded
 * when the user decides what directory to put it in. The progress dialog
 * does not appear in these cases. Also, we currently have a small
 * problem where our progress dialog times the download from the point
 * the dialog appears, not from the time the download starts. This is due
 * to the timestamp that is passed to us is just plain weird, and I
 * haven't worked out how to turn it into a useable time. The fact that
 * the download starts early means that the file is actually downloaded
 * to a temp file and only at the end is it moved to it's final location.
 * 
 * d-3a) The two external downloader options.  These options are
 * handled completely by epiphany. The first thing that we do is call the
 * Cancel() method of nsIExternalHelperAppService to cancel the mozilla
 * download. We then pass the url to our own LaunchExternalDownloader()
 * method. This method will ask for a download directory as appropriate
 * as with the "Save to disk" action.
 * 
 * d-3b) Finally, depending on whether GTM or a command line handler was
 * selected in prefs, the external handler will be called with the url
 * passed and the directory selected.
 * 
 * e-1) The "Run with Helper App" action.  This action is currently only
 * working with a minimal implementation.  First, we explicitly call
 * ShowProgressDialog() so the user knows that the file is being
 * downloaded. We also need this so that we only run the helper after the
 * file is completely downloaded. The file will download to temp location
 * that it would be moved from if the action was "Save to Disk".  We have
 * to call ShowProgressDialog() ourselves because we are not using
 * mozilla's helper mechanism which would usually make the call for us.
 * 
 * e-2) If there is a default helper app in our mime database and alwaysAsk
 * is false, epiphany will run the default helper automatically. Otherwise it
 * will pop up a helper chooser dialog which lists the helpers that gnome-vfs
 * knows about as well as providing a GnomeFileEntry to allow the user to
 * select and arbitrary application. The default value of the GnomeFileEntry
 * is the helper stored in our database if one exits.
 * 
 * f) General notes.  We cannot use this infrastructure to override
 * native mozilla types. mozilla will attempt to render these types and
 * never call out to us. We are at the end of the chain as the handler of
 * last resort, so native and plugin types will never reach us. This also
 * means that a file with an incorrect mime-type ( eg: .tar.bz2 marked as
 * text/plain ) will be incorrectly rendered by mozilla. We cannot help
 * this.
 * 
 * Despite the apparent user-side similarity with explicit downloads by
 * a shift-click or context-menu item, there is actually none at all.
 * Explicit downloads are handled by the nsIStreamTransfer manager which
 * we use as is. Currently the progress dialog for the stream transfer
 * manager is un-overridable, so it appears in XUL. This will change in
 * due course.
 * 
 * Matt would like the modifiy the progress dialog so each file currently
 * being downloaded becomes a clist entry in a master dialog rather than
 * causing a separate progress dialog. a lot of progress dialogs gets
 * really messy.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

extern "C" {
#include "libgnomevfs/gnome-vfs-mime-handlers.h"
}

#include "ephy-embed-shell.h"
#include "ephy-prefs.h"
#include "eel-gconf-extensions.h"
#include "ephy-glade.h"
#include "ephy-string.h"
#include "ephy-gui.h"
#include "ephy-embed-utils.h"
#include "ephy-file-helpers.h"
#include "ContentHandler.h"

#include <gtk/gtkentry.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkprogress.h>
#include <gtk/gtkoptionmenu.h>
#include <libgnome/gnome-exec.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-config.h>
#include <libgnome/gnome-util.h>
#include <libgnomevfs/gnome-vfs-mime.h>

#include "FilePicker.h"
#include "MozillaPrivate.h"

#include "nsCRT.h"
#include "nsCOMPtr.h"
#include "nsISupportsArray.h"
#include "nsIServiceManager.h"
#include "nsWeakReference.h"

#include "nsString.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIChannel.h"
#include "nsILocalFile.h"
#include "nsIPrefService.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowInternal.h"
#include "nsIMIMEInfo.h"

class GContentHandler;
//class GDownloadProgressListener;
struct MimeAskActionDialog;
struct HelperAppChooserDialog;

enum
{
	RESPONSE_SAVE = 1,
	RESPONSE_OPEN = 2
};

/*
 * MimeAskActionDialog: the representation of dialogs used to ask
 * about actions on MIME types
 */
struct MimeAskActionDialog
{
	MimeAskActionDialog(GContentHandler *aContentHandler,
			    GtkWidget *aParentWidget,
			    const char *aMimeType);
	~MimeAskActionDialog();

	GContentHandler *mContentHandler;
	GladeXML *mGXml;
	GtkWidget *mParent;
	GtkWidget *mAppMenu;
	
	GnomeVFSMimeApplication *mDefaultApp;
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(GContentHandler, nsIHelperAppLauncherDialog)

GContentHandler::GContentHandler() : mUri(nsnull),
				     mMimeType(nsnull),
				     mDownloadCanceled(PR_FALSE)
{
	NS_INIT_ISUPPORTS();
	/* member initializers and constructor code */
}

GContentHandler::~GContentHandler()
{
	/* destructor code */
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
	/* aForced reflects if the content being sent is normally viewable
	 * in mozilla or not. That fact doesn't affect us, so ignore it
         */

	nsresult rv;

	mLauncher = aLauncher;
	mContext = aContext;
	rv = Init ();
	
	MIMEAskAction ();

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
	nsresult rv;

	mContext = aWindowContext;

	nsCOMPtr<nsIDOMWindowInternal> windowInternal = 
					do_QueryInterface (aWindowContext);
	
	nsCOMPtr<nsILocalFile> saveDir;
	char *dirName;
	
	dirName = eel_gconf_get_string (CONF_STATE_DOWNLOADING_DIR);
	if (dirName == NULL)
	{
		dirName = g_strdup (g_get_home_dir());
	}

	saveDir = do_CreateInstance (NS_LOCAL_FILE_CONTRACTID);
	saveDir->InitWithPath (NS_ConvertUTF8toUCS2(dirName));
	g_free (dirName);

	nsCOMPtr <nsILocalFile> saveFile (do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));

	PRInt16 okToSave = nsIFilePicker::returnCancel;

	nsCOMPtr<nsIFilePicker> filePicker =
				do_CreateInstance (G_FILEPICKER_CONTRACTID);

	const nsAString &title = NS_ConvertUTF8toUCS2(_("Select the destination filename"));

	filePicker->Init (windowInternal,
			   PromiseFlatString(title).get(), 
			   nsIFilePicker::modeSave);
	filePicker->SetDefaultString (aDefaultFile);
	filePicker->SetDisplayDirectory (saveDir);

	filePicker->Show (&okToSave);

	if (okToSave == nsIFilePicker::returnOK)
	{
		filePicker->GetFile (getter_AddRefs(saveFile));

		nsString uFileName;
		saveFile->GetPath(uFileName);
		const nsCString &aFileName = NS_ConvertUCS2toUTF8(uFileName);

		char *dir = g_path_get_dirname (aFileName.get());

		eel_gconf_set_string (CONF_STATE_DOWNLOADING_DIR, dir);
		g_free (dir);

		nsCOMPtr<nsIFile> directory;
		rv = saveFile->GetParent (getter_AddRefs(directory));

		NS_IF_ADDREF (*_retval = saveFile);
		return NS_OK;
	}
	else
	{
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

////////////////////////////////////////////////////////////////////////////////
// begin local public methods impl
////////////////////////////////////////////////////////////////////////////////

NS_METHOD GContentHandler::FindHelperApp (void)
{
	if (mUrlHelper)
	{
		return LaunchHelperApp ();
	}
	else
	{
		if (NS_SUCCEEDED(SynchroniseMIMEInfo()))
		{
			return mLauncher->LaunchWithApplication(nsnull, PR_FALSE);
		}
		else
		{
				return NS_ERROR_FAILURE;
		}
	}
}

NS_METHOD GContentHandler::LaunchHelperApp (void)
{
	if (mMimeType)
	{
		nsresult rv;
		nsCOMPtr<nsIExternalHelperAppService> helperService =
			do_GetService (NS_EXTERNALHELPERAPPSERVICE_CONTRACTID);

		nsCOMPtr<nsPIExternalAppLauncher> appLauncher =
			do_QueryInterface (helperService, &rv);
		if (NS_SUCCEEDED(rv))
		{
			appLauncher->DeleteTemporaryFileOnExit(mTempFile);
		}

		nsString uFileName;
		mTempFile->GetPath(uFileName);
		const nsCString &aFileName = NS_ConvertUCS2toUTF8(uFileName);

		const nsCString &document = (mUrlHelper) ? mUrl : aFileName;

		char *param = g_strdup (document.get());
		ephy_file_launch_application (mHelperApp->command,
					      param,
					      mHelperApp->requires_terminal);

		if(mUrlHelper) mLauncher->Cancel();

		g_free (param);
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

NS_METHOD GContentHandler::GetContext (nsISupports * *_retval)
{
	NS_IF_ADDREF (*_retval = mContext);
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
		if (aScheme.Equals(uri_scheme)) return TRUE;
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

	nsCOMPtr<nsIMIMEInfo> mimeInfo;
	rv = mLauncher->GetMIMEInfo(getter_AddRefs(mimeInfo));
	if(NS_FAILED(rv)) return NS_ERROR_FAILURE;

	command_with_path = g_find_program_in_path (mHelperApp->command);
	if (command_with_path == NULL) return NS_ERROR_FAILURE;
	nsCOMPtr<nsILocalFile> helperFile;
	rv = NS_NewNativeLocalFile(nsDependentCString(command_with_path),
				   PR_TRUE,
				   getter_AddRefs(helperFile));
	if(NS_FAILED(rv)) return NS_ERROR_FAILURE;
	g_free (command_with_path);

	rv = mimeInfo->SetPreferredApplicationHandler(helperFile);
	if(NS_FAILED(rv)) return NS_ERROR_FAILURE;	

	nsMIMEInfoHandleAction mimeInfoAction;
	mimeInfoAction = nsIMIMEInfo::useHelperApp;

	if(mHelperApp->requires_terminal) //Information passing kludge!
	{
		rv = mimeInfo->SetApplicationDescription
				(NS_LITERAL_STRING("runInTerminal").get());
		if(NS_FAILED(rv)) return NS_ERROR_FAILURE;
	}

	rv = mimeInfo->SetPreferredAction(mimeInfoAction);
	if(NS_FAILED(rv)) return NS_ERROR_FAILURE;

	return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// begin local private methods impl
////////////////////////////////////////////////////////////////////////////////
NS_METHOD GContentHandler::Init (void)
{
	nsresult rv;

	nsCOMPtr<nsIMIMEInfo> MIMEInfo;
	rv = mLauncher->GetMIMEInfo (getter_AddRefs(MIMEInfo));
	rv = MIMEInfo->GetMIMEType (&mMimeType);

#if MOZILLA_SNAPSHOT > 11
	rv = mLauncher->GetSource(getter_AddRefs(mUri));
	rv = mLauncher->GetTargetFile(getter_AddRefs(mTempFile));
#else
	rv = mLauncher->GetDownloadInfo(getter_AddRefs(mUri),
					&mTimeDownloadStarted,
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
			mMimeType = g_strdup ("application/octet-stream");
	}

	return NS_OK;
}

NS_METHOD GContentHandler::MIMEAskAction (void)
{
	nsCOMPtr<nsIDOMWindow> parent = do_QueryInterface (mContext);
	GtkWidget *parentWidget = MozillaFindGtkParent (parent);

	new MimeAskActionDialog(this, parentWidget, mMimeType);

	return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// begin MIMEAskActionDialog methods.
////////////////////////////////////////////////////////////////////////////////

MimeAskActionDialog::MimeAskActionDialog(GContentHandler *aContentHandler,
					 GtkWidget *aParentWidget,
					 const char *aMimeType) :
					 mContentHandler(aContentHandler),
					 mParent(aParentWidget)
{
	GdkPixbuf *mime_icon;
	GtkWidget *label;
	GtkWidget *dialogWidget;
	const char *description;
	char ltext[255]; //philipl: Fixed length buffer == potential security problem...

	mGXml = ephy_glade_widget_new ("epiphany.glade", "mime_ask_action_dialog", 
				      &dialogWidget, this);
	mAppMenu = glade_xml_get_widget (mGXml, "mime_ask_dialog_app_menu");

	mDefaultApp = gnome_vfs_mime_get_default_application(aMimeType);

	GtkWidget *aMimeIcon = glade_xml_get_widget (mGXml,
						     "mime_ask_action_icon");
	mime_icon = ephy_gui_get_pixbuf_from_mime_type (aMimeType, 32);
	gtk_image_set_from_pixbuf (GTK_IMAGE(aMimeIcon), mime_icon);
	g_object_unref (mime_icon);

	description = gnome_vfs_mime_get_description (aMimeType);
	if (!description) description = aMimeType;
	
	g_snprintf (ltext, 255, "<b>%s</b>", description);
	label = glade_xml_get_widget (mGXml, "mime_ask_action_description");
	gtk_label_set_markup (GTK_LABEL (label), ltext);

	gtk_window_set_transient_for (GTK_WINDOW (dialogWidget), 
				      GTK_WINDOW (aParentWidget));

	gtk_widget_show(dialogWidget);
}

MimeAskActionDialog::~MimeAskActionDialog()
{
#if 0
	if(mApps)
		gnome_vfs_mime_application_list_free(mApps);
#endif

	gtk_widget_destroy(glade_xml_get_widget(mGXml, "mime_ask_action_dialog"));
	g_object_unref(G_OBJECT(mGXml));
}

////////////////////////////////////////////////////////////////////////////////
// begin MIMEAskActionDialog callbacks.
////////////////////////////////////////////////////////////////////////////////

static void
mime_ask_dialog_save (MimeAskActionDialog *dialog)
{
	gtk_widget_hide (glade_xml_get_widget (dialog->mGXml, 
					       "mime_ask_action_dialog"));

	nsresult rv;
	nsCOMPtr<nsIHelperAppLauncher> launcher;
	rv = dialog->mContentHandler->GetLauncher (getter_AddRefs(launcher));
	
	launcher->SaveToDisk (nsnull,PR_FALSE);
	
	delete dialog;
}

static void
mime_ask_dialog_cancel (MimeAskActionDialog *dialog)
{
	nsresult rv;
	nsCOMPtr<nsIHelperAppLauncher> launcher;
	rv = dialog->mContentHandler->GetLauncher (getter_AddRefs(launcher));

	launcher->Cancel ();

	delete dialog;
}

static void
mime_ask_dialog_open (MimeAskActionDialog *dialog)
{
	nsresult rv;
	nsCOMPtr<nsIHelperAppLauncher> launcher;
	rv = dialog->mContentHandler->GetLauncher (getter_AddRefs(launcher));
	GnomeVFSMimeApplication *app = dialog->mDefaultApp;

	if (app)
	{
		dialog->mContentHandler->SetHelperApp (app, FALSE);
		dialog->mContentHandler->FindHelperApp ();
		delete dialog;
	}
	else
	{	
		mime_ask_dialog_cancel (dialog);
		ephy_embed_utils_nohandler_dialog_run (dialog->mParent);
	}
}

extern "C" void
mime_ask_dialog_response_cb (GtkDialog *gtkdialog, int response, MimeAskActionDialog *dialog)
{
	switch (response)
	{
		case RESPONSE_SAVE:
			mime_ask_dialog_save (dialog);
			break;
		case RESPONSE_OPEN:
			mime_ask_dialog_open (dialog);
			break;
		default:
			mime_ask_dialog_cancel (dialog);
			break;
	}
}
