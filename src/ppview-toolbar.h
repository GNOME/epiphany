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

#ifndef PPVIEW_TOOLBAR_H
#define PPVIEW_TOOLBAR_H

#include "ephy-window.h"
#include <glib-object.h>
#include <glib.h>
#include <gtk/gtkbutton.h>

G_BEGIN_DECLS

typedef struct PPViewToolbar PPViewToolbar;
typedef struct PPViewToolbarClass PPViewToolbarClass;

#define PPVIEW_TOOLBAR_TYPE             (ppview_toolbar_get_type ())
#define PPVIEW_TOOLBAR(obj)             (GTK_CHECK_CAST ((obj), PPVIEW_TOOLBAR_TYPE, PPViewToolbar))
#define PPVIEW_TOOLBAR_CLASS(klass)     (GTK_CHECK_CLASS_CAST ((klass), PPVIEW_TOOLBAR, PPViewToolbarClass))
#define IS_PPVIEW_TOOLBAR(obj)          (GTK_CHECK_TYPE ((obj), PPVIEW_TOOLBAR_TYPE))
#define IS_PPVIEW_TOOLBAR_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), PPVIEW_TOOLBAR))

typedef struct PPViewToolbarPrivate PPViewToolbarPrivate;

struct PPViewToolbar
{
        GObject parent;
        PPViewToolbarPrivate *priv;
};

struct PPViewToolbarClass
{
        GObjectClass parent_class;
};

GType          ppview_toolbar_get_type			(void);

PPViewToolbar *ppview_toolbar_new			(EphyWindow *window);

G_END_DECLS

#endif
