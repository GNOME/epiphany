/*
 *  Copyright (C) 2002 Philip Langdale
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

#include "libgnomevfs/gnome-vfs-mime-handlers.h"

/* see the FIXME below */
#include <locale.h>

#include <libgnome/gnome-exec.h>

#include "PrintProgressListener.h"


NS_IMPL_ISUPPORTS1(GPrintListener, nsIWebProgressListener)

GPrintListener::GPrintListener()
{
	NS_INIT_ISUPPORTS();
	mFilename = NULL;
}

GPrintListener::GPrintListener(char *filename)
{
	GPrintListener ();
	mFilename = filename ? g_strdup (filename) : NULL;
}

GPrintListener::~GPrintListener()
{
	g_free (mFilename);
}

/* void onStateChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aStateFlags, in unsigned long aStatus); */
NS_IMETHODIMP GPrintListener::OnStateChange(nsIWebProgress *aWebProgress,
					    nsIRequest *aRequest,
					    PRUint32 aStateFlags,
					    PRUint32 aStatus)
{
	if (aStateFlags & nsIWebProgressListener::STATE_STOP)
	{
		GnomeVFSMimeApplication *app;
		gchar *command;

		/* FIXME(MOZILLA) ugly workaround for a mozilla problem with 
		 * reseting the LC_* environment when printing */
		setlocale(LC_ALL,"");
		if (!mFilename) return NS_OK;

		/* get the postscript handler */
		app = gnome_vfs_mime_get_default_application
						("application/postscript");
		if (app)
		{
			command = g_strconcat (app->command, " ",
					       mFilename, NULL);
			gnome_execute_shell (g_get_home_dir(), command);
			gnome_vfs_mime_application_free (app);
			g_free (command);
		}
		else return NS_ERROR_FAILURE;
	}
	return NS_OK;
}

/* void onProgressChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aCurSelfProgress, in long aMaxSelfProgress, in long aCurTotalProgress, in long aMaxTotalProgress); */
NS_IMETHODIMP GPrintListener::OnProgressChange(nsIWebProgress *aWebProgress,
					       nsIRequest *aRequest,
					       PRInt32 aCurSelfProgress,
					       PRInt32 aMaxSelfProgress,
					       PRInt32 aCurTotalProgress,
					       PRInt32 aMaxTotalProgress)
{
	return NS_OK;
}

/* void onLocationChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsIURI location); */
NS_IMETHODIMP GPrintListener::OnLocationChange(nsIWebProgress *aWebProgress,
					       nsIRequest *aRequest,
					       nsIURI *location)
{
	return NS_OK;
}

/* void onStatusChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsresult aStatus, in wstring aMessage); */
NS_IMETHODIMP GPrintListener::OnStatusChange(nsIWebProgress *aWebProgress,
					     nsIRequest *aRequest,
					     nsresult aStatus,
					     const PRUnichar *aMessage)
{
	return NS_OK;
}

/* void onSecurityChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long state); */
NS_IMETHODIMP GPrintListener::OnSecurityChange(nsIWebProgress *aWebProgress,
					       nsIRequest *aRequest,
					       PRUint32 state)
{
	return NS_OK;
}


