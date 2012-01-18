/*
 *  Copyright © 2003 Marco Pesenti Gritti
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

#ifndef EPHY_LOCATION_CONTROLLER_H
#define EPHY_LOCATION_CONTROLLER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define EPHY_TYPE_LOCATION_CONTROLLER            (ephy_location_controller_get_type ())
#define EPHY_LOCATION_CONTROLLER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_LOCATION_CONTROLLER, EphyLocationController))
#define EPHY_LOCATION_CONTROLLER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_LOCATION_CONTROLLER, EphyLocationControllerClass))
#define EPHY_IS_LOCATION_CONTROLLER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_LOCATION_CONTROLLER))
#define EPHY_IS_LOCATION_CONTROLLER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), EPHY_TYPE_LOCATION_CONTROLLER))
#define EPHY_LOCATION_CONTROLLER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), EPHY_TYPE_LOCATION_CONTROLLER, EphyLocationControllerClass))

typedef struct _EphyLocationController		EphyLocationController;
typedef struct _EphyLocationControllerPrivate	EphyLocationControllerPrivate;
typedef struct _EphyLocationControllerClass	EphyLocationControllerClass;

struct _EphyLocationController
{
	GObject parent;

	/*< private >*/
	EphyLocationControllerPrivate *priv;
};

struct _EphyLocationControllerClass
{
	GObjectClass parent_class;

	/* Signals */
	void (* lock_clicked)	(EphyLocationController *controller);
};

GType		ephy_location_controller_get_type		(void);

const char     *ephy_location_controller_get_address	(EphyLocationController *controller);

void		ephy_location_controller_set_address	(EphyLocationController *controller,
							 const char *address);

G_END_DECLS

#endif
