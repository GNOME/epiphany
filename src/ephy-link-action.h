/*
 *  Copyright (C) 2004 Christian Persch
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

#ifndef EPHY_LINK_ACTION_H
#define EPHY_LINK_ACTION_H

#include <gtk/gtkaction.h>

G_BEGIN_DECLS

#define EPHY_TYPE_LINK_ACTION		(ephy_link_action_get_type ())
#define EPHY_LINK_ACTION(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_LINK_ACTION, EphyLinkAction))
#define EPHY_LINK_ACTION_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_LINK_ACTION, EphyLinkActionClass))
#define EPHY_IS_LINK_ACTION(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_LINK_ACTION))
#define EPHY_IS_LINK_ACTION_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_LINK_ACTION))
#define EPHY_LINK_ACTION_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_LINK_ACTION, EphyLinkActionClass))

typedef struct _EphyLinkAction		EphyLinkAction;
typedef struct _EphyLinkActionClass	EphyLinkActionClass;

struct _EphyLinkAction
{
	GtkAction parent_instance;
};

struct _EphyLinkActionClass
{
	GtkActionClass parent_class;
};

GType ephy_link_action_get_type (void);

G_END_DECLS

#endif
