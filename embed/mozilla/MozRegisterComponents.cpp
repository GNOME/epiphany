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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ContentHandler.h"
#include "FilePicker.h"
#include "GlobalHistory.h"
#include "PrintingPromptService.h"
#include "MozDownload.h"
#include "EphyAboutRedirector.h"
#include "EphyContentPolicy.h"

#ifdef HAVE_MOZILLA_PSM
#include "GtkNSSClientAuthDialogs.h"
#include "GtkNSSDialogs.h"
#include "GtkNSSKeyPairDialogs.h"
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

NS_GENERIC_FACTORY_CONSTRUCTOR(EphyAboutRedirector)
NS_GENERIC_FACTORY_CONSTRUCTOR(MozDownload)	
NS_GENERIC_FACTORY_CONSTRUCTOR(GFilePicker)
NS_GENERIC_FACTORY_CONSTRUCTOR(GContentHandler)
NS_GENERIC_FACTORY_CONSTRUCTOR(MozGlobalHistory)
NS_GENERIC_FACTORY_CONSTRUCTOR(GPrintingPromptService)
NS_GENERIC_FACTORY_CONSTRUCTOR(EphyContentPolicy)

#ifdef HAVE_MOZILLA_PSM
NS_GENERIC_FACTORY_CONSTRUCTOR(GtkNSSClientAuthDialogs)
NS_GENERIC_FACTORY_CONSTRUCTOR(GtkNSSDialogs)
NS_GENERIC_FACTORY_CONSTRUCTOR(GtkNSSKeyPairDialogs)
#endif
 
static NS_METHOD
RegisterContentPolicy(nsIComponentManager *aCompMgr, nsIFile *aPath,
		      const char *registryLocation, const char *componentType,
		      const nsModuleComponentInfo *info)
{
	nsCOMPtr<nsICategoryManager> cm =
		do_GetService(NS_CATEGORYMANAGER_CONTRACTID);
	NS_ENSURE_TRUE (cm, NS_ERROR_FAILURE);

	char *oldval;
	return cm->AddCategoryEntry("content-policy",
				    EPHY_CONTENT_POLICY_CONTRACTID,
				    EPHY_CONTENT_POLICY_CONTRACTID,
				    PR_TRUE, PR_TRUE, &oldval);
	nsMemory::Free (oldval);
}

static const nsModuleComponentInfo sAppComps[] = {
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
		EPHY_ABOUT_REDIRECTOR_CLASSNAME,
		EPHY_ABOUT_REDIRECTOR_CID,
		EPHY_ABOUT_REDIRECTOR_EPIPHANY_CONTRACTID,
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
	},
	{
		EPHY_CONTENT_POLICY_CLASSNAME,
		EPHY_CONTENT_POLICY_CID,
		EPHY_CONTENT_POLICY_CONTRACTID,
		EphyContentPolicyConstructor,
		RegisterContentPolicy
	},
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


