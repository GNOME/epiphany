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

#ifndef EPHY_LOCATION_ACTION_H
#define EPHY_LOCATION_ACTION_H

#include "ephy-link-action.h"

G_BEGIN_DECLS

#define EPHY_TYPE_LOCATION_ACTION            (ephy_location_action_get_type ())
#define EPHY_LOCATION_ACTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_LOCATION_ACTION, EphyLocationAction))
#define EPHY_LOCATION_ACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_LOCATION_ACTION, EphyLocationActionClass))
#define EPHY_IS_LOCATION_ACTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_LOCATION_ACTION))
#define EPHY_IS_LOCATION_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), EPHY_TYPE_LOCATION_ACTION))
#define EPHY_LOCATION_ACTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), EPHY_TYPE_LOCATION_ACTION, EphyLocationActionClass))

typedef struct _EphyLocationAction		EphyLocationAction;
typedef struct _EphyLocationActionPrivate	EphyLocationActionPrivate;
typedef struct _EphyLocationActionClass		EphyLocationActionClass;

struct _EphyLocationAction
{
	EphyLinkAction parent;

	/*< private >*/
	EphyLocationActionPrivate *priv;
};

struct _EphyLocationActionClass
{
	EphyLinkActionClass parent_class;

	/* Signals */
	void (* lock_clicked)	(EphyLocationAction *action);
};

GType		ephy_location_action_get_type		(void);

const char     *ephy_location_action_get_address	(EphyLocationAction *action);

void		ephy_location_action_set_address	(EphyLocationAction *action,
							 const char *address,
							 const char *typed_address);

G_END_DECLS

#endif
