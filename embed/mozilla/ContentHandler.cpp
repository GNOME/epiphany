/*
 *  Copyright (C) 2001 Philip Langdale
 *  		  2003 Marco Pesenti Gritti, Xan Lopez
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

#include "ephy-prefs.h"
#include "eel-gconf-extensions.h"
#include "ephy-embed-shell.h"

#include <libgnomevfs/gnome-vfs-mime.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <glib/gi18n.h>

class GContentHandler;

/* Implementation file */
NS_IMPL_ISUPPORTS1(GContentHandler, nsIHelperAppLauncherDialog)

GContentHandler::GContentHandler() : mUri(nsnull),
				     mMimeType(nsnull)
{
}

GContentHandler::~GContentHandler()
{
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

	mLauncher = aLauncher;
	rv = Init ();
	if (NS_FAILED (rv)) return rv;

	single = ephy_embed_shell_get_embed_single (embed_shell);
	g_signal_emit_by_name (single, "handle_content", mMimeType,
			       mUrl.get(), &handled);

	if (!handled)
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
		if (NS_SUCCEEDED (SynchroniseMIMEInfo()))
		{
			return mLauncher->LaunchWithApplication (nsnull, PR_FALSE);
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
		if (NS_SUCCEEDED (rv))
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

		if(mUrlHelper) mLauncher->Cancel();
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

	nsCOMPtr<nsIMIMEInfo> mimeInfo;
	rv = mLauncher->GetMIMEInfo(getter_AddRefs(mimeInfo));
	if(NS_FAILED (rv)) return NS_ERROR_FAILURE;

	command_with_path = g_find_program_in_path (mHelperApp->command);
	if (command_with_path == NULL) return NS_ERROR_FAILURE;
	nsCOMPtr<nsILocalFile> helperFile;
	rv = NS_NewNativeLocalFile (nsDependentCString(command_with_path),
				   PR_TRUE,
				   getter_AddRefs(helperFile));
	if(NS_FAILED (rv)) return NS_ERROR_FAILURE;
	g_free (command_with_path);

	rv = mimeInfo->SetPreferredApplicationHandler(helperFile);
	if(NS_FAILED (rv)) return NS_ERROR_FAILURE;	

	nsMIMEInfoHandleAction mimeInfoAction;
	mimeInfoAction = nsIMIMEInfo::useHelperApp;

	if(mHelperApp->requires_terminal) //Information passing kludge!
	{
		rv = mimeInfo->SetApplicationDescription
				(NS_LITERAL_STRING("runInTerminal").get());
		if(NS_FAILED (rv)) return NS_ERROR_FAILURE;
	}

	rv = mimeInfo->SetPreferredAction(mimeInfoAction);
	if(NS_FAILED (rv)) return NS_ERROR_FAILURE;

	return NS_OK;
}

NS_METHOD GContentHandler::Init (void)
{
	nsresult rv;

	nsCOMPtr<nsIMIMEInfo> MIMEInfo;
	rv = mLauncher->GetMIMEInfo (getter_AddRefs(MIMEInfo));
	rv = MIMEInfo->GetMIMEType (&mMimeType);

#if MOZILLA_SNAPSHOT > 11
	rv = mLauncher->GetSource (getter_AddRefs(mUri));
	rv = mLauncher->GetTargetFile (getter_AddRefs(mTempFile));
#else
	rv = mLauncher->GetDownloadInfo (getter_AddRefs(mUri),
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
	GContentHandler *mContentHandler = this;
	GnomeVFSMimeApplication *DefaultApp = gnome_vfs_mime_get_default_application(mMimeType);

	EphyMimePermission permission;
	permission = ephy_embed_shell_check_mime (embed_shell, mMimeType);
	
	if (!auto_open || !DefaultApp || permission != EPHY_MIME_PERMISSION_SAFE)
	{
		nsCOMPtr<nsIHelperAppLauncher> launcher;
		
		rv = mContentHandler->GetLauncher (getter_AddRefs(launcher));
		if (NS_FAILED (rv)) return rv;
		launcher->SaveToDisk (nsnull,PR_FALSE);
	}
	else
	{
		rv = mContentHandler->SetHelperApp (DefaultApp, FALSE);
		if (NS_FAILED (rv)) return rv;
		rv = mContentHandler->FindHelperApp ();
		if (NS_FAILED (rv)) return rv;
	}

	return NS_OK;
}
