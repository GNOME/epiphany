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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "EphyAboutRedirector.h"
#include "StartHereProtocolHandler.h"
#include "ContentHandler.h"
#include "ExternalProtocolService.h"
#include "FilePicker.h"
#include "FtpProtocolHandler.h"
#include "IRCProtocolHandler.h"
#include "MailtoProtocolHandler.h"
#include "PrintingPromptService.h"
#include "ProgressListener.h"

#include <nsIFactory.h>
#include <nsIComponentManager.h>
#include <nsCOMPtr.h>

#include <glib.h>

static NS_DEFINE_CID(kContentHandlerCID, G_CONTENTHANDLER_CID);
static NS_DEFINE_CID(kProtocolServiceCID, G_EXTERNALPROTOCOLSERVICE_CID);
static NS_DEFINE_CID(kFilePickerCID, G_FILEPICKER_CID);
static NS_DEFINE_CID(kStartHereProcotolHandlerCID, G_START_HERE_PROTOCOLHANDLER_CID);
static NS_DEFINE_CID(kEphyAboutRedirectorCID, EPHY_ABOUT_REDIRECTOR_CID);
static NS_DEFINE_CID(knsFtpProtocolHandlerCID, NS_FTPPROTOCOLHANDLER_CID);
static NS_DEFINE_CID(kFtpHandlerCID, G_FTP_PROTOCOL_CID);
static NS_DEFINE_CID(kIRCHandlerCID, G_IRC_PROTOCOL_CID);
static NS_DEFINE_CID(kMailtoHandlerCID, G_MAILTO_PROTOCOL_CID);
static NS_DEFINE_CID(kPrintingPromptServiceCID, G_PRINTINGPROMPTSERVICE_CID);
static NS_DEFINE_CID(kProgressDialogCID, G_PROGRESSDIALOG_CID);

//RegisterFactory is local
NS_METHOD RegisterFactory (nsresult (aFactoryFunc)(nsIFactory** aFactory),
			   const nsCID & aClass, const char *aClassName,
			   const char *aContractID, PRBool aReplace);

//Annoying globals to track the mozilla ftp handler so it can be restored.
static PRBool ftpRegistered = PR_FALSE;
static nsCOMPtr<nsIFactory> nsFtpFactory;

/* FIXME why we need to use "C" here ???? */

extern  "C" gboolean
mozilla_register_components (void)
{
	gboolean ret = TRUE;
	nsresult rv;

        rv = RegisterFactory (NS_NewProgressListenerFactory, kProgressDialogCID,
                              G_PROGRESSDIALOG_CLASSNAME,
                              NS_DOWNLOAD_CONTRACTID, PR_TRUE);
        if (NS_FAILED(rv)) ret = FALSE;

	rv = RegisterFactory (NS_NewContentHandlerFactory, kContentHandlerCID,
			      NS_IHELPERAPPLAUNCHERDLG_CLASSNAME,
			      NS_IHELPERAPPLAUNCHERDLG_CONTRACTID, PR_TRUE);
	if (NS_FAILED(rv)) ret = FALSE;
	
	rv = RegisterFactory   (NS_NewExternalProtocolServiceFactory,
				kProtocolServiceCID,
				G_EXTERNALPROTOCOLSERVICE_CLASSNAME,
				NS_EXTERNALPROTOCOLSERVICE_CONTRACTID,
				PR_TRUE);
	if (NS_FAILED(rv)) ret = FALSE;

	rv = RegisterFactory (NS_NewFilePickerFactory, kFilePickerCID,
			      G_FILEPICKER_CLASSNAME, G_FILEPICKER_CONTRACTID,
			      PR_TRUE);
	if (NS_FAILED(rv)) ret = FALSE;

	rv = RegisterFactory (NS_NewStartHereHandlerFactory,
			      kStartHereProcotolHandlerCID,
			      G_START_HERE_PROTOCOLHANDLER_CLASSNAME,
			      G_START_HERE_PROTOCOLHANDLER_CONTRACTID,
			      PR_TRUE);
	if (NS_FAILED(rv)) ret = FALSE;

	rv = RegisterFactory (NS_NewEphyAboutRedirectorFactory,
			      kEphyAboutRedirectorCID,
			      EPHY_ABOUT_REDIRECTOR_CLASSNAME,
			      EPHY_ABOUT_REDIRECTOR_OPTIONS_CONTRACTID,
			      PR_TRUE);
	if (NS_FAILED(rv)) ret = FALSE;

	rv = RegisterFactory (NS_NewEphyAboutRedirectorFactory,
			      kEphyAboutRedirectorCID,
			      EPHY_ABOUT_REDIRECTOR_CLASSNAME,
			      EPHY_ABOUT_REDIRECTOR_EPIPHANY_CONTRACTID,
			      PR_TRUE);
	if (NS_FAILED(rv)) ret = FALSE;

	rv = RegisterFactory (NS_NewEphyAboutRedirectorFactory,
			      kEphyAboutRedirectorCID,
			      EPHY_ABOUT_REDIRECTOR_CLASSNAME,
			      EPHY_ABOUT_REDIRECTOR_MARCO_CONTRACTID,
			      PR_TRUE);
	if (NS_FAILED(rv)) ret = FALSE;

	rv = RegisterFactory (NS_NewEphyAboutRedirectorFactory,
			      kEphyAboutRedirectorCID,
			      EPHY_ABOUT_REDIRECTOR_CLASSNAME,
			      EPHY_ABOUT_REDIRECTOR_CONSPIRACY_CONTRACTID,
			      PR_TRUE);
	if (NS_FAILED(rv)) ret = FALSE;


        rv = RegisterFactory (NS_NewFtpHandlerFactory, kFtpHandlerCID,
			      G_FTP_CONTENT_CLASSNAME, G_FTP_CONTENT_CONTRACTID,
			      PR_TRUE);
	if (NS_FAILED(rv)) ret = FALSE;

        rv = RegisterFactory (NS_NewIRCHandlerFactory, kIRCHandlerCID,
			      G_IRC_PROTOCOL_CLASSNAME,   
			      G_IRC_PROTOCOL_CONTRACTID, PR_TRUE);
        if (NS_FAILED(rv)) ret = FALSE;

        rv = RegisterFactory (NS_NewIRCHandlerFactory, kIRCHandlerCID,
			      G_IRC_CONTENT_CLASSNAME,   
			      G_IRC_CONTENT_CONTRACTID, PR_TRUE);        
        if (NS_FAILED(rv)) ret = FALSE;

	rv = RegisterFactory (NS_NewPrintingPromptServiceFactory,
			      kPrintingPromptServiceCID,
			      G_PRINTINGPROMPTSERVICE_CLASSNAME, 
			      G_PRINTINGPROMPTSERVICE_CONTRACTID, PR_TRUE);
	if (NS_FAILED(rv)) ret = FALSE;

	return ret;
}

NS_METHOD RegisterFactory (nsresult (aFactoryFunc)(nsIFactory** aFactory),
			   const nsCID & aClass, const char *aClassName,
			   const char *aContractID, PRBool aReplace)
{
	nsresult rv = NS_OK;

	nsCOMPtr<nsIFactory> factory;
	rv = aFactoryFunc(getter_AddRefs(factory));
	if (NS_FAILED(rv)) return rv;
	rv = nsComponentManager::RegisterFactory(aClass, aClassName,
						 aContractID,
						 factory, aReplace);
	return rv;
}

/**
 * mozilla_register_FtpProtocolHandler: Register Ftp Protocol Handler
 */
extern "C" gboolean 
mozilla_register_FtpProtocolHandler (void)
{
	if (ftpRegistered == PR_TRUE) return TRUE;

	nsresult rv = NS_OK;
     
        rv = nsComponentManager::FindFactory (knsFtpProtocolHandlerCID,
                                              getter_AddRefs(nsFtpFactory));
        if (NS_FAILED(rv)) return FALSE;

	rv = RegisterFactory (NS_NewFtpHandlerFactory, kFtpHandlerCID,
			      G_FTP_PROTOCOL_CLASSNAME, 
			      G_FTP_PROTOCOL_CONTRACTID, PR_TRUE);  

	if (NS_FAILED(rv)) return FALSE;

	ftpRegistered = PR_TRUE;
	return NS_SUCCEEDED (rv) ? TRUE : FALSE;
}

/**
 * mozilla_unregister_FtpProtocolHandler: Unregister Ftp Protocol Handler
 */
extern "C" gboolean 
mozilla_unregister_FtpProtocolHandler (void)
{
	if (ftpRegistered == PR_FALSE) return FALSE;
        
        nsresult rv = NS_OK;
	
	rv = nsComponentManager::RegisterFactory(knsFtpProtocolHandlerCID,
						 NS_FTPPROTOCOLHANDLER_CLASSNAME,
						 G_FTP_PROTOCOL_CONTRACTID,
						 nsFtpFactory, PR_TRUE);

	ftpRegistered = PR_FALSE;
        return NS_SUCCEEDED (rv) ? TRUE : FALSE;
}

/**
 * mozilla_register_MailtoProtocolHandler: Register Mailto Protocol Handler
 */
extern "C" gboolean 
mozilla_register_MailtoProtocolHandler (void)
{
        nsresult rv = NS_OK;

        rv = RegisterFactory (NS_NewMailtoHandlerFactory, kMailtoHandlerCID,
			      G_MAILTO_PROTOCOL_CLASSNAME,   
			      G_MAILTO_PROTOCOL_CONTRACTID, PR_TRUE);
        if (NS_FAILED(rv)) return FALSE;

        rv = RegisterFactory (NS_NewMailtoHandlerFactory, kMailtoHandlerCID,
			      G_MAILTO_CONTENT_CLASSNAME,   
			      G_MAILTO_CONTENT_CONTRACTID, PR_TRUE);        
        return NS_SUCCEEDED (rv) ? TRUE : FALSE;
}
