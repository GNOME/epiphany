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
 */

#ifndef EPHY_ARROW_TOOLBUTTON_H
#define EPHY_ARROW_TOOLBUTTON_H

#include <glib.h>
#include <gtk/gtkmenushell.h>

#include "eggtoolbutton.h"

G_BEGIN_DECLS

typedef struct EphyArrowToolButtonClass EphyArrowToolButtonClass;

#define EPHY_ARROW_TOOLBUTTON_TYPE             (ephy_arrow_toolbutton_get_type ())
#define EPHY_ARROW_TOOLBUTTON(obj)             (GTK_CHECK_CAST ((obj), EPHY_ARROW_TOOLBUTTON_TYPE, EphyArrowToolButton))
#define EPHY_ARROW_TOOLBUTTON_CLASS(klass)     (GTK_CHECK_CLASS_CAST ((klass), EPHY_ARROW_TOOLBUTTON_TYPE, EphyArrowToolButtonClass))
#define IS_EPHY_ARROW_TOOLBUTTON(obj)          (GTK_CHECK_TYPE ((obj), EPHY_ARROW_TOOLBUTTON_TYPE))
#define IS_EPHY_ARROW_TOOLBUTTON_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), EPHY_ARROW_TOOLBUTTON))

typedef struct EphyArrowToolButton EphyArrowToolButton;
typedef struct EphyArrowToolButtonPrivate EphyArrowToolButtonPrivate;

struct EphyArrowToolButton
{
	EggToolButton parent;
        EphyArrowToolButtonPrivate *priv;
};

struct EphyArrowToolButtonClass
{
        EggToolButtonClass parent_class;

	void (*menu_activated) (EphyArrowToolButton *b);
};

GType		ephy_arrow_toolbutton_get_type		(void);

GtkMenuShell    *ephy_arrow_toolbutton_get_menu		(EphyArrowToolButton *b);

G_END_DECLS;

#endif /* EPHY_ARROW_TOOLBUTTON_H */
