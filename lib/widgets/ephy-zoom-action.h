/*
 *  Copyright © 2003 Marco Pesenti Gritti
 *  Copyright © 2003 Christian Persch
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef EPHY_ZOOM_ACTION_H
#define EPHY_ZOOM_ACTION_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_ZOOM_ACTION            (ephy_zoom_action_get_type ())
#define EPHY_ZOOM_ACTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_ZOOM_ACTION, EphyZoomAction))
#define EPHY_ZOOM_ACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_ZOOM_ACTION, EphyZoomActionClass))
#define EPHY_IS_ZOOM_ACTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_ZOOM_ACTION))
#define EPHY_IS_ZOOM_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), EPHY_TYPE_ZOOM_ACTION))
#define EPHY_ZOOM_ACTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), EPHY_TYPE_ZOOM_ACTION, EphyZoomActionClass))

typedef struct _EphyZoomAction		EphyZoomAction;
typedef struct _EphyZoomActionClass	EphyZoomActionClass;
typedef struct _EphyZoomActionPrivate	EphyZoomActionPrivate;

struct _EphyZoomAction
{
	GtkAction parent;
	
	/*< private >*/
	EphyZoomActionPrivate *priv;
};

struct _EphyZoomActionClass
{
	GtkActionClass parent_class;

	void (* zoom_to_level)	(EphyZoomAction *action, float level);
};

GType	ephy_zoom_action_get_type	(void);

void	ephy_zoom_action_set_zoom_level	(EphyZoomAction *action, float zoom);

float	ephy_zoom_action_get_zoom_level	(EphyZoomAction *action);

G_END_DECLS

#endif
