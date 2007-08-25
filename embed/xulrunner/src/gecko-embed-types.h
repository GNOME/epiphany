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
 *    Ramiro Estrugo <ramiro@eazel.com>
 *  ---------------------------------------------------------------------------
 *
 *  $Id$
 */

#ifndef __GECKO_EMBED_TYPES_H__
#define __GECKO_EMBED_TYPES_H__

#include <glib-object.h>
#include "gecko-embed-type-builtins.h"

G_BEGIN_DECLS

/* These are straight out of nsIWebProgressListener.h */

typedef enum
{
  GECKO_EMBED_FLAG_START		= 1U << 0,
  GECKO_EMBED_FLAG_REDIRECTING		= 1U << 1,
  GECKO_EMBED_FLAG_TRANSFERRING		= 1U << 2,
  GECKO_EMBED_FLAG_NEGOTIATING		= 1U << 3,
  GECKO_EMBED_FLAG_STOP 		= 1U << 4,
  
  GECKO_EMBED_FLAG_IS_REQUEST 		= 1U << 16,
  GECKO_EMBED_FLAG_IS_DOCUMENT		= 1U << 17,
  GECKO_EMBED_FLAG_IS_NETWORK		= 1U << 18,
  GECKO_EMBED_FLAG_IS_WINDOW		= 1U << 19,

  GECKO_EMBED_FLAG_RESTORING		= 1U << 24,
} GeckoEmbedProgressFlags;

/* These are from various networking headers */

typedef enum
{
  /* NS_ERROR_UNKNOWN_HOST */
  GECKO_EMBED_STATUS_FAILED_DNS     = 2152398878U,
 /* NS_ERROR_CONNECTION_REFUSED */
  GECKO_EMBED_STATUS_FAILED_CONNECT = 2152398861U,
 /* NS_ERROR_NET_TIMEOUT */
  GECKO_EMBED_STATUS_FAILED_TIMEOUT = 2152398862U,
 /* NS_BINDING_ABORTED */
  GECKO_EMBED_STATUS_FAILED_USERCANCELED = 2152398850U
} GeckoEmbedStatusEnum;

/* These used to be straight out of nsIWebNavigation.h until the API
   changed.  Now there's a mapping table that maps these values to the
   internal values. */

typedef enum 
{
  GECKO_EMBED_FLAG_RELOADNORMAL			= 0,
  GECKO_EMBED_FLAG_RELOADBYPASSCACHE		= 1,
  GECKO_EMBED_FLAG_RELOADBYPASSPROXY		= 2,
  GECKO_EMBED_FLAG_RELOADBYPASSPROXYANDCACHE	= 3,
  GECKO_EMBED_FLAG_RELOADCHARSETCHANGE		= 4
} GeckoEmbedReloadFlags;

/* These are straight out of nsIWebBrowserChrome.h */

typedef enum
{
  GECKO_EMBED_FLAG_DEFAULTCHROME	= 1U << 0,
  GECKO_EMBED_FLAG_WINDOWBORDERSON	= 1U << 1,
  GECKO_EMBED_FLAG_WINDOWCLOSEON	= 1U << 2,
  GECKO_EMBED_FLAG_WINDOWRESIZEON	= 1U << 3,
  GECKO_EMBED_FLAG_MENUBARON		= 1U << 4,
  GECKO_EMBED_FLAG_TOOLBARON		= 1U << 5,
  GECKO_EMBED_FLAG_LOCATIONBARON	= 1U << 6,
  GECKO_EMBED_FLAG_STATUSBARON		= 1U << 7,
  GECKO_EMBED_FLAG_PERSONALTOOLBARON	= 1U << 8,
  GECKO_EMBED_FLAG_SCROLLBARSON		= 1U << 9,
  GECKO_EMBED_FLAG_TITLEBARON		= 1U << 10,
  GECKO_EMBED_FLAG_EXTRACHROMEON	= 1U << 11,
  GECKO_EMBED_FLAG_ALLCHROME		= 1U << 12,
  GECKO_EMBED_FLAG_WINDOWRAISED		= 1U << 25,
  GECKO_EMBED_FLAG_WINDOWLOWERED	= 1U << 26,
  GECKO_EMBED_FLAG_CENTERSCREEN		= 1U << 27,
  GECKO_EMBED_FLAG_DEPENDENT		= 1U << 28,
  GECKO_EMBED_FLAG_MODAL		= 1U << 29,
  GECKO_EMBED_FLAG_OPENASDIALOG		= 1U << 30,
  GECKO_EMBED_FLAG_OPENASCHROME		= 1U << 31, 
} GeckoEmbedChromeFlags;

G_END_DECLS

#endif /* !__GECKO_EMBED_TYPES_H__ */
