/*
 *  Copyright (C) 2002 Philip Langdale
 *  Copyright (C) 2004 Crispin Flowerday
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

#include <nsCOMPtr.h>
#include <nsCRT.h>
#include <nsIProgrammingLanguage.h>

#ifdef HAVE_GECKO_1_9
#include <nsIClassInfoImpl.h>
#endif

#undef MOZILLA_INTERNAL_API
#include <nsEmbedString.h>
#define MOZILLA_INTERNAL_API 1

#include "EphySidebar.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-single.h"
#include "ephy-debug.h"

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
	nsEmbedCString title;
	EphyEmbedSingle *single;

	NS_UTF16ToCString (nsEmbedString(aTitle),
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
	nsEmbedCString title;
	EphyEmbedSingle *single;

	NS_UTF16ToCString (nsEmbedString(aSuggestedTitle),
			   NS_CSTRING_ENCODING_UTF8, title);

	LOG ("Adding search engine, engineurl=%s iconurl=%s title=%s", aEngineURL, aIconURL, title.get());

	single = EPHY_EMBED_SINGLE (ephy_embed_shell_get_embed_single (embed_shell));

	gboolean result = FALSE;
	g_signal_emit_by_name (single, "add-search-engine",
			       aEngineURL, aIconURL, title.get(), &result);

	return NS_OK;
}
