/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Copyright (C) 2003 Christian Persch
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

/* Relevant Mozilla bug numbers:
 *
 * The API will change soon:
 * 	http://bugzilla.mozilla.org/show_bug.cgi?id=191839
 *	"Content Policy API sucks rock"
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "EphyContentPolicy.h"

#include "eel-gconf-extensions.h"
#include "ephy-debug.h"

#include <nsCOMPtr.h>
#include <nsIURI.h>

#ifdef ALLOW_PRIVATE_STRINGS
#include <nsString.h>
#endif

#define CONF_LOCKDOWN_DISABLE_UNSAFE_PROTOCOLS	"/apps/epiphany/lockdown/disable_unsafe_protocols"
#define CONF_LOCKDOWN_ADDITIONAL_SAFE_PROTOCOLS	"/apps/epiphany/lockdown/additional_safe_protocols"

NS_IMPL_ISUPPORTS1(EphyContentPolicy, nsIContentPolicy)

EphyContentPolicy::EphyContentPolicy()
{
	LOG ("EphyContentPolicy constructor")

	mLocked = eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_UNSAFE_PROTOCOLS);
	mSafeProtocols = eel_gconf_get_string_list (CONF_LOCKDOWN_ADDITIONAL_SAFE_PROTOCOLS);

	mSafeProtocols = g_slist_prepend (mSafeProtocols, g_strdup ("https"));
	mSafeProtocols = g_slist_prepend (mSafeProtocols, g_strdup ("http"));
}

EphyContentPolicy::~EphyContentPolicy()
{
	LOG ("EphyContentPolicy destructor")

	g_slist_foreach (mSafeProtocols, (GFunc) g_free, NULL);
	g_slist_free (mSafeProtocols);
}

/* boolean shouldLoad (in PRInt32 contentType, in nsIURI contentLocation, in nsISupports ctxt, in nsIDOMWindow window); */
NS_IMETHODIMP EphyContentPolicy::ShouldLoad(PRInt32 contentType,
					    nsIURI *contentLocation,
					    nsISupports *ctxt,
					    nsIDOMWindow *window,
					    PRBool *_retval)
{
	if (!mLocked)
	{
		LOG ("Not locked!")

		*_retval = PR_TRUE;

		return NS_OK;
	}

	nsCAutoString scheme;
	contentLocation->GetScheme (scheme);

	nsCAutoString spec;
	contentLocation->GetSpec (spec);

	LOG ("ShouldLoad type=%d location=%s (scheme %s)", contentType, spec.get(), scheme.get())

	*_retval = PR_FALSE;

	/* Allow the load if the protocol is in safe list, or it's about:blank */
	if (g_slist_find_custom (mSafeProtocols, scheme.get(), (GCompareFunc) strcmp)
	    || spec.Equals ("about:blank"))
	{
		*_retval = PR_TRUE;
	}

	LOG ("Decision: %sallowing load", *_retval == PR_TRUE ? "" : "NOT ")

	return NS_OK;
}

/* boolean shouldProcess (in PRInt32 contentType, in nsIURI documentLocation, in nsISupports ctxt, in nsIDOMWindow window); */
NS_IMETHODIMP EphyContentPolicy::ShouldProcess(PRInt32 contentType,
					       nsIURI *documentLocation,
					       nsISupports *ctxt,
					       nsIDOMWindow *window,
					       PRBool *_retval)
{
	/* As far as I can tell from reading mozilla code, this is never called. */

	LOG ("ShouldProcess: this is quite unexpected!")

	*_retval = PR_TRUE;

	return NS_OK;
}
