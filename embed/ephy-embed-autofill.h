/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2018 Abdullah Alansari
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

#ifndef EPHY_EMBED_AUTOFILL_H
#define EPHY_EMBED_AUTOFILL_H

#include "ephy-embed-shell.h"
#include "ephy-web-view.h"

#include <glib.h>

G_BEGIN_DECLS

void
ephy_embed_autofill_signal_received_cb (EphyEmbedShell *shell,
                                        unsigned long page_id,
                                        const char *css_selector,
                                        bool is_fillable_element,
                                        bool has_personal_fields,
                                        bool has_card_fields,
                                        unsigned long element_x,
                                        unsigned long element_y,
                                        unsigned long element_width,
                                        unsigned long element_height,
                                        EphyWebView *view);

G_END_DECLS

#endif
