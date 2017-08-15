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

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define EPHY_TYPE_TABS_CATALOG (ephy_tabs_catalog_get_type ())

G_DECLARE_INTERFACE (EphyTabsCatalog, ephy_tabs_catalog, EPHY, TABS_CATALOG, GObject)

struct _EphyTabsCatalogInterface {
  GTypeInterface parent_iface;

  GList * (*get_tabs_info) (EphyTabsCatalog *catalog);
};

GList *ephy_tabs_catalog_get_tabs_info (EphyTabsCatalog *catalog);

typedef struct {
  char *title;
  char *url;
  char *favicon;
} EphyTabInfo;

EphyTabInfo *ephy_tab_info_new  (const char *title,
                                 const char *url,
                                 const char *favicon);
void         ephy_tab_info_free (EphyTabInfo *info);

G_END_DECLS
