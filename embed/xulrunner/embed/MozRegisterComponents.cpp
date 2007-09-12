/*
 *  Copyright © 2001,2002,2003 Philip Langdale
 *  Copyright © 2003 Marco Pesenti Gritti
 *  Copyright © 2004, 2005, 2006 Christian Persch
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  $Id$
 */

#include <xpcom-config.h>
#include "config.h"

#include <glib/gmessages.h>

#include <nsStringAPI.h>

#include <nsComponentManagerUtils.h>
#include <nsCOMPtr.h>
#include <nsCURILoader.h>
#include <nsDocShellCID.h>
#include <nsICategoryManager.h>
#include <nsIClassInfoImpl.h>
#include <nsIComponentManager.h>
#include <nsIComponentRegistrar.h>
#include <nsIGenericFactory.h>
#include <nsILocalFile.h>
#include <nsIScriptNameSpaceManager.h>
#include <nsIServiceManager.h>
#include <nsMemory.h>
#include <nsNetCID.h>
#include <nsServiceManagerUtils.h>

#ifdef HAVE_MOZILLA_PSM
#include <nsISecureBrowserUI.h>
#endif

#include "ContentHandler.h"
#include "EphyAboutModule.h"
#include "EphyContentPolicy.h"
#include "EphyPromptService.h"
#include "EphySidebar.h"
#include "GeckoCookiePromptService.h"
#include "GeckoPrintService.h"
#include "GeckoPrintSession.h"
#include "GlobalHistory.h"
#include "MozDownload.h"

#ifdef ENABLE_FILEPICKER
#include "FilePicker.h"
#endif

#ifdef ENABLE_SPELLCHECKER
#include "GeckoSpellCheckEngine.h"
#endif

#ifdef HAVE_MOZILLA_PSM
#include "GtkNSSClientAuthDialogs.h"
#include "GtkNSSDialogs.h"
#include "GtkNSSKeyPairDialogs.h"
#include "GtkNSSSecurityWarningDialogs.h"
#include "GeckoFormSigningDialog.h"
#endif

NS_GENERIC_FACTORY_CONSTRUCTOR(EphyAboutModule)
NS_GENERIC_FACTORY_CONSTRUCTOR(EphyContentPolicy)
NS_GENERIC_FACTORY_CONSTRUCTOR(EphyPromptService)
NS_GENERIC_FACTORY_CONSTRUCTOR(EphySidebar)
NS_GENERIC_FACTORY_CONSTRUCTOR(GContentHandler)
NS_GENERIC_FACTORY_CONSTRUCTOR(GeckoCookiePromptService)
NS_GENERIC_FACTORY_CONSTRUCTOR(GeckoPrintService)
NS_GENERIC_FACTORY_CONSTRUCTOR(GeckoPrintSession)
NS_GENERIC_FACTORY_CONSTRUCTOR(MozDownload)
NS_GENERIC_FACTORY_CONSTRUCTOR(MozGlobalHistory)

#ifdef ENABLE_FILEPICKER
NS_GENERIC_FACTORY_CONSTRUCTOR(GFilePicker)
#endif

#ifdef ENABLE_SPELLCHECKER
NS_GENERIC_FACTORY_CONSTRUCTOR(GeckoSpellCheckEngine)
#endif

#ifdef HAVE_MOZILLA_PSM
NS_GENERIC_FACTORY_CONSTRUCTOR(GtkNSSClientAuthDialogs)
NS_GENERIC_FACTORY_CONSTRUCTOR(GtkNSSDialogs)
NS_GENERIC_FACTORY_CONSTRUCTOR(GtkNSSKeyPairDialogs)
NS_GENERIC_FACTORY_CONSTRUCTOR(GtkNSSSecurityWarningDialogs)
NS_GENERIC_FACTORY_CONSTRUCTOR(GeckoFormSigningDialog)
#endif

#define XPINSTALL_CONTRACTID NS_CONTENT_HANDLER_CONTRACTID_PREFIX "application/x-xpinstall"

/* class information */ 
NS_DECL_CLASSINFO(EphySidebar)

/* FIXME: uninstall XPI handler */

static const nsModuleComponentInfo sAppComps[] = {
	{
		MOZ_DOWNLOAD_CLASSNAME,
		MOZ_DOWNLOAD_CID,
#ifdef NS_TRANSFER_CONTRACTID
		NS_TRANSFER_CONTRACTID,
#else
		NS_DOWNLOAD_CONTRACTID,
#endif
		MozDownloadConstructor
	},
#ifdef ENABLE_FILEPICKER
	{
		G_FILEPICKER_CLASSNAME,
		G_FILEPICKER_CID,
		G_FILEPICKER_CONTRACTID,
		GFilePickerConstructor
	},
#endif
#ifdef HAVE_MOZILLA_PSM
	{
		GTK_NSSCLIENTAUTHDIALOGS_CLASSNAME,
		GTK_NSSCLIENTAUTHDIALOGS_CID,
		NS_CLIENTAUTHDIALOGS_CONTRACTID,
		GtkNSSClientAuthDialogsConstructor
	},
	{
		GTK_NSSDIALOGS_CLASSNAME,
		GTK_NSSDIALOGS_CID,
		NS_BADCERTLISTENER_CONTRACTID,
		GtkNSSDialogsConstructor
	},
	{
		GTK_NSSDIALOGS_CLASSNAME,
		GTK_NSSDIALOGS_CID,
		NS_CERTIFICATEDIALOGS_CONTRACTID,
		GtkNSSDialogsConstructor
	},
	{
		GTK_NSSDIALOGS_CLASSNAME,
		GTK_NSSDIALOGS_CID,
		NS_DOMCRYPTODIALOGS_CONTRACTID,
		GtkNSSDialogsConstructor
	},
	{
		GTK_NSSDIALOGS_CLASSNAME,
		GTK_NSSDIALOGS_CID,
		NS_TOKENDIALOGS_CONTRACTID,
		GtkNSSDialogsConstructor
	},
	{
		GTK_NSSDIALOGS_CLASSNAME,
		GTK_NSSDIALOGS_CID,
		NS_TOKENPASSWORDSDIALOG_CONTRACTID,
		GtkNSSDialogsConstructor
	},
	{
		GTK_NSSKEYPAIRDIALOGS_CLASSNAME,
		GTK_NSSKEYPAIRDIALOGS_CID,
		NS_GENERATINGKEYPAIRINFODIALOGS_CONTRACTID,
		GtkNSSKeyPairDialogsConstructor
	},
	{
		GTK_NSSSECURITYWARNINGDIALOGS_CLASSNAME,
		GTK_NSSSECURITYWARNINGDIALOGS_CID,
		NS_SECURITYWARNINGDIALOGS_CONTRACTID,
		GtkNSSSecurityWarningDialogsConstructor
	},
	{
		GECKO_FORMSIGNINGDIALOGS_CLASSNAME,
		GECKO_FORMSIGNINGDIALOGS_CID,
		NS_FORMSIGNINGDIALOG_CONTRACTID,
		GeckoFormSigningDialogConstructor
	},
#endif /* HAVE_MOZILLA_PSM */
	{
		NS_IHELPERAPPLAUNCHERDLG_CLASSNAME,
		G_CONTENTHANDLER_CID,
		NS_IHELPERAPPLAUNCHERDLG_CONTRACTID,
		GContentHandlerConstructor
	},
	{
		EPHY_GLOBALHISTORY_CLASSNAME,
		EPHY_GLOBALHISTORY_CID,
		NS_GLOBALHISTORY2_CONTRACTID,
		MozGlobalHistoryConstructor
	},
	{
		GECKO_PRINT_SERVICE_CLASSNAME,
		GECKO_PRINT_SERVICE_IID,
		"@mozilla.org/embedcomp/printingprompt-service;1",
		GeckoPrintServiceConstructor
	},
	{
		GECKO_PRINT_SESSION_CLASSNAME,
		GECKO_PRINT_SESSION_IID,
		"@mozilla.org/gfx/printsession;1",
		GeckoPrintSessionConstructor
	},
	{
		EPHY_CONTENT_POLICY_CLASSNAME,
		EPHY_CONTENT_POLICY_CID,
		EPHY_CONTENT_POLICY_CONTRACTID,
		EphyContentPolicyConstructor,
		EphyContentPolicy::Register,
		EphyContentPolicy::Unregister
	},
	{
		EPHY_SIDEBAR_CLASSNAME,
		EPHY_SIDEBAR_CID,
		NS_SIDEBAR_CONTRACTID,
		EphySidebarConstructor,
		EphySidebar::Register,
		EphySidebar::Unregister,
		nsnull /* no factory destructor */,
		NS_CI_INTERFACE_GETTER_NAME(EphySidebar),
		nsnull /* no language helper */,
		&NS_CLASSINFO_NAME(EphySidebar),
		nsIClassInfo::DOM_OBJECT
	},
	{
		EPHY_ABOUT_EPIPHANY_CLASSNAME,
		EPHY_ABOUT_MODULE_CID,
		EPHY_ABOUT_EPIPHANY_CONTRACTID,
		EphyAboutModuleConstructor
	},
	{
		EPHY_ABOUT_RECOVER_CLASSNAME,
		EPHY_ABOUT_MODULE_CID,
		EPHY_ABOUT_RECOVER_CONTRACTID,
		EphyAboutModuleConstructor
	},
	{
		EPHY_ABOUT_NETERROR_CLASSNAME,
		EPHY_ABOUT_MODULE_CID,
		EPHY_ABOUT_NETERROR_CONTRACTID,
		EphyAboutModuleConstructor
	},
	{
		EPHY_PROMPT_SERVICE_CLASSNAME,
		EPHY_PROMPT_SERVICE_IID,
		"@mozilla.org/embedcomp/prompt-service;1",
		EphyPromptServiceConstructor
	},
#ifdef HAVE_NSINONBLOCKINGALERTSERVICE_H
	{
		EPHY_PROMPT_SERVICE_CLASSNAME,
		EPHY_PROMPT_SERVICE_IID,
		"@mozilla.org/embedcomp/nbalert-service;1",
		EphyPromptServiceConstructor
	},
#endif /* HAVE_NSINONBLOCKINGALERTSERVICE_H */
#ifdef ENABLE_SPELLCHECKER
	{
		GECKO_SPELL_CHECK_ENGINE_CLASSNAME,
		GECKO_SPELL_CHECK_ENGINE_IID,
		GECKO_SPELL_CHECK_ENGINE_CONTRACTID,
		GeckoSpellCheckEngineConstructor
	},
#endif /* ENABLE_SPELLCHECK */
        {
                EPHY_COOKIEPROMPTSERVICE_CLASSNAME,
                EPHY_COOKIEPROMPTSERVICE_CID,
                EPHY_COOKIEPROMPTSERVICE_CONTRACTID,
                GeckoCookiePromptServiceConstructor
        }
};

gboolean
mozilla_register_components (void)
{
	gboolean ret = TRUE;
	nsresult rv;

	nsCOMPtr<nsIComponentRegistrar> cr;
	NS_GetComponentRegistrar(getter_AddRefs(cr));
	NS_ENSURE_TRUE (cr, FALSE);

	nsCOMPtr<nsIComponentManager> cm;
	NS_GetComponentManager (getter_AddRefs (cm));
	NS_ENSURE_TRUE (cm, FALSE);

	for (guint i = 0; i < G_N_ELEMENTS (sAppComps); i++)
	{
		nsCOMPtr<nsIGenericFactory> componentFactory;
		rv = NS_NewGenericFactory(getter_AddRefs(componentFactory),
					  &(sAppComps[i]));
		if (NS_FAILED(rv) || !componentFactory)
		{
			g_warning ("Failed to make a factory for %s\n", sAppComps[i].mDescription);

			ret = FALSE;
			continue;  // don't abort registering other components
		}

		rv = cr->RegisterFactory(sAppComps[i].mCID,
					 sAppComps[i].mDescription,
					 sAppComps[i].mContractID,
					 componentFactory);
		if (NS_FAILED(rv))
		{
			g_warning ("Failed to register %s\n", sAppComps[i].mDescription);

			ret = FALSE;
		}

		if (sAppComps[i].mRegisterSelfProc)
		{
			rv = sAppComps[i].mRegisterSelfProc (cm, nsnull, nsnull, nsnull, &sAppComps[i]);

			if (NS_FAILED (rv))
			{
				g_warning ("Failed to register-self for %s\n", sAppComps[i].mDescription);
				ret = FALSE;
			}
		}
	}

	return ret;
}
