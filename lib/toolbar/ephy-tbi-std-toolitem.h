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

#ifndef EPHY_TBI_STD_TOOLITEM_H
#define EPHY_TBI_STD_TOOLITEM_H

#include "ephy-toolbar-item.h"

G_BEGIN_DECLS

/* object forward declarations */

typedef struct _EphyTbiStdToolitem EphyTbiStdToolitem;
typedef struct _EphyTbiStdToolitemClass EphyTbiStdToolitemClass;
typedef struct _EphyTbiStdToolitemPrivate EphyTbiStdToolitemPrivate;

/**
 * TbiStdToolitem object
 */

#define EPHY_TYPE_TBI_STD_TOOLITEM		(ephy_tbi_std_toolitem_get_type())
#define EPHY_TBI_STD_TOOLITEM(object)		(G_TYPE_CHECK_INSTANCE_CAST((object), \
						 EPHY_TYPE_TBI_STD_TOOLITEM,\
						 EphyTbiStdToolitem))
#define EPHY_TBI_STD_TOOLITEM_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass), \
						 EPHY_TYPE_TBI_STD_TOOLITEM,\
						 EphyTbiStdToolitemClass))
#define EPHY_IS_TBI_STD_TOOLITEM(object)	(G_TYPE_CHECK_INSTANCE_TYPE((object), \
						 EPHY_TYPE_TBI_STD_TOOLITEM))
#define EPHY_IS_TBI_STD_TOOLITEM_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), \
						 EPHY_TYPE_TBI_STD_TOOLITEM))
#define EPHY_TBI_STD_TOOLITEM_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), \
						 EPHY_TYPE_TBI_STD_TOOLITEM,\
						 EphyTbiStdToolitemClass))
typedef enum
{
	EPHY_TBI_STD_TOOLITEM_STOP,
	EPHY_TBI_STD_TOOLITEM_RELOAD,
	EPHY_TBI_STD_TOOLITEM_HOME,
	EPHY_TBI_STD_TOOLITEM_GO,
	EPHY_TBI_STD_TOOLITEM_NEW,
	EPHY_TBI_STD_TOOLITEM_ERROR
} EphyTbiStdToolitemItem;


struct _EphyTbiStdToolitemClass
{
	EphyTbItemClass parent_class;
};

/* Remember: fields are public read-only */
struct _EphyTbiStdToolitem
{
	EphyTbItem parent_object;

	EphyTbiStdToolitemPrivate *priv;
};

/* this class is abstract */

GType			ephy_tbi_std_toolitem_get_type		(void);
EphyTbiStdToolitem *	ephy_tbi_std_toolitem_new		(void);
void			ephy_tbi_std_toolitem_set_item		(EphyTbiStdToolitem *sit,
								 EphyTbiStdToolitemItem it);

G_END_DECLS

#endif

