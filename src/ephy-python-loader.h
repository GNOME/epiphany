/*
 *  Copyright © 2003 Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2005 Christian Persch
 *  Copyright © 2004, 2005 Jean-François Rameau
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

#ifndef EPHY_PYTHON_LOADER_H
#define EPHY_PYTHON_LOADER_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define EPHY_TYPE_PYTHON_LOADER		(ephy_python_loader_get_type ())
#define EPHY_PYTHON_LOADER(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_PYTHON_LOADER, EphyPythonLoader))
#define EPHY_PYTHON_LOADER_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_PYTHON_LOADER, EphyPythonLoaderClass))
#define EPHY_IS_PYTHON_LOADER(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_PYTHON_LOADER))
#define EPHY_IS_PYTHON_LOADER_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_PYTHON_LOADER))
#define EPHY_PYTHON_LOADER_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_PYTHON_LOADER, EphyPythonLoaderClass))

typedef struct _EphyPythonLoader	EphyPythonLoader;
typedef struct _EphyPythonLoaderClass	EphyPythonLoaderClass;
typedef struct _EphyPythonLoaderPrivate	EphyPythonLoaderPrivate;

struct _EphyPythonLoaderClass
{
	GObjectClass parent_class;
};

struct _EphyPythonLoader
{
	GObject parent_instance;

	/*< private >*/
	EphyPythonLoaderPrivate *priv;
};

GType	ephy_python_loader_get_type		(void);

G_END_DECLS

#endif /* !EPHY_PYTHON_LOADER_H */
