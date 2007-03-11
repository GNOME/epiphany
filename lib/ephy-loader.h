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

#ifndef EPHY_LOADER_H
#define EPHY_LOADER_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define EPHY_TYPE_LOADER		(ephy_loader_get_type ())
#define EPHY_LOADER(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_LOADER, EphyLoader))
#define EPHY_LOADER_IFACE(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_LOADER, EphyLoaderIface))
#define EPHY_IS_LOADER(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_LOADER))
#define EPHY_IS_LOADER_IFACE(iface)	(G_TYPE_CHECK_CLASS_TYPE ((iface), EPHY_TYPE_LOADER))
#define EPHY_LOADER_GET_IFACE(inst)	(G_TYPE_INSTANCE_GET_INTERFACE ((inst), EPHY_TYPE_LOADER, EphyLoaderIface))

typedef struct _EphyLoader		EphyLoader;
typedef struct _EphyLoaderIface	EphyLoaderIface;
	
struct _EphyLoaderIface
{
	GTypeInterface base_iface;

	/* Identifier */
	const char *type;

	/* Methods */
	GObject *    (* get_object)	(EphyLoader *loader,
					 GKeyFile *keyfile);
	void	     (* release_object)	(EphyLoader *loader,
					 GObject *object);
};

GType	    ephy_loader_get_type	(void);

const char *ephy_loader_type		(const EphyLoader *loader);

GObject    *ephy_loader_get_object	(EphyLoader *loader,
					 GKeyFile *keyfile);

void	    ephy_loader_release_object	(EphyLoader *loader,
					 GObject *object);

G_END_DECLS

#endif
