/*
 *  Copyright (C) 2003 Marco Pesenti Gritti
 *  Copyright (C) 2003 Christian Persch
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

#ifndef EPHY_EXTENSIONS_MANAGER_H
#define EPHY_EXTENSIONS_MANAGER_H

#include "ephy-extension.h"

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define EPHY_TYPE_EXTENSIONS_MANAGER		(ephy_extensions_manager_get_type ())
#define EPHY_EXTENSIONS_MANAGER(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_EXTENSIONS_MANAGER, EphyExtensionsManager))
#define EPHY_EXTENSIONS_MANAGER_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_EXTENSIONS_MANAGER, EphyExtensionsManagerClass))
#define EPHY_IS_EXTENSIONS_MANAGER(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_EXTENSIONS_MANAGER))
#define EPHY_IS_EXTENSIONS_MANAGER_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_EXTENSIONS_MANAGER))
#define EPHY_EXTENSIONS_MANAGER_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_EXTENSIONS_MANAGER, EphyExtensionsManagerClass))

typedef struct EphyExtensionsManager		EphyExtensionsManager;
typedef struct EphyExtensionsManagerClass	EphyExtensionsManagerClass;
typedef struct EphyExtensionsManagerPrivate	EphyExtensionsManagerPrivate;

struct EphyExtensionsManagerClass
{
	GObjectClass parent_class;
};

struct EphyExtensionsManager
{
	GObject parent_instance;

	EphyExtensionsManagerPrivate *priv;
};

GType			 ephy_extensions_manager_get_type	(void);

EphyExtensionsManager	*ephy_extensions_manager_new 		(void);

EphyExtension		*ephy_extensions_manager_load		(EphyExtensionsManager *manager,
								 const char *filename);

EphyExtension		*ephy_extensions_manager_add		(EphyExtensionsManager *manager,
								 GType type);

G_END_DECLS

#endif
