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

#ifndef EPHY_TBI_LOCATION_H
#define EPHY_TBI_LOCATION_H

#include "ephy-toolbar-item.h"

G_BEGIN_DECLS

/* object forward declarations */

typedef struct _EphyTbiLocation EphyTbiLocation;
typedef struct _EphyTbiLocationClass EphyTbiLocationClass;
typedef struct _EphyTbiLocationPrivate EphyTbiLocationPrivate;

/**
 * TbiLocation object
 */

#define EPHY_TYPE_TBI_LOCATION			(ephy_tbi_location_get_type())
#define EPHY_TBI_LOCATION(object)		(G_TYPE_CHECK_INSTANCE_CAST((object), EPHY_TYPE_TBI_LOCATION,\
						 EphyTbiLocation))
#define EPHY_TBI_LOCATION_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), EPHY_TYPE_TBI_LOCATION,\
						 EphyTbiLocationClass))
#define EPHY_IS_TBI_LOCATION(object)		(G_TYPE_CHECK_INSTANCE_TYPE((object), EPHY_TYPE_TBI_LOCATION))
#define EPHY_IS_TBI_LOCATION_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), EPHY_TYPE_TBI_LOCATION))
#define EPHY_TBI_LOCATION_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), EPHY_TYPE_TBI_LOCATION,\
						 EphyTbiLocationClass))

struct _EphyTbiLocationClass
{
	EphyTbItemClass parent_class;
};

/* Remember: fields are public read-only */
struct _EphyTbiLocation
{
	EphyTbItem parent_object;

	EphyTbiLocationPrivate *priv;
};

/* this class is abstract */

GType			ephy_tbi_location_get_type	(void);
EphyTbiLocation *	ephy_tbi_location_new		(void);

G_END_DECLS

#endif
