/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2003 Ricardo Fernandez Pascual
 * Copyright (C) 2004 Paolo Borelli
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

#ifndef __EGG_DROPDOWN_TOOL_BUTTON_H__
#define __EGG_DROPDOWN_TOOL_BUTTON_H__

#include <glib.h>
#include <gtk/gtkmenushell.h>
#include <gtk/gtktoolbutton.h>

G_BEGIN_DECLS

#define EGG_TYPE_DROPDOWN_TOOL_BUTTON         (egg_dropdown_tool_button_get_type ())
#define EGG_DROPDOWN_TOOL_BUTTON(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EGG_TYPE_DROPDOWN_TOOL_BUTTON, EggDropdownToolButton))
#define EGG_DROPDOWN_TOOL_BUTTON_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EGG_TYPE_DROPDOWN_TOOL_BUTTON, EggDropdownToolButtonClass))
#define EGG_IS_DROPDOWN_TOOL_BUTTON(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EGG_TYPE_DROPDOWN_TOOL_BUTTON))
#define EGG_IS_DROPDOWN_TOOL_BUTTON_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EGG_TYPE_DROPDOWN_TOOL_BUTTON))
#define EGG_DROPDOWN_TOOL_BUTTON_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EGG_TYPE_DROPDOWN_TOOL_BUTTON, EggDropdownToolButtonClass))

typedef struct _EggDropdownToolButtonClass   EggDropdownToolButtonClass;
typedef struct _EggDropdownToolButton        EggDropdownToolButton;
typedef struct _EggDropdownToolButtonPrivate EggDropdownToolButtonPrivate;

struct _EggDropdownToolButton
{
  GtkToolButton parent;

  /*< private >*/
  EggDropdownToolButtonPrivate *priv;
};

struct _EggDropdownToolButtonClass
{
  GtkToolButtonClass parent_class;

  void (*menu_activated) (EggDropdownToolButton *button);
};

GType         egg_dropdown_tool_button_get_type       (void);
GtkToolItem  *egg_dropdown_tool_button_new            (void);
GtkToolItem  *egg_dropdown_tool_button_new_from_stock (const gchar *stock_id);

void          egg_dropdown_tool_button_set_menu       (EggDropdownToolButton *button,
                                                       GtkMenuShell          *menu);
GtkMenuShell *egg_dropdown_tool_button_get_menu       (EggDropdownToolButton *button);

void          egg_dropdown_tool_button_set_arrow_tooltip (EggDropdownToolButton *button,
                                                          GtkTooltips *tooltips,
                                                          const gchar *tip_text,
                                                          const gchar *tip_private);


G_END_DECLS;

#endif /* __EGG_DROPDOWN_TOOL_BUTTON_H__ */
