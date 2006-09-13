/*
 *  Copyright © 2002 Jorn Baayen
 *  Copyright © 2003-2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2005 Christian Persch
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

#ifndef EPHY_TOOLBAR_H
#define EPHY_TOOLBAR_H

#include <glib.h>
#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "egg-editable-toolbar.h"
#include "ephy-window.h"

G_BEGIN_DECLS

#define EPHY_TYPE_TOOLBAR		(ephy_toolbar_get_type ())
#define EPHY_TOOLBAR(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_TOOLBAR, EphyToolbar))
#define EPHY_TOOLBAR_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_TOOLBAR, EphyToolbarClass))
#define EPHY_IS_TOOLBAR(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_TOOLBAR))
#define EPHY_IS_TOOLBAR_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_TOOLBAR))
#define EPHY_TOOLBAR_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_TOOLBAR, EphyToolbarClass))

typedef struct _EphyToolbar		EphyToolbar;
typedef struct _EphyToolbarClass	EphyToolbarClass;
typedef struct _EphyToolbarPrivate	EphyToolbarPrivate;

struct _EphyToolbar
{
	EggEditableToolbar parent_object;

	/*< private >*/
	EphyToolbarPrivate *priv;
};

struct _EphyToolbarClass
{
	EggEditableToolbarClass parent_class;

	/* Signals */
	void (* activation_finished)	(EphyToolbar *toolbar);
	void (* exit_clicked)		(EphyToolbar *toolbar);
	void (* lock_clicked)		(EphyToolbar *toolbar);
};

GType		ephy_toolbar_get_type			(void);

EphyToolbar    *ephy_toolbar_new			(EphyWindow *window);

GtkActionGroup *ephy_toolbar_get_action_group		(EphyToolbar *toolbar);

void		ephy_toolbar_set_favicon		(EphyToolbar *toolbar,
							 GdkPixbuf *icon);

void		ephy_toolbar_set_show_leave_fullscreen	(EphyToolbar *toolbar,
							 gboolean show);

void		ephy_toolbar_activate_location		(EphyToolbar *t);

const char     *ephy_toolbar_get_location		(EphyToolbar *t);

void		ephy_toolbar_set_location		(EphyToolbar *toolbar,
							 const char *address,
							 const char *typed_address);

void		ephy_toolbar_set_navigation_actions	(EphyToolbar *toolbar,
							 gboolean back,
							 gboolean forward,
							 gboolean up);

void		ephy_toolbar_set_security_state		(EphyToolbar *toolbar,
							 gboolean is_secure,
							 gboolean show_lock,
							 const char *stock_id,
							 const char *tooltip);

void		ephy_toolbar_set_spinning		(EphyToolbar *toolbar,
							 gboolean spinning);

void		ephy_toolbar_set_zoom			(EphyToolbar *toolbar,
							 gboolean can_zoom,
							 float zoom);

G_END_DECLS

#endif /* !EPHY_TOOLBAR_H */
