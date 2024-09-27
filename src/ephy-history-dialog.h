/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2003 Marco Pesenti Gritti <mpeseng@tin.it>
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

#include <adwaita.h>

#include "ephy-history-service.h"
#include "ephy-window.h"

G_BEGIN_DECLS

#define EPHY_TYPE_HISTORY_DIALOG (ephy_history_dialog_get_type ())

G_DECLARE_FINAL_TYPE (EphyHistoryDialog, ephy_history_dialog, EPHY, HISTORY_DIALOG, AdwDialog)

EphyWindow     *ephy_history_dialog_get_parent_window (EphyHistoryDialog  *self);
void            ephy_history_dialog_set_parent_window (EphyHistoryDialog  *self,
                                                       EphyWindow         *window);

GtkWidget      *ephy_history_dialog_new               (EphyHistoryService *history_service);

G_END_DECLS
