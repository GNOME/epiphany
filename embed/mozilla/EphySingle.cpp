/*
 *  Copyright (C) 2003 Christian Persch
 *  Copyright (C) 2003 Marco Pesenti Gritti
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "EphySingle.h"

#include "ephy-debug.h"

#include <nsEmbedString.h>
#include <nsIURI.h>
#include <nsIPermissionManager.h>
#include <nsICookieManager.h>
#include <nsIServiceManager.h>
#include <nsICookie2.h>

NS_IMPL_ISUPPORTS1(EphySingle, nsIObserver)

EphySingle::EphySingle()
{
	LOG ("EphySingle constructor")

	mOwner = nsnull;
}

nsresult
EphySingle::Init (EphyEmbedSingle *aOwner)
{
	LOG ("EphySingle::Init")

	mOwner = aOwner;

	mObserverService = do_GetService ("@mozilla.org/observer-service;1");
	NS_ENSURE_TRUE (mObserverService, NS_ERROR_FAILURE);

	mObserverService->AddObserver (this, "cookie-changed", PR_FALSE);
	mObserverService->AddObserver (this, "cookie-rejected", PR_FALSE);
	mObserverService->AddObserver (this, "perm-changed", PR_FALSE);

	return NS_OK;
}

nsresult
EphySingle::Detach ()
{
	if (mObserverService)
	{
		mObserverService->RemoveObserver (this, "cookie-changed");
		mObserverService->RemoveObserver (this, "cookie-rejected");
		mObserverService->RemoveObserver (this, "perm-changed");
	}

	return NS_OK;
}

EphySingle::~EphySingle()
{
	LOG ("EphySingle destructor")

	Detach();
	mOwner = nsnull;
}

nsresult
EphySingle::EmitCookieNotification (const char *name,
				    nsISupports *aSubject)
{
	LOG ("EmitCookieNotification %s", name)

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
	LOG ("EmitPermissionNotification %s", name)

	nsCOMPtr<nsIPermission> perm = do_QueryInterface (aSubject);
	NS_ENSURE_TRUE (perm, NS_ERROR_FAILURE);

	EphyPermissionInfo *info =
		mozilla_permission_to_ephy_permission (perm);

	g_signal_emit_by_name (EPHY_PERMISSION_MANAGER (mOwner), name, info);

	ephy_permission_info_free (info);

	return NS_OK;
}

/* void observe (in nsISupports aSubject, in string aTopic, in wstring aData); */
NS_IMETHODIMP EphySingle::Observe(nsISupports *aSubject,
				  const char *aTopic,
				  const PRUnichar *aData)
{
	nsresult rv = NS_OK;

	if (strcmp (aTopic, "cookie-changed") == 0)
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
			LOG ("EphySingle::cookie-changed::cleared")

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
		LOG ("EphySingle::cookie-rejected")

		nsCOMPtr<nsIURI> uri = do_QueryInterface (aSubject);
		if (uri)
		{
			nsEmbedCString spec;
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
			LOG ("EphySingle::perm-changed::cleared")

			g_signal_emit_by_name (EPHY_PERMISSION_MANAGER (mOwner), "permissions-cleared");
		}
		else
		{
			g_warning ("EphySingle unexpected data!\n");
			rv = NS_ERROR_FAILURE;
		}
	}
	else
	{
		g_warning ("EphySingle observed unknown topic!\n");
		rv = NS_ERROR_FAILURE;
	}

	return rv;
}

EphyCookie *
mozilla_cookie_to_ephy_cookie (nsICookie *cookie)
{
	EphyCookie *info = ephy_cookie_new ();

	nsEmbedCString transfer;

	cookie->GetHost (transfer);
	info->domain = g_strdup (transfer.get());
	cookie->GetName (transfer);
	info->name = g_strdup (transfer.get());
	cookie->GetValue (transfer);
	info->value = g_strdup (transfer.get());
	cookie->GetPath (transfer);
	info->path = g_strdup (transfer.get());

	PRBool isSecure;
	cookie->GetIsSecure (&isSecure);
	info->is_secure = isSecure != PR_FALSE;

	nsCookieStatus status;
	cookie->GetStatus (&status);
	info->p3p_state = status;

	nsCookiePolicy policy;
	cookie->GetPolicy (&policy);
	info->p3p_policy = policy;

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

	return info;
}

EphyPermissionInfo *
mozilla_permission_to_ephy_permission (nsIPermission *perm)
{
	EphyPermissionType type = (EphyPermissionType) 0;

	nsresult result;
	nsEmbedCString str;
	result = perm->GetType(str);
	NS_ENSURE_SUCCESS (result, NULL);

	if (strcmp (str.get(), "cookie") == 0)
	{
		type = EPT_COOKIE;
	}
	else if (strcmp (str.get(), "image"))
	{
		type = EPT_IMAGE;
	}
	else if (strcmp (str.get(), "popup"))
	{
		type = EPT_POPUP;
	}		

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

	nsEmbedCString host;
	perm->GetHost(host);

	return ephy_permission_info_new (host.get(), type, permission);
}
