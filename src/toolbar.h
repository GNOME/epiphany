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

G_BEGIN_DECLS

typedef struct ToolbarClass ToolbarClass;

#define TOOLBAR_TYPE             (toolbar_get_type ())
#define TOOLBAR(obj)             (GTK_CHECK_CAST ((obj), TOOLBAR_TYPE, Toolbar))
#define TOOLBAR_CLASS(klass)     (GTK_CHECK_CLASS_CAST ((klass), TOOLBAR, ToolbarClass))
#define IS_TOOLBAR(obj)          (GTK_CHECK_TYPE ((obj), TOOLBAR_TYPE))
#define IS_TOOLBAR_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), TOOLBAR))

typedef struct ToolbarPrivate ToolbarPrivate;

struct Toolbar
{
        GObject parent_object;
        ToolbarPrivate *priv;
};

struct ToolbarClass
{
        GObjectClass parent_class;
};

GType         toolbar_get_type			(void);

Toolbar      *toolbar_new			(EphyWindow *window);

void          toolbar_set_visibility		(Toolbar *t,
						 gboolean visibility);

void          toolbar_spinner_start		(Toolbar *t);

void          toolbar_spinner_stop		(Toolbar *t);

char         *toolbar_get_location              (Toolbar *t);

void          toolbar_set_location              (Toolbar *t,
						 const char *location);

void	      toolbar_activate_location		(Toolbar *t);

void          toolbar_update_favicon            (Toolbar *t);

void	      toolbar_update_navigation_actions (Toolbar *t,
						 gboolean back,
						 gboolean forward,
						 gboolean up);

G_END_DECLS

#endif
