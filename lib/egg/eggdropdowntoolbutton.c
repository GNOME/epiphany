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


#include "config.h"

#include "eggdropdowntoolbutton.h"

#include <gtk/gtktogglebutton.h>
#include <gtk/gtkarrow.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmain.h>
#include <gdk/gdkkeysyms.h>


#define EGG_DROPDOWN_TOOL_BUTTON_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EGG_TYPE_DROPDOWN_TOOL_BUTTON, EggDropdownToolButtonPrivate))

struct _EggDropdownToolButtonPrivate
{
  GtkWidget *button;
  GtkWidget *arrow_button;
  GtkMenu   *menu;
};

static void egg_dropdown_tool_button_init       (EggDropdownToolButton      *button);
static void egg_dropdown_tool_button_class_init (EggDropdownToolButtonClass *klass);
static void egg_dropdown_tool_button_finalize   (GObject *object);

enum {
  MENU_ACTIVATED,
  LAST_SIGNAL
};

static gint signals[LAST_SIGNAL];

static GObjectClass *parent_class = NULL;

GType
egg_dropdown_tool_button_get_type (void)
{
  static GType type = 0;

  if (type == 0)
    {
      static const GTypeInfo info =
	{
	  sizeof (EggDropdownToolButtonClass),
	  (GBaseInitFunc) 0,
	  (GBaseFinalizeFunc) 0,
	  (GClassInitFunc) egg_dropdown_tool_button_class_init,
	  (GClassFinalizeFunc) 0,
	  NULL,
	  sizeof (EggDropdownToolButton),
	  0, /* n_preallocs */
	  (GInstanceInitFunc) egg_dropdown_tool_button_init
	};

      type = g_type_register_static (GTK_TYPE_TOOL_BUTTON,
                                     "EggDropdownToolButton",
                                     &info, 0);
    }

  return type;
}

static gboolean
egg_dropdown_tool_button_set_tooltip (GtkToolItem *tool_item,
                                      GtkTooltips *tooltips,
                                      const char *tip_text,
                                      const char *tip_private)
{
  EggDropdownToolButton *button;

  g_return_val_if_fail (EGG_IS_DROPDOWN_TOOL_BUTTON (tool_item), FALSE);

  button = EGG_DROPDOWN_TOOL_BUTTON (tool_item);
  gtk_tooltips_set_tip (tooltips, button->priv->button, tip_text, tip_private);

  return TRUE;
}

static void
egg_dropdown_tool_button_class_init (EggDropdownToolButtonClass *klass)
{
  GObjectClass *object_class;
  GtkToolItemClass *toolitem_class;
  GtkToolButtonClass *toolbutton_class;

  parent_class = g_type_class_peek_parent (klass);

  object_class = (GObjectClass *)klass;
  toolitem_class = (GtkToolItemClass *)klass;
  toolbutton_class = (GtkToolButtonClass *)klass;

  object_class->finalize = egg_dropdown_tool_button_finalize;
  toolitem_class->set_tooltip = egg_dropdown_tool_button_set_tooltip;

  signals[MENU_ACTIVATED] =
    g_signal_new ("menu-activated",
                  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (EggDropdownToolButtonClass, menu_activated),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  g_type_class_add_private (object_class, sizeof (EggDropdownToolButtonPrivate));
}

static void
button_state_changed_cb (GtkWidget             *widget,
			 GtkStateType           previous_state,
			 EggDropdownToolButton *button)
{
  EggDropdownToolButtonPrivate *priv = button->priv;
  GtkWidget *b;
  GtkStateType state = GTK_WIDGET_STATE (widget);

  b = (widget == priv->arrow_button) ? priv->button : priv->arrow_button;

  g_signal_handlers_block_by_func (G_OBJECT (b),
                                   G_CALLBACK (button_state_changed_cb),
                                   button);

  if (state == GTK_STATE_PRELIGHT &&
      previous_state != GTK_STATE_ACTIVE)
    {
      gtk_widget_set_state (b, state);
    }
  else if (state == GTK_STATE_NORMAL)
    {
      gtk_widget_set_state (b, state);
    }
  else if (state == GTK_STATE_ACTIVE)
    {
      gtk_widget_set_state (b, GTK_STATE_NORMAL);
    }

  g_signal_handlers_unblock_by_func (G_OBJECT (b),
                                     G_CALLBACK (button_state_changed_cb),
                                     button);
}

static void
menu_position_func (GtkMenu               *menu,
		    int                   *x,
		    int                   *y,
		    gboolean              *push_in,
		    EggDropdownToolButton *button)
{
  EggDropdownToolButtonPrivate *priv;
  GtkRequisition menu_requisition;
  int max_x, max_y;

  priv = button->priv;

  gdk_window_get_origin (GTK_WIDGET (button)->window, x, y);

  if (gtk_widget_get_direction (GTK_WIDGET (button)) == GTK_TEXT_DIR_RTL)
    *x += GTK_WIDGET (button)->allocation.x +
          GTK_WIDGET (button)->allocation.width -
          menu_requisition.width;
  else
    *x += GTK_WIDGET (button)->allocation.x;

  *y += GTK_WIDGET (button)->allocation.height;

  /* Make sure we are on the screen.  */
  gtk_widget_size_request (GTK_WIDGET (priv->menu), &menu_requisition);
  max_x = MAX (0, gdk_screen_width () - menu_requisition.width);
  max_y = MAX (0, gdk_screen_height () - menu_requisition.height);

  *x = CLAMP (*x, 0, max_x);
  *y = CLAMP (*y, 0, max_y);
}

static void
popup_menu_under_arrow (EggDropdownToolButton *button,
                        GdkEventButton        *event)
{
  EggDropdownToolButtonPrivate *priv = button->priv;

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->arrow_button), TRUE);

  g_signal_emit (button, signals[MENU_ACTIVATED], 0);

  if (!priv->menu)
    return;

  gtk_menu_popup (priv->menu, NULL, NULL,
                  (GtkMenuPositionFunc) menu_position_func,
                  button,
                  event ? event->button : 0,
                  event ? event->time : gtk_get_current_event_time ());
}

static gboolean
arrow_button_press_event_cb (GtkWidget             *widget,
                             GdkEventButton        *event,
                             EggDropdownToolButton *button)
{
  popup_menu_under_arrow (button, event);

  return TRUE;
}

static gboolean
arrow_key_press_event_cb (GtkWidget             *widget,
                          GdkEventKey           *event,
                          EggDropdownToolButton *button)
{
  if (event->keyval == GDK_space    ||
      event->keyval == GDK_KP_Space ||
      event->keyval == GDK_Return   ||
      event->keyval == GDK_KP_Enter ||
      event->keyval == GDK_Menu)
    {
      popup_menu_under_arrow (button, NULL);
    }

  return FALSE;
}

/* right click on the button shows the menu */
static gboolean
button_button_press_event_cb (GtkWidget             *widget,
                              GdkEventButton        *event, 
                              EggDropdownToolButton *button)
{
  if (event->button == 3)
    {
      popup_menu_under_arrow (button, event);

      return TRUE;
    }

  return FALSE;
}

static void
button_popup_menu_cb (GtkWidget             *widget,
                      EggDropdownToolButton *button)
{
    popup_menu_under_arrow (button, NULL);
}

static void
egg_dropdown_tool_button_init (EggDropdownToolButton *button)
{
  GtkWidget *hbox;
  GtkWidget *arrow;
  GtkWidget *arrow_button;
  GtkWidget *real_button;

  button->priv = EGG_DROPDOWN_TOOL_BUTTON_GET_PRIVATE (button);

  gtk_tool_item_set_homogeneous (GTK_TOOL_ITEM (button), FALSE);

  hbox = gtk_hbox_new (FALSE, 0);

  real_button = GTK_BIN (button)->child;
  g_object_ref (real_button);
  gtk_container_remove (GTK_CONTAINER (button), real_button);
  gtk_container_add (GTK_CONTAINER (hbox), real_button);
  g_object_unref (real_button);

  arrow_button = gtk_toggle_button_new ();
  arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_OUT);
  gtk_button_set_relief (GTK_BUTTON (arrow_button), GTK_RELIEF_NONE);
  gtk_container_add (GTK_CONTAINER (arrow_button), arrow);

  gtk_box_pack_end (GTK_BOX (hbox), arrow_button,
                    FALSE, FALSE, 0);
  gtk_widget_show_all (hbox);

  gtk_container_add (GTK_CONTAINER (button), hbox);

  button->priv->button = real_button;
  button->priv->arrow_button = arrow_button;

  g_signal_connect (real_button, "state_changed",
                    G_CALLBACK (button_state_changed_cb), button);
  g_signal_connect (real_button, "button_press_event",
                    G_CALLBACK (button_button_press_event_cb), button);
  g_signal_connect (real_button, "popup_menu",
                    G_CALLBACK (button_popup_menu_cb), button);
  g_signal_connect (arrow_button, "state_changed",
                    G_CALLBACK (button_state_changed_cb), button);
  g_signal_connect (arrow_button, "key_press_event",
                    G_CALLBACK (arrow_key_press_event_cb), button);
  g_signal_connect (arrow_button, "button_press_event",
                    G_CALLBACK (arrow_button_press_event_cb), button);
}

static void
egg_dropdown_tool_button_finalize (GObject *object)
{
  EggDropdownToolButton *button;

  button = EGG_DROPDOWN_TOOL_BUTTON (object);

  if (button->priv->menu)
    g_object_unref (button->priv->menu);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

GtkToolItem *
egg_dropdown_tool_button_new (void)
{
  EggDropdownToolButton *button;

  button = g_object_new (EGG_TYPE_DROPDOWN_TOOL_BUTTON,
			 NULL);

  return GTK_TOOL_ITEM (button);
}

GtkToolItem *
egg_dropdown_tool_button_new_from_stock (const gchar *stock_id)
{
  EggDropdownToolButton *button;

  g_return_val_if_fail (stock_id != NULL, NULL);

  button = g_object_new (EGG_TYPE_DROPDOWN_TOOL_BUTTON,
			 "stock_id", stock_id,
			 NULL);

  return GTK_TOOL_ITEM (button);
}

/* Callback for the "deactivate" signal on the pop-up menu.
 * This is used so that we unset the state of the toggle button
 * when the pop-up menu disappears. 
 */
static int
menu_deactivate_cb (GtkMenuShell          *menu_shell,
		    EggDropdownToolButton *button)
{
  EggDropdownToolButtonPrivate *priv = button->priv;

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->arrow_button), FALSE);

  return TRUE;
}

void
egg_dropdown_tool_button_set_menu (EggDropdownToolButton *button,
                                   GtkMenuShell          *menu)
{
  EggDropdownToolButtonPrivate *priv;

  g_return_if_fail (EGG_IS_DROPDOWN_TOOL_BUTTON (button));
  g_return_if_fail (GTK_IS_MENU (menu));

  priv = button->priv;

  if (priv->menu != GTK_MENU (menu))
    {
      if (priv->menu)
        g_object_unref (priv->menu);

      priv->menu = GTK_MENU (menu);

      g_object_ref (priv->menu);

      g_signal_connect (button->priv->menu, "deactivate",
                        G_CALLBACK (menu_deactivate_cb), button);
    }
}

GtkMenuShell *
egg_dropdown_tool_button_get_menu (EggDropdownToolButton *button)
{
  return GTK_MENU_SHELL (button->priv->menu);
}

void
egg_dropdown_tool_button_set_arrow_tooltip (EggDropdownToolButton *button,
                                            GtkTooltips *tooltips,
                                            const gchar *tip_text,
                                            const gchar *tip_private)
{
  g_return_if_fail (EGG_IS_DROPDOWN_TOOL_BUTTON (button));

  gtk_tooltips_set_tip (tooltips, button->priv->arrow_button, tip_text, tip_private);
}

