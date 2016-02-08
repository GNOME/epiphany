/*
 *  Copyright © 2003 Peter Harvey
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

#ifndef EPHY_OPEN_TABS_ACTION_H
#define EPHY_OPEN_TABS_ACTION_H

#include <gtk/gtk.h>

#include "ephy-node.h"

GtkActionGroup	*ephy_open_tabs_group_new	(EphyNode *node);

#endif
