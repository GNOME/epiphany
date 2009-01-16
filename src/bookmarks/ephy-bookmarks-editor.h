/*
 *  Copyright © 2002 Jorn Baayen <jorn@nl.linux.org>
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef EPHY_BOOKMARKS_EDITOR_H
#define EPHY_BOOKMARKS_EDITOR_H

#include <gtk/gtk.h>

#include "ephy-node-view.h"
#include "ephy-bookmarks.h"

G_BEGIN_DECLS

#define EPHY_TYPE_BOOKMARKS_EDITOR		(ephy_bookmarks_editor_get_type ())
#define EPHY_BOOKMARKS_EDITOR(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_BOOKMARKS_EDITOR, EphyBookmarksEditor))
#define EPHY_BOOKMARKS_EDITOR_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_BOOKMARKS_EDITOR, EphyBookmarksEditorClass))
#define EPHY_IS_BOOKMARKS_EDITOR(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_BOOKMARKS_EDITOR))
#define EPHY_IS_BOOKMARKS_EDITOR_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_BOOKMARKS_EDITOR))
#define EPHY_BOOKMARKS_EDITOR_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_BOOKMARKS_EDITOR, EphyBookmarksEditorClass))

typedef struct _EphyBookmarksEditorPrivate EphyBookmarksEditorPrivate;

typedef struct
{
	GtkWindow parent;

	/*< private >*/
	EphyBookmarksEditorPrivate *priv;
} EphyBookmarksEditor;

typedef struct
{
	GtkDialogClass parent;
} EphyBookmarksEditorClass;

GType		     ephy_bookmarks_editor_get_type (void);

GtkWidget	    *ephy_bookmarks_editor_new        (EphyBookmarks *bookmarks);

void		     ephy_bookmarks_editor_set_parent (EphyBookmarksEditor *ebe,
						       GtkWidget *window);

G_END_DECLS

#endif /* EPHY_BOOKMARKS_EDITOR_H */
