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

#ifndef EPHY_TBI_H
#define EPHY_TBI_H

#include "ephy-toolbar-item.h"
#include "ephy-window.h"

G_BEGIN_DECLS

/* object forward declarations */

typedef struct _EphyTbi EphyTbi;
typedef struct _EphyTbiClass EphyTbiClass;
typedef struct _EphyTbiPrivate EphyTbiPrivate;

/**
 * Tbi object
 */

#define EPHY_TYPE_TBI			(ephy_tbi_get_type())
#define EPHY_TBI(object)		(G_TYPE_CHECK_INSTANCE_CAST((object), EPHY_TYPE_TBI,\
					 EphyTbi))
#define EPHY_TBI_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), EPHY_TYPE_TBI,\
					 EphyTbiClass))
#define EPHY_IS_TBI_(object)		(G_TYPE_CHECK_INSTANCE_TYPE((object), EPHY_TYPE_TBI))
#define EPHY_IS_TBI_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), EPHY_TYPE_TBI))
#define EPHY_TBI_GET_CLASS(obj)		(G_TYPE_INSTANCE_GET_CLASS((obj), EPHY_TYPE_TBI,\
					 EphyTbiClass))

struct _EphyTbiClass
{
	EphyTbItemClass parent_class;

};

/* Remember: fields are public read-only */
struct _EphyTbi
{
	EphyTbItem parent_object;

	EphyWindow *window;
};

/* this class is abstract */

GType		ephy_tbi_get_type		(void);
EphyTbi *	ephy_tbi_new			(void);
void		ephy_tbi_set_window		(EphyTbi *tbi, EphyWindow *w);
EphyWindow *	ephy_tbi_get_window		(EphyTbi *tbi);

G_END_DECLS

#endif
