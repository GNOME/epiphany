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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef EPHY_LINK_ACTION_H
#define EPHY_LINK_ACTION_H

#include "ephy-window-action.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_LINK_ACTION			(ephy_link_action_get_type ())
#define EPHY_LINK_ACTION(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_LINK_ACTION, EphyLinkAction))
#define EPHY_LINK_ACTION_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_LINK_ACTION, EphyLinkActionClass))
#define EPHY_IS_LINK_ACTION(o)			(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_LINK_ACTION))
#define EPHY_IS_LINK_ACTION_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_LINK_ACTION))
#define EPHY_LINK_ACTION_GET_CLASS(o)		(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_LINK_ACTION, EphyLinkActionClass))

#define EPHY_TYPE_LINK_ACTION_GROUP		(ephy_link_action_group_get_type ())
#define EPHY_LINK_ACTION_GROUP(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_LINK_ACTION_GROUP, EphyLinkActionGroup))
#define EPHY_LINK_ACTION_GROUP_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_LINK_ACTION_GROUP, EphyLinkActionGroupClass))
#define EPHY_IS_LINK_ACTION_GROUP(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_LINK_ACTION_GROUP))
#define EPHY_IS_LINK_ACTION_GROUP_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_LINK_ACTION_GROUP))
#define EPHY_LINK_ACTION_GROUP_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_LINK_ACTION_GROUP, EphyLinkActionGroupClass))

typedef struct _EphyLinkAction			EphyLinkAction;
typedef struct _EphyLinkActionClass		EphyLinkActionClass;
typedef struct _EphyLinkActionPrivate		EphyLinkActionPrivate;

typedef struct _EphyLinkActionGroup		EphyLinkActionGroup;
typedef struct _EphyLinkActionGroupClass	EphyLinkActionGroupClass;

struct _EphyLinkAction
{
	EphyWindowAction parent_instance;

	EphyLinkActionPrivate *priv;
};

struct _EphyLinkActionClass
{
	EphyWindowActionClass parent_class;
};

struct _EphyLinkActionGroup
{
	GtkActionGroup parent_instance;
};

struct _EphyLinkActionGroupClass
{
	GtkActionGroupClass parent_class;
};

GType ephy_link_action_get_type	  (void);
guint ephy_link_action_get_button (EphyLinkAction *action);

GType ephy_link_action_group_get_type (void);

EphyLinkActionGroup * ephy_link_action_group_new (const char *name);

G_END_DECLS

#endif
