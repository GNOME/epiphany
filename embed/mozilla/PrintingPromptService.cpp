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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtkdialog.h>

#include "print-dialog.h"
#include "ephy-embed.h"
#include "MozillaPrivate.h"

#include <nsIPrintSettings.h>
#include <nsCOMPtr.h>
#include <nsIFactory.h>
#include <nsString.h>
#include <nsIServiceManager.h>
#include <nsXPComFactory.h>

#include <nsIPrintingPromptService.h>

/* Header file */
class GPrintingPromptService : public nsIPrintingPromptService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRINTINGPROMPTSERVICE

  GPrintingPromptService();
  virtual ~GPrintingPromptService();
  /* additional members */
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(GPrintingPromptService, nsIPrintingPromptService)

GPrintingPromptService::GPrintingPromptService()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

GPrintingPromptService::~GPrintingPromptService()
{
  /* destructor code */
}

/* void showPrintDialog (in nsIDOMWindow parent, in nsIWebBrowserPrint webBrowserPrint, in nsIPrintSettings printSettings); */
NS_IMETHODIMP GPrintingPromptService::ShowPrintDialog(nsIDOMWindow *parent, nsIWebBrowserPrint *webBrowserPrint, nsIPrintSettings *printSettings)
{
	EphyDialog *dialog;
	EmbedPrintInfo *info;

	GtkWidget *gtkParent = MozillaFindGtkParent(parent);

	dialog = print_dialog_new_with_parent (gtkParent, NULL, &info);
	ephy_dialog_set_modal (dialog, TRUE);

	gint ret = ephy_dialog_run (dialog);
	if(ret == GTK_RESPONSE_OK)
	{
		MozillaCollatePrintSettings(info, printSettings);
		print_free_info(info);

		return NS_OK;
	}
	else
		return NS_ERROR_FAILURE;
}

/* void showProgress (in nsIDOMWindow parent, in nsIWebBrowserPrint webBrowserPrint, in nsIPrintSettings printSettings, in nsIObserver openDialogObserver, in boolean isForPrinting, out nsIWebProgressListener webProgressListener, out nsIPrintProgressParams printProgressParams, out boolean notifyOnOpen); */
NS_IMETHODIMP GPrintingPromptService::ShowProgress(nsIDOMWindow *parent, nsIWebBrowserPrint *webBrowserPrint, nsIPrintSettings *printSettings, nsIObserver *openDialogObserver, PRBool isForPrinting, nsIWebProgressListener **webProgressListener, nsIPrintProgressParams **printProgressParams, PRBool *notifyOnOpen)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void showPageSetup (in nsIDOMWindow parent, in nsIPrintSettings printSettings, in nsIObserver printObserver); */
NS_IMETHODIMP GPrintingPromptService::ShowPageSetup(nsIDOMWindow *parent, nsIPrintSettings *printSettings, 
						    nsIObserver *printObserver)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void showPrinterProperties (in nsIDOMWindow parent, in wstring printerName, in nsIPrintSettings printSettings); */
NS_IMETHODIMP GPrintingPromptService::ShowPrinterProperties(nsIDOMWindow *parent, const PRUnichar *printerName, nsIPrintSettings *printSettings)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_DEF_FACTORY (GPrintingPromptService, GPrintingPromptService);

/**
 * NS_NewPromptServiceFactory:
 */ 
nsresult NS_NewPrintingPromptServiceFactory(nsIFactory** aFactory)
{
	NS_ENSURE_ARG_POINTER(aFactory);
	*aFactory = nsnull;

	nsGPrintingPromptServiceFactory *result = new nsGPrintingPromptServiceFactory;
	if (result == NULL)
	{
		return NS_ERROR_OUT_OF_MEMORY;
	}
    
	NS_ADDREF(result);
	*aFactory = result;
  
	return NS_OK;
}
