/*
 *  Copyright (C) 2002  Ricardo Fernández Pascual
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

#ifndef EPHY_ENCODING_MENU_H
#define EPHY_ENCODING_MENU_H

#include "ephy-window.h"

/* object forward declarations */

typedef struct _EphyEncodingMenu EphyEncodingMenu;
typedef struct _EphyEncodingMenuClass EphyEncodingMenuClass;
typedef struct _EphyEncodingMenuPrivate EphyEncodingMenuPrivate;

/**
 * Editor object
 */

#define EPHY_TYPE_ENCODING_MENU		(ephy_encoding_menu_get_type())
#define EPHY_ENCODING_MENU(object)	(G_TYPE_CHECK_INSTANCE_CAST((object), EPHY_TYPE_ENCODING_MENU, EphyEncodingMenu))
#define EPHY_ENCODING_MENU_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), EPHY_TYPE_ENCODING_MENU, EphyEncodingMenuClass))
#define EPHY_IS_ENCODING_MENU(object)	(G_TYPE_CHECK_INSTANCE_TYPE((object), EPHY_TYPE_ENCODING_MENU))
#define EPHY_IS_ENCODING_MENU_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), EPHY_TYPE_ENCODING_MENU))
#define EPHY_ENCODING_MENU_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), EPHY_TYPE_ENCODING_MENU, EphyEncodingMenuClass))

struct _EphyEncodingMenuClass
{
	GObjectClass parent_class;
};

/* Remember: fields are public read-only */
struct _EphyEncodingMenu
{
	GObject parent_object;

	EphyEncodingMenuPrivate *priv;
};

GType              ephy_encoding_menu_get_type		(void);

EphyEncodingMenu  *ephy_encoding_menu_new		(EphyWindow *window);

#endif

