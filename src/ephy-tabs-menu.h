/*
 *  Copyright (C) 2003 David Bordoley
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

#ifndef EPHY_TABS_MENU_H
#define EPHY_TABS_MENU_H

#include "ephy-window.h"

/* object forward declarations */

typedef struct _EphyTabsMenu EphyTabsMenu;
typedef struct _EphyTabsMenuClass EphyTabsMenuClass;
typedef struct _EphyTabsMenuPrivate EphyTabsMenuPrivate;

/**
 * Editor object
 */

#define EPHY_TYPE_TABS_MENU	(ephy_tabs_menu_get_type())
#define EPHY_TABS_MENU(object)	(G_TYPE_CHECK_INSTANCE_CAST((object), EPHY_TYPE_TABS_MENU, EphyTabsMenu))
#define EPHY_TABS_MENU_CLASS(klass)(G_TYPE_CHECK_CLASS_CAST((klass), EPHY_TYPE_TABS_MENU, EphyTabsMenuClass))
#define EPHY_IS_TABS_MENU(object)	(G_TYPE_CHECK_INSTANCE_TYPE((object), EPHY_TYPE_TABS_MENU))
#define EPHY_IS_TABS_MENU_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), EPHY_TYPE_TABS_MENU))
#define EPHY_TABS_MENU_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), EPHY_TYPE_TABS_MENU, EphyTabsMenuClass))

struct _EphyTabsMenuClass
{
	GObjectClass parent_class;
};

/* Remember: fields are public read-only */
struct _EphyTabsMenu
{
	GObject parent_object;

	EphyTabsMenuPrivate *priv;
};

GType              ephy_tabs_menu_get_type		(void);

EphyTabsMenu	  *ephy_tabs_menu_new			(EphyWindow *window);

void		   ephy_tabs_menu_update		(EphyTabsMenu *wrhm);

#endif

