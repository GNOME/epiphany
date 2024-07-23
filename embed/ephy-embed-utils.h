/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2005 Christian Persch
 *  Copyright © 2004 Crispin Flowerday
 *  Copyright © 2004 Adam Hooper
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

#include "ephy-web-view.h"
#include <webkit/webkit.h>

G_BEGIN_DECLS

#define BLANK_PAGE_TITLE N_("Blank page") /* Title for the blank page */
#define NEW_TAB_PAGE_TITLE N_("New Tab")  /* Title for the new tab page */

#define EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED(embed) (WEBKIT_WEB_VIEW (ephy_embed_get_web_view (embed)))
#define EPHY_GET_EMBED_FROM_EPHY_WEB_VIEW(view) (EPHY_EMBED (gtk_widget_get_parent (gtk_widget_get_parent (gtk_widget_get_parent ((GTK_WIDGET (view)))))))

#define EPHY_WEBKIT_BACK_FORWARD_LIMIT 100

char*    ephy_embed_utils_link_message_parse                    (const char *message);
gboolean ephy_embed_utils_address_has_web_scheme                (const char *address);
gboolean ephy_embed_utils_address_is_existing_absolute_filename (const char *address);
gboolean ephy_embed_utils_address_is_valid                      (const char *address);
char*    ephy_embed_utils_normalize_address                     (const char *address);
char *   ephy_embed_utils_autosearch_address                    (const char *search_key);
char *   ephy_embed_utils_normalize_or_autosearch_address       (const char *address);
gboolean ephy_embed_utils_url_is_empty                          (const char *location);
gboolean ephy_embed_utils_is_no_show_address                    (const char *address);
char    *ephy_embed_utils_get_title_from_address                (const char *address);
void     ephy_embed_utils_shutdown                              (void);

G_END_DECLS
