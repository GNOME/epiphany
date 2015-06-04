/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2005 Christian Persch
 *  Copyright © 2004 Crispin Flowerday
 *  Copyright © 2004 Adam Hooper
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef EPHY_EMBED_UTILS_H
#define EPHY_EMBED_UTILS_H

#include "ephy-web-view.h"
#include <webkit2/webkit2.h>

G_BEGIN_DECLS

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
gboolean ephy_embed_utils_urls_have_same_origin                 (const char *a_url,
                                                                 const char *b_url);
char    *ephy_embed_utils_get_js_result_as_string               (WebKitJavascriptResult *js_result);
double   ephy_embed_utils_get_js_result_as_number               (WebKitJavascriptResult *js_result);
void     ephy_embed_utils_shutdown                              (void);

G_END_DECLS

#endif
