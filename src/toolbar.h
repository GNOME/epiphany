/*
 *  Copyright (C) 2002 Jorn Baayen
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

#ifndef TOOLBAR_H
#define TOOLBAR_H

#include "egg-editable-toolbar.h"
#include "ephy-window.h"

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#define EPHY_TYPE_TOOLBAR		(toolbar_get_type ())
#define EPHY_TOOLBAR(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_TOOLBAR, Toolbar))
#define EPHY_TOOLBAR_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_TOOLBAR, ToolbarClass))
#define EPHY_IS_TOOLBAR(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_TOOLBAR))
#define EPHY_IS_TOOLBAR_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_TOOLBAR))
#define EPHY_TOOLBAR_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_TOOLBAR, ToolbarClass))

typedef struct ToolbarClass ToolbarClass;
typedef struct ToolbarPrivate ToolbarPrivate;

struct Toolbar
{
        EggEditableToolbar parent_object;
        ToolbarPrivate *priv;
};

struct ToolbarClass
{
        EggEditableToolbarClass parent_class;
};

GType         toolbar_get_type			(void);

Toolbar      *toolbar_new			(EphyWindow *window);

void          toolbar_spinner_start		(Toolbar *t);

void          toolbar_spinner_stop		(Toolbar *t);

char         *toolbar_get_location              (Toolbar *t);

void          toolbar_set_location              (Toolbar *t,
						 const char *location);

void	      toolbar_activate_location		(Toolbar *t);

void	      toolbar_clear_location_history	(Toolbar *t);

void          toolbar_update_favicon            (Toolbar *t);

void	      toolbar_update_navigation_actions (Toolbar *t,
						 gboolean back,
						 gboolean forward,
						 gboolean up);

void	      toolbar_update_zoom		(Toolbar *t,
						 float zoom);

void	      toolbar_set_visibility		(Toolbar *t,
						 gboolean normal_toolbars,
						 gboolean bmk_toolbars);

G_END_DECLS

#endif
