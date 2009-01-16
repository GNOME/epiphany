/*
 *  Copyright © 2004, 2005 Jean-François Rameau
 *  Copyright © 2004, 2005 Adam Hooper
 *  Copyright © 2004, 2005 Crispin Flowerday
 *  Copyright © 2004, 2005 Christian Persch
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

#include <Python.h>

#include "config.h"

#include "ephy-python.h"
#include "ephy-debug.h"

#include <pygobject.h>
#include <pygtk/pygtk.h>

void pyepiphany_register_classes (PyObject *d); 
void pyepiphany_add_constants (PyObject *module, const gchar *strip_prefix);

extern PyMethodDef pyepiphany_functions[];

static guint idle_gc_handler	   = 0;
static guint idle_shutdown_handler = 0;
static gboolean python_initialised = FALSE;

void
ephy_python_init (void)
{
	char *argv[1];
	PyObject *m, *d;
	
	Py_Initialize();
	python_initialised = TRUE;

	argv[0] = g_get_prgname ();
	PySys_SetArgv (1, argv);

	init_pygobject ();
	init_pygtk ();

	m = Py_InitModule ("epiphany", pyepiphany_functions);
	d = PyModule_GetDict (m);

	pyepiphany_register_classes (d);
	pyepiphany_add_constants (m, "EPHY_");
}

static gboolean
idle_shutdown (void)
{
	g_return_val_if_fail (idle_gc_handler == 0, FALSE);

	Py_Finalize ();

	idle_shutdown_handler = 0;
	return FALSE;
}

void
ephy_python_shutdown (void)
{
	if (!python_initialised) return;

	g_return_if_fail (idle_shutdown_handler == 0);

	LOG ("EphyPython shutdown with %s GC scheduled",
	     idle_gc_handler != 0 ? "a" : "no");

	if (idle_gc_handler != 0)
	{
		/* Process remaining GCs now */
		while (PyGC_Collect ()) ;

		g_source_remove (idle_gc_handler);
		idle_gc_handler = 0;

		/* IMPORTANT! We get here while running PyGC_Collect from idle!
		 * Don't do Py_Finalize while inside python!
		 */
		idle_shutdown_handler = g_idle_add ((GSourceFunc) idle_shutdown, NULL);
	}
	else
	{
		Py_Finalize();
	}
}

static gboolean
idle_gc (void)
{
	long value;

	/* LOG ("Running GC from idle"); */

	/* FIXME what does the return value of PyGC_Collect mean? */
	value = PyGC_Collect ();

	/* LOG ("Idle GC returned %ld", value); */

	if (value == 0)
	{
		idle_gc_handler = 0;
	}

	return value != 0;
}

void
ephy_python_schedule_gc (void)
{
	/* LOG ("Scheduling a GC with %s GC already scheduled", idle_gc_handler != 0 ? "a" : "no"); */

	if (python_initialised && idle_gc_handler == 0)
	{
		idle_gc_handler = g_idle_add ((GSourceFunc) idle_gc, NULL);
	}
}
