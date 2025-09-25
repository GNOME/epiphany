/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*
 *  Copyright © 2007 Xan Lopez
 *  Copyright © 2009 Igalia S.L.
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

#include "ephy-find-toolbar.h"
#include "ephy-web-view.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_EMBED (ephy_embed_get_type ())

G_DECLARE_FINAL_TYPE (EphyEmbed, ephy_embed, EPHY, EMBED, GtkBox)

typedef enum {
  EPHY_EMBED_TOP_WIDGET_POLICY_RETAIN_ON_TRANSITION,
  EPHY_EMBED_TOP_WIDGET_POLICY_DESTROY_ON_TRANSITION
} EphyEmbedTopWidgetPolicy;

EphyWebView*     ephy_embed_get_web_view                  (EphyEmbed  *embed);
EphyFindToolbar* ephy_embed_get_find_toolbar              (EphyEmbed  *embed);
void             ephy_embed_add_top_widget                (EphyEmbed                *embed,
                                                           GtkWidget                *widget,
                                                           EphyEmbedTopWidgetPolicy  policy);
void             ephy_embed_remove_top_widget             (EphyEmbed  *embed,
                                                           GtkWidget  *widget);
void             ephy_embed_entering_fullscreen           (EphyEmbed *embed);
void             ephy_embed_leaving_fullscreen            (EphyEmbed *embed);
void             ephy_embed_set_delayed_load_request      (EphyEmbed *embed,
                                                           WebKitURIRequest          *request,
                                                           WebKitWebViewSessionState *state);
gboolean         ephy_embed_has_load_pending              (EphyEmbed *embed);
gboolean         ephy_embed_inspector_is_loaded           (EphyEmbed *embed);
const char      *ephy_embed_get_title                     (EphyEmbed *embed);
const char      *ephy_embed_get_typed_input               (EphyEmbed *embed);
void             ephy_embed_set_typed_input               (EphyEmbed  *embed,
                                                           const char *input);
gboolean         ephy_embed_get_do_animate_reader_mode    (EphyEmbed *embed);
void             ephy_embed_set_do_animate_reader_mode    (EphyEmbed *embed,
                                                           gboolean   do_animate);
void             ephy_embed_attach_notification_container (EphyEmbed *embed);
void             ephy_embed_detach_notification_container (EphyEmbed *embed);
void             ephy_embed_download_started              (EphyEmbed    *embed,
                                                           EphyDownload *ephy_download);
WebKitWebViewSessionState
                *ephy_embed_get_session_state             (EphyEmbed *embed);

G_END_DECLS
