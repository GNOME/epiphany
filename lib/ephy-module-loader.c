/*
 *  Copyright (C) 2003 Marco Pesenti Gritti
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

#include "ephy-module-loader.h"
#include "ephy-file-helpers.h"
#include "ephy-debug.h"

#include <gmodule.h>

typedef struct _EphyModuleLoaderClass EphyModuleLoaderClass;

struct _EphyModuleLoaderClass
{
	GTypeModuleClass parent_class;
};

struct _EphyModuleLoader
{
	GTypeModule parent_instance;

	GModule *library;

	char *path;
	GType type;
};

typedef GType (*register_module_fn) (GTypeModule *);

static void ephy_module_loader_init		(EphyModuleLoader *action);
static void ephy_module_loader_class_init	(EphyModuleLoaderClass *class);
static void ephy_module_loader_finalize		(GObject *object);

static GObjectClass *parent_class = NULL;

GType
ephy_module_loader_get_type (void)
{
	static GType type = 0;

	if (!type)
	{
		static const GTypeInfo type_info =
		{
			sizeof (EphyModuleLoaderClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) ephy_module_loader_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,
			sizeof (EphyModuleLoader),
			0, /* n_preallocs */
			(GInstanceInitFunc) ephy_module_loader_init,
		};

		type = g_type_register_static (G_TYPE_TYPE_MODULE,
					       "EphyModuleLoader",
					       &type_info, 0);
	}

	return type;
}

EphyModuleLoader *
ephy_module_loader_new (const char *path)
{
	EphyModuleLoader *result;

	if (path == NULL || path[0] == '\0')
	{
		return NULL;
	}

	result = g_object_new (EPHY_TYPE_MODULE_LOADER, NULL);

	g_type_module_set_name (G_TYPE_MODULE (result), path);
	result->path = g_strdup (path);

	return result;
}

static gboolean
ephy_module_loader_load (GTypeModule *module)
{
	EphyModuleLoader *loader = EPHY_MODULE_LOADER (module);
	register_module_fn register_module;

	LOG ("ephy_module_loader_load %s", loader->path)

	loader->library = g_module_open (loader->path, 0);

	if (!loader->library)
	{
		g_warning (g_module_error());

		return FALSE;
	}

	/* extract symbols from the lib */
	if (!g_module_symbol (loader->library, "register_module",
			      (void *) &register_module))
	{
		g_warning (g_module_error());
		g_module_close (loader->library);

		return FALSE;
	}

	g_assert (register_module != NULL);

	loader->type = register_module (module);

	if (loader->type == 0)
	{
		return FALSE;
	}

	return TRUE;
}

static void
ephy_module_loader_unload (GTypeModule *module)
{
	EphyModuleLoader *loader = EPHY_MODULE_LOADER (module);

	g_module_close (loader->library);

	loader->library = NULL;
	loader->type = 0;
}

const char *
ephy_module_loader_get_path (EphyModuleLoader *loader)
{
	g_return_val_if_fail (EPHY_IS_MODULE_LOADER (loader), NULL);

	return loader->path;
}

static void
ephy_module_loader_class_init (EphyModuleLoaderClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GTypeModuleClass *loader_class = G_TYPE_MODULE_CLASS (class);

	parent_class = (GObjectClass *) g_type_class_peek_parent (class);

	object_class->finalize = ephy_module_loader_finalize;

	loader_class->load = ephy_module_loader_load;
	loader_class->unload = ephy_module_loader_unload;
}

static void
ephy_module_loader_init (EphyModuleLoader *loader)
{
	LOG ("EphyModuleLoader initialising")

	loader->library = NULL;
	loader->path = NULL;
	loader->type = 0;
}

static void
ephy_module_loader_finalize (GObject *object)
{
	EphyModuleLoader *loader = EPHY_MODULE_LOADER (object);

	LOG ("EphyModuleLoader finalising")

	g_free (loader->path);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GObject *
ephy_module_loader_factory (EphyModuleLoader *loader)
{
	if (loader->type == 0)
	{
		return NULL;
	}

	return g_object_new (loader->type, NULL);
}
