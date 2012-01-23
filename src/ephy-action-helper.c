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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#include "ephy-action-helper.h"

#define SENSITIVITY_KEY	"EphyAction::Sensitivity"

/**
 * ephy_action_change_sensitivity_flags:
 * @action: a #GtkAction object
 * @flags: arbitrary combination of bit flags, defined by the user
 * @set: %TRUE if @flags should be added to @action
 *
 * This helper function provides an extra layer on top of #GtkAction to
 * manage its sensitivity. It uses bit @flags defined by the user, like
 * in ephy-window.c, SENS_FLAG_*.
 *
 * Effectively, the @action won't be sensitive until it has no flags
 * set. This means you can stack @flags for different events or
 * conditions at the same time.
 */
void 
ephy_action_change_sensitivity_flags (GtkAction *action,
				      guint flags,
				      gboolean set)
{
	static GQuark sensitivity_quark = 0;
	GObject *object = (GObject *) action;
	guint value;

	if (G_UNLIKELY (sensitivity_quark == 0))
	{
		sensitivity_quark = g_quark_from_static_string (SENSITIVITY_KEY);
	}

	value = GPOINTER_TO_UINT (g_object_get_qdata (object, sensitivity_quark));

	if (set)
	{
		value |= flags;
	}
	else
	{
		value &= ~flags;
	}

	g_object_set_qdata (object, sensitivity_quark, GUINT_TO_POINTER (value));

	gtk_action_set_sensitive (GTK_ACTION (action), value == 0);
}
