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
 *
 *  $Id$
 */

#ifndef EPHY_BOOKMARK_ACTION_H
#define EPHY_BOOKMARK_ACTION_H

#include <gtk/gtk.h>
#include <gtk/gtkaction.h>

#define EPHY_TYPE_BOOKMARK_ACTION            (ephy_bookmark_action_get_type ())
#define EPHY_BOOKMARK_ACTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_BOOKMARK_ACTION, EphyBookmarkAction))
#define EPHY_BOOKMARK_ACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_BOOKMARK_ACTION, EphyBookmarkActionClass))
#define EPHY_IS_BOOKMARK_ACTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_BOOKMARK_ACTION))
#define EPHY_IS_BOOKMARK_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), EPHY_TYPE_BOOKMARK_ACTION))
#define EPHY_BOOKMARK_ACTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), EPHY_TYPE_BOOKMARK_ACTION, EphyBookmarkActionClass))

typedef struct _EphyBookmarkAction      EphyBookmarkAction;
typedef struct _EphyBookmarkActionClass EphyBookmarkActionClass;
typedef struct EphyBookmarkActionPrivate EphyBookmarkActionPrivate;

struct _EphyBookmarkAction
{
	GtkAction parent;

	/*< private >*/
	EphyBookmarkActionPrivate *priv;
};

struct _EphyBookmarkActionClass
{
	GtkActionClass parent_class;

	void (*open)          (EphyBookmarkAction *action,
			       char *address);
	void (*open_in_tab)   (EphyBookmarkAction *action,
			       char *address,
			       gboolean new_window);
};

GType      ephy_bookmark_action_get_type	(void);

GtkAction *ephy_bookmark_action_new		(const char *name,
						 guint id);

#endif
