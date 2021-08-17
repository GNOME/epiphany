/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2019 Abdullah Alansari
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

#include "ephy-embed-shell.h"
#include "ephy-web-view.h"

#include <glib.h>

G_BEGIN_DECLS

void
ephy_embed_autofill_signal_received_cb (EphyEmbedShell *shell,
                                        unsigned long   page_id,
                                        const char     *css_selector,
                                        gboolean        is_fillable_element,
                                        gboolean        has_personal_fields,
                                        gboolean        has_card_fields,
                                        unsigned long   element_x,
                                        unsigned long   element_y,
                                        unsigned long   element_width,
                                        unsigned long   element_height,
                                        EphyWebView    *view);

G_END_DECLS

