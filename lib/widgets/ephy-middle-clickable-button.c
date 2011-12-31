/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2011 Alexandre Mazari
 *  Copyright © 2011 Igalia S.L.
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

#include "config.h"
#include "ephy-middle-clickable-button.h"

G_DEFINE_TYPE (EphyMiddleClickableButton, ephy_middle_clickable_button, GTK_TYPE_BUTTON)

static gboolean 
ephy_middle_clickable_button_handle_event (GtkWidget * widget,
                                           GdkEventButton * event)
{
  gboolean ret;
  int actual_button;
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (ephy_middle_clickable_button_parent_class);

  actual_button = event->button;

  if (actual_button == 2)
    event->button = 1;

  if (event->type == GDK_BUTTON_PRESS)
    ret = widget_class->button_press_event (widget, event);
  else
    ret = widget_class->button_release_event (widget, event);
  
  event->button = actual_button;

  return ret;
}

static void
ephy_middle_clickable_button_class_init (EphyMiddleClickableButtonClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  widget_class->button_press_event = ephy_middle_clickable_button_handle_event;
  widget_class->button_release_event = ephy_middle_clickable_button_handle_event;
}

static void
ephy_middle_clickable_button_init (EphyMiddleClickableButton *class)
{
}

GtkWidget *
ephy_middle_clickable_button_new ()
{
  return gtk_widget_new (EPHY_TYPE_MIDDLE_CLICKABLE_BUTTON, NULL);
}
