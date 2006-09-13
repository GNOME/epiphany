/*
 *  Copyright © 2004 Christian Persch
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
 *
 *  $Id$
 */

#ifndef EPHY_HOME_ACTION_H
#define EPHY_HOME_ACTION_H

#include "ephy-link-action.h"

G_BEGIN_DECLS

#define EPHY_TYPE_HOME_ACTION            (ephy_home_action_get_type ())
#define EPHY_HOME_ACTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_HOME_ACTION, EphyHomeAction))
#define EPHY_HOME_ACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_HOME_ACTION, EphyHomeActionClass))
#define EPHY_IS_HOME_ACTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_HOME_ACTION))
#define EPHY_IS_HOME_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), EPHY_TYPE_HOME_ACTION))
#define EPHY_HOME_ACTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), EPHY_TYPE_HOME_ACTION, EphyHomeActionClass))

typedef struct _EphyHomeAction      EphyHomeAction;
typedef struct _EphyHomeActionClass EphyHomeActionClass;

struct _EphyHomeAction
{
	EphyLinkAction parent_instance;
};

struct _EphyHomeActionClass
{
	EphyLinkActionClass parent_class;
};

GType ephy_home_action_get_type (void);

G_END_DECLS

#endif /* EPHY_HOME_ACTION_H */
