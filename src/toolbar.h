/*
 *  Copyright (C) 2002 Jorn Baayen
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

#ifndef TOOLBAR_H
#define TOOLBAR_H

#include "ephy-window.h"
#include <glib-object.h>
#include <glib.h>
#include "ephy-toolbar.h"

G_BEGIN_DECLS

typedef struct ToolbarClass ToolbarClass;

#define TOOLBAR_TYPE             (toolbar_get_type ())
#define TOOLBAR(obj)             (GTK_CHECK_CAST ((obj), TOOLBAR_TYPE, Toolbar))
#define TOOLBAR_CLASS(klass)     (GTK_CHECK_CLASS_CAST ((klass), TOOLBAR, ToolbarClass))
#define IS_TOOLBAR(obj)          (GTK_CHECK_TYPE ((obj), TOOLBAR_TYPE))
#define IS_TOOLBAR_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), TOOLBAR))

typedef struct ToolbarPrivate ToolbarPrivate;

typedef enum
{
	TOOLBAR_BACK_BUTTON,
	TOOLBAR_FORWARD_BUTTON,
	TOOLBAR_STOP_BUTTON,
	TOOLBAR_UP_BUTTON
} ToolbarButtonID;

struct Toolbar
{
        EphyToolbar parent_object;
        ToolbarPrivate *priv;
};

struct ToolbarClass
{
        EphyToolbarClass parent_class;
};

GType         toolbar_get_type			(void);

Toolbar      *toolbar_new			(EphyWindow *window);

void          toolbar_set_visibility		(Toolbar *t,
						 gboolean visibility);

void	      toolbar_button_set_sensitive	(Toolbar *t,
						 ToolbarButtonID id,
						 gboolean sensitivity);

void          toolbar_spinner_start		(Toolbar *t);

void          toolbar_spinner_stop		(Toolbar *t);

char         *toolbar_get_location              (Toolbar *t);

void          toolbar_set_location              (Toolbar *t,
						 const char *location);

gint	      toolbar_get_zoom			(Toolbar *t);

void	      toolbar_set_zoom			(Toolbar *t, gint zoom);

void	      toolbar_activate_location		(Toolbar *t);

void          toolbar_update_favicon            (Toolbar *t);

G_END_DECLS

#endif
