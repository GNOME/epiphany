/*
 *  Copyright (C) 2002 Philip Langdale
 *  Copyright (C) 2003-2004 Christian Persch
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
NS_IMPL_ISUPPORTS1(GPrintingPromptService, nsIPrintingPromptService)

GPrintingPromptService::GPrintingPromptService()
{
	LOG ("GPrintingPromptService ctor (%p)", this);
}

GPrintingPromptService::~GPrintingPromptService()
{
	LOG ("GPrintingPromptService dtor (%p)", this);
}

/* void showPrintDialog (in nsIDOMWindow parent, in nsIWebBrowserPrint webBrowserPrint, in nsIPrintSettings printSettings); */
NS_IMETHODIMP GPrintingPromptService::ShowPrintDialog(nsIDOMWindow *parent, nsIWebBrowserPrint *webBrowserPrint, nsIPrintSettings *printSettings)
{
	EphyDialog *dialog;

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

	if (!eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_PRINT_SETUP))
	{
		GtkWidget *gtkParent = EphyUtils::FindGtkParent(parent);
		NS_ENSURE_TRUE (gtkParent, NS_ERROR_FAILURE);

		dialog = ephy_print_dialog_new (gtkParent, embed);
		ephy_dialog_set_modal (dialog, TRUE);

		int ret = ephy_dialog_run (dialog);

		g_object_unref (dialog);

		if (ret != GTK_RESPONSE_OK)
		{
			return NS_ERROR_ABORT;
		}
	}

	EmbedPrintInfo *info;

	info = ephy_print_get_print_info ();

	/* work around mozilla bug which borks when printing selection without having one */
	if (info->pages == 2 && ephy_command_manager_can_do_command
	    (EPHY_COMMAND_MANAGER (embed), "cmd_copy") == FALSE)
	{
		info->pages = 0;
	}

	EphyUtils::CollatePrintSettings (info, printSettings, FALSE);

	ephy_print_info_free (info);

	return NS_OK;
}

/* void showProgress (in nsIDOMWindow parent, in nsIWebBrowserPrint webBrowserPrint, in nsIPrintSettings printSettings, in nsIObserver openDialogObserver, in boolean isForPrinting, out nsIWebProgressListener webProgressListener, out nsIPrintProgressParams printProgressParams, out boolean notifyOnOpen); */
NS_IMETHODIMP GPrintingPromptService::ShowProgress(nsIDOMWindow *parent, nsIWebBrowserPrint *webBrowserPrint, nsIPrintSettings *printSettings, nsIObserver *openDialogObserver, PRBool isForPrinting, nsIWebProgressListener **webProgressListener, nsIPrintProgressParams **printProgressParams, PRBool *notifyOnOpen)
{
    return NS_ERROR_NOT_IMPLEMENTED;
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
