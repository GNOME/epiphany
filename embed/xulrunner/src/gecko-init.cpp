/*
 *  Copyright © Christopher Blizzard
 *  Copyright © 2006 Christian Persch
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2.1, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  ---------------------------------------------------------------------------
 *  Derived from Mozilla.org code, which had the following attributions:
 *
 *  The Original Code is mozilla.org code.
 * 
 *  The Initial Developer of the Original Code is
 *  Christopher Blizzard. Portions created by Christopher Blizzard are Copyright © Christopher Blizzard.  All Rights Reserved.
 *  Portions created by the Initial Developer are Copyright © 2001
 *  the Initial Developer. All Rights Reserved.
 *
 *  Contributor(s):
 *    Christopher Blizzard <blizzard@mozilla.org>
 *  ---------------------------------------------------------------------------
 *
 *  $Id$
 */

#include <xpcom-config.h>
#include <config.h>

#include <stdlib.h>

#include "GeckoSingle.h"

#include "nsIDocShell.h"
#include "nsIWebProgress.h"
#include "nsIWebBrowserStream.h"
#include "nsIWidget.h"
#include "nsIDirectoryService.h"
#include "nsAppDirectoryServiceDefs.h"

// for NS_APPSHELL_CID
#include "nsWidgetsCID.h"

// for do_GetInterface
#include "nsIInterfaceRequestor.h"
// for do_CreateInstance
#include "nsIComponentManager.h"

// for initializing our window watcher service
#include "nsIWindowWatcher.h"

#include "nsILocalFile.h"
#include "nsXULAppAPI.h"

// all of the crap that we need for event listeners
// and when chrome windows finish loading
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMWindowInternal.h"

// For seting scrollbar visibilty
#include <nsIDOMBarProp.h>

// for the focus hacking we need to do
#include "nsIFocusController.h"

// app component registration
#include "nsIGenericFactory.h"
#include "nsIComponentRegistrar.h"

// all of our local includes
#include "gecko-init.h"
#include "GeckoSingle.h"
#include "EmbedWindow.h"
#include "EmbedProgress.h"
#include "EmbedContentListener.h"
#include "EmbedEventListener.h"
#include "EmbedWindowCreator.h"
#include "GeckoPromptService.h"

#ifdef MOZ_ACCESSIBILITY_ATK
#include "nsIAccessibilityService.h"
#include "nsIAccessible.h"
#include "nsIDOMDocument.h"
#endif

#include <nsServiceManagerUtils.h>
#include "nsXPCOMGlue.h"

#include "gecko-init.h"
#include "gecko-init-private.h"
#include "gecko-init-internal.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(GeckoPromptService)

static const nsModuleComponentInfo defaultAppComps[] = {
  {
    GECKO_PROMPT_SERVICE_CLASSNAME,
    GECKO_PROMPT_SERVICE_CID,
    "@mozilla.org/embedcomp/prompt-service;1",
    GeckoPromptServiceConstructor
  },
#ifdef HAVE_NSINONBLOCKINGALERTSERVICE_H
  {
    GECKO_PROMPT_SERVICE_CLASSNAME,
    GECKO_PROMPT_SERVICE_CID,
    "@mozilla.org/embedcomp/nbalert-service;1",
    GeckoPromptServiceConstructor
  },
#endif /* HAVE_NSINONBLOCKINGALERTSERVICE_H */
};

GtkWidget   *sOffscreenWindow = 0;
GtkWidget   *sOffscreenFixed  = 0;
const nsModuleComponentInfo *sAppComps = defaultAppComps;
int sNumAppComps = sizeof (defaultAppComps) / sizeof (nsModuleComponentInfo);
nsILocalFile *sProfileDir  = nsnull;
nsISupports  *sProfileLock = nsnull;
nsIDirectoryServiceProvider* sAppFileLocProvider;

class GTKEmbedDirectoryProvider : public nsIDirectoryServiceProvider2
{
  public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIDIRECTORYSERVICEPROVIDER
    NS_DECL_NSIDIRECTORYSERVICEPROVIDER2
};

static const GTKEmbedDirectoryProvider kDirectoryProvider;

NS_IMPL_QUERY_INTERFACE2(GTKEmbedDirectoryProvider,
                         nsIDirectoryServiceProvider,
                         nsIDirectoryServiceProvider2)

NS_IMETHODIMP_(nsrefcnt)
GTKEmbedDirectoryProvider::AddRef()
{
  return 2;
}

NS_IMETHODIMP_(nsrefcnt)
GTKEmbedDirectoryProvider::Release()
{
  return 1;
}

NS_IMETHODIMP
GTKEmbedDirectoryProvider::GetFile(const char *aKey, PRBool *aPersist,
                                   nsIFile* *aResult)
{
  if (sAppFileLocProvider) {
    nsresult rv = sAppFileLocProvider->GetFile(aKey, aPersist,
                                                             aResult);
    if (NS_SUCCEEDED(rv))
      return rv;
  }

  if (sProfileDir && !strcmp(aKey, NS_APP_USER_PROFILE_50_DIR)) {
    *aPersist = PR_TRUE;
    return sProfileDir->Clone(aResult);
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GTKEmbedDirectoryProvider::GetFiles(const char *aKey,
                                    nsISimpleEnumerator* *aResult)
{
  nsCOMPtr<nsIDirectoryServiceProvider2>
    dp2(do_QueryInterface(sAppFileLocProvider));

  if (!dp2)
    return NS_ERROR_FAILURE;

  return dp2->GetFiles(aKey, aResult);
}

/* static */
nsresult
RegisterAppComponents(void)
{
  nsCOMPtr<nsIComponentRegistrar> cr;
  nsresult rv = NS_GetComponentRegistrar(getter_AddRefs(cr));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIComponentManager> cm;
  rv = NS_GetComponentManager (getter_AddRefs (cm));
  NS_ENSURE_SUCCESS (rv, rv);
  
  for (int i = 0; i < sNumAppComps; ++i) {
    nsCOMPtr<nsIGenericFactory> componentFactory;
    rv = NS_NewGenericFactory(getter_AddRefs(componentFactory),
                              &(sAppComps[i]));
    if (NS_FAILED(rv)) {
      NS_WARNING("Unable to create factory for component");
      continue;  // don't abort registering other components
    }

    rv = cr->RegisterFactory(sAppComps[i].mCID, sAppComps[i].mDescription,
                             sAppComps[i].mContractID, componentFactory);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Unable to register factory for component");

    // Call the registration hook of the component, if any
    if (sAppComps[i].mRegisterSelfProc) {
      rv = sAppComps[i].mRegisterSelfProc(cm, nsnull, nsnull, nsnull,
                                          &(sAppComps[i]));
      NS_ASSERTION(NS_SUCCEEDED(rv), "Unable to self-register component");
    }
  }

  return rv;
}

/* static */
nsresult
StartupProfile (const char* aProfileDir, const char* aProfileName)
{
  /* Running without profile */
  if (!aProfileDir || !aProfileName)
    return NS_OK;

  if (sProfileDir && GeckoSingle::sWidgetCount != 0) {
    NS_ERROR("Cannot change profile directory during run!");
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  nsresult rv;
  nsCOMPtr<nsILocalFile> profileDir;
  rv = NS_NewNativeLocalFile (nsDependentCString (aProfileDir), PR_TRUE,
                              &sProfileDir);
  if (NS_FAILED (rv))
    return rv;

  if (aProfileName) {
    rv = sProfileDir->AppendNative (nsDependentCString (aProfileName));
    if (NS_FAILED (rv))
      return rv; // FIXMEchpe release sProfileDir
  }

  rv = XRE_LockProfileDirectory (sProfileDir, &sProfileLock);
  if (NS_FAILED (rv))
    return rv; // FIXMEchpe release sProfileDir

  if (GeckoSingle::sWidgetCount)
    XRE_NotifyProfile();

  return NS_OK;
}

gboolean
gecko_init ()
{
  return gecko_init_with_params (nsnull, nsnull, nsnull, nsnull);
}

gboolean
gecko_init_with_profile (const char *aGREPath,
                         const char* aProfileDir,
			 const char* aProfileName)
{
  return gecko_init_with_params (aGREPath, aProfileDir, aProfileName, nsnull);
}

gboolean
gecko_init_with_params (const char *aGREPath,
                        const char* aProfileDir,
			const char* aProfileName,
			nsIDirectoryServiceProvider* aAppFileLocProvider)
{
  nsresult rv;
  nsCOMPtr<nsILocalFile> binDir;

#if 0 //def XPCOM_GLUE
    const char* xpcomLocation = GRE_GetXPCOMPath();

    // Startup the XPCOM Glue that links us up with XPCOM.
    nsresult rv = XPCOMGlueStartup(xpcomLocation);  
    if (NS_FAILED(rv)) return;
#endif

  NS_IF_ADDREF (sAppFileLocProvider = aAppFileLocProvider);

  /* FIrst try to lock the profile */
  rv = StartupProfile (aProfileDir, aProfileName);
  if (NS_FAILED (rv))
    return FALSE;

  const char* aCompPath = g_getenv("GECKO_HOME");

  if (aCompPath) {
    rv = NS_NewNativeLocalFile(nsEmbedCString(aCompPath), PR_TRUE, getter_AddRefs(binDir));
    NS_ENSURE_SUCCESS(rv,false);
  }

  if (!aGREPath)
    aGREPath = getenv("MOZILLA_FIVE_HOME");

  if (!aGREPath)
    return FALSE;

  nsCOMPtr<nsILocalFile> greDir;
  rv = NS_NewNativeLocalFile (nsDependentCString (aGREPath), PR_TRUE,
                              getter_AddRefs (greDir));
  if (NS_FAILED(rv))
    return FALSE;

  rv = XRE_InitEmbedding(greDir, binDir,
                         const_cast<GTKEmbedDirectoryProvider*> (&kDirectoryProvider),
                         nsnull, nsnull);
  if (NS_FAILED (rv))
    return FALSE;

  if (sProfileDir)
    XRE_NotifyProfile();

  rv = RegisterAppComponents();
  NS_ASSERTION(NS_SUCCEEDED(rv), "Warning: Failed to register app components.\n");

  // create our local object
  EmbedWindowCreator *creator = new EmbedWindowCreator();
  nsCOMPtr<nsIWindowCreator> windowCreator =
    static_cast<nsIWindowCreator *>(creator);

  // Attach it via the watcher service
  nsCOMPtr<nsIWindowWatcher> watcher (do_GetService(NS_WINDOWWATCHER_CONTRACTID));
  if (watcher)
    watcher->SetWindowCreator(windowCreator);

  return true;
}

/* static */
void
EnsureOffscreenWindow(void)
{
  if (sOffscreenWindow)
    return;

  sOffscreenWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_realize (sOffscreenWindow);
  sOffscreenFixed = gtk_fixed_new();
  gtk_container_add (GTK_CONTAINER (sOffscreenWindow), sOffscreenFixed);
  gtk_widget_realize (sOffscreenFixed);
}

/* static */
void
gecko_reparent_to_offscreen(GtkWidget *aWidget)
{
  EnsureOffscreenWindow();

  gtk_widget_reparent(aWidget, sOffscreenFixed);
}

/* static */
void
DestroyOffscreenWindow(void)
{
  if (!sOffscreenWindow)
    return;
  gtk_widget_destroy(sOffscreenWindow);
  sOffscreenWindow = nsnull;
  sOffscreenFixed = nsnull;
}

void
gecko_shutdown()
{
  // destroy the offscreen window
  DestroyOffscreenWindow();

  NS_IF_RELEASE (sProfileDir);

  // shut down XPCOM/Embedding
  XRE_TermEmbedding();

  // we no longer need a reference to the DirectoryServiceProvider
  NS_IF_RELEASE (sAppFileLocProvider);

  /* FIXMchpe before or after TermEmbedding?? */
  NS_IF_RELEASE (sProfileLock);
}
