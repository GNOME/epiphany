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

#ifndef EPHY_BOOKMARKS_TOOLBAR_H
#define EPHY_BOOKMARKS_TOOLBAR_H

#include "ephy-window.h"

typedef struct _EphyBookmarksToolbar EphyBookmarksToolbar;
typedef struct _EphyBookmarksToolbarClass EphyBookmarksToolbarClass;
typedef struct _EphyBookmarksToolbarPrivate EphyBookmarksToolbarPrivate;

#define EPHY_TYPE_BOOKMARKS_TOOLBAR	(ephy_bookmarks_toolbar_get_type())
#define EPHY_BOOKMARKS_TOOLBAR(object)	(G_TYPE_CHECK_INSTANCE_CAST((object), EPHY_TYPE_BOOKMARKS_TOOLBAR, EphyBookmarksToolbar))
#define EPHY_BOOKMARKS_TOOLBAR_CLASS(klass)(G_TYPE_CHECK_CLASS_CAST((klass), EPHY_TYPE_BOOKMARKS_TOOLBAR, EphyBookmarksToolbarClass))
#define EPHY_IS_BOOKMARKS_TOOLBAR(object)	(G_TYPE_CHECK_INSTANCE_TYPE((object), EPHY_TYPE_BOOKMARKS_TOOLBAR))
#define EPHY_IS_BOOKMARKS_TOOLBAR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), EPHY_TYPE_BOOKMARKS_TOOLBAR))
#define EPHY_BOOKMARKS_TOOLBAR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), EPHY_TYPE_BOOKMARKS_TOOLBAR, EphyBookmarksToolbarClass))

struct _EphyBookmarksToolbarClass
{
	GObjectClass parent_class;
};

struct _EphyBookmarksToolbar
{
	GObject parent_object;

	EphyBookmarksToolbarPrivate *priv;
};

GType			ephy_bookmarks_toolbar_get_type		(void);

EphyBookmarksToolbar   *ephy_bookmarks_toolbar_new		(EphyWindow *window);

void		        ephy_bookmarks_toolbar_update		(EphyBookmarksToolbar *wrhm);

void		        ephy_bookmarks_toolbar_show		(EphyBookmarksToolbar *wrhm);

void		        ephy_bookmarks_toolbar_hide		(EphyBookmarksToolbar *wrhm);

#endif

