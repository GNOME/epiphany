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
 */

#ifndef EPHY_TOOLBARS_GROUP_H
#define EPHY_TOOLBARS_GROUP_H

#include <gtk/gtkwidget.h>
#include <libxml/parser.h>

G_BEGIN_DECLS

typedef struct EphyToolbarsGroupClass EphyToolbarsGroupClass;

#define EPHY_TOOLBARS_GROUP_TYPE             (ephy_toolbars_group_get_type ())
#define EPHY_TOOLBARS_GROUP(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TOOLBARS_GROUP_TYPE, EphyToolbarsGroup))
#define EPHY_TOOLBARS_GROUP_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TOOLBARS_GROUP_TYPE, EphyToolbarsGroupClass))
#define IS_EPHY_TOOLBARS_GROUP(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TOOLBARS_GROUP_TYPE))
#define IS_EPHY_TOOLBARS_GROUP_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), EPHY_TOOLBARS_GROUP_TYPE))
#define EPHY_TOOLBARS_GROUP_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), EPHY_TOOLBARS_GROUP_TYPE, EphyToolbarsGroupClass))


typedef struct EphyToolbarsGroup EphyToolbarsGroup;
typedef struct EphyToolbarsGroupPrivate EphyToolbarsGroupPrivate;

typedef struct
{
	char *id;
} EphyToolbarsToolbar;

typedef struct
{
	char *id;
	gboolean separator;
	char *action;
	EphyToolbarsToolbar *parent;
} EphyToolbarsItem;

typedef void (* EphyToolbarsGroupForeachToolbarFunc)     (EphyToolbarsToolbar *toolbar,
							  gpointer data);
typedef void (* EphyToolbarsGroupForeachItemFunc)        (EphyToolbarsItem *item,
							  gpointer data);

struct EphyToolbarsGroup
{
        GObject parent_object;
        EphyToolbarsGroupPrivate *priv;
};

struct EphyToolbarsGroupClass
{
        GObjectClass parent_class;

	void (* changed) (EphyToolbarsGroup *group);
};

GType			ephy_toolbars_group_get_type		(void);

EphyToolbarsGroup      *ephy_toolbars_group_new			(void);

void			ephy_toolbars_group_set_source		(EphyToolbarsGroup *group,
								 const char *defaults,
								 const char *user);

EphyToolbarsToolbar    *ephy_toolbars_group_add_toolbar		(EphyToolbarsGroup *t);

void			ephy_toolbars_group_add_item		(EphyToolbarsGroup *t,
								 EphyToolbarsToolbar *parent,
								 EphyToolbarsItem *sibling,
								 const char *name);

void			ephy_toolbars_group_remove_toolbar	(EphyToolbarsGroup *t,
								 EphyToolbarsToolbar *toolbar);

void			ephy_toolbars_group_remove_item		(EphyToolbarsGroup *t,
								 EphyToolbarsItem *item);

void			ephy_toolbars_group_foreach_available	(EphyToolbarsGroup *group,
								 EphyToolbarsGroupForeachItemFunc func,
								 gpointer data);

void			ephy_toolbars_group_foreach_toolbar	(EphyToolbarsGroup *group,
								 EphyToolbarsGroupForeachToolbarFunc func,
								 gpointer data);

void			ephy_toolbars_group_foreach_item	(EphyToolbarsGroup *group,
								 EphyToolbarsGroupForeachItemFunc func,
								 gpointer data);

char		       *ephy_toolbars_group_to_string		(EphyToolbarsGroup *t);

char		       *ephy_toolbars_group_get_path		(EphyToolbarsGroup *t,
								 gpointer item);

G_END_DECLS

#endif
