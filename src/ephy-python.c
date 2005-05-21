/*
 *  Copyright (C) 2004, 2005 Jean-François Rameau
 *  Copyright (C) 2004, 2005 Adam Hooper
 *  Copyright (C) 2004, 2005 Crispin Flowerday
 *  Copyright (C) 2004, 2005 Christian Persch
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

#include <Python.h>

#include "config.h"

#include "ephy-python.h"

#include <pygobject.h>
#include <pygtk/pygtk.h>

void pyepiphany_register_classes (PyObject *d); 
void pyepiphany_add_constants (PyObject *module, const gchar *strip_prefix);

extern PyMethodDef pyepiphany_functions[];

void
ephy_python_init (void)
{
	char *argv[1];
	PyObject *m, *d;
	
	Py_Initialize();

	argv[0] = g_get_prgname ();
	PySys_SetArgv (1, argv);

	init_pygobject ();
	init_pygtk ();

	m = Py_InitModule ("epiphany", pyepiphany_functions);
	d = PyModule_GetDict (m);

	pyepiphany_register_classes (d);
	pyepiphany_add_constants (m, "EPHY_");
}
