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

#ifndef EPHY_TBI_FAVICON_H
#define EPHY_TBI_FAVICON_H

#include "ephy-toolbar-item.h"

G_BEGIN_DECLS

/* object forward declarations */

typedef struct _EphyTbiFavicon EphyTbiFavicon;
typedef struct _EphyTbiFaviconClass EphyTbiFaviconClass;
typedef struct _EphyTbiFaviconPrivate EphyTbiFaviconPrivate;

/**
 * TbiFavicon object
 */

#define EPHY_TYPE_TBI_FAVICON		 (ephy_tbi_favicon_get_type())
#define EPHY_TBI_FAVICON(object)	 (G_TYPE_CHECK_INSTANCE_CAST((object), EPHY_TYPE_TBI_FAVICON,\
					  EphyTbiFavicon))
#define EPHY_TBI_FAVICON_CLASS(klass)	 (G_TYPE_CHECK_CLASS_CAST((klass), EPHY_TYPE_TBI_FAVICON,\
					  EphyTbiFaviconClass))
#define EPHY_IS_TBI_FAVICON(object)	 (G_TYPE_CHECK_INSTANCE_TYPE((object), EPHY_TYPE_TBI_FAVICON))
#define EPHY_IS_TBI_FAVICON_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), EPHY_TYPE_TBI_FAVICON))
#define EPHY_TBI_FAVICON_GET_CLASS(obj)	 (G_TYPE_INSTANCE_GET_CLASS((obj), EPHY_TYPE_TBI_FAVICON,\
					  EphyTbiFaviconClass))

struct _EphyTbiFaviconClass
{
	EphyTbItemClass parent_class;
};

/* Remember: fields are public read-only */
struct _EphyTbiFavicon
{
	EphyTbItem parent_object;

	EphyTbiFaviconPrivate *priv;
};

/* this class is abstract */

GType			ephy_tbi_favicon_get_type		(void);
EphyTbiFavicon *	ephy_tbi_favicon_new			(void);

G_END_DECLS

#endif
