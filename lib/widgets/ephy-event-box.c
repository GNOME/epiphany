/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "ephy-event-box.h"


static void ephy_event_box_class_init    (EphyEventBoxClass *klass);
static void ephy_event_box_init          (EphyEventBox      *event_box);
static void ephy_event_box_realize       (GtkWidget             *widget);
static void ephy_event_box_unrealize     (GtkWidget             *widget);
static void ephy_event_box_size_request  (GtkWidget             *widget,
					  GtkRequisition        *requisition);
static void ephy_event_box_size_allocate (GtkWidget             *widget,
					  GtkAllocation         *allocation);
static void ephy_event_box_map           (GtkWidget             *widget);
static void ephy_event_box_unmap         (GtkWidget             *widget);

static GtkBinClass *parent_class = NULL;

GType
ephy_event_box_get_type (void)
{
  static GType event_box_type = 0;

  if (!event_box_type)
    {
      static const GTypeInfo event_box_info =
      {
	sizeof (EphyEventBoxClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) ephy_event_box_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (EphyEventBox),
	0,		/* n_preallocs */
	(GInstanceInitFunc) ephy_event_box_init,
      };

      event_box_type = g_type_register_static (GTK_TYPE_BIN, "EphyEventBox",
					       &event_box_info, 0);
    }

  return event_box_type;
}


static void
ephy_event_box_class_init (EphyEventBoxClass *class)
{
  GtkWidgetClass *widget_class;

  parent_class = g_type_class_peek_parent (class);
  
  widget_class = (GtkWidgetClass*) class;

  widget_class->realize = ephy_event_box_realize;
  widget_class->unrealize = ephy_event_box_unrealize;
  widget_class->map = ephy_event_box_map;
  widget_class->unmap = ephy_event_box_unmap;
  widget_class->size_request = ephy_event_box_size_request;
  widget_class->size_allocate = ephy_event_box_size_allocate;
}

static void
ephy_event_box_init (EphyEventBox *event_box)
{
  GTK_WIDGET_SET_FLAGS (event_box, GTK_NO_WINDOW);
}

GtkWidget*
ephy_event_box_new (void)
{
  return g_object_new (EPHY_TYPE_EVENT_BOX, NULL);
}

static void
ephy_event_box_map (GtkWidget *widget)
{
  EphyEventBox *event_box;

  event_box = EPHY_EVENT_BOX (widget);
  
  (* GTK_WIDGET_CLASS (parent_class)->map) (widget);
  
  gdk_window_show (event_box->input_window);
}

static void
ephy_event_box_unmap (GtkWidget *widget)
{
  EphyEventBox *event_box;

  event_box = EPHY_EVENT_BOX (widget);
  
  gdk_window_hide (event_box->input_window);
  
  (* GTK_WIDGET_CLASS (parent_class)->unmap) (widget);
}

static void
ephy_event_box_realize (GtkWidget *widget)
{
  GdkWindowAttr attributes;
  EphyEventBox *event_box;
  gint attributes_mask;
  gint border_width;

  (* GTK_WIDGET_CLASS (parent_class)->realize) (widget);
  
  event_box = EPHY_EVENT_BOX (widget);
  
  border_width = GTK_CONTAINER (widget)->border_width;
 
  attributes.x = widget->allocation.x + border_width;
  attributes.y = widget->allocation.y + border_width;
  attributes.width = widget->allocation.width - 2*border_width;
  attributes.height = widget->allocation.height - 2*border_width;
  attributes.window_type = GDK_WINDOW_CHILD;
  //attributes.override_redirect = TRUE;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.event_mask = gtk_widget_get_events (widget)
			| GDK_BUTTON_MOTION_MASK
			| GDK_BUTTON_PRESS_MASK
			| GDK_BUTTON_RELEASE_MASK
			| GDK_ENTER_NOTIFY_MASK
			| GDK_LEAVE_NOTIFY_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_NOREDIR;

  event_box->input_window = gdk_window_new (widget->window,
					    &attributes, attributes_mask);
  gdk_window_set_user_data (event_box->input_window, widget);
}

static void
ephy_event_box_unrealize (GtkWidget *widget)
{
  EphyEventBox *event_box;

  (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);

  event_box = EPHY_EVENT_BOX (widget);
  
  gdk_window_set_user_data (event_box->input_window, NULL);
  gdk_window_destroy (event_box->input_window);
  event_box->input_window = NULL;
}

static void
ephy_event_box_size_request (GtkWidget      *widget,
			     GtkRequisition *requisition)
{
  GtkBin *bin = GTK_BIN (widget);

  requisition->width = GTK_CONTAINER (widget)->border_width * 2;
  requisition->height = GTK_CONTAINER (widget)->border_width * 2;

  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      GtkRequisition child_requisition;
      
      gtk_widget_size_request (bin->child, &child_requisition);

      requisition->width += child_requisition.width;
      requisition->height += child_requisition.height;
    }
}

static void
ephy_event_box_size_allocate (GtkWidget     *widget,
			      GtkAllocation *allocation)
{
  GtkBin *bin;
  GtkAllocation child_allocation;
  EphyEventBox *event_box;
  
  widget->allocation = *allocation;
  bin = GTK_BIN (widget);

  child_allocation.x = allocation->x + GTK_CONTAINER (widget)->border_width;
  child_allocation.y = allocation->y + GTK_CONTAINER (widget)->border_width;
  child_allocation.width = MAX (allocation->width - GTK_CONTAINER (widget)->border_width * 2, 0);
  child_allocation.height = MAX (allocation->height - GTK_CONTAINER (widget)->border_width * 2, 0);

  if (GTK_WIDGET_REALIZED (widget))
    {
      event_box = EPHY_EVENT_BOX (widget);
      gdk_window_move_resize (event_box->input_window,
			      child_allocation.x,
			      child_allocation.y,
			      child_allocation.width,
			      child_allocation.height);
    }
  
  if (bin->child)
    gtk_widget_size_allocate (bin->child, &child_allocation);
}
