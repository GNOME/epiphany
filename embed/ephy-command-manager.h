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
 */

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef EPHY_COMMAND_MANAGER_H
#define EPHY_COMMAND_MANAGER_H

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#define EPHY_TYPE_COMMAND_MANAGER		(ephy_command_manager_get_type ())
#define EPHY_COMMAND_MANAGER(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_COMMAND_MANAGER, EphyCommandManager))
#define EPHY_COMMAND_MANAGER_IFACE(k)		(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_COMMAND_MANAGER, EphyCommandManagerIface))
#define EPHY_IS_COMMAND_MANAGER(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_COMMAND_MANAGER))
#define EPHY_IS_COMMAND_MANAGER_IFACE(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_COMMAND_MANAGER))
#define EPHY_COMMAND_MANAGER_GET_IFACE(inst)	(G_TYPE_INSTANCE_GET_INTERFACE ((inst), EPHY_TYPE_COMMAND_MANAGER, EphyCommandManagerIface))

typedef struct _EphyCommandManager	EphyCommandManager;
typedef struct _EphyCommandManagerIface	EphyCommandManagerIface;

struct _EphyCommandManagerIface
{
	GTypeInterface base_iface;

	void		(* do_command)		(EphyCommandManager *manager,
						 const char *command);
	gboolean	(* can_do_command)	(EphyCommandManager *manager,
						 const char *command);

	/* Signals */

	void		(* command_changed)	(EphyCommandManager *manager,
						 char *command);
};

GType		ephy_command_manager_get_type		(void);

void		ephy_command_manager_do_command		(EphyCommandManager *manager,
							 const char *command);

gboolean	ephy_command_manager_can_do_command	(EphyCommandManager *manager,
							 const char *command);

G_END_DECLS

#endif
