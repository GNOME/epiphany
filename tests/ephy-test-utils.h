/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2013 Igalia S.L.
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

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef EPHY_TEST_UTILS_H
#define EPHY_TEST_UTILS_H

#include "ephy-embed.h"
#include "ephy-web-view.h"

#include <glib.h>

G_BEGIN_DECLS

guint ephy_test_utils_get_web_view_ready_counter (void);

void ephy_test_utils_check_ephy_web_view_address (EphyWebView *view,
                                                  const gchar *address);

void ephy_test_utils_check_ephy_embed_address (EphyEmbed *embed,
                                               const gchar *address);

GMainLoop* ephy_test_utils_setup_ensure_web_views_are_loaded (void);

void ephy_test_utils_ensure_web_views_are_loaded (GMainLoop *loop);

GMainLoop* ephy_test_utils_setup_wait_until_load_is_committed (EphyWebView *view);

void ephy_test_utils_wait_until_load_is_committed (GMainLoop *loop);

G_END_DECLS

#endif
