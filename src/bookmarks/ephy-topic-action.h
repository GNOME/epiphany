/*
 *  Copyright (C) 2003 Marco Pesenti Gritti
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

#ifndef EPHY_TOPIC_ACTION_H
#define EPHY_TOPIC_ACTION_H

#include <gtk/gtk.h>
#include <gtk/gtkaction.h>

#define EPHY_TYPE_TOPIC_ACTION            (ephy_topic_action_get_type ())
#define EPHY_TOPIC_ACTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_TOPIC_ACTION, EphyTopicAction))
#define EPHY_TOPIC_ACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_TOPIC_ACTION, EphyTopicActionClass))
#define EPHY_IS_TOPIC_ACTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_TOPIC_ACTION))
#define EPHY_IS_TOPIC_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), EPHY_TYPE_TOPIC_ACTION))
#define EPHY_TOPIC_ACTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), EPHY_TYPE_TOPIC_ACTION, EphyTopicActionClass))

typedef struct _EphyTopicAction       EphyTopicAction;
typedef struct _EphyTopicActionClass  EphyTopicActionClass;
typedef struct EphyTopicActionPrivate EphyTopicActionPrivate;

struct _EphyTopicAction
{
	GtkAction parent;

	/*< private >*/
	EphyTopicActionPrivate *priv;
};

struct _EphyTopicActionClass
{
	GtkActionClass parent_class;

	void (*open)         (EphyTopicAction *action,
			      char *address);
	void (*open_in_tab)  (EphyTopicAction *action,
			      char *address,
			      gboolean new_window);
	void (*open_in_tabs) (EphyTopicAction *action,
			      GList *uri_list);
};

GType      ephy_topic_action_get_type	(void);

GtkAction *ephy_topic_action_new	(const char *name,
					 guint id);

#endif
