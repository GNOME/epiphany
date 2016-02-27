/*
 *  Copyright © 2003 Marco Pesenti Gritti <mpeseng@tin.it>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef EPHY_HISTORY_WINDOW_H
#define EPHY_HISTORY_WINDOW_H

#include <gtk/gtk.h>

#include "ephy-history-service.h"

G_BEGIN_DECLS

#define EPHY_TYPE_HISTORY_WINDOW (ephy_history_window_get_type ())

G_DECLARE_FINAL_TYPE (EphyHistoryWindow, ephy_history_window, EPHY, HISTORY_WINDOW, GtkDialog)

GtkWidget	    *ephy_history_window_new        (EphyHistoryService *history_service);

G_END_DECLS

#endif /* EPHY_HISTORY_WINDOW_H */
