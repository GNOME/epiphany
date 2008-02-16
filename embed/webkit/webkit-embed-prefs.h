/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*  Copyright © 2008 Xan Lopez <xan@gnome.org>
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

#ifndef __WEBKIT_EMBED_PREFS_H__
#define __WEBKIT_EMBED_PREFS_H__

#include "webkit-embed.h"

G_BEGIN_DECLS

void webkit_embed_prefs_init         (void);
void webkit_embed_prefs_shutdown     (void);
void webkit_embed_prefs_add_embed    (WebKitEmbed *embed);

G_END_DECLS

#endif /* __WEBKIT_EMBED_PREFS_H__ */
