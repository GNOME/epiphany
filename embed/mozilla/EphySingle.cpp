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

#include <nsString.h>
#include <nsICookie2.h>
#include <nsIURI.h>
#include <nsIPermissionManager.h>
#include <nsICookieManager.h>
#include <nsIServiceManager.h>

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

	nsresult rv;
	mObserverService = do_GetService ("@mozilla.org/observer-service;1", &rv);
	if (NS_FAILED (rv) || !mObserverService) return NS_ERROR_FAILURE;

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
EphySingle::EmitCookieNotification (const char *name, nsISupports *aSubject)
{
	LOG ("EmitCookieNotification %s", name)

	nsCOMPtr<nsICookie> cookie = do_QueryInterface (aSubject);
	if (!cookie) return NS_ERROR_FAILURE;

	EphyCookie *info = mozilla_cookie_to_ephy_cookie (cookie);

	g_signal_emit_by_name (EPHY_COOKIE_MANAGER (mOwner), name, info);

	ephy_cookie_free (info);

	return NS_OK;
}

nsresult
EphySingle::EmitPermissionNotification (const char *name, nsISupports *aSubject)
{
	LOG ("EmitPermissionNotification %s", name)

	nsCOMPtr<nsIPermission> perm = do_QueryInterface (aSubject);
	if (!perm) return NS_ERROR_FAILURE;

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
			rv = EmitCookieNotification ("added", aSubject);
		}
		/* "deleted" */
		else if (aData[0] == 'd')
		{
			rv = EmitCookieNotification ("deleted", aSubject);
		}
		/* "changed" */
		else if (aData[0] == 'c' && aData[1] == 'h')
		{
			rv = EmitCookieNotification ("changed", aSubject);
		}
		/* "cleared" */
		else if (aData[0] == 'c' && aData[2] == 'l')
		{
			LOG ("EphySingle::cookie-changed::cleared")

			g_signal_emit_by_name (EPHY_COOKIE_MANAGER (mOwner), "cleared");
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
			nsCAutoString spec;
			uri->GetSpec (spec);

			g_signal_emit_by_name (EPHY_COOKIE_MANAGER (mOwner), "rejected", spec.get());
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
			rv = EmitPermissionNotification ("added", aSubject);
		}
		/* "deleted" */
		else if (aData[0] == 'd')
		{
			rv = EmitPermissionNotification ("deleted", aSubject);
		}
		/* "changed" */
		else if (aData[0] == 'c' && aData[1] == 'h')
		{
			rv = EmitPermissionNotification ("changed", aSubject);
		}
		/* "cleared" */
		else if (aData[0] == 'c' && aData[1] == 'l')
		{
			LOG ("EphySingle::perm-changed::cleared")

			g_signal_emit_by_name (EPHY_PERMISSION_MANAGER (mOwner), "cleared");
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

	nsCAutoString transfer;

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

#if MOZILLA_SNAPSHOT > 9
	nsCOMPtr<nsICookie2> cookie2 = do_QueryInterface (cookie);
	if (cookie2)
	{
		
		PRBool isSession;
		cookie2->GetIsSession (&isSession);
		info->is_session = isSession != PR_FALSE;
		
		if (!isSession)
		{
			PRInt64 expiry;
			cookie2->GetExpiry (&expiry);
			info->real_expires = expiry;
		}
	}
#endif

	return info;
}

EphyPermissionInfo *
mozilla_permission_to_ephy_permission (nsIPermission *perm)
{
	EphyPermissionType type = (EphyPermissionType) 0;

	nsresult result;
#if MOZILLA_SNAPSHOT >= 10
	nsCAutoString str;
	result = perm->GetType(str);
	if (NS_FAILED (result)) return NULL;

	if (str.Equals ("cookie"))
	{
		type = EPT_COOKIE;
	}
	else if (str.Equals ("image"))
	{
		type = EPT_IMAGE;
	}
	else if (str.Equals ("popup"))
	{
		type = EPT_POPUP;
	}		
#else
	PRUint32 num;
	result = perm->GetType(&num);
	if (NS_FAILED (result)) return NULL;

	type = (EphyPermissionType) num;
#endif

	PRUint32 cap;
	perm->GetCapability(&cap);
	gboolean allowed;
	switch (cap)
	{
		case nsIPermissionManager::ALLOW_ACTION:
			allowed = TRUE;
			break;
		case nsIPermissionManager::DENY_ACTION:
		case nsIPermissionManager::UNKNOWN_ACTION:
		default :
			allowed = FALSE;
			break;
	}

	nsCString host;
	perm->GetHost(host);

	return ephy_permission_info_new (host.get(), type, allowed);
}
