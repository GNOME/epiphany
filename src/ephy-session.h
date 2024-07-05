/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2002 Jorn Baayen
 *  Copyright © 2003 Marco Pesenti Gritti
 *  Copyright © 2003 Christian Persch
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

#include "ephy-window.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_SESSION (ephy_session_get_type ())

G_DECLARE_FINAL_TYPE (EphySession, ephy_session, EPHY, SESSION, GObject)

void             ephy_session_save                    (EphySession *session);
void             ephy_session_load                    (EphySession *session,
                                                       const char *filename,
                                                       GCancellable *cancellable,
                                                       GAsyncReadyCallback callback,
                                                       gpointer user_data);
gboolean         ephy_session_load_finish             (EphySession *session,
                                                       GAsyncResult *result,
                                                       GError **error);
void             ephy_session_load_from_stream        (EphySession *session,
                                                       GInputStream *stream,
                                                       GCancellable *cancellable,
                                                       GAsyncReadyCallback callback,
                                                       gpointer user_data);
gboolean         ephy_session_load_from_stream_finish (EphySession *session,
                                                       GAsyncResult *result,
                                                       GError **error);
void             ephy_session_resume                  (EphySession *session,
                                                       GCancellable *cancellable,
                                                       GAsyncReadyCallback callback,
                                                       gpointer user_data);
gboolean         ephy_session_resume_finish           (EphySession *session,
                                                       GAsyncResult *result,
                                                       GError **error);


void             ephy_session_close                   (EphySession *session);

void             ephy_session_undo_close_tab          (EphySession *session);

gboolean         ephy_session_get_can_undo_tab_closed (EphySession *session);

void             ephy_session_clear                   (EphySession *session);

gboolean         ephy_session_is_closing              (EphySession *session);

G_END_DECLS
