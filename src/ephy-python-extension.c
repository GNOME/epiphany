/*
 *  Copyright © 2003 Marco Pesenti Gritti
 *  Copyright © 2003 Christian Persch
 *  Copyright © 2004, 2005 Adam Hooper
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

#include "Python.h"

#include "config.h"

#include "ephy-python-extension.h"
#include "ephy-python.h"

#define NO_IMPORT_PYGOBJECT
#include <pygobject.h>

#include "ephy-extension.h"
#include "ephy-window.h"
#include "ephy-file-helpers.h"
#include "ephy-debug.h"

static void ephy_python_extension_iface_init (EphyExtensionIface *iface);

#define EPHY_PYTHON_EXTENSION_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_PYTHON_EXTENSION, EphyPythonExtensionPrivate))

struct _EphyPythonExtensionPrivate
{
	char *filename;
	PyObject *module;
};

enum
{
	PROP_0,
	PROP_FILENAME
};

static int
set_python_search_path (const char *filename)
{
	char *dirname;
	char *dot_dir;
	int ret = 2;
	PyObject *sys_path;
	PyObject *pValue;

	sys_path = PySys_GetObject ("path");

	/* Systems extensions dir */
	pValue = PyString_FromString (EXTENSIONS_DIR);
	PyList_Insert (sys_path, 0, pValue);
	Py_DECREF (pValue);

	/* Home dir */
	dot_dir = g_strconcat (ephy_dot_dir (), "/extensions", NULL);
	pValue = PyString_FromString (dot_dir);
	PyList_Insert (sys_path, 0, pValue);
	Py_DECREF (pValue);
	g_free (dot_dir);

	/* Absolute path specified in .xml file */
	dirname = g_path_get_dirname (filename);
	if (g_path_is_absolute (dirname))
	{
		pValue = PyString_FromString (dirname);
		PyList_Insert (sys_path, 0, pValue);
		Py_DECREF (pValue);
		ret++;
	}
	g_free (dirname);

	return ret;
}

static void
unset_python_search_path (int num_dirs)
{
	PyObject *sys_path = PySys_GetObject ("path");

	PySequence_DelSlice (sys_path, 0, num_dirs);
}

static void
ephy_python_extension_init (EphyPythonExtension *extension)
{
	LOG ("EphyPythonExtension initialising");

	extension->priv = EPHY_PYTHON_EXTENSION_GET_PRIVATE (extension);
}

static void
call_python_func (EphyExtension *extension,
		  const char *func_name,
		  EphyWindow *window,
		  EphyEmbed *embed) /* HACK: tab may be NULL */
{
	PyObject *pDict, *pFunc;
	PyObject *pArgs, *pValue, *pEmbed = NULL, *pWindow;
	EphyPythonExtension *py_ext;
	
	py_ext = EPHY_PYTHON_EXTENSION (extension);

        /* Happens if the module load fails, e.g. python couldn't
         * parse it, so be quiet about it, we will have already warned */
        if (py_ext->priv->module == NULL)
	{
		return;
	}

	pDict = PyModule_GetDict (py_ext->priv->module);
	pFunc = PyDict_GetItemString (pDict, func_name);

	if (pFunc && PyCallable_Check (pFunc))
	{
		pArgs = PyTuple_New (embed == NULL ? 1 : 2);

		pWindow = pygobject_new (G_OBJECT (window));
		PyTuple_SetItem (pArgs, 0, pWindow);

		if (embed != NULL)
		{
			pEmbed = pygobject_new (G_OBJECT (embed));
			PyTuple_SetItem (pArgs, 1, pEmbed);
		}

		pValue = PyObject_CallObject (pFunc, pArgs);
		if (pValue == NULL)
		{
			PyErr_Print ();
			PyErr_Clear ();
			g_warning ("Python code for '%s' failed to execute",
				   func_name);
		}
		Py_XDECREF (pValue);
		Py_DECREF (pArgs);
	}
	else
	{
		if (PyErr_Occurred ())
		{
			PyErr_Print ();
			PyErr_Clear ();
		}
	}
}

static void
impl_attach_tab (EphyExtension *extension,
		 EphyWindow *window,
		 EphyEmbed *embed)
{
	call_python_func (extension, "attach_tab", window, embed);
}

static void
impl_detach_tab (EphyExtension *extension,
		 EphyWindow *window,
		 EphyEmbed *embed)
{
	call_python_func (extension, "detach_tab", window, embed);

	ephy_python_schedule_gc ();
}

static void
impl_attach_window (EphyExtension *extension,
		    EphyWindow *window)
{
	call_python_func (extension, "attach_window", window, NULL);
}

static void
impl_detach_window (EphyExtension *extension,
		    EphyWindow *window)
{
	call_python_func (extension, "detach_window", window, NULL);

	ephy_python_schedule_gc ();
}

static void
ephy_python_extension_iface_init (EphyExtensionIface *iface)
{
	iface->attach_tab = impl_attach_tab;
	iface->detach_tab = impl_detach_tab;
	iface->attach_window = impl_attach_window;
	iface->detach_window = impl_detach_window;
}

G_DEFINE_TYPE_WITH_CODE (EphyPythonExtension, ephy_python_extension, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_EXTENSION,
                                                ephy_python_extension_iface_init))

static GObject *
ephy_python_extension_constructor (GType type,
				   guint n_construct_properties,
				   GObjectConstructParam *construct_params)
{
	GObject *object;
	EphyPythonExtension *ext;
	char *module_name;	/* filename minus optional ".py" */
				/* Note: could equally be a directory */
	PyObject *pModules, *pModule, *pReload;
			  
	int num_temp_paths;

	object = G_OBJECT_CLASS (ephy_python_extension_parent_class)->constructor (type,
                                                                                   n_construct_properties,
                                                                                   construct_params);

	ext = EPHY_PYTHON_EXTENSION (object);

	module_name = g_path_get_basename (ext->priv->filename);

	num_temp_paths = set_python_search_path (ext->priv->filename);

	pModules = PySys_GetObject ("modules");
	g_assert (pModules != NULL);

	pModule = PyDict_GetItemString (pModules, module_name);

	if (pModule == NULL)
	{
		pModule = PyImport_ImportModule (module_name);

		if (pModule == NULL)
		{
			PyErr_Print ();
			PyErr_Clear ();
			g_warning ("Could not initialize Python module '%s'",
				   module_name);
		}
	}
	else
	{
		pReload = PyImport_ReloadModule (pModule);

		if (pReload == NULL)
		{
			PyErr_Print ();
			PyErr_Clear ();
			g_warning ("Could not reload Python module '%s'\n"
				   "Falling back to previous version",
				   module_name);
		}
		else
		{
			Py_DECREF (pReload);
		}
	}

	unset_python_search_path (num_temp_paths);

	ext->priv->module = pModule;

	g_free (module_name);

	return object;
}

static void
ephy_python_extension_finalize (GObject *object)
{
	EphyPythonExtension *extension =
			EPHY_PYTHON_EXTENSION (object);

	LOG ("EphyPythonExtension finalizing");

	g_free (extension->priv->filename);
	Py_XDECREF (extension->priv->module);

	G_OBJECT_CLASS (ephy_python_extension_parent_class)->finalize (object);
}

static void
ephy_python_extension_get_property (GObject *object,
				    guint prop_id,
				    GValue *value,
				    GParamSpec *pspec)
{
	/* no readable properties */
	g_return_if_reached ();
}

static void
ephy_python_extension_set_property (GObject *object,
				    guint prop_id,
				    const GValue *value,
				    GParamSpec *pspec)
{
	EphyPythonExtension *ext = EPHY_PYTHON_EXTENSION (object);

	switch (prop_id)
	{
		case PROP_FILENAME:
			ext->priv->filename = g_value_dup_string (value);
			break;
		default:
			g_return_if_reached ();
	}
}

static void
ephy_python_extension_class_init (EphyPythonExtensionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = ephy_python_extension_finalize;
	object_class->constructor = ephy_python_extension_constructor;
	object_class->get_property = ephy_python_extension_get_property;
	object_class->set_property = ephy_python_extension_set_property;

	g_object_class_install_property
			(object_class,
			 PROP_FILENAME,
			 g_param_spec_string ("filename",
					      "Filename",
					      "Filename",
					      NULL,
					      G_PARAM_WRITABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class, sizeof (EphyPythonExtensionPrivate));
}

