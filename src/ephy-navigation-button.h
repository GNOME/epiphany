/*
 *  Copyright (C) 2002  Ricardo Fernández Pascual
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

#ifndef EPHY_NAVIGATION_BUTTON_H
#define EPHY_NAVIGATION_BUTTON_H

#include "ephy-tbi.h"

G_BEGIN_DECLS

/* object forward declarations */

typedef struct _EphyNavigationButton EphyNavigationButton;
typedef struct _EphyNavigationButtonClass EphyNavigationButtonClass;
typedef struct _EphyNavigationButtonPrivate EphyNavigationButtonPrivate;

/**
 * TbiZoom object
 */

#define EPHY_TYPE_NAVIGATION_BUTTON			(ephy_navigation_button_get_type())
#define EPHY_NAVIGATION_BUTTON(object)			(G_TYPE_CHECK_INSTANCE_CAST((object), \
							 EPHY_TYPE_NAVIGATION_BUTTON, EphyNavigationButton))
#define EPHY_NAVIGATION_BUTTON_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), EPHY_TYPE_NAVIGATION_BUTTON,\
							 EphyNavigationButtonClass))
#define EPHY_IS_NAVIGATION_BUTTON(object)		(G_TYPE_CHECK_INSTANCE_TYPE((object), \
							 EPHY_TYPE_NAVIGATION_BUTTON))
#define EPHY_IS_NAVIGATION_BUTTON_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE((klass), EPHY_TYPE_NAVIGATION_BUTTON))
#define EPHY_NAVIGATION_BUTTON_GET_CLASS(obj)		(G_TYPE_INSTANCE_GET_CLASS((obj), EPHY_TYPE_NAVIGATION_BUTTON,\
							 EphyNavigationButtonClass))

typedef enum
{
	EPHY_NAVIGATION_DIRECTION_UP,
	EPHY_NAVIGATION_DIRECTION_BACK,
	EPHY_NAVIGATION_DIRECTION_FORWARD
} EphyNavigationDirection;

struct _EphyNavigationButtonClass
{
	EphyTbiClass parent_class;
};

/* Remember: fields are public read-only */
struct _EphyNavigationButton
{
	EphyTbi parent_object;
	EphyNavigationButtonPrivate *priv;
};

/* this class is abstract */

GType				ephy_navigation_button_get_type		(void);
EphyNavigationButton *		ephy_navigation_button_new		(void);
void				ephy_navigation_button_set_direction	(EphyNavigationButton *a,
									 EphyNavigationDirection d);
void				ephy_navigation_button_set_show_arrow	(EphyNavigationButton *b,
									 gboolean value);
EphyNavigationDirection		ephy_navigation_button_get_direction	(EphyNavigationButton *b);
void				ephy_navigation_button_set_sensitive	(EphyNavigationButton *b, gboolean s);

G_END_DECLS

#endif
