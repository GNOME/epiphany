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

#ifndef EPHY_PLUGIN_H
#define EPHY_PLUGIN_H

#include <glib-object.h>

G_BEGIN_DECLS

#define EPHY_TYPE_PLUGIN            (ephy_plugin_get_type ())
#define EPHY_PLUGIN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_PLUGIN, EphyPlugin))
#define EPHY_PLUGIN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_PLUGIN, EphyPluginClass))
#define EPHY_IS_PLUGIN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_PLUGIN))
#define EPHY_IS_PLUGIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), EPHY_TYPE_PLUGIN))
#define EPHY_PLUGIN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), EPHY_TYPE_PLUGIN, EphyPluginClass))

typedef struct _EphyPlugin      EphyPlugin;

GType       ephy_plugin_get_type   (void);

EphyPlugin *ephy_plugin_new        (const char *name);

G_END_DECLS

#endif
