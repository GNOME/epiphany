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
 *  $Id$
 */

#ifndef EPHY_PYTHON_EXTENSION_H
#define EPHY_PYTHON_EXTENSION_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define EPHY_TYPE_PYTHON_EXTENSION		(ephy_python_extension_get_type ())
#define EPHY_PYTHON_EXTENSION(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_PYTHON_EXTENSION, EphyPythonExtension))
#define EPHY_PYTHON_EXTENSION_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_PYTHON_EXTENSION, EphyPythonExtensionClass))
#define EPHY_IS_PYTHON_EXTENSION(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_PYTHON_EXTENSION))
#define EPHY_IS_PYTHON_EXTENSION_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_PYTHON_EXTENSION))
#define EPHY_PYTHON_EXTENSION_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_PYTHON_EXTENSION, EphyPythonExtensionClass))

typedef struct _EphyPythonExtension		EphyPythonExtension;
typedef struct _EphyPythonExtensionClass	EphyPythonExtensionClass;
typedef struct _EphyPythonExtensionPrivate	EphyPythonExtensionPrivate;

struct _EphyPythonExtensionClass
{
	GObjectClass parent_class;
};

struct _EphyPythonExtension
{
	GObject parent_instance;

	/*< private >*/
	EphyPythonExtensionPrivate *priv;
};

GType	ephy_python_extension_get_type		(void);

G_END_DECLS

#endif
