/*
 *  Copyright (C) 2002 Christophe Fergeau
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

#ifndef EPHY_ARROW_TOOLBUTTON_H
#define EPHY_ARROW_TOOLBUTTON_H

#include <glib.h>
#include <gtk/gtkmenushell.h>
#include <gtk/gtktoolbutton.h>

G_BEGIN_DECLS

#define EPHY_TYPE_ARROW_TOOLBUTTON	   (ephy_arrow_toolbutton_get_type ())
#define EPHY_ARROW_TOOLBUTTON(o)	   (G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_ARROW_TOOLBUTTON, EphyArrowToolButton))
#define EPHY_ARROW_TOOLBUTTON_CLASS(k)	   (G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_ARROW_TOOLBUTTON, EphyArrowToolButtonClass))
#define EPHY_IS_ARROW_TOOLBUTTON(o)	   (G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_ARROW_TOOLBUTTON))
#define EPHY_IS_ARROW_TOOLBUTTON_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_ARROW_TOOLBUTTON))
#define EPHY_ARROW_TOOLBUTTON_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_ARROW_TOOLBUTTON, EphyArrowToolButtonClass))

typedef struct EphyArrowToolButtonClass EphyArrowToolButtonClass;
typedef struct EphyArrowToolButton EphyArrowToolButton;
typedef struct EphyArrowToolButtonPrivate EphyArrowToolButtonPrivate;

struct EphyArrowToolButton
{
	GtkToolButton parent;

	/*< private >*/
        EphyArrowToolButtonPrivate *priv;
};

struct EphyArrowToolButtonClass
{
        GtkToolButtonClass parent_class;

	void (*menu_activated) (EphyArrowToolButton *b);
};

GType		ephy_arrow_toolbutton_get_type		(void);

GtkMenuShell    *ephy_arrow_toolbutton_get_menu		(EphyArrowToolButton *b);

G_END_DECLS;

#endif /* EPHY_ARROW_TOOLBUTTON_H */
