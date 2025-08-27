/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2019 Jan-Michael Brummer <jan.brummer@tabos.org>
 *
 *  This file is part of Epiphany.
 *
 *  Epiphany is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Epiphany is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Epiphany.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "ephy-desktop-utils.h"
#include "ephy-embed-utils.h"

#include <gtk/gtk.h>

gboolean
is_desktop_pantheon (void)
{
#if USE_GRANITE
  const gchar *xdg_current_desktop = g_getenv ("XDG_CURRENT_DESKTOP");

  if (!xdg_current_desktop)
    return FALSE;

  return !!strstr (xdg_current_desktop, "Pantheon");
#else
  return FALSE;
#endif
}

gboolean
is_desktop_gnome (void)
{
  const gchar *xdg_current_desktop = g_getenv ("XDG_CURRENT_DESKTOP");

  if (!xdg_current_desktop)
    return FALSE;

  return !!strstr (xdg_current_desktop, "GNOME");
}

const char *
ephy_get_fallback_favicon_name (const char      *uri,
                                EphyFaviconType  type)
{
  if (uri) {
    if (g_str_has_prefix (uri, "ephy-about:overview") || g_str_has_prefix (uri, "about:overview"))
      return type == EPHY_FAVICON_TYPE_SHOW_MISSING_PLACEHOLDER ? "view-grid-symbolic" : NULL;
    else if (g_str_has_prefix (uri, "ephy-about:newtab") || g_str_has_prefix (uri, "about:newtab"))
      return NULL;
    else if (g_str_has_prefix (uri, "ephy-about:") || g_str_has_prefix (uri, "about:"))
      return "ephy-webpage-symbolic";
  }

  return NULL;
}
