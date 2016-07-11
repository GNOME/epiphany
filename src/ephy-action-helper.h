/*
 *  Copyright © 2005 Christian Persch
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

#ifndef EPHY_ACTION_HELPER_H
#define EPHY_ACTION_HELPER_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

enum
{
	SENS_FLAG = 1 << 0
};

void ephy_action_change_sensitivity_flags (GSimpleAction *action,
                                           guint    	  flags,
                                           gboolean       set);

G_END_DECLS

#endif /* !EPHY_ACTION_HELPER_H */
