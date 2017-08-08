/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2017 Gabriel Ivascu <gabrielivascu@gnome.org>
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

#include <glib-object.h>

G_BEGIN_DECLS

#define EPHY_TYPE_HISTORY_RECORD (ephy_history_record_get_type ())

G_DECLARE_FINAL_TYPE (EphyHistoryRecord, ephy_history_record, EPHY, HISTORY_RECORD, GObject)

EphyHistoryRecord *ephy_history_record_new                 (const char *id,
                                                            const char *title,
                                                            const char *history_uri,
                                                            gint64      last_visit_time);
void               ephy_history_record_set_id              (EphyHistoryRecord *self,
                                                            const char        *id);
const char        *ephy_history_record_get_id              (EphyHistoryRecord *self);
const char        *ephy_history_record_get_title           (EphyHistoryRecord *self);
const char        *ephy_history_record_get_uri             (EphyHistoryRecord *self);
gint64             ephy_history_record_get_last_visit_time (EphyHistoryRecord *self);
gboolean           ephy_history_record_add_visit_time      (EphyHistoryRecord *self,
                                                            gint64             visit_time);

G_END_DECLS
