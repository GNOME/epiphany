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

#ifndef EPHY_TOOLBAR_BONOBO_VIEW_H
#define EPHY_TOOLBAR_BONOBO_VIEW_H

#include <glib-object.h>

#include <bonobo/bonobo-ui-component.h>

#include "ephy-toolbar.h"

G_BEGIN_DECLS

/* object forward declarations */

typedef struct _EphyTbBonoboView EphyTbBonoboView;
typedef struct _EphyTbBonoboViewClass EphyTbBonoboViewClass;
typedef struct _EphyTbBonoboViewPrivate EphyTbBonoboViewPrivate;

/**
 * TbBonoboView object
 */

#define EPHY_TYPE_TB_BONOBO_VIEW		(ephy_tb_bonobo_view_get_type())
#define EPHY_TB_BONOBO_VIEW(object)		(G_TYPE_CHECK_INSTANCE_CAST((object), \
						 EPHY_TYPE_TB_BONOBO_VIEW,\
						 EphyTbBonoboView))
#define EPHY_TB_BONOBO_VIEW_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass), EPHY_TYPE_TB_BONOBO_VIEW,\
						 EphyTbBonoboViewClass))
#define EPHY_IS_TB_BONOBO_VIEW(object)		(G_TYPE_CHECK_INSTANCE_TYPE((object), \
						 EPHY_TYPE_TB_BONOBO_VIEW))
#define EPHY_IS_TB_BONOBO_VIEW_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), EPHY_TYPE_TB_BONOBO_VIEW))
#define EPHY_TB_BONOBO_VIEW_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), EPHY_TYPE_TB_BONOBO_VIEW,\
						 EphyTbBonoboViewClass))

struct _EphyTbBonoboViewClass
{
	GObjectClass parent_class;
};

/* Remember: fields are public read-only */
struct _EphyTbBonoboView
{
	GObject parent_object;

	EphyTbBonoboViewPrivate *priv;
};

/* this class is abstract */

GType			ephy_tb_bonobo_view_get_type		(void);
EphyTbBonoboView *	ephy_tb_bonobo_view_new			(void);
void			ephy_tb_bonobo_view_set_toolbar		(EphyTbBonoboView *tbv, EphyToolbar *tb);
void			ephy_tb_bonobo_view_set_path		(EphyTbBonoboView *tbv,
								 BonoboUIComponent *ui,
								 const gchar *path);

#endif

