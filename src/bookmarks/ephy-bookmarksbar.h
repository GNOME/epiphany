/*
 *  Copyright (C) 2002 Jorn Baayen
 *  Copyright (C) 2003-2004 Marco Pesenti Gritti
 *  Copyright (C) 2003-2004 Christian Persch
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

#ifndef EPHY_BOOKMARKSBAR_H
#define EPHY_BOOKMARKSBAR_H

#include "egg-editable-toolbar.h"
#include "ephy-window.h"

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#define EPHY_TYPE_BOOKMARKSBAR		(ephy_bookmarksbar_get_type ())
#define EPHY_BOOKMARKSBAR(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_BOOKMARKSBAR, EphyBookmarksBar))
#define EPHY_BOOKMARKSBAR_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_BOOKMARKSBAR, EphyBookmarksBarClass))
#define EPHY_IS_BOOKMARKSBAR(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_BOOKMARKSBAR))
#define EPHY_IS_BOOKMARKSBAR_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_BOOKMARKSBAR))
#define EPHY_BOOKMARKSBAR_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_BOOKMARKSBAR, EphyBookmarksBarClass))

typedef struct EphyBookmarksBar		EphyBookmarksBar;
typedef struct EphyBookmarksBarClass	EphyBookmarksBarClass;
typedef struct EphyBookmarksBarPrivate	EphyBookmarksBarPrivate;

struct EphyBookmarksBar
{
	EggEditableToolbar parent_object;

	/*< private >*/
	EphyBookmarksBarPrivate *priv;
};

struct EphyBookmarksBarClass
{
	EggEditableToolbarClass parent_class;
};

GType		ephy_bookmarksbar_get_type	(void);

GtkWidget      *ephy_bookmarksbar_new	(EphyWindow *window);

G_END_DECLS

#endif
