/*
 *  Copyright (C) 2001,2002,2003 Philip Langdale
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

#include "ContentHandler.h"
#include "FilePicker.h"
#include "GlobalHistory.h"
#include "ExternalProtocolHandlers.h"
#include "PrintingPromptService.h"
#include "MozDownload.h"
#include "ExternalProtocolService.h"
#include "EphyAboutRedirector.h"

#ifdef HAVE_MOZILLA_PSM
#include "GtkNSSDialogs.h"
#endif

#include <nsIGenericFactory.h>
#include <nsIComponentRegistrar.h>
#include <nsCOMPtr.h>
#include <nsILocalFile.h>

#include <glib.h>

NS_GENERIC_FACTORY_CONSTRUCTOR(EphyAboutRedirector)
NS_GENERIC_FACTORY_CONSTRUCTOR(MozDownload)	
NS_GENERIC_FACTORY_CONSTRUCTOR(GFilePicker)
NS_GENERIC_FACTORY_CONSTRUCTOR(GContentHandler)
NS_GENERIC_FACTORY_CONSTRUCTOR(MozGlobalHistory)
NS_GENERIC_FACTORY_CONSTRUCTOR(GPrintingPromptService)
NS_GENERIC_FACTORY_CONSTRUCTOR(GIRCProtocolHandler)
NS_GENERIC_FACTORY_CONSTRUCTOR(GFtpProtocolHandler)
NS_GENERIC_FACTORY_CONSTRUCTOR(GNewsProtocolHandler)
NS_GENERIC_FACTORY_CONSTRUCTOR(GExternalProtocolService)

#ifdef HAVE_MOZILLA_PSM
NS_GENERIC_FACTORY_CONSTRUCTOR(GtkNSSDialogs)
#endif

static const nsModuleComponentInfo sAppComps[] = {
	{
		G_EXTERNALPROTOCOLSERVICE_CLASSNAME,
		G_EXTERNALPROTOCOLSERVICE_CID,
		NS_EXTERNALPROTOCOLSERVICE_CONTRACTID,
 		GExternalProtocolServiceConstructor
	},
	{
		MOZ_DOWNLOAD_CLASSNAME,
		MOZ_DOWNLOAD_CID,
		NS_DOWNLOAD_CONTRACTID,
		MozDownloadConstructor
	},
	{
		G_FILEPICKER_CLASSNAME,
		G_FILEPICKER_CID,
		G_FILEPICKER_CONTRACTID,
		GFilePickerConstructor
	},
#ifdef HAVE_MOZILLA_PSM
	{
		GTK_NSSDIALOGS_CLASSNAME,
		GTK_NSSDIALOGS_CID,
		NS_BADCERTLISTENER_CONTRACTID,
		GtkNSSDialogsConstructor
	},
#endif
	{
		NS_IHELPERAPPLAUNCHERDLG_CLASSNAME,
		G_CONTENTHANDLER_CID,
		NS_IHELPERAPPLAUNCHERDLG_CONTRACTID,
		GContentHandlerConstructor
	},
	{
		EPHY_GLOBALHISTORY_CLASSNAME,
		EPHY_GLOBALHISTORY_CID,
		NS_GLOBALHISTORY_CONTRACTID,
		MozGlobalHistoryConstructor
	},
	{
		G_PRINTINGPROMPTSERVICE_CLASSNAME,
		G_PRINTINGPROMPTSERVICE_CID,
		G_PRINTINGPROMPTSERVICE_CONTRACTID,
		GPrintingPromptServiceConstructor
	},
	{
		G_IRC_PROTOCOL_CLASSNAME,
		G_IRC_PROTOCOL_CID,
		G_IRC_PROTOCOL_CONTRACTID,
		GIRCProtocolHandlerConstructor
	},
	{
		G_IRC_CONTENT_CLASSNAME,
		G_IRC_PROTOCOL_CID,
		G_IRC_CONTENT_CONTRACTID,
		GIRCProtocolHandlerConstructor
	},
	{
		G_NEWS_PROTOCOL_CLASSNAME,
		G_NEWS_PROTOCOL_CID,
		G_NEWS_PROTOCOL_CONTRACTID,
		GNewsProtocolHandlerConstructor
	},
	{
		G_NEWS_CONTENT_CLASSNAME,
		G_NEWS_PROTOCOL_CID,
		G_NEWS_CONTENT_CONTRACTID,
		GNewsProtocolHandlerConstructor
	},
	{
		G_FTP_CONTENT_CLASSNAME,
		G_FTP_PROTOCOL_CID,
		G_FTP_CONTENT_CONTRACTID,
		GFtpProtocolHandlerConstructor
	},
	{
		EPHY_ABOUT_REDIRECTOR_CLASSNAME,
		EPHY_ABOUT_REDIRECTOR_CID,
		EPHY_ABOUT_REDIRECTOR_EPIPHANY_CONTRACTID,
		EphyAboutRedirectorConstructor
	},
	{
		EPHY_ABOUT_REDIRECTOR_CLASSNAME,
		EPHY_ABOUT_REDIRECTOR_CID,
		EPHY_ABOUT_REDIRECTOR_OPTIONS_CONTRACTID,
		EphyAboutRedirectorConstructor
	},
	{
		EPHY_ABOUT_REDIRECTOR_CLASSNAME,
		EPHY_ABOUT_REDIRECTOR_CID,
		EPHY_ABOUT_REDIRECTOR_CONSPIRACY_CONTRACTID,
		EphyAboutRedirectorConstructor
	},
	{
		EPHY_ABOUT_REDIRECTOR_CLASSNAME,
		EPHY_ABOUT_REDIRECTOR_CID,
		EPHY_ABOUT_REDIRECTOR_MARCO_CONTRACTID,
		EphyAboutRedirectorConstructor
	}
};

static const int sNumAppComps = sizeof(sAppComps) / sizeof(nsModuleComponentInfo);

static const nsModuleComponentInfo sFtpComps = {
	G_FTP_PROTOCOL_CLASSNAME,
	G_FTP_PROTOCOL_CID,
	G_FTP_PROTOCOL_CONTRACTID,
	GFtpProtocolHandlerConstructor
};

NS_GENERIC_FACTORY_CONSTRUCTOR(GMailtoProtocolHandler)

static const nsModuleComponentInfo sMailtoComps[] = {
	{
		G_MAILTO_PROTOCOL_CLASSNAME,
		G_MAILTO_PROTOCOL_CID,
		G_MAILTO_PROTOCOL_CONTRACTID,
		GMailtoProtocolHandlerConstructor
	},
	{
		G_MAILTO_CONTENT_CLASSNAME,
		G_MAILTO_PROTOCOL_CID,
		G_MAILTO_CONTENT_CONTRACTID,
		GMailtoProtocolHandlerConstructor
	}
};

static const int sNumMailtoComps = sizeof(sMailtoComps) / sizeof(nsModuleComponentInfo);

static const nsModuleComponentInfo sModuleComps[] = {
	{
		 G_EXTERNALPROTOCOLSERVICE_CLASSNAME,
		 G_EXTERNALPROTOCOLSERVICE_CID,
		 NS_EXTERNALPROTOCOLSERVICE_CONTRACTID
	}
};

static NS_DEFINE_CID(knsFtpProtocolHandlerCID, NS_FTPPROTOCOLHANDLER_CID);

/* Annoying globals to track the mozilla ftp handler so it can be restored. */
static PRBool ftpRegistered = PR_FALSE;
static nsCOMPtr<nsIFactory> nsFtpFactory;

gboolean
mozilla_register_components (void)
{
	gboolean ret = TRUE;
	nsresult rv;

	nsCOMPtr<nsIComponentRegistrar> cr;
	rv = NS_GetComponentRegistrar(getter_AddRefs(cr));
	NS_ENSURE_SUCCESS(rv, rv);

	for (int i = 0; i < sNumAppComps; i++)
	{
		nsCOMPtr<nsIGenericFactory> componentFactory;
		rv = NS_NewGenericFactory(getter_AddRefs(componentFactory),
					  &(sAppComps[i]));
		if (NS_FAILED(rv))
		{
			ret = FALSE;
			continue;  // don't abort registering other components
		}

		rv = cr->RegisterFactory(sAppComps[i].mCID,
					 sAppComps[i].mDescription,
					 sAppComps[i].mContractID,
					 componentFactory);
		if (NS_FAILED(rv))
			ret = FALSE;
	}

	return ret;
}

/**
 * mozilla_register_FtpProtocolHandler: Register Ftp Protocol Handler
 */
gboolean
mozilla_register_FtpProtocolHandler (void)
{
	if (ftpRegistered == PR_TRUE) return TRUE;

	nsresult rv = NS_OK;

	nsCOMPtr<nsIComponentManager> cm;
	rv = NS_GetComponentManager(getter_AddRefs(cm));
	if (NS_FAILED(rv) || !cm) return FALSE;

	rv = cm->GetClassObject(knsFtpProtocolHandlerCID,
				NS_GET_IID(nsIFactory),
				getter_AddRefs(nsFtpFactory));
        if (NS_FAILED(rv)) return FALSE;

	nsCOMPtr<nsIGenericFactory> ftpFactory;
	rv = NS_NewGenericFactory(getter_AddRefs(ftpFactory),
				  &sFtpComps);
	if (NS_FAILED(rv) || !ftpFactory) return FALSE;

	nsCOMPtr<nsIComponentRegistrar> cr;
	rv = NS_GetComponentRegistrar(getter_AddRefs(cr));
	if (NS_FAILED(rv) || !cr) return FALSE;

	rv = cr->RegisterFactory(sFtpComps.mCID,
				 sFtpComps.mDescription,
				 sFtpComps.mContractID,
				 ftpFactory);  
	if (NS_FAILED(rv)) return FALSE;

	ftpRegistered = PR_TRUE;
	return NS_SUCCEEDED (rv) ? TRUE : FALSE;
}

/**
 * mozilla_unregister_FtpProtocolHandler: Unregister Ftp Protocol Handler
 */
gboolean
mozilla_unregister_FtpProtocolHandler (void)
{
	if (ftpRegistered == PR_FALSE) return FALSE;
        
        nsresult rv = NS_OK;

	nsCOMPtr<nsIComponentRegistrar> cr;
	rv = NS_GetComponentRegistrar(getter_AddRefs(cr));
	if (NS_FAILED(rv) || !cr) return FALSE;

	rv = cr->RegisterFactory(knsFtpProtocolHandlerCID,
				 NS_FTPPROTOCOLHANDLER_CLASSNAME,
				 G_FTP_PROTOCOL_CONTRACTID,
				 nsFtpFactory);

	ftpRegistered = PR_FALSE;
        return NS_SUCCEEDED (rv) ? TRUE : FALSE;
}

/**
 * mozilla_register_MailtoProtocolHandler: Register Mailto Protocol Handler
 */
gboolean 
mozilla_register_MailtoProtocolHandler (void)
{
	gboolean retVal = TRUE;
        nsresult rv;

	nsCOMPtr<nsIComponentRegistrar> cr;
	rv = NS_GetComponentRegistrar(getter_AddRefs(cr));
	if (NS_FAILED(rv) || !cr) return FALSE;

	for (int i = 0; i < sNumMailtoComps; i++)
	{
		nsCOMPtr<nsIGenericFactory> componentFactory;
		rv = NS_NewGenericFactory(getter_AddRefs(componentFactory),
					  &(sMailtoComps[i]));
		if (NS_FAILED(rv))
		{
			retVal = FALSE;
			continue;  // don't abort registering other components
		}

		rv = cr->RegisterFactory(sMailtoComps[i].mCID,
					 sMailtoComps[i].mDescription,
					 sMailtoComps[i].mContractID,
					 componentFactory);
		if (NS_FAILED(rv))
			retVal = FALSE;
	}
	return retVal;
}
