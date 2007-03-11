/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
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
 *  $Id$
 */

#include "config.h"

#include "ephy-command-manager.h"

static void
ephy_command_manager_base_init (gpointer g_class);

GType
ephy_command_manager_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		const GTypeInfo our_info =
		{
			sizeof (EphyCommandManagerIface),
			ephy_command_manager_base_init,
			NULL,
		};

		type = g_type_register_static (G_TYPE_INTERFACE,
					       "EphyCommandManager",
					       &our_info,
					       (GTypeFlags)0);
	}

	return type;
}

static void
ephy_command_manager_base_init (gpointer g_class)
{
	static gboolean initialized = FALSE;

	if (!initialized)
	{
/**
 * EphyCommandManager::command-changed:
 * @manager:
 * @command: The command whose avalability has changed
 *
 * The ::command-changed signal is emitted when @command has changed from being
 * available to unavailable, or vice-versa. The new availability can be tested
 * with ephy_command_manager_can_do_command().
 **/
		g_signal_new ("command_changed",
			      EPHY_TYPE_COMMAND_MANAGER,
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EphyCommandManagerIface, command_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_STRING);

		initialized = TRUE;
	}
}

/**
 * ephy_command_manager_do_command:
 * @manager: an #EphyCommandManager
 * @command: the command
 *
 * Performs @command.
 **/
void
ephy_command_manager_do_command (EphyCommandManager *manager,
				 const char *command)
{
	EphyCommandManagerIface *iface = EPHY_COMMAND_MANAGER_GET_IFACE (manager);
	iface->do_command (manager, command);
}

/**
 * ephy_command_manager_can_do_command:
 * @manager: an #EphyCommandManager
 * @command: the command
 *
 * Returns %TRUE if @command can be performed.
 *
 * Return value: %TRUE if @command can be performed.
 **/
gboolean
ephy_command_manager_can_do_command (EphyCommandManager *manager,
					const char *command)
{
	EphyCommandManagerIface *iface = EPHY_COMMAND_MANAGER_GET_IFACE (manager);
	return iface->can_do_command (manager, command);
}
