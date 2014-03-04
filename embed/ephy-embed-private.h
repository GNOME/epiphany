/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2012 Igalia S.L.
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

#include "ephy-embed-shell.h"
#include "ephy-history-types.h"
#include "ephy-web-view.h"

#ifndef EPHY_EMBED_PRIVATE_H
#define EPHY_EMBED_PRIVATE_H

G_BEGIN_DECLS

/* EphyWebView */

#define EPHY_WEB_VIEW_NON_SEARCH_REGEX  "(" \
                                        "^[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9](:[0-9]+)?.*$|" \
                                        "^::[0-9a-f:]*$|" \
                                        "^[0-9a-f:]+:[0-9a-f:]*$|" \
                                        "^https?://[^/\\.[:space:]]+.*$|" \
                                        "^about:.*$|" \
                                        "^data:.*$|" \
                                        "^file:.*$" \
                                        ")"

#define EPHY_WEB_VIEW_DOMAIN_REGEX "^localhost(\\.[^[:space:]]+)?(:\\d+)?(:[0-9]+)?(/.*)?$|" \
                                   "^[^\\.[:space:]]+\\.[^\\.[:space:]]+.*$|"

void                       ephy_web_view_set_visit_type           (EphyWebView *view, 
                                                                   EphyHistoryPageVisitType visit_type);
EphyHistoryPageVisitType   ephy_web_view_get_visit_type           (EphyWebView *view);
void                       ephy_web_view_popups_manager_reset     (EphyWebView               *view);
void                       ephy_web_view_save                     (EphyWebView               *view,
                                                                   const char                *uri);
void                       ephy_web_view_load_homepage            (EphyWebView               *view);

char *                     ephy_web_view_create_web_application   (EphyWebView               *view,
                                                                   const char                *title,
                                                                   GdkPixbuf                 *icon);
G_END_DECLS

#endif

