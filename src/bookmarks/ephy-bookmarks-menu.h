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
 *
 *  $Id$
 */

#ifndef EPHY_BOOKMARKS_MENU_H
#define EPHY_BOOKMARKS_MENU_H

#include <glib-object.h>
#include <gtk/gtkuimanager.h>

G_BEGIN_DECLS

#define EPHY_TYPE_BOOKMARKS_MENU	    (ephy_bookmarks_menu_get_type())
#define EPHY_BOOKMARKS_MENU(object)	    (G_TYPE_CHECK_INSTANCE_CAST((object), EPHY_TYPE_BOOKMARKS_MENU, EphyBookmarksMenu))
#define EPHY_BOOKMARKS_MENU_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), EPHY_TYPE_BOOKMARKS_MENU, EphyBookmarksMenuClass))
#define EPHY_IS_BOOKMARKS_MENU(object)	    (G_TYPE_CHECK_INSTANCE_TYPE((object), EPHY_TYPE_BOOKMARKS_MENU))
#define EPHY_IS_BOOKMARKS_MENU_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), EPHY_TYPE_BOOKMARKS_MENU))
#define EPHY_BOOKMARKS_MENU_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), EPHY_TYPE_BOOKMARKS_MENU, EphyBookmarksMenuClass))

typedef struct _EphyBookmarksMenu		EphyBookmarksMenu;
typedef struct _EphyBookmarksMenuClass		EphyBookmarksMenuClass;
typedef struct _EphyBookmarksMenuPrivate	EphyBookmarksMenuPrivate;

struct _EphyBookmarksMenuClass
{
	GObjectClass parent_class;

	void (*open)          (EphyBookmarksMenu *menu,
			       const char *address,
			       gboolean open_in_new);
};

struct _EphyBookmarksMenu
{
	GObject parent_object;

	/*< private >*/
	EphyBookmarksMenuPrivate *priv;
};

GType              ephy_bookmarks_menu_get_type		(void);

EphyBookmarksMenu *ephy_bookmarks_menu_new		(GtkUIManager *manager,
							 const char *path);

G_END_DECLS

#endif
