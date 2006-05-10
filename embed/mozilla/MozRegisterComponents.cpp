/*
 *  Copyright (C) 2001,2002,2003 Philip Langdale
 *  Copyright (C) 2003 Marco Pesenti Gritti
 *  Copyright (C) 2004, 2005 Christian Persch
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

#include <glib/gmessages.h>

#include <nsStringAPI.h>

#include <nsComponentManagerUtils.h>
#include <nsCOMPtr.h>
#include <nsDocShellCID.h>
#include <nsICategoryManager.h>
#include <nsIComponentManager.h>
#include <nsIComponentRegistrar.h>
#include <nsIGenericFactory.h>
#include <nsILocalFile.h>
#include <nsIServiceManager.h>
#include <nsMemory.h>
#include <nsNetCID.h>
#include <nsServiceManagerUtils.h>

#ifdef HAVE_GECKO_1_9
#include <nsIClassInfoImpl.h>
#endif

#ifdef HAVE_MOZILLA_PSM
#include <nsISecureBrowserUI.h>
#endif

#include "ContentHandler.h"
#include "EphyAboutModule.h"
#include "EphyContentPolicy.h"
#include "EphyPromptService.h"
#include "EphySidebar.h"
#include "GlobalHistory.h"
#include "MozDownload.h"
#include "PrintingPromptService.h"

#ifdef ENABLE_FILEPICKER
#include "FilePicker.h"
#endif

#ifdef HAVE_MOZILLA_PSM
#include "GtkNSSClientAuthDialogs.h"
#include "GtkNSSDialogs.h"
#include "GtkNSSKeyPairDialogs.h"
#include "GtkNSSSecurityWarningDialogs.h"
#endif

NS_GENERIC_FACTORY_CONSTRUCTOR(EphyAboutModule)
NS_GENERIC_FACTORY_CONSTRUCTOR(EphyContentPolicy)
NS_GENERIC_FACTORY_CONSTRUCTOR(EphyPromptService)
NS_GENERIC_FACTORY_CONSTRUCTOR(EphySidebar)
NS_GENERIC_FACTORY_CONSTRUCTOR(GContentHandler)
NS_GENERIC_FACTORY_CONSTRUCTOR(GPrintingPromptService)
NS_GENERIC_FACTORY_CONSTRUCTOR(MozDownload)
NS_GENERIC_FACTORY_CONSTRUCTOR(MozGlobalHistory)

#ifdef ENABLE_FILEPICKER
NS_GENERIC_FACTORY_CONSTRUCTOR(GFilePicker)
#endif

#ifdef HAVE_MOZILLA_PSM
NS_GENERIC_FACTORY_CONSTRUCTOR(GtkNSSClientAuthDialogs)
NS_GENERIC_FACTORY_CONSTRUCTOR(GtkNSSDialogs)
NS_GENERIC_FACTORY_CONSTRUCTOR(GtkNSSKeyPairDialogs)
NS_GENERIC_FACTORY_CONSTRUCTOR(GtkNSSSecurityWarningDialogs)
#endif

/* class information */ 
NS_DECL_CLASSINFO(EphySidebar)

static NS_METHOD
RegisterContentPolicy(nsIComponentManager *aCompMgr, nsIFile *aPath,
		      const char *registryLocation, const char *componentType,
		      const nsModuleComponentInfo *info)
{
	nsCOMPtr<nsICategoryManager> cm =
		do_GetService(NS_CATEGORYMANAGER_CONTRACTID);
	NS_ENSURE_TRUE (cm, NS_ERROR_FAILURE);

	nsresult rv;
	char *oldval = nsnull;
	rv = cm->AddCategoryEntry ("content-policy",
				   EPHY_CONTENT_POLICY_CONTRACTID,
				   EPHY_CONTENT_POLICY_CONTRACTID,
				   PR_TRUE, PR_TRUE, &oldval);
	if (oldval)
		nsMemory::Free (oldval);
	return rv;
}

static NS_METHOD
RegisterSidebar(nsIComponentManager *aCompMgr, nsIFile *aPath,
                const char *registryLocation, const char *componentType,
                const nsModuleComponentInfo *info)
{
	nsCOMPtr<nsICategoryManager> cm =
		do_GetService(NS_CATEGORYMANAGER_CONTRACTID);
	NS_ENSURE_TRUE (cm, NS_ERROR_FAILURE);

	return cm->AddCategoryEntry("JavaScript global property",
				    "sidebar", NS_SIDEBAR_CONTRACTID,
				    PR_FALSE, PR_TRUE, nsnull);
}

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
		NS_GLOBALHISTORY2_CONTRACTID,
		MozGlobalHistoryConstructor
	},
	{
		G_PRINTINGPROMPTSERVICE_CLASSNAME,
		G_PRINTINGPROMPTSERVICE_CID,
		G_PRINTINGPROMPTSERVICE_CONTRACTID,
		GPrintingPromptServiceConstructor
	},
	{
		EPHY_CONTENT_POLICY_CLASSNAME,
		EPHY_CONTENT_POLICY_CID,
		EPHY_CONTENT_POLICY_CONTRACTID,
		EphyContentPolicyConstructor,
		RegisterContentPolicy
	},
	{
		EPHY_SIDEBAR_CLASSNAME,
		EPHY_SIDEBAR_CID,
		NS_SIDEBAR_CONTRACTID,
		EphySidebarConstructor,
		RegisterSidebar,
		nsnull /* no unregister func */,
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
