/* eggtoolbutton.h
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

#ifndef __EGG_TOOL_BUTTON_H__
#define __EGG_TOOL_BUTTON_H__

#include "eggtoolitem.h"

G_BEGIN_DECLS

#define EGG_TYPE_TOOL_BUTTON            (egg_tool_button_get_type ())
#define EGG_TOOL_BUTTON(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EGG_TYPE_TOOL_BUTTON, EggToolButton))
#define EGG_TOOL_BUTTON_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EGG_TYPE_TOOL_BUTTON, EggToolButtonClass))
#define EGG_IS_TOOL_BUTTON(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EGG_TYPE_TOOL_BUTTON))
#define EGG_IS_TOOL_BUTTON_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), EGG_TYPE_TOOL_BUTTON))
#define EGG_TOOL_BUTTON_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), EGG_TYPE_TOOL_BUTTON, EggToolButtonClass))

typedef struct _EggToolButton      EggToolButton;
typedef struct _EggToolButtonClass EggToolButtonClass;

struct _EggToolButton
{
  EggToolItem parent;

  /*< private >*/
  GtkWidget *button;
  GtkWidget *box;
  GtkWidget *label;
  GtkWidget *icon;

  gchar *stock_id;
  guint label_set : 1;
  guint icon_set : 1;
};

struct _EggToolButtonClass
{
  EggToolItemClass parent_class;

  GType button_type;
};

GType        egg_tool_button_get_type       (void);
EggToolItem *egg_tool_button_new            (void);
EggToolItem *egg_tool_button_new_from_stock (const gchar *stock_id);


void egg_tool_button_set_icon_widget (EggToolButton *button,
				      GtkWidget     *icon);
void egg_tool_button_set_label       (EggToolButton *button,
				      const gchar   *label);


G_END_DECLS

#endif /* __EGG_TOOL_BUTTON_H__ */
