/*
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
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  $Id$
 */

#include <xpcom-config.h>
#include "config.h"

#include <string.h>

#ifndef HAVE_GECKO_1_9
// for nsNetUtil.h
#define MOZILLA_INTERNAL_API 1
#endif

#include <nsStringAPI.h>

#include <nsAppDirectoryServiceDefs.h>
#include <nsCOMPtr.h>
#include <nsEnumeratorUtils.h>
#include <nsIDirectoryService.h>
#include <nsIIOService.h>
#include <nsILocalFile.h>
#include <nsISupportsArray.h>
#include <nsIToolkitChromeRegistry.h>
#include <nsNetUtil.h>

#include "EphyDirectoryProvider.h"

NS_IMPL_ISUPPORTS2 (EphyDirectoryProvider,
 		    nsIDirectoryServiceProvider,
		    nsIDirectoryServiceProvider2)


/* nsIFile getFile (in string prop, out PRBool persistent); */
NS_IMETHODIMP
EphyDirectoryProvider::GetFile (const char *prop,
			        PRBool *persistent,
			        nsIFile **_retval)
{
	return NS_ERROR_FAILURE;
}

/* nsISimpleEnumerator getFiles (in string prop); */
NS_IMETHODIMP
EphyDirectoryProvider::GetFiles (const char *prop,
				 nsISimpleEnumerator **_retval)
{
	nsresult rv = NS_ERROR_FAILURE;

	if (prop && strcmp (prop, NS_CHROME_MANIFESTS_FILE_LIST) == 0)
	{
		nsCOMPtr<nsILocalFile> manifestDir;
		rv = NS_NewNativeLocalFile (nsDependentCString(SHARE_DIR "/chrome"), PR_TRUE,
					    getter_AddRefs (manifestDir));
		NS_ENSURE_SUCCESS (rv, rv);

		nsCOMPtr<nsISupports> element (do_QueryInterface (manifestDir, &rv));
		NS_ENSURE_SUCCESS (rv, rv);

		/* FIXME: this sucks!
		 * When we don't implement a directory service provider,
		 * the chrome registry takes its manifests files from the
		 * app chrome dir; but it doesn't append this dir when
		 * we do provide our own (additional) chrome manifest dirs!
		 * http://lxr.mozilla.org/seamonkey/source/chrome/src/nsChromeRegistry.cpp#1147
		 */
		nsCOMPtr<nsIProperties> dirServ (do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv));
		NS_ENSURE_SUCCESS (rv, rv);

		nsCOMPtr<nsIFile> chromeDir;
		rv = dirServ->Get (NS_APP_CHROME_DIR, NS_GET_IID (nsIFile),
				   getter_AddRefs (chromeDir));
		NS_ENSURE_SUCCESS (rv, rv);

		nsCOMPtr<nsISupportsArray> array;
		rv = NS_NewISupportsArray (getter_AddRefs (array));
		NS_ENSURE_SUCCESS (rv, rv);

		rv = array->AppendElement (manifestDir);
		rv |= array->AppendElement (chromeDir);
		NS_ENSURE_SUCCESS (rv, rv);

		rv = NS_NewArrayEnumerator (_retval, array);
		NS_ENSURE_SUCCESS (rv, rv);

		rv = NS_SUCCESS_AGGREGATE_RESULT;
	}

	return rv;
}
