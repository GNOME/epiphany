/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2017 Gabriel Ivascu <gabrielivascu@gnome.org>
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
#include "ephy-tabs-catalog.h"

G_DEFINE_INTERFACE (EphyTabsCatalog, ephy_tabs_catalog, G_TYPE_OBJECT);

static void
ephy_tabs_catalog_default_init (EphyTabsCatalogInterface *iface)
{
  iface->get_tabs_info = ephy_tabs_catalog_get_tabs_info;
}

/**
 * ephy_tabs_catalog_get_tabs_info:
 * @catalog: an #EphyTabsCatalog
 *
 * Returns the title, URL and favicon URI of every tab of @catalog.
 *
 * Return value: (transfer full): a #GList of #EphyTabInfo
 **/
GList *
ephy_tabs_catalog_get_tabs_info (EphyTabsCatalog *catalog)
{
  EphyTabsCatalogInterface *iface;

  g_assert (EPHY_IS_TABS_CATALOG (catalog));

  iface = EPHY_TABS_CATALOG_GET_IFACE (catalog);
  return iface->get_tabs_info (catalog);
}

EphyTabInfo *
ephy_tab_info_new (const char *title,
                   const char *url,
                   const char *favicon)
{
  EphyTabInfo *info;

  info = g_new (EphyTabInfo, 1);
  info->title = g_strdup (title);
  info->url = g_strdup (url);
  info->favicon = g_strdup (favicon);

  return info;
}

void
ephy_tab_info_free (EphyTabInfo *info)
{
  g_assert (info);

  g_free (info->title);
  g_free (info->url);
  g_free (info->favicon);
  g_free (info);
}
