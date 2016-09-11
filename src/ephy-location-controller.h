/*
 *  Copyright © 2003 Marco Pesenti Gritti
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

#include <glib-object.h>

G_BEGIN_DECLS

#define EPHY_TYPE_LOCATION_CONTROLLER (ephy_location_controller_get_type ())

G_DECLARE_FINAL_TYPE (EphyLocationController, ephy_location_controller, EPHY, LOCATION_CONTROLLER, GObject)

const char     *ephy_location_controller_get_address	(EphyLocationController *controller);

void		ephy_location_controller_set_address	(EphyLocationController *controller,
							 const char *address);

G_END_DECLS
