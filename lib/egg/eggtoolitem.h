/* eggtoolitem.c
 *
 * Copyright (C) 2002 Anders Carlsson <andersca@codefactory.se>
 * Copyright (C) 2002 James Henstridge <james@daa.com.au>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __EGG_TOOL_ITEM_H__
#define __EGG_TOOL_ITEM_H__

#include <gtk/gtkbin.h>
#include <gtk/gtktooltips.h>

#define EGG_TYPE_TOOL_ITEM            (egg_tool_item_get_type ())
#define EGG_TOOL_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EGG_TYPE_TOOL_ITEM, EggToolItem))
#define EGG_TOOL_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EGG_TYPE_TOOL_ITEM, EggToolItemClass))
#define EGG_IS_TOOL_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EGG_TYPE_TOOL_ITEM))
#define EGG_IS_TOOL_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), EGG_TYPE_TOOL_ITEM))
#define EGG_TOOL_ITEM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), EGG_TYPE_TOOL_ITEM, EggToolItemClass))

typedef struct _EggToolItem      EggToolItem;
typedef struct _EggToolItemClass EggToolItemClass;

struct _EggToolItem
{
  GtkBin parent;

  GtkOrientation orientation;
  GtkIconSize icon_size;
  GtkToolbarStyle style;

  gchar *tip_text;
  gchar *tip_private;
  
  guint visible_horizontal : 1;
  guint visible_vertical : 1;
  guint homogeneous : 1;
  guint expandable : 1;
  guint pack_end : 1;
};

struct _EggToolItemClass
{
  GtkBinClass parent_class;

  void       (* clicked)             (EggToolItem    *tool_item);
  GtkWidget *(* create_menu_proxy)   (EggToolItem    *tool_item);
  void       (* set_orientation)     (EggToolItem    *tool_item,
				      GtkOrientation  orientation);
  void       (* set_icon_size)       (EggToolItem    *tool_item,
				      GtkIconSize     icon_size);
  void       (* set_toolbar_style)   (EggToolItem    *tool_item,
				      GtkToolbarStyle style);
  void       (* set_relief_style)    (EggToolItem    *tool_item,
				      GtkReliefStyle  relief_style);
  void	     (* set_tooltip)	     (EggToolItem    *tool_item,
				      GtkTooltips    *tooltips,
				      const gchar    *tip_text,
				      const gchar    *tip_private);
};

GType        egg_tool_item_get_type (void);
EggToolItem *egg_tool_item_new      (void);

void egg_tool_item_set_orientation   (EggToolItem     *tool_item,
				      GtkOrientation   orientation);
void egg_tool_item_set_icon_size     (EggToolItem     *tool_item,
				      GtkIconSize      icon_size);
void egg_tool_item_set_toolbar_style (EggToolItem     *tool_item,
				      GtkToolbarStyle  style);
void egg_tool_item_set_relief_style  (EggToolItem     *tool_item,
				      GtkReliefStyle   style);
void egg_tool_item_set_homogeneous   (EggToolItem     *tool_item,
				      gboolean         homogeneous);
void egg_tool_item_set_expandable    (EggToolItem     *tool_item,
				      gboolean         expandable);
void egg_tool_item_set_pack_end      (EggToolItem     *tool_item,
				      gboolean         pack_end);
void egg_tool_item_set_tooltip       (EggToolItem     *tool_item,
				      GtkTooltips     *tooltips,
				      const gchar     *tip_text,
				      const gchar     *tip_private);

#endif /* __EGG_TOOL_ITEM_H__ */
