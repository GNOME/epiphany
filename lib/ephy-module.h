/*
 *  Copyright (C) 2003 Marco Pesenti Gritti
 *  Copyright (C) 2003, 2004 Christian Persch
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

#ifndef EPHY_MODULE_H
#define EPHY_MODULE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define EPHY_TYPE_MODULE		(ephy_module_get_type ())
#define EPHY_MODULE(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_MODULE, EphyModule))
#define EPHY_MODULE_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_MODULE, EphyModuleClass))
#define EPHY_IS_MODULE(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_MODULE))
#define EPHY_IS_MODULE_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((obj), EPHY_TYPE_MODULE))
#define EPHY_MODULE_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), EPHY_TYPE_MODULE, EphyModuleClass))

typedef struct _EphyModule	EphyModule;

GType		 ephy_module_get_type	(void);

EphyModule	*ephy_module_new	(const char *path,
					 gboolean resident);

const char	*ephy_module_get_path	(EphyModule *module);

GObject		*ephy_module_new_object	(EphyModule *module);

G_END_DECLS

#endif
