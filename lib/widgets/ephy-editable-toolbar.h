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

#ifndef EPHY_EDITABLE_TOOLBAR_H
#define EPHY_EDITABLE_TOOLBAR_H

#include <glib-object.h>
#include <glib.h>
#include "egg-menu-merge.h"

G_BEGIN_DECLS

typedef struct EphyEditableToolbarClass EphyEditableToolbarClass;

#define EPHY_EDITABLE_TOOLBAR_TYPE             (ephy_editable_toolbar_get_type ())
#define EPHY_EDITABLE_TOOLBAR(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_EDITABLE_TOOLBAR_TYPE, EphyEditableToolbar))
#define EPHY_EDITABLE_TOOLBAR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_EDITABLE_TOOLBAR_TYPE, EphyEditableToolbarClass))
#define IS_EPHY_EDITABLE_TOOLBAR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_EDITABLE_TOOLBAR_TYPE))
#define IS_EPHY_EDITABLE_TOOLBAR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), EPHY_EDITABLE_TOOLBAR_TYPE))
#define EPHY_EDITABLE_TOOLBAR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), EPHY_EDITABLE_TOOLBAR_TYPE, EphyEditableToolbarClass))


typedef struct EphyEditableToolbar EphyEditableToolbar;
typedef struct EphyEditableToolbarPrivate EphyEditableToolbarPrivate;

struct EphyEditableToolbar
{
        GObject parent_object;
        EphyEditableToolbarPrivate *priv;
};

struct EphyEditableToolbarClass
{
        GObjectClass parent_class;

	EggAction * (* get_action) (EphyEditableToolbar *etoolbar,
				    const char *type,
				    const char *name);
};

GType			ephy_editable_toolbar_get_type	 (void);

EphyEditableToolbar    *ephy_editable_toolbar_new	 (EggMenuMerge *merge);

void			ephy_editable_toolbar_edit	 (EphyEditableToolbar *etoolbar,
							  GtkWidget *window);

EggAction              *ephy_editable_toolbar_get_action (EphyEditableToolbar *etoolbar,
							  const char *type,
							  const char *name);


G_END_DECLS

#endif
