/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*
 *  Copyright © 2007 Xan Lopez
 *  Copyright © 2009 Igalia S.L.
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef EPHY_EMBED_H
#define EPHY_EMBED_H

#include "ephy-find-toolbar.h"
#include "ephy-web-view.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_EMBED (ephy_embed_get_type ())

G_DECLARE_FINAL_TYPE (EphyEmbed, ephy_embed, EPHY, EMBED, GtkBox)

EphyWebView*     ephy_embed_get_web_view             (EphyEmbed  *embed);
EphyFindToolbar* ephy_embed_get_find_toolbar         (EphyEmbed  *embed);
void             ephy_embed_add_top_widget           (EphyEmbed  *embed,
                                                      GtkWidget  *widget,
                                                      gboolean    destroy_on_transition);
void             ephy_embed_remove_top_widget        (EphyEmbed  *embed,
                                                      GtkWidget  *widget);
void             ephy_embed_entering_fullscreen      (EphyEmbed *embed);
void             ephy_embed_leaving_fullscreen       (EphyEmbed *embed);
void             ephy_embed_set_delayed_load_request (EphyEmbed *embed,
                                                      WebKitURIRequest          *request,
                                                      WebKitWebViewSessionState *state);
gboolean         ephy_embed_has_load_pending         (EphyEmbed *embed);
gboolean         ephy_embed_inspector_is_loaded      (EphyEmbed *embed);
const char      *ephy_embed_get_title                (EphyEmbed *embed);

G_END_DECLS

#endif
