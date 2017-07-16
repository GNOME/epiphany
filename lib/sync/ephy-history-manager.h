/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2017 Gabriel Ivascu <ivascu.gabriel59@gmail.com>
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

#include "ephy-history-record.h"
#include "ephy-history-service.h"

#include <glib-object.h>

G_BEGIN_DECLS

#define EPHY_TYPE_HISTORY_MANAGER (ephy_history_manager_get_type ())

G_DECLARE_FINAL_TYPE (EphyHistoryManager, ephy_history_manager, EPHY, HISTORY_MANAGER, GObject)

EphyHistoryManager *ephy_history_manager_new (EphyHistoryService *service);

G_END_DECLS
