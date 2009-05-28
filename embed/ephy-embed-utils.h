/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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
 *  $Id: 
 */

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef EPHY_EMBED_UTILS_H
#define EPHY_EMBED_UTILS_H

#include "ephy-embed.h"

#include <webkit/webkit.h>

G_BEGIN_DECLS

#define EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED(embed) (WEBKIT_WEB_VIEW (gtk_bin_get_child (GTK_BIN (gtk_bin_get_child (GTK_BIN (embed))))))
#define EPHY_WEBKIT_BACK_FORWARD_LIMIT 100

char 	   * ephy_embed_utils_link_message_parse  (char *message);
const char * ephy_embed_utils_get_title_composite (EphyEmbed *embed);
gboolean     ephy_embed_utils_address_has_web_scheme (const char *address);

G_END_DECLS

#endif
