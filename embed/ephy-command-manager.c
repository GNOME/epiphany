/*
 *  Copyright (C) 2000-2003 Marco Pesenti Gritti
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ephy-command-manager.h"

enum
{
	COMMAND_CHANGED,
	LAST_SIGNAL
};

static void
ephy_command_manager_base_init (gpointer base_class);

static guint ephy_command_manager_signals[LAST_SIGNAL] = { 0 };

GType
ephy_command_manager_get_type (void)
{
        static GType ephy_command_manager_type = 0;

        if (ephy_command_manager_type == 0)
        {
                static const GTypeInfo our_info =
                {
                        sizeof (EphyCommandManagerClass),
                        ephy_command_manager_base_init,
                        NULL,
                };

                ephy_command_manager_type = g_type_register_static (G_TYPE_INTERFACE,
							  "EphyCommandManager",
							  &our_info,
							  (GTypeFlags)0);
        }

        return ephy_command_manager_type;
}

static void
ephy_command_manager_base_init (gpointer g_class)
{
	static gboolean initialized = FALSE;

	if (!initialized)
	{
	ephy_command_manager_signals[COMMAND_CHANGED] =
                g_signal_new ("command_changed",
                              EPHY_TYPE_COMMAND_MANAGER,
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (EphyCommandManagerClass, command_changed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE,
                              1,
                              G_TYPE_STRING);
	initialized = TRUE;
	}
}

void
ephy_command_manager_do_command (EphyCommandManager *manager,
				 const char *command)
{
	EphyCommandManagerClass *klass = EPHY_COMMAND_MANAGER_GET_CLASS (manager);
        klass->do_command (manager, command);
}

gboolean
ephy_command_manager_can_do_command (EphyCommandManager *manager,
				        const char *command)
{
	EphyCommandManagerClass *klass = EPHY_COMMAND_MANAGER_GET_CLASS (manager);
        return klass->can_do_command (manager, command);
}
