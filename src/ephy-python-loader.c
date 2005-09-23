/*
 *  Copyright (C) 2003 Marco Pesenti Gritti
 *  Copyright (C) 2003, 2004, 2005 Christian Persch
 *  Copyright (C) 2004, 2005 Jean-François Rameau
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

#include "config.h"

#include "ephy-python-loader.h"
#include "ephy-loader.h"
#include "ephy-python.h"
#include "ephy-python-extension.h"
#include "ephy-debug.h"

#define EPHY_PYTHON_LOADER_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_PYTHON_LOADER, EphyPythonLoaderPrivate))

struct _EphyPythonLoaderPrivate
{
	gpointer dummy;
};

static GObjectClass *parent_class = NULL;

static GObject *
impl_get_object (EphyLoader *eloader,
		 GData **attributes)
{
	char *filename;
	GObject *object;

	filename = g_datalist_get_data (attributes, "Module");
	if (filename == NULL)
	{
		filename = g_datalist_get_data (attributes, "module");
	}
	if (filename == NULL)
	{
		g_warning ("NULL module name!\n");
		return NULL;
	}

	object = g_object_new (EPHY_TYPE_PYTHON_EXTENSION,
			       "filename", filename,
			       NULL);

	/* we own one ref */
	return g_object_ref (object);
}

static void
impl_release_object (EphyLoader *eloader,
		     GObject *object)
{
	g_return_if_fail (object != NULL);

	g_object_unref (object);
}

static void
ephy_python_loader_iface_init (EphyLoaderIface *iface)
{
	iface->type = "python";
	iface->get_object = impl_get_object;
	iface->release_object = impl_release_object;
}

static void
ephy_python_loader_init (EphyPythonLoader *loader)
{
	loader->priv = EPHY_PYTHON_LOADER_GET_PRIVATE (loader);

	LOG ("EphyPythonLoader initialising");

	/* Initialize Python engine */
	ephy_python_init ();
}

static void
ephy_python_loader_finalize (GObject *object)
{
	LOG ("EphyPythonLoader finalising");

	parent_class->finalize (object);

	ephy_python_shutdown ();
}

static void
ephy_python_loader_class_init (EphyPythonLoaderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = ephy_python_loader_finalize;

	g_type_class_add_private (object_class, sizeof (EphyPythonLoaderPrivate));
}

GType
ephy_python_loader_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyPythonLoaderClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) ephy_python_loader_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (EphyPythonLoader),
			0, /* n_preallocs */
			(GInstanceInitFunc) ephy_python_loader_init
		};
		static const GInterfaceInfo loader_info =
		{
			(GInterfaceInitFunc) ephy_python_loader_iface_init,
			NULL,
			NULL
		};

		type = g_type_register_static (G_TYPE_OBJECT, "EphyPythonLoader",
					       &our_info, 0);

		g_type_add_interface_static (type, EPHY_TYPE_LOADER, &loader_info);
	}

	return type;
}
