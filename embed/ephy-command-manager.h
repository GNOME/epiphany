/*
 *  Copyright (C) 2000, 2001, 2002 Marco Pesenti Gritti
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
 */

#ifndef EPHY_COMMAND_MANAGER_H
#define EPHY_COMMAND_MANAGER_H

#include "ephy-embed-types.h"

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#define EPHY_TYPE_COMMAND_MANAGER		(ephy_command_manager_get_type ())
#define EPHY_COMMAND_MANAGER(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_COMMAND_MANAGER, EphyCommandManager))
#define EPHY_COMMAND_MANAGER_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_COMMAND_MANAGER, EphyCommandManagerClass))
#define EPHY_IS_COMMAND_MANAGER(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_COMMAND_MANAGER))
#define EPHY_IS_COMMAND_MANAGER_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_COMMAND_MANAGER))
#define EPHY_COMMAND_MANAGER_GET_CLASS(inst)	(G_TYPE_INSTANCE_GET_INTERFACE ((inst), EPHY_TYPE_COMMAND_MANAGER, EphyCommandManagerClass))

typedef struct EphyCommandManagerClass EphyCommandManagerClass;
typedef struct _EphyCommandManager EphyCommandManager;

struct EphyCommandManagerClass
{
        GTypeInterface base_iface;

	gresult (* do_command)      (EphyCommandManager *manager,
				     const char *command);
	gresult (* can_do_command)  (EphyCommandManager *manager,
				     const char *command);
	gresult (* observe_command) (EphyCommandManager *manager,
				     const char *command);

	/* Signals */

	void    (* command_changed) (EphyCommandManager *manager,
				     char *command);
};

GType	ephy_command_manager_get_type		(void);

gresult	ephy_command_manager_do_command		(EphyCommandManager *manager,
						 const char *command);

gresult	ephy_command_manager_can_do_command	(EphyCommandManager *manager,
						 const char *command);

gresult ephy_command_manager_observe_command	(EphyCommandManager *manager,
						 const char *command);

G_END_DECLS

#endif
