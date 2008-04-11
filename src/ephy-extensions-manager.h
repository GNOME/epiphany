/*
 *  Copyright © 2003 Marco Pesenti Gritti
 *  Copyright © 2003 Christian Persch
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

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef EPHY_EXTENSIONS_MANAGER_H
#define EPHY_EXTENSIONS_MANAGER_H

#include "ephy-extension.h"
#include "ephy-node.h"

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define EPHY_TYPE_EXTENSIONS_MANAGER		(ephy_extensions_manager_get_type ())
#define EPHY_EXTENSIONS_MANAGER(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_EXTENSIONS_MANAGER, EphyExtensionsManager))
#define EPHY_EXTENSIONS_MANAGER_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_EXTENSIONS_MANAGER, EphyExtensionsManagerClass))
#define EPHY_IS_EXTENSIONS_MANAGER(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_EXTENSIONS_MANAGER))
#define EPHY_IS_EXTENSIONS_MANAGER_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_EXTENSIONS_MANAGER))
#define EPHY_EXTENSIONS_MANAGER_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_EXTENSIONS_MANAGER, EphyExtensionsManagerClass))

typedef struct _EphyExtensionsManager		EphyExtensionsManager;
typedef struct _EphyExtensionsManagerClass	EphyExtensionsManagerClass;
typedef struct _EphyExtensionsManagerPrivate	EphyExtensionsManagerPrivate;

typedef struct
{
	char *identifier;
	GKeyFile *keyfile;
	guint active  :1;
	guint enabled :1;
} EphyExtensionInfo;
	
struct _EphyExtensionsManagerClass
{
	GObjectClass parent_class;

	/* Signals */
	void	(* added)	(EphyExtensionsManager *manager,
				 EphyExtensionInfo *info);
	void	(* changed)	(EphyExtensionsManager *manager,
				 EphyExtensionInfo *info);
	void	(* removed)	(EphyExtensionsManager *manager,
				 EphyExtensionInfo *info);
};

struct _EphyExtensionsManager
{
	GObject parent_instance;

	/*< private >*/
	EphyExtensionsManagerPrivate *priv;
};

GType	  ephy_extensions_manager_get_type	 (void);

void	  ephy_extensions_manager_startup	 (EphyExtensionsManager *manager);

void	  ephy_extensions_manager_load		 (EphyExtensionsManager *manager,
						  const char *identifier);

void	  ephy_extensions_manager_unload	 (EphyExtensionsManager *manager,
						  const char *identifier);

void	  ephy_extensions_manager_register	 (EphyExtensionsManager *manager,
						  GObject *object);

GList	 *ephy_extensions_manager_get_extensions (EphyExtensionsManager *manager);

G_END_DECLS

#endif
