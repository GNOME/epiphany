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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "ephy-embed.h"
#include "ephy-embed-event.h"
#include "ephy-embed-private.h"
#include "ephy-location-controller.h"
#include "ephy-session.h"
#include "ephy-shell.h"
#include "ephy-window.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* EphyWindow */

EphyLocationController  *ephy_window_get_location_controller  (EphyWindow               *window);

EphyEmbedEvent          *ephy_window_get_context_event        (EphyWindow               *window);

GtkWidget               *ephy_window_get_current_find_toolbar (EphyWindow               *window);

void                     ephy_window_set_location             (EphyWindow               *window,
                                                               const char               *address);


/* EphyShell */

void                     ephy_shell_set_startup_context       (EphyShell                *shell,
                                                               EphyShellStartupContext  *ctx);

EphyShellStartupContext *ephy_shell_startup_context_new       (EphyStartupFlags          startup_flags,
                                                               char                     *bookmarks_filename,
                                                               char                     *session_filename,
                                                               char                     *bookmark_url,
                                                               char                    **arguments,
                                                               guint32                   user_time);

void                     _ephy_shell_create_instance          (EphyEmbedShellMode        mode);

/* EphySession */

void                     ephy_session_clear                   (EphySession *session);

G_END_DECLS
