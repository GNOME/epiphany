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

#ifndef EGG_TOOLBARS_GROUP_H
#define EGG_TOOLBARS_GROUP_H

#include <gtk/gtkwidget.h>
#include <libxml/parser.h>

G_BEGIN_DECLS

typedef struct EggToolbarsGroupClass EggToolbarsGroupClass;

#define EGG_TOOLBARS_GROUP_TYPE             (egg_toolbars_group_get_type ())
#define EGG_TOOLBARS_GROUP(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), EGG_TOOLBARS_GROUP_TYPE, EggToolbarsGroup))
#define EGG_TOOLBARS_GROUP_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), EGG_TOOLBARS_GROUP_TYPE, EggToolbarsGroupClass))
#define IS_EGG_TOOLBARS_GROUP(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EGG_TOOLBARS_GROUP_TYPE))
#define IS_EGG_TOOLBARS_GROUP_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), EGG_TOOLBARS_GROUP_TYPE))
#define EGG_TOOLBARS_GROUP_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), EGG_TOOLBARS_GROUP_TYPE, EggToolbarsGroupClass))


typedef struct EggToolbarsGroup EggToolbarsGroup;
typedef struct EggToolbarsGroupPrivate EggToolbarsGroupPrivate;

typedef struct
{
  char *id;
} EggToolbarsToolbar;

typedef struct
{
  char *id;
  gboolean separator;
  char *action;
  EggToolbarsToolbar *parent;
} EggToolbarsItem;

typedef void (*EggToolbarsGroupForeachToolbarFunc) (EggToolbarsToolbar *toolbar,
						    gpointer            data);
typedef void (*EggToolbarsGroupForeachItemFunc)    (EggToolbarsItem *item,
						    gpointer         data);

struct EggToolbarsGroup
{
  GObject parent_object;
  EggToolbarsGroupPrivate *priv;
};

struct EggToolbarsGroupClass
{
  GObjectClass parent_class;

  void (*changed) (EggToolbarsGroup * group);
};

GType                   egg_toolbars_group_get_type          (void);
EggToolbarsGroup       *egg_toolbars_group_new		     (void);
void			egg_toolbars_group_set_source        (EggToolbarsGroup                  *group,
						              const char                        *defaults,
							      const char                        *user);
void			egg_toolbars_group_remove_action     (EggToolbarsGroup                  *group,
							      const char			*action);

/* These should be used only by editable toolbar */
EggToolbarsToolbar     *egg_toolbars_group_add_toolbar       (EggToolbarsGroup                  *t);
void			egg_toolbars_group_add_item          (EggToolbarsGroup                  *t,
						              EggToolbarsToolbar                *parent,
				                              int                                position,
							      const char                        *name);
void			egg_toolbars_group_remove_toolbar    (EggToolbarsGroup                  *t,
							      EggToolbarsToolbar                *toolbar);
void			egg_toolbars_group_remove_item	     (EggToolbarsGroup                  *t,
							      EggToolbarsItem                   *item);
void			egg_toolbars_group_foreach_available (EggToolbarsGroup                  *group,
							      EggToolbarsGroupForeachItemFunc    func,
							      gpointer                           data);
void			egg_toolbars_group_foreach_toolbar   (EggToolbarsGroup * group,
					                      EggToolbarsGroupForeachToolbarFunc func,
							      gpointer				 data);
void			egg_toolbars_group_foreach_item      (EggToolbarsGroup                  *group,
							      EggToolbarsGroupForeachItemFunc    func,
							      gpointer                           data);
char		       *egg_toolbars_group_to_string	     (EggToolbarsGroup                  *t);
char		       *egg_toolbars_group_get_path	     (EggToolbarsGroup                  *t,
							      gpointer                           item);

G_END_DECLS
#endif
