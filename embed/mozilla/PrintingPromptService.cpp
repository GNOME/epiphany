/*
 *  Copyright (C) 2002 Philip Langdale
 *  Copyright (C) 2003-2004 Christian Persch
 *  Copyright (C) 2005 Juerg Billeter
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

#include "PrintingPromptService.h"
#include "EphyUtils.h"
#include "AutoJSContextStack.h"

#include <gtk/gtkdialog.h>
 
#include <libgnomeprintui/gnome-print-dialog.h>

#include "print-dialog.h"
#include "ephy-embed.h"
#include "ephy-command-manager.h"
#include "eel-gconf-extensions.h"
#include "ephy-prefs.h"
#include "ephy-debug.h"

#include <nsIPrintSettings.h>
#include <nsCOMPtr.h>
#include <nsIServiceManager.h>

/* Implementation file */
NS_IMPL_ISUPPORTS3(GPrintingPromptService, nsIPrintingPromptService, nsIWebProgressListener, nsIPrintProgressParams)

GPrintingPromptService::GPrintingPromptService()
{
	LOG ("GPrintingPromptService ctor (%p)", this);
	
	mPrintInfo = NULL;
}

GPrintingPromptService::~GPrintingPromptService()
{
	LOG ("GPrintingPromptService dtor (%p)", this);
	
	if (mPrintInfo != NULL)
	{
		ephy_print_info_free (mPrintInfo);
	}
}

/* void showPrintDialog (in nsIDOMWindow parent, in nsIWebBrowserPrint webBrowserPrint, in nsIPrintSettings printSettings); */
NS_IMETHODIMP GPrintingPromptService::ShowPrintDialog(nsIDOMWindow *parent, nsIWebBrowserPrint *webBrowserPrint, nsIPrintSettings *printSettings)
{
	EmbedPrintInfo *info;
	GtkWidget *dialog;

	if (eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_PRINTING))
	{
		return NS_ERROR_ABORT;
	}

	nsresult rv;
	AutoJSContextStack stack;
	rv = stack.Init ();
	if (NS_FAILED (rv)) return rv;

	EphyEmbed *embed = EPHY_EMBED (EphyUtils::FindEmbed (parent));
	NS_ENSURE_TRUE (embed, NS_ERROR_FAILURE);

	info = ephy_print_get_print_info ();

	if (!eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_PRINT_SETUP))
	{
		GtkWidget *gtkParent = EphyUtils::FindGtkParent(parent);
		NS_ENSURE_TRUE (gtkParent, NS_ERROR_FAILURE);

		dialog = ephy_print_dialog_new (gtkParent, info);
		gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

		int ret;
		
		while (TRUE)
		{
			ret = gtk_dialog_run (GTK_DIALOG (dialog));;
			
			if (ret != GNOME_PRINT_DIALOG_RESPONSE_PRINT)
				break;
				
			if (ephy_print_verify_postscript (GNOME_PRINT_DIALOG (dialog)))
				break;
		}
		
		gtk_widget_destroy (dialog);

		if (ret != GNOME_PRINT_DIALOG_RESPONSE_PRINT)
		{
			ephy_print_info_free (info);

			return NS_ERROR_ABORT;
		}
	}

	/* work around mozilla bug which borks when printing selection without having one */
	if (info->range == GNOME_PRINT_RANGE_SELECTION &&
	    ephy_command_manager_can_do_command
	    (EPHY_COMMAND_MANAGER (embed), "cmd_copy") == FALSE)
	{
		info->range = GNOME_PRINT_RANGE_ALL;
	}

	EphyUtils::CollatePrintSettings (info, printSettings, FALSE);
	
	mPrintInfo = info;

	return NS_OK;
}

/* void showProgress (in nsIDOMWindow parent, in nsIWebBrowserPrint webBrowserPrint, in nsIPrintSettings printSettings, in nsIObserver openDialogObserver, in boolean isForPrinting, out nsIWebProgressListener webProgressListener, out nsIPrintProgressParams printProgressParams, out boolean notifyOnOpen); */
NS_IMETHODIMP GPrintingPromptService::ShowProgress(nsIDOMWindow *parent, nsIWebBrowserPrint *webBrowserPrint, nsIPrintSettings *printSettings, nsIObserver *openDialogObserver, PRBool isForPrinting, nsIWebProgressListener **webProgressListener, nsIPrintProgressParams **printProgressParams, PRBool *notifyOnOpen)
{
	*printProgressParams = NS_STATIC_CAST(nsIPrintProgressParams*, this);
	NS_ADDREF(*printProgressParams);

	*webProgressListener = NS_STATIC_CAST(nsIWebProgressListener*, this);
	NS_ADDREF(*webProgressListener);

	return NS_OK;
}

/* void showPageSetup (in nsIDOMWindow parent, in nsIPrintSettings printSettings, in nsIObserver printObserver); */
NS_IMETHODIMP GPrintingPromptService::ShowPageSetup(nsIDOMWindow *parent,
						    nsIPrintSettings *printSettings, 
						    nsIObserver *printObserver)
{
	EphyDialog *dialog;
	nsresult rv = NS_ERROR_ABORT;

	if (eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_PRINTING) ||
	    eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_PRINT_SETUP))
	{
		return rv;
	}

	AutoJSContextStack stack;
	rv = stack.Init ();
	if (NS_FAILED (rv)) return rv;

	dialog = ephy_print_setup_dialog_new ();
	ephy_dialog_set_modal (dialog, TRUE);

	int ret = ephy_dialog_run (dialog);
	if (ret == GTK_RESPONSE_OK)
	{
		rv = NS_OK;
	}

	g_object_unref (dialog);

	return rv;
}

/* void showPrinterProperties (in nsIDOMWindow parent, in wstring printerName, in nsIPrintSettings printSettings); */
NS_IMETHODIMP GPrintingPromptService::ShowPrinterProperties(nsIDOMWindow *parent, const PRUnichar *printerName, nsIPrintSettings *printSettings)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


/* void onStateChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long aStateFlags, in nsresult aStatus); */
NS_IMETHODIMP GPrintingPromptService::OnStateChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 aStateFlags, nsresult aStatus)
{
	if ((aStateFlags & STATE_STOP) && mPrintInfo)
	{
		if (NS_SUCCEEDED (aStatus))
		{
			ephy_print_do_print_and_free (mPrintInfo);
		}
		else
		{
			ephy_print_info_free (mPrintInfo);
		}

		mPrintInfo = NULL;
	}
	return NS_OK;
}

/* void onProgressChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aCurSelfProgress, in long aMaxSelfProgress, in long aCurTotalProgress, in long aMaxTotalProgress); */
NS_IMETHODIMP GPrintingPromptService::OnProgressChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress, PRInt32 aCurTotalProgress, PRInt32 aMaxTotalProgress)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void onLocationChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsIURI location); */
NS_IMETHODIMP GPrintingPromptService::OnLocationChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsIURI *location)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void onStatusChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsresult aStatus, in wstring aMessage); */
NS_IMETHODIMP GPrintingPromptService::OnStatusChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsresult aStatus, const PRUnichar *aMessage)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void onSecurityChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long state); */
NS_IMETHODIMP GPrintingPromptService::OnSecurityChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 state)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute wstring docTitle; */
NS_IMETHODIMP GPrintingPromptService::GetDocTitle(PRUnichar * *aDocTitle)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP GPrintingPromptService::SetDocTitle(const PRUnichar * aDocTitle)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute wstring docURL; */
NS_IMETHODIMP GPrintingPromptService::GetDocURL(PRUnichar * *aDocURL)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP GPrintingPromptService::SetDocURL(const PRUnichar * aDocURL)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

