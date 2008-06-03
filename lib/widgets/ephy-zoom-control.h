/*
 *  Copyright © 2003  Christian Persch
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
 *  $Id$
 */

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef EPHY_ZOOM_CONTROL_H
#define EPHY_ZOOM_CONTROL_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_ZOOM_CONTROL			(ephy_zoom_control_get_type())
#define EPHY_ZOOM_CONTROL(object)		(G_TYPE_CHECK_INSTANCE_CAST((object), EPHY_TYPE_ZOOM_CONTROL, EphyZoomControl))
#define EPHY_ZOOM_CONTROL_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), EPHY_TYPE_ZOOM_CONTROL, EphyZoomControlClass))
#define EPHY_IS_ZOOM_CONTROL(object)		(G_TYPE_CHECK_INSTANCE_TYPE((object), EPHY_TYPE_ZOOM_CONTROL))
#define EPHY_IS_ZOOM_CONTROL_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), EPHY_TYPE_ZOOM_CONTROL))
#define EPHY_ZOOM_CONTROL_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), EPHY_TYPE_ZOOM_CONTROL, EphyZoomControlClass))

typedef struct _EphyZoomControl		EphyZoomControl;
typedef struct _EphyZoomControlClass	EphyZoomControlClass;
typedef struct _EphyZoomControlPrivate	EphyZoomControlPrivate;

struct _EphyZoomControlClass
{
	GtkToolItemClass parent_class;

	/* signals */
	void (*zoom_to_level) 	(EphyZoomControl *control, float level);
};

struct _EphyZoomControl
{
	GtkToolItem parent_object;

	/*< private >*/
	EphyZoomControlPrivate *priv;
};

GType	ephy_zoom_control_get_type	 (void);

void	ephy_zoom_control_set_zoom_level (EphyZoomControl *control, float zoom);

float	ephy_zoom_control_get_zoom_level (EphyZoomControl *control);

G_END_DECLS

#endif
