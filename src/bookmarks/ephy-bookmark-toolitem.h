/*
 *  Copyright (C) 2003  Christian Persch
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

#ifndef EPHY_BOOKMARK_TOOLITEM_H
#define EPHY_BOOKMARK_TOOLITEM_H

#include "eggtoolitem.h"

G_BEGIN_DECLS

/* object forward declarations */

typedef struct _EphyBookmarkToolitem EphyBookmarkToolitem;
typedef struct _EphyBookmarkToolitemClass EphyBookmarkToolitemClass;
typedef struct _EphyBookmarkToolitemPrivate EphyBookmarkToolitemPrivate;

/**
 * EphyBookmarkToolitem object
 */

#define EPHY_TYPE_BOOKMARK_TOOLITEM		(ephy_bookmark_toolitem_get_type())
#define EPHY_BOOKMARK_TOOLITEM(object)		(G_TYPE_CHECK_INSTANCE_CAST((object), EPHY_TYPE_BOOKMARK_TOOLITEM, EphyBookmarkToolitem))
#define EPHY_BOOKMARK_TOOLITEM_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass), EPHY_TYPE_BOOKMARK_TOOLITEM, EphyBookmarkToolitemClass))
#define EPHY_IS_BOOKMARK_TOOLITEM(object)	(G_TYPE_CHECK_INSTANCE_TYPE((object), EPHY_TYPE_BOOKMARK_TOOLITEM))
#define EPHY_IS_BOOKMARK_TOOLITEM_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), EPHY_TYPE_BOOKMARK_TOOLITEM))
#define EPHY_BOOKMARK_TOOLITEM_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), EPHY_TYPE_BOOKMARK_TOOLITEM, EphyBookmarkToolitemClass))

struct _EphyBookmarkToolitemClass
{
	EggToolItemClass parent_class;
};

struct _EphyBookmarkToolitem
{
	EggToolItem parent_object;
};

GType	ephy_bookmark_toolitem_get_type	 (void);

G_END_DECLS

#endif
