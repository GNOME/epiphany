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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ephy-extensions-manager.h"

#include "ephy-module-loader.h"
#include "ephy-debug.h"

#include <gmodule.h>
#include <dirent.h>

#define EPHY_EXTENSIONS_MANAGER_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_EXTENSIONS_MANAGER, EphyExtensionsManagerPrivate))

struct EphyExtensionsManagerPrivate
{
	GSList *loaders;
	GSList *extensions;
};

static GObjectClass *parent_class = NULL;

static void ephy_extensions_manager_class_init	(EphyExtensionsManagerClass *klass);
static void ephy_extensions_manager_iface_init	(EphyExtensionIface *iface);
static void ephy_extensions_manager_init	(EphyExtensionsManager *manager);

GType
ephy_extensions_manager_get_type (void)
{
	static GType type = 0;

	if (type == 0)
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyExtensionsManagerClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) ephy_extensions_manager_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (EphyExtensionsManager),
			0, /* n_preallocs */
			(GInstanceInitFunc) ephy_extensions_manager_init
		};

		static const GInterfaceInfo extension_info =
		{
			(GInterfaceInitFunc) ephy_extensions_manager_iface_init,
			NULL,
			NULL
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "EphyExtensionsManager",
					       &our_info, 0);

		g_type_add_interface_static (type,
					     EPHY_TYPE_EXTENSION,
					     &extension_info);
	}

	return type;
}

static EphyExtension *
ephy_extensions_manager_instantiate_extension (EphyExtensionsManager *manager,
					       EphyModuleLoader *loader)
{
	EphyExtension *extension;

	extension = EPHY_EXTENSION (ephy_module_loader_factory (loader));

	if (EPHY_IS_EXTENSION (extension))
	{
		manager->priv->extensions =
			g_slist_append (manager->priv->extensions, extension);

		return extension;
	}

	return NULL;
}

EphyExtension *
ephy_extensions_manager_load (EphyExtensionsManager *manager,
			      const char *filename)
{
	EphyExtension *extension = NULL;

	if (g_str_has_suffix (filename, G_MODULE_SUFFIX))
	{
		EphyModuleLoader *loader;

		loader = ephy_module_loader_new (filename);

		if (loader != NULL)
		{
			manager->priv->loaders =
				g_slist_prepend (manager->priv->loaders, loader);

			extension = ephy_extensions_manager_instantiate_extension
				(manager, loader);

			g_type_module_unuse (G_TYPE_MODULE (loader));
		}
	}

	return extension;
}

void
ephy_extensions_manager_load_dir (EphyExtensionsManager *manager,
				  const char *path)
{
	DIR *d;
	struct dirent *e;

	d = opendir (path);
	if (d == NULL)
	{
		return;
	}

	while ((e = readdir (d)) != NULL)
	{
		char *filename;

		filename = g_build_filename (path, e->d_name, NULL);

		ephy_extensions_manager_load (manager, filename);

		g_free (filename);
	}
	closedir (d);
}

EphyExtension *
ephy_extensions_manager_add (EphyExtensionsManager *manager,
			     GType type)
{
	EphyExtension *extension;

	LOG ("adding extensions of type %s", g_type_name (type))

	extension = EPHY_EXTENSION (g_object_new (type, NULL));
	if (!EPHY_IS_EXTENSION (extension))
	{
		g_object_unref (extension);

		return NULL;
	}

	manager->priv->extensions =
		g_slist_append (manager->priv->extensions, extension);

	return extension;
}

static void
ephy_extensions_manager_init (EphyExtensionsManager *manager)
{
	manager->priv = EPHY_EXTENSIONS_MANAGER_GET_PRIVATE (manager);

	LOG ("EphyExtensionsManager initialising")

	manager->priv->loaders = NULL;
	manager->priv->extensions = NULL;
}

static void
ephy_extensions_manager_finalize (GObject *object)
{
	EphyExtensionsManager *manager = EPHY_EXTENSIONS_MANAGER (object);

	LOG ("EphyExtensionsManager finalising")

	g_slist_foreach (manager->priv->extensions, (GFunc) g_object_unref, NULL);
	g_slist_free (manager->priv->extensions);

	g_slist_free (manager->priv->loaders);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
impl_attach_window (EphyExtension *extension,
		    EphyWindow *window)
{
	EphyExtensionsManager *manager = EPHY_EXTENSIONS_MANAGER (extension);

	LOG ("multiplexing attach_window")

	g_slist_foreach (manager->priv->extensions,
			 (GFunc) ephy_extension_attach_window, window);
}

static void
impl_detach_window (EphyExtension *extension,
		    EphyWindow *window)
{
	EphyExtensionsManager *manager = EPHY_EXTENSIONS_MANAGER (extension);

	LOG ("multiplexing detach_window")

	g_object_ref (window);

	g_slist_foreach (manager->priv->extensions,
			 (GFunc) ephy_extension_detach_window, window);

	g_object_unref (window);
}

static void
ephy_extensions_manager_iface_init (EphyExtensionIface *iface)
{
	iface->attach_window = impl_attach_window;
	iface->detach_window = impl_detach_window;
}

static void
ephy_extensions_manager_class_init (EphyExtensionsManagerClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	parent_class = (GObjectClass *) g_type_class_peek_parent (class);

	object_class->finalize = ephy_extensions_manager_finalize;

	g_type_class_add_private (object_class, sizeof (EphyExtensionsManagerPrivate));
}

EphyExtensionsManager *
ephy_extensions_manager_new (void)
{
	return EPHY_EXTENSIONS_MANAGER (g_object_new (EPHY_TYPE_EXTENSIONS_MANAGER, NULL));
}
