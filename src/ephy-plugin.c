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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ephy-plugin.h"
#include "ephy-file-helpers.h"

#include <gmodule.h>

typedef struct _EphyPluginClass EphyPluginClass;

struct _EphyPluginClass
{
	GTypeModuleClass parent_class;
};

struct _EphyPlugin
{
	GTypeModule parent_instance;

	GModule *library;

	void (*init) (GTypeModule *);
	void (*exit) (void);

	char *name;
};

static void ephy_plugin_init       (EphyPlugin *action);
static void ephy_plugin_class_init (EphyPluginClass *class);
static void ephy_plugin_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

GType
ephy_plugin_get_type (void)
{
	static GType type = 0;

	if (!type)
	{
		static const GTypeInfo type_info =
		{
			sizeof (EphyPluginClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) ephy_plugin_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,
			sizeof (EphyPlugin),
			0, /* n_preallocs */
			(GInstanceInitFunc) ephy_plugin_init,
		};

		type = g_type_register_static (G_TYPE_TYPE_MODULE,
					       "EphyPlugin",
					       &type_info, 0);
	}
	return type;
}

EphyPlugin *
ephy_plugin_new (const char *name)
{
	EphyPlugin *result;

	result = g_object_new (EPHY_TYPE_PLUGIN, NULL);

	g_type_module_set_name (G_TYPE_MODULE (result), name);
	result->name = g_strdup (name);

	if (!g_type_module_use (G_TYPE_MODULE (result)))
	{
		return NULL;
	}

	return result;
}

static gboolean
ephy_plugin_load (GTypeModule *module)
{
	EphyPlugin *plugin = EPHY_PLUGIN (module);
	char *module_path;

	module_path = g_strdup (plugin->name);

	if (!module_path)
	{
		g_warning ("Unable to locate theme engine in module_path: \"%s\",",
			   plugin->name);
		return FALSE;
	}

	plugin->library = g_module_open (module_path, 0);
	g_free (module_path);

	if (!plugin->library)
	{
		g_warning (g_module_error());
		return FALSE;
	}

	/* extract symbols from the lib */
	if (!g_module_symbol (plugin->library, "plugin_init",
	    (gpointer *)&plugin->init) ||
            !g_module_symbol (plugin->library, "plugin_exit",
	    (gpointer *)&plugin->exit))
	{
		g_warning (g_module_error());
		g_module_close (plugin->library);

		return FALSE;
	}

	plugin->init (module);

	return TRUE;
}

static void
ephy_plugin_unload (GTypeModule *module)
{
	EphyPlugin *plugin = EPHY_PLUGIN (module);

	plugin->exit();

	g_module_close (plugin->library);

	plugin->library = NULL;
	plugin->init = NULL;
	plugin->exit = NULL;
}

static void
ephy_plugin_class_init (EphyPluginClass *class)
{
	GTypeModuleClass *plugin_class;
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->finalize = ephy_plugin_finalize;

	parent_class = g_type_class_peek_parent (class);
	plugin_class = G_TYPE_MODULE_CLASS (class);

	plugin_class->load = ephy_plugin_load;
	plugin_class->unload = ephy_plugin_unload;
}

static void
ephy_plugin_init (EphyPlugin *action)
{
}

static void
ephy_plugin_finalize (GObject *object)
{
	g_return_if_fail (EPHY_IS_PLUGIN (object));

	G_OBJECT_CLASS (parent_class)->finalize (object);
}
