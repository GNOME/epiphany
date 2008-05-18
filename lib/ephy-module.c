/*
 *  Copyright © 2003 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
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

#include "ephy-module.h"
#include "ephy-file-helpers.h"
#include "ephy-debug.h"

#include <gmodule.h>

typedef struct _EphyModuleClass EphyModuleClass;

struct _EphyModuleClass
{
	GTypeModuleClass parent_class;
};

struct _EphyModule
{
	GTypeModule parent_instance;

	GModule *library;

	char *path;
	GType type;
	guint resident : 1;
};

typedef GType (*EphyModuleRegisterFunc) (GTypeModule *);

static void ephy_module_init		(EphyModule *action);
static void ephy_module_class_init	(EphyModuleClass *class);

static GObjectClass *parent_class = NULL;

GType
ephy_module_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		const GTypeInfo type_info =
		{
			sizeof (EphyModuleClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) ephy_module_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,
			sizeof (EphyModule),
			0, /* n_preallocs */
			(GInstanceInitFunc) ephy_module_init,
		};

		type = g_type_register_static (G_TYPE_TYPE_MODULE,
					       "EphyModule",
					       &type_info, 0);
	}

	return type;
}

static gboolean
ephy_module_load (GTypeModule *gmodule)
{
	EphyModule *module = EPHY_MODULE (gmodule);
	EphyModuleRegisterFunc register_func;
	gboolean is_absolute;
	GModuleFlags flags = G_MODULE_BIND_LOCAL | G_MODULE_BIND_LAZY;

	LOG ("Loading %s", module->path);

	/* In debug builds, we bind immediately so we can find missing
	 * symbols in extensions on load; otherwise we bind lazily
	 */
#ifdef GNOME_ENABLE_DEBUG
	flags &= ~G_MODULE_BIND_LAZY;
#endif

	is_absolute = g_path_is_absolute (module->path);

	if (module->library == NULL && ! is_absolute)
	{
		char *path = g_build_filename (EXTENSIONS_DIR, module->path, NULL);

		module->library = g_module_open (path, flags);

		g_free (path);
	}

	if (module->library == NULL && ! is_absolute)
	{
		char *path = g_build_filename (ephy_dot_dir(), "extensions", module->path, NULL);
		
		module->library = g_module_open (path, flags);
 
		g_free (path);
	}

	if (module->library == NULL)
	{
		module->library = g_module_open (module->path, flags);
	}

	if (module->library == NULL)
	{
		g_warning (g_module_error());

		return FALSE;
	}

	/* extract symbols from the lib */
	if (!g_module_symbol (module->library, "register_module",
			      (void *) &register_func))
	{
		g_warning (g_module_error());
		g_module_close (module->library);

		return FALSE;
	}

	/* symbol can still be NULL even though g_module_symbol returned TRUE */
	if (!register_func)
	{
		g_warning ("Symbol 'register_module' is NULL!");
		g_module_close (module->library);

		return FALSE;
	}

	module->type = register_func (gmodule);

	if (module->type == 0)
	{
		g_warning ("Failed to register the GType(s)!");
		g_module_close (module->library);

		return FALSE;
	}

	if (module->resident)
	{
		g_module_make_resident (module->library);
	}

	return TRUE;
}

static void
ephy_module_unload (GTypeModule *gmodule)
{
	EphyModule *module = EPHY_MODULE (gmodule);

	LOG ("Unloading %s", module->path);

	g_module_close (module->library);

	module->library = NULL;
	module->type = 0;
}

const char *
ephy_module_get_path (EphyModule *module)
{
	g_return_val_if_fail (EPHY_IS_MODULE (module), NULL);

	return module->path;
}

GObject *
ephy_module_new_object (EphyModule *module)
{
	LOG ("Creating object of type %s", g_type_name (module->type));

	if (module->type == 0)
	{
		return NULL;
	}

	return g_object_new (module->type, NULL);
}

static void
ephy_module_init (EphyModule *module)
{
	LOG ("EphyModule %p initialising", module);
}

static void
ephy_module_finalize (GObject *object)
{
	EphyModule *module = EPHY_MODULE (object);

	LOG ("EphyModule %p finalising", module);

	g_free (module->path);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
ephy_module_class_init (EphyModuleClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GTypeModuleClass *module_class = G_TYPE_MODULE_CLASS (class);

	parent_class = (GObjectClass *) g_type_class_peek_parent (class);

	object_class->finalize = ephy_module_finalize;

	module_class->load = ephy_module_load;
	module_class->unload = ephy_module_unload;
}

EphyModule *
ephy_module_new (const char *path,
		 gboolean resident)
{
	EphyModule *result;

	if (path == NULL || path[0] == '\0')
	{
		return NULL;
	}

	result = g_object_new (EPHY_TYPE_MODULE, NULL);

	g_type_module_set_name (G_TYPE_MODULE (result), path);
	result->path = g_strdup (path);
	result->resident = resident != FALSE;

	return result;
}
