/*
 *  Copyright (C) 2002 Jorn Baayen <jorn@nl.linux.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#ifndef EPHY_NEW_BOOKMARK_H
#define EPHY_NEW_BOOKMARK_H

#include <gtk/gtkdialog.h>

#include "ephy-bookmarks.h"

G_BEGIN_DECLS

#define EPHY_TYPE_NEW_BOOKMARK     (ephy_new_bookmark_get_type ())
#define EPHY_NEW_BOOKMARK(o)       (G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_NEW_BOOKMARK, EphyNewBookmark))
#define EPHY_NEW_BOOKMARK_CLASS(k) (G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_NEW_BOOKMARK, EphyNewBookmarkClass))
#define EPHY_IS_NEW_BOOKMARK(o)    (G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_NEW_BOOKMARK))
#define EPHY_IS_NEW_BOOKMARK_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_NEW_BOOKMARK))
#define EPHY_NEW_BOOKMARK_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_NEW_BOOKMARK, EphyNewBookmarkClass))

typedef struct EphyNewBookmarkPrivate EphyNewBookmarkPrivate;

typedef struct
{
	GtkDialog parent;

	EphyNewBookmarkPrivate *priv;
} EphyNewBookmark;

typedef struct
{
	GtkDialogClass parent;
} EphyNewBookmarkClass;

GType		     ephy_new_bookmark_get_type     (void);

GtkWidget	    *ephy_new_bookmark_new          (EphyBookmarks *bookmarks,
						     GtkWindow *parent,
						     const char *location);

void		     ephy_new_bookmark_set_title    (EphyNewBookmark *bookmark,
						     const char *title);

void		     ephy_new_bookmark_set_smarturl (EphyNewBookmark *bookmark,
						     const char *url);

G_END_DECLS

#endif /* EPHY_NEW_BOOKMARK_H */
