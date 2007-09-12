/*
 *  Copyright © 2002 Philip Langdale
 *  Copyright © 2004 Crispin Flowerday
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

#include "mozilla-config.h"
#include "config.h"

#include <nsStringAPI.h>

#include <nsICategoryManager.h>
#include <nsIClassInfoImpl.h>
#include <nsIScriptNameSpaceManager.h>
#include <nsMemory.h>
#include <nsServiceManagerUtils.h>
#include <nsXPCOMCID.h>

#include "ephy-debug.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-single.h"

#include "EphySidebar.h"

NS_IMPL_ISUPPORTS1_CI(EphySidebar, nsISidebar)

EphySidebar::EphySidebar()
{
}

EphySidebar::~EphySidebar()
{
}

/* void addPanel (in wstring aTitle, in string aContentURL, in string aCustomizeURL); */
NS_IMETHODIMP
EphySidebar::AddPanel (const PRUnichar *aTitle,
		       const char *aContentURL,
		       const char *aCustomizeURL)
{
	NS_ENSURE_ARG (aTitle);
	NS_ENSURE_ARG (aContentURL);

	nsCString title;
	EphyEmbedSingle *single;

	/* FIXME: length-limit string */
	NS_UTF16ToCString (nsDependentString(aTitle),
			   NS_CSTRING_ENCODING_UTF8, title);

	LOG ("Adding sidebar, url=%s title=%s", aContentURL, title.get());

	single = EPHY_EMBED_SINGLE (ephy_embed_shell_get_embed_single (embed_shell));

	gboolean result = FALSE;
	g_signal_emit_by_name (single, "add-sidebar",
			       aContentURL, title.get(), &result);

	return NS_OK;
}

/* void addPersistentPanel (in wstring aTitle, in string aContentURL, in string aCustomizeURL); */
NS_IMETHODIMP
EphySidebar::AddPersistentPanel (const PRUnichar *aTitle,
				 const char *aContentURL,
				 const char *aCustomizeURL)
{
	return AddPanel (aTitle, aContentURL, aCustomizeURL);
}

/* void addSearchEngine (in string engineURL, in string iconURL, in wstring suggestedTitle, in wst
ring suggestedCategory); */
NS_IMETHODIMP
EphySidebar::AddSearchEngine (const char *aEngineURL,
			      const char *aIconURL,
			      const PRUnichar *aSuggestedTitle,
			      const PRUnichar *aSuggestedCategory)
{
	NS_ENSURE_ARG (aSuggestedTitle);
	NS_ENSURE_ARG (aIconURL);
	NS_ENSURE_ARG (aEngineURL);

	nsCString title;
	EphyEmbedSingle *single;

	/* FIXME: length-limit string */
	NS_UTF16ToCString (nsDependentString(aSuggestedTitle),
			   NS_CSTRING_ENCODING_UTF8, title);

	LOG ("Adding search engine, engineurl=%s iconurl=%s title=%s", aEngineURL, aIconURL, title.get());

	single = EPHY_EMBED_SINGLE (ephy_embed_shell_get_embed_single (embed_shell));

	gboolean result = FALSE;
	g_signal_emit_by_name (single, "add-search-engine",
			       aEngineURL, aIconURL, title.get(), &result);

	return NS_OK;
}

#ifdef HAVE_GECKO_1_9

/* void addMicrosummaryGenerator (in string generatorURL); */
NS_IMETHODIMP
EphySidebar::AddMicrosummaryGenerator (const char *generatorURL)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

#endif /* HAVE_GECKO_1_9 */

/* static */ NS_METHOD
EphySidebar::Register (nsIComponentManager* aComponentManager,
		       nsIFile* aPath,
		       const char* aRegistryLocation,
		       const char* aComponentType,
		       const nsModuleComponentInfo* aInfo)
{
  nsresult rv;
  nsCOMPtr<nsICategoryManager> catMan (do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS (rv, rv);

  rv = catMan->AddCategoryEntry (JAVASCRIPT_GLOBAL_PROPERTY_CATEGORY,
				 "sidebar",
				 NS_SIDEBAR_CONTRACTID,
				 PR_FALSE /* don't persist */,
				 PR_TRUE /* replace */,
				 nsnull);
  NS_ENSURE_SUCCESS (rv, rv);

  return rv;
}

/* static */ NS_METHOD
EphySidebar::Unregister (nsIComponentManager* aComponentManager,
			 nsIFile* aPath,
			 const char* aRegistryLocation,
			 const nsModuleComponentInfo* aInfo)
{
  nsresult rv;
  nsCOMPtr<nsICategoryManager> catMan (do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS (rv, rv);

  rv = catMan->DeleteCategoryEntry (JAVASCRIPT_GLOBAL_PROPERTY_CATEGORY,
				    "sidebar",
				    PR_FALSE /* don't persist */);
  NS_ENSURE_SUCCESS (rv, rv);

  return rv;
}
