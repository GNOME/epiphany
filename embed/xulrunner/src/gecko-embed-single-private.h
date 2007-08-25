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

#ifndef gecko_embed_single_private_h
#define gecko_embed_single_private_h

#include "gecko-embed-single.h"

class GeckoSingle;
class nsIDirectoryServiceProvider;
struct nsModuleComponentInfo;

G_BEGIN_DECLS

extern GeckoSingle* gecko_embed_single_get_GeckoSingle (void);

extern void gecko_embed_single_set_directory_service_provider (nsIDirectoryServiceProvider *aProvider);

extern void gecko_embed_single_set_app_components (const nsModuleComponentInfo *aComps,
                                                   int aNumComps);

extern void gecko_embed_single_create_window (GeckoEmbed **aNewEmbed,
					      guint aChromeFlags);

G_END_DECLS

#endif /* gecko_embed_single_private_h */
