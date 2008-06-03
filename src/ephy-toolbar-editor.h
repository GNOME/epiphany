/*
 *  Copyright © 2000-2004 Marco Pesenti Gritti
 *  Copyright © 2003-2005 Christian Persch
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
 *  $Id$
 */

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef EPHY_TOOLBAR_EDITOR_H
#define EPHY_TOOLBAR_EDITOR_H

#include <gtk/gtk.h>
#include "ephy-window.h"

G_BEGIN_DECLS

#define EPHY_TYPE_TOOLBAR_EDITOR         (ephy_toolbar_editor_get_type ())
#define EPHY_TOOLBAR_EDITOR(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_TOOLBAR_EDITOR, EphyToolbarEditor))
#define EPHY_TOOLBAR_EDITOR_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_TOOLBAR_EDITOR, EphyToolbarEditorClass))
#define EPHY_IS_TOOLBAR_EDITOR(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_TOOLBAR_EDITOR))
#define EPHY_IS_TOOLBAR_EDITOR_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_TOOLBAR_EDITOR))
#define EPHY_TOOLBAR_EDITOR_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_TOOLBAR_EDITOR, EphyToolbarEditorClass))

typedef struct _EphyToolbarEditor		EphyToolbarEditor;
typedef struct _EphyToolbarEditorPrivate	EphyToolbarEditorPrivate;
typedef struct _EphyToolbarEditorClass		EphyToolbarEditorClass;

struct _EphyToolbarEditor
{
	GtkDialog parent_instance;

	/*< private >*/
	EphyToolbarEditorPrivate *priv;
};

struct _EphyToolbarEditorClass
{
	GtkDialogClass parent_class;
};

GType		ephy_toolbar_editor_get_type	(void);

GtkWidget      *ephy_toolbar_editor_show	(EphyWindow *window);

G_END_DECLS

#endif /* !EPHY_TOOLBAR_EDITOR_H */
