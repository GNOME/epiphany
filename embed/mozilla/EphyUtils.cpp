/*
 *  Copyright © 2004 Marco Pesenti Gritti
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

#include <nsStringAPI.h>

#include <gtkmozembed.h>
#include <nsCOMPtr.h>
#include <nsIDOMWindow.h>
#include <nsIEmbeddingSiteWindow.h>
#include <nsIFile.h>
#include <nsIIOService.h>
#include <nsIServiceManager.h>
#include <nsIURI.h>
#include <nsIWebBrowserChrome.h>
#include <nsIWindowWatcher.h>
#include <nsIXPConnect.h>
#include <nsServiceManagerUtils.h>
#include <nsXPCOM.h>

#ifdef HAVE_GECKO_1_9
#include <nsPIDOMWindow.h>
#include <nsDOMJSUtils.h> /* for GetScriptContextFromJSContext */
#include <nsIScriptContext.h>
#include <nsIScriptGlobalObject.h>
#endif

#include "ephy-embed-shell.h"
#include "ephy-embed-single.h"
#include "ephy-file-helpers.h"

#include "EphyUtils.h"

nsresult
EphyUtils::GetIOService (nsIIOService **ioService)
{
	nsresult rv;

	nsCOMPtr<nsIServiceManager> mgr; 
	NS_GetServiceManager (getter_AddRefs (mgr));
	if (!mgr) return NS_ERROR_FAILURE;

	rv = mgr->GetServiceByContractID ("@mozilla.org/network/io-service;1",
					  NS_GET_IID (nsIIOService),
					  (void **)ioService);
	return rv;
}

nsresult
EphyUtils::NewURI (nsIURI **result,
		   const nsAString &spec,
		   const char *charset,
		   nsIURI *baseURI)
{
	nsCString cSpec;
	NS_UTF16ToCString (spec, NS_CSTRING_ENCODING_UTF8, cSpec);

	return NewURI (result, cSpec, charset, baseURI);
}

nsresult
EphyUtils::NewURI (nsIURI **result,
		   const nsACString &spec,
		   const char *charset,
		   nsIURI *baseURI)
{
	nsresult rv;
	nsCOMPtr<nsIIOService> ioService;
	rv = EphyUtils::GetIOService (getter_AddRefs (ioService));
	NS_ENSURE_SUCCESS (rv, rv);

	return ioService->NewURI (spec, charset, baseURI, result);
}

nsresult
EphyUtils::NewFileURI (nsIURI **result,
		       nsIFile *spec)
{
	nsresult rv;
	nsCOMPtr<nsIIOService> ioService;
	rv = EphyUtils::GetIOService (getter_AddRefs (ioService));
	NS_ENSURE_SUCCESS (rv, rv);

	return ioService->NewFileURI (spec, result);
}

GtkWidget *
EphyUtils::FindEmbed (nsIDOMWindow *aDOMWindow)
{
	if (!aDOMWindow) return nsnull;

	nsCOMPtr<nsIWindowWatcher> wwatch
		(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
	NS_ENSURE_TRUE (wwatch, nsnull);

	/* this DOM window may belong to some inner frame, we need
	 * to get the topmost DOM window to get the embed
	 */
	nsCOMPtr<nsIDOMWindow> topWindow;
	aDOMWindow->GetTop (getter_AddRefs (topWindow));
	if (!topWindow) return nsnull;
	
	nsCOMPtr<nsIWebBrowserChrome> windowChrome;
	wwatch->GetChromeForWindow (topWindow, getter_AddRefs(windowChrome));
	NS_ENSURE_TRUE (windowChrome, nsnull);

	nsCOMPtr<nsIEmbeddingSiteWindow> window (do_QueryInterface(windowChrome));
	NS_ENSURE_TRUE (window, nsnull);

	nsresult rv;
	GtkWidget *mozembed;
	GtkWidget **cache_ptr = &mozembed;
	rv = window->GetSiteWindow ((void **) cache_ptr);
	NS_ENSURE_SUCCESS (rv, nsnull);

	return mozembed;
}

GtkWidget *
EphyUtils::FindGtkParent (nsIDOMWindow *aDOMWindow)
{
	GtkWidget *embed = FindEmbed (aDOMWindow);
	if (!embed) return nsnull;

	GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (embed));
	if (!GTK_WIDGET_TOPLEVEL (toplevel)) return nsnull;

	return toplevel;
}

char *
EphyUtils::ConvertUTF16toUTF8 (const PRUnichar *aText,
                               PRInt32 aMaxLength)
{
        if (aText == nsnull) return NULL;

        /* This depends on the assumption that
         * typeof(PRUnichar) == typeof (gunichar2) == uint16,
         * which should be pretty safe.
         */
        glong n_read = 0, n_written = 0;
        char *converted = g_utf16_to_utf8 ((gunichar2*) aText, aMaxLength,
                                            &n_read, &n_written, NULL);
        /* FIXME loop from the end while !g_unichar_isspace (char)? */

        return converted;
}

/* This isn't completely accurate: if you do window.prompt in one window, then
 * call this in another window, it still returns TRUE ! Those are the wonders
 * of recursive mainloops :-(
 */
PRBool
EphyJSUtils::IsCalledFromScript ()
{
	nsresult rv;
	nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &rv));
	NS_ENSURE_SUCCESS (rv, PR_FALSE);
		
	nsCOMPtr<nsIXPCNativeCallContext> ncc;
	rv = xpc->GetCurrentNativeCallContext (getter_AddRefs (ncc));
	NS_ENSURE_SUCCESS(rv, PR_FALSE);

	return nsnull != ncc;
}

/* NOTE: Only call this when we're SURE that we're called directly from JS! */
nsIDOMWindow *
EphyJSUtils::GetDOMWindowFromCallContext ()
{
  /* TODO: We can do this on 1.8 too, but we'd need to use headers which include private string API
   * so we'll have to move this to MozillaPrivate
   */
#ifdef HAVE_GECKO_1_9
  nsresult rv;
  nsCOMPtr<nsIXPConnect> xpc (do_GetService(nsIXPConnect::GetCID(), &rv));
  NS_ENSURE_SUCCESS (rv, nsnull);
		
  nsCOMPtr<nsIXPCNativeCallContext> ncc;
  rv = xpc->GetCurrentNativeCallContext (getter_AddRefs (ncc));
  NS_ENSURE_SUCCESS (rv, nsnull);

  JSContext *cx = nsnull;
  rv = ncc->GetJSContext(&cx);
  NS_ENSURE_TRUE (NS_SUCCEEDED (rv) && cx, nsnull);

  nsIScriptContext* scriptContext = GetScriptContextFromJSContext (cx);
  if (!scriptContext) return nsnull;

  nsIScriptGlobalObject *globalObject = scriptContext->GetGlobalObject();
  if (!globalObject) return nsnull;

  nsCOMPtr<nsPIDOMWindow> piWindow (do_QueryInterface (globalObject));
  if (!piWindow) return nsnull;

  return piWindow->GetOuterWindow ();
#else
  return nsnull;
#endif
}
