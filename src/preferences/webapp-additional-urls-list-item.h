/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2018 Igalia S.L.
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

#include <adwaita.h>

G_BEGIN_DECLS

#define EPHY_TYPE_WEBAPP_ADDITIONAL_URLS_LIST_ITEM ephy_webapp_additional_urls_list_item_get_type()

G_DECLARE_FINAL_TYPE (EphyWebappAdditionalURLsListItem, ephy_webapp_additional_urls_list_item, EPHY, WEBAPP_ADDITIONAL_URLS_LIST_ITEM, GObject);

EphyWebappAdditionalURLsListItem *ephy_webapp_additional_urls_list_item_new            (const gchar                      *url);
const gchar                      *ephy_webapp_additional_urls_list_item_get_url        (EphyWebappAdditionalURLsListItem *self);
void                              ephy_webapp_additional_urls_list_item_set_url        (EphyWebappAdditionalURLsListItem *self,
                                                                                        const gchar                      *url);
gboolean                          ephy_webapp_additional_urls_list_item_add_to_builder (EphyWebappAdditionalURLsListItem *item,
                                                                                        GVariantBuilder                  *builder);

G_END_DECLS
