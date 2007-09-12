/*
 *  Copyright © 2003 Christian Persch
 *  Copyright © 2003 Marco Pesenti Gritti
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

#include "mozilla-config.h"
#include "config.h"

#include <nsStringAPI.h>

#include <nsICookie.h>
#include <nsICookie2.h>
#include <nsICookieManager.h>
#include <nsIHttpChannel.h>
#include <nsIObserverService.h>
#include <nsIPermission.h>
#include <nsIPermissionManager.h>
#include <nsIPropertyBag2.h>
#include <nsIServiceManager.h>
#include <nsIURI.h>
#include <nsServiceManagerUtils.h>
#include <nsWeakReference.h>

#ifdef ALLOW_PRIVATE_API
#include <nsIIDNService.h>
#endif

#include "ephy-debug.h"

#include "EphySingle.h"

NS_IMPL_ISUPPORTS1(EphySingle, nsIObserver)

EphySingle::EphySingle()
: mOwner(nsnull)
, mIsOnline(PR_TRUE) /* nsIOService doesn't send an initial notification, assume we start on-line */
{
	LOG ("EphySingle ctor");
}

nsresult
EphySingle::Init (EphyEmbedSingle *aOwner)
{
	mObserverService = do_GetService ("@mozilla.org/observer-service;1");
	NS_ENSURE_TRUE (mObserverService, NS_ERROR_FAILURE);

	nsresult rv;
	rv = mObserverService->AddObserver (this, "cookie-changed", PR_FALSE);
	rv |= mObserverService->AddObserver (this, "cookie-rejected", PR_FALSE);
	rv |= mObserverService->AddObserver (this, "perm-changed", PR_FALSE);
	rv |= mObserverService->AddObserver (this, "network:offline-status-changed", PR_FALSE);
	rv |= mObserverService->AddObserver (this, "signonChanged", PR_FALSE);
	rv |= mObserverService->AddObserver (this, "http-on-examine-response", PR_FALSE);
	rv |= mObserverService->AddObserver (this, "http-on-modify-request", PR_FALSE);
	NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

	mOwner = aOwner;

	LOG ("EphySingle::Init");

	return NS_OK;
}

nsresult
EphySingle::Detach ()
{
	LOG ("EphySingle::Detach");

	if (mObserverService)
	{
		mObserverService->RemoveObserver (this, "cookie-changed");
		mObserverService->RemoveObserver (this, "cookie-rejected");
		mObserverService->RemoveObserver (this, "perm-changed");
		mObserverService->RemoveObserver (this, "signonChanged");
		mObserverService->RemoveObserver (this, "network:offline-status-changed");
		mObserverService->RemoveObserver (this, "http-on-examine-response");
		mObserverService->RemoveObserver (this, "http-on-modify-request");

#if 1
		/* HACK: Work around https://bugzilla.mozilla.org/show_bug.cgi?id=292699 */
		mObserverService->NotifyObservers(nsnull, "profile-change-net-teardown", nsnull); 
#endif
	}

	return NS_OK;
}

EphySingle::~EphySingle()
{
	LOG ("EphySingle dtor");

	mOwner = nsnull;
}

nsresult
EphySingle::EmitCookieNotification (const char *name,
				    nsISupports *aSubject)
{
	LOG ("EmitCookieNotification %s", name);

	nsCOMPtr<nsICookie> cookie = do_QueryInterface (aSubject);
	NS_ENSURE_TRUE (cookie, NS_ERROR_FAILURE);

	EphyCookie *info = mozilla_cookie_to_ephy_cookie (cookie);

	g_signal_emit_by_name (EPHY_COOKIE_MANAGER (mOwner), name, info);

	ephy_cookie_free (info);

	return NS_OK;
}

nsresult
EphySingle::EmitPermissionNotification (const char *name,
					nsISupports *aSubject)
{
	LOG ("EmitPermissionNotification %s", name);

	nsCOMPtr<nsIPermission> perm = do_QueryInterface (aSubject);
	NS_ENSURE_TRUE (perm, NS_ERROR_FAILURE);

	EphyPermissionInfo *info =
		mozilla_permission_to_ephy_permission (perm);

	g_signal_emit_by_name (EPHY_PERMISSION_MANAGER (mOwner), name, info);

	ephy_permission_info_free (info);

	return NS_OK;
}

nsresult
EphySingle::ExamineCookies (nsISupports *aSubject)
{
	PRBool isBlockingCookiesChannel = PR_FALSE;

	nsCOMPtr<nsIPropertyBag2> props (do_QueryInterface(aSubject));
	if (props &&
	    NS_SUCCEEDED (props->GetPropertyAsBool(
			  NS_LITERAL_STRING("epiphany-blocking-cookies"), 
			  &isBlockingCookiesChannel)) &&
	    isBlockingCookiesChannel)
	{
		nsCOMPtr<nsIHttpChannel> httpChannel (do_QueryInterface(aSubject));

		if (httpChannel)
			httpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Cookie"),
						      EmptyCString(), PR_FALSE);
	}

	return NS_OK;
}

nsresult
EphySingle::ExamineResponse (nsISupports *aSubject)
{
	return ExamineCookies (aSubject);
}

nsresult
EphySingle::ExamineRequest (nsISupports *aSubject)
{
	return ExamineCookies (aSubject);
}

/* void observe (in nsISupports aSubject, in string aTopic, in wstring aData); */
NS_IMETHODIMP EphySingle::Observe(nsISupports *aSubject,
				  const char *aTopic,
				  const PRUnichar *aData)
{
	nsresult rv = NS_OK;

	LOG ("EphySingle::Observe topic %s", aTopic);

	if (strcmp (aTopic, "http-on-examine-response") == 0)
	{
		rv = ExamineResponse (aSubject);
	}
	else if (strcmp (aTopic, "http-on-modify-request") == 0)
	{
		rv = ExamineRequest (aSubject);
	}
	else if (strcmp (aTopic, "cookie-changed") == 0)
	{
		/* "added" */
		if (aData[0] == 'a')
		{
			rv = EmitCookieNotification ("cookie-added", aSubject);
		}
		/* "deleted" */
		else if (aData[0] == 'd')
		{
			rv = EmitCookieNotification ("cookie-deleted", aSubject);
		}
		/* "changed" */
		else if (aData[0] == 'c' && aData[1] == 'h')
		{
			rv = EmitCookieNotification ("cookie-changed", aSubject);
		}
		/* "cleared" */
		else if (aData[0] == 'c' && aData[1] == 'l')
		{
			LOG ("EphySingle::cookie-changed::cleared");

			g_signal_emit_by_name (EPHY_COOKIE_MANAGER (mOwner), "cookies-cleared");
		}
		else
		{
			g_warning ("EphySingle unexpected data!\n");
			rv = NS_ERROR_FAILURE;
		}
	}
	else if (strcmp (aTopic, "cookie-rejected") == 0)
	{
		LOG ("EphySingle::cookie-rejected");

		nsCOMPtr<nsIURI> uri = do_QueryInterface (aSubject);
		if (uri)
		{
			nsCString spec;
			uri->GetSpec (spec);

			g_signal_emit_by_name (EPHY_COOKIE_MANAGER (mOwner), "cookie-rejected", spec.get());
		}
		else
		{
			rv = NS_ERROR_FAILURE;
		}
	}
	else if (strcmp (aTopic, "perm-changed") == 0)
	{
		/* "added" */
		if (aData[0] == 'a')
		{
			rv = EmitPermissionNotification ("permission-added", aSubject);
		}
		/* "deleted" */
		else if (aData[0] == 'd')
		{
			rv = EmitPermissionNotification ("permission-deleted", aSubject);
		}
		/* "changed" */
		else if (aData[0] == 'c' && aData[1] == 'h')
		{
			rv = EmitPermissionNotification ("permission-changed", aSubject);
		}
		/* "cleared" */
		else if (aData[0] == 'c' && aData[1] == 'l')
		{
			LOG ("EphySingle::perm-changed::cleared");

			g_signal_emit_by_name (EPHY_PERMISSION_MANAGER (mOwner), "permissions-cleared");
		}
		else
		{
			g_warning ("EphySingle unexpected data!\n");
			rv = NS_ERROR_FAILURE;
		}
	}
	else if (strcmp (aTopic, "signonChanged") == 0)
	{
		/* aData can be PRUnichar[] "signons", "rejects", "nocaptures" and "nopreviews" */
		if (aData[0] == 's')
		{
			g_signal_emit_by_name (mOwner, "passwords-changed");
		}
	}
	else if (strcmp (aTopic, "network:offline-status-changed") == 0)
	{
		/* aData is either (PRUnichar[]) "offline" or "online" */
		mIsOnline = (aData && aData[0] == 'o' && aData[1] == 'n');

		g_object_notify (G_OBJECT (mOwner), "network-status");
	}
	else
	{
		g_warning ("EphySingle observed unknown topic '%s'!\n", aTopic);
		rv = NS_ERROR_FAILURE;
	}

	LOG ("EphySingle::Observe %s", NS_SUCCEEDED (rv) ? "success" : "FAILURE");

	return rv;
}

EphyCookie *
mozilla_cookie_to_ephy_cookie (nsICookie *cookie)
{
	EphyCookie *info;

	nsCString transfer;

	cookie->GetHost (transfer);

	nsCOMPtr<nsIIDNService> idnService
		(do_GetService ("@mozilla.org/network/idn-service;1"));
	NS_ENSURE_TRUE (idnService, nsnull);

	nsCString decoded;
	/* ToUTF8 never fails, no need to check return value */
	idnService->ConvertACEtoUTF8 (transfer, decoded);

	info = ephy_cookie_new ();
	info->domain = g_strdup (decoded.get());

	cookie->GetName (transfer);
	info->name = g_strdup (transfer.get());
	cookie->GetValue (transfer);
	info->value = g_strdup (transfer.get());
	cookie->GetPath (transfer);
	info->path = g_strdup (transfer.get());

	PRBool isSecure;
	cookie->GetIsSecure (&isSecure);
	info->is_secure = isSecure != PR_FALSE;

	PRUint64 dateTime;
	cookie->GetExpires (&dateTime);
	info->expires = dateTime;

	nsCOMPtr<nsICookie2> cookie2 = do_QueryInterface (cookie);
	NS_ENSURE_TRUE (cookie2, info);
		
	PRBool isSession;
	cookie2->GetIsSession (&isSession);
	info->is_session = isSession != PR_FALSE;
		
	if (!isSession)
	{
		PRInt64 expiry;
		cookie2->GetExpiry (&expiry);
		info->real_expires = expiry;
	}

        PRBool isHttpOnly = PR_FALSE;
        cookie2->GetIsHttpOnly (&isHttpOnly);
        info->is_http_only = isHttpOnly != PR_FALSE;

	return info;
}

EphyPermissionInfo *
mozilla_permission_to_ephy_permission (nsIPermission *perm)
{
	nsresult rv;
	nsCString type;
	rv = perm->GetType(type);
	NS_ENSURE_SUCCESS (rv, NULL);

	PRUint32 cap;
	perm->GetCapability(&cap);
	EphyPermission permission;
	switch (cap)
	{
		case nsIPermissionManager::ALLOW_ACTION:
			permission = EPHY_PERMISSION_ALLOWED;
			break;
		case nsIPermissionManager::DENY_ACTION:
			permission = EPHY_PERMISSION_DENIED;
			break;
		case nsIPermissionManager::UNKNOWN_ACTION:
		default :
			permission = EPHY_PERMISSION_DEFAULT;
			break;
	}

	nsCString host;
	perm->GetHost(host);

	nsCOMPtr<nsIIDNService> idnService
		(do_GetService ("@mozilla.org/network/idn-service;1"));
	NS_ENSURE_TRUE (idnService, nsnull);

	nsCString decodedHost;
	idnService->ConvertACEtoUTF8 (host, decodedHost);

	return ephy_permission_info_new (decodedHost.get(), type.get(), permission);
}
