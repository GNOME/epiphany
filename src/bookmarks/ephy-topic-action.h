/*
 *  Copyright © 2003, 2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
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

#ifndef EPHY_TOPIC_ACTION_H
#define EPHY_TOPIC_ACTION_H

#include "ephy-link-action.h"
#include "ephy-node.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_TOPIC_ACTION			(ephy_topic_action_get_type ())
#define EPHY_TOPIC_ACTION(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_TOPIC_ACTION, EphyTopicAction))
#define EPHY_TOPIC_ACTION_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_TOPIC_ACTION, EphyTopicActionClass))
#define EPHY_IS_TOPIC_ACTION(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_TOPIC_ACTION))
#define EPHY_IS_TOPIC_ACTION_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((obj), EPHY_TYPE_TOPIC_ACTION))
#define EPHY_TOPIC_ACTION_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), EPHY_TYPE_TOPIC_ACTION, EphyTopicActionClass))

typedef struct _EphyTopicAction			EphyTopicAction;
typedef struct _EphyTopicActionPrivate		EphyTopicActionPrivate;
typedef struct _EphyTopicActionClass		EphyTopicActionClass;

struct _EphyTopicAction
{
	GtkAction parent_instance;

	/*< private >*/
	EphyTopicActionPrivate *priv;
};

struct _EphyTopicActionClass
{
	EphyLinkActionClass parent_class;
};


GType		ephy_topic_action_get_type	(void);

GtkAction      *ephy_topic_action_new		(EphyNode *node,
						 GtkUIManager *manager,
						 const char *name);

void		ephy_topic_action_set_topic	(EphyTopicAction *action,
						 EphyNode *node);

EphyNode       *ephy_topic_action_get_topic	(EphyTopicAction *action);

void		ephy_topic_action_updated	(EphyTopicAction *action);

G_END_DECLS

#endif
