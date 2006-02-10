/*
 *  Copyright (C) 2001,2002,2003 Philip Langdale
 *  Copyright (C) 2003 Marco Pesenti Gritti
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

#include "ContentHandler.h"
#include "GlobalHistory.h"
#include "PrintingPromptService.h"
#include "MozDownload.h"
#include "EphyContentPolicy.h"
#include "EphySidebar.h"
#include "EphyPromptService.h"

#ifdef ENABLE_FILEPICKER
#include "FilePicker.h"
#endif

#ifdef HAVE_MOZILLA_PSM
#include "GtkNSSClientAuthDialogs.h"
#include "GtkNSSDialogs.h"
#include "GtkNSSKeyPairDialogs.h"
#include "GtkNSSSecurityWarningDialogs.h"
#include <nsISecureBrowserUI.h>
#endif

#ifdef HAVE_GECKO_1_8
#include "EphyAboutModule.h"
#endif

#include <nsMemory.h>
#include <nsDocShellCID.h>
#include <nsIGenericFactory.h>
#include <nsIComponentRegistrar.h>
#include <nsICategoryManager.h>
#include <nsCOMPtr.h>
#include <nsILocalFile.h>
#include <nsNetCID.h>

#include <glib/gmessages.h>

NS_GENERIC_FACTORY_CONSTRUCTOR(MozDownload)
NS_GENERIC_FACTORY_CONSTRUCTOR(GContentHandler)
NS_GENERIC_FACTORY_CONSTRUCTOR(MozGlobalHistory)
NS_GENERIC_FACTORY_CONSTRUCTOR(GPrintingPromptService)
NS_GENERIC_FACTORY_CONSTRUCTOR(EphyContentPolicy)
NS_GENERIC_FACTORY_CONSTRUCTOR(EphySidebar)
NS_GENERIC_FACTORY_CONSTRUCTOR(EphyPromptService)

#ifdef ENABLE_FILEPICKER
NS_GENERIC_FACTORY_CONSTRUCTOR(GFilePicker)
#endif

#ifdef HAVE_MOZILLA_PSM
NS_GENERIC_FACTORY_CONSTRUCTOR(GtkNSSClientAuthDialogs)
NS_GENERIC_FACTORY_CONSTRUCTOR(GtkNSSDialogs)
NS_GENERIC_FACTORY_CONSTRUCTOR(GtkNSSKeyPairDialogs)
NS_GENERIC_FACTORY_CONSTRUCTOR(GtkNSSSecurityWarningDialogs)
#endif

#ifdef HAVE_GECKO_1_8
NS_GENERIC_FACTORY_CONSTRUCTOR(EphyAboutModule)
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
#ifdef HAVE_GECKO_1_8
{
	EPHY_ABOUT_EPIPHANY_CLASSNAME,
	EPHY_ABOUT_MODULE_CID,
	EPHY_ABOUT_EPIPHANY_CONTRACTID,
	EphyAboutModuleConstructor
},
{
	EPHY_ABOUT_NETERROR_CLASSNAME,
	EPHY_ABOUT_MODULE_CID,
	EPHY_ABOUT_NETERROR_CONTRACTID,
	EphyAboutModuleConstructor
},
#endif
	{
		EPHY_PROMPT_SERVICE_CLASSNAME,
		EPHY_PROMPT_SERVICE_IID,
		"@mozilla.org/embedcomp/prompt-service;1",
		EphyPromptServiceConstructor
	},
};

#if defined(HAVE_MOZILLA_PSM) && !defined(HAVE_GECKO_1_8)
/* 5999dfd3-571f-4fcf-964b-386879f5cded */
#define NEW_CID { 0x5999dfd3, 0x571f, 0x4fcf, { 0x96, 0x4b, 0x38, 0x68, 0x79, 0xf5, 0xcd, 0xed } }

static nsresult
reregister_secure_browser_ui (nsIComponentManager *cm,
			      nsIComponentRegistrar *cr)
{
	NS_ENSURE_ARG (cm);
	NS_ENSURE_ARG (cr);

	/* Workaround as a result of:
	 *  https://bugzilla.mozilla.org/show_bug.cgi?id=94974
	 * see
	 *  http://bugzilla.gnome.org/show_bug.cgi?id=164670
	 */

	nsresult rv;
	nsCOMPtr<nsIFactory> factory;
	rv = cm->GetClassObjectByContractID (NS_SECURE_BROWSER_UI_CONTRACTID, NS_GET_IID(nsIFactory), getter_AddRefs (factory));
	NS_ENSURE_SUCCESS (rv, rv);

	nsCID *cidPtr = nsnull;
	rv = cr->ContractIDToCID(NS_SECURE_BROWSER_UI_CONTRACTID, &cidPtr);
	NS_ENSURE_TRUE (NS_SUCCEEDED (rv) && cidPtr, rv);

	rv = cr->UnregisterFactory (*cidPtr, factory);
	NS_ENSURE_SUCCESS (rv, rv);

	const nsCID new_cid = NEW_CID;
	rv = cr->RegisterFactory (new_cid, "Epiphany Secure Browser Class", "@gnome.org/project/epiphany/hacks/secure-browser-ui;1", factory);
	nsMemory::Free (cidPtr);

	return rv;
}
#endif /* defined(HAVE_MOZILLA_PSM) && !defined(HAVE_GECKO_1_8) */

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

#if defined(HAVE_MOZILLA_PSM) && !defined(HAVE_GECKO_1_8)
	/* Workaround for http://bugzilla.gnome.org/show_bug.cgi?id=164670 */
	rv = reregister_secure_browser_ui (cm, cr);
	if (NS_FAILED (rv))
	{
		g_warning ("Failed to divert the nsISecureBrowserUI implementation!\n");
	}
#endif /* defined(HAVE_MOZILLA_PSM) && !defined(HAVE_GECKO_1_8) */

	return ret;
}
