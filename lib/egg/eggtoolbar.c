/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * GtkToolbar copyright (C) Federico Mena
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

#include <gtk/gtkarrow.h>
#include "eggtoolbar.h"
#include "eggradiotoolbutton.h"
#include "eggseparatortoolitem.h"
#include <gtk/gtkmenu.h>
#include <gtk/gtkradiobutton.h>
#include <gtk/gtktoolbar.h>

#define DEFAULT_IPADDING 0
#define DEFAULT_SPACE_SIZE  5
#define DEFAULT_SPACE_STYLE GTK_TOOLBAR_SPACE_LINE

#define DEFAULT_ICON_SIZE GTK_ICON_SIZE_LARGE_TOOLBAR
#define DEFAULT_TOOLBAR_STYLE GTK_TOOLBAR_BOTH

#define SPACE_LINE_DIVISION 10
#define SPACE_LINE_START    3
#define SPACE_LINE_END      7

#define TOOLBAR_ITEM_VISIBLE(item) \
(GTK_WIDGET_VISIBLE (item) && \
((toolbar->orientation == GTK_ORIENTATION_HORIZONTAL && item->visible_horizontal) || \
 (toolbar->orientation == GTK_ORIENTATION_VERTICAL && item->visible_vertical)))

#ifndef _
#  define _(s) (s)
#endif

enum {
  PROP_0,
  PROP_ORIENTATION,
  PROP_TOOLBAR_STYLE,
  PROP_SHOW_ARROW
};

enum {
  ORIENTATION_CHANGED,
  STYLE_CHANGED,
  POPUP_CONTEXT_MENU,
  LAST_SIGNAL
};

static void egg_toolbar_init       (EggToolbar      *toolbar);
static void egg_toolbar_class_init (EggToolbarClass *klass);

static void egg_toolbar_set_property (GObject      *object,
				      guint         prop_id,
				      const GValue *value,
				      GParamSpec   *pspec);
static void egg_toolbar_get_property (GObject      *object,
				      guint         prop_id,
				      GValue       *value,
				      GParamSpec   *pspec);

static gint     egg_toolbar_expose         (GtkWidget        *widget,
					    GdkEventExpose   *event);
static void     egg_toolbar_realize        (GtkWidget        *widget);
static void     egg_toolbar_unrealize      (GtkWidget        *widget);
static void     egg_toolbar_size_request   (GtkWidget        *widget,
					    GtkRequisition   *requisition);
static void     egg_toolbar_size_allocate  (GtkWidget        *widget,
					    GtkAllocation    *allocation);
static void     egg_toolbar_style_set      (GtkWidget        *widget,
					    GtkStyle         *prev_style);
static void     egg_toolbar_direction_changed (GtkWidget        *widget,
                                               GtkTextDirection  previous_direction);

static gboolean egg_toolbar_focus          (GtkWidget        *widget,
					    GtkDirectionType  dir);
static void     egg_toolbar_screen_changed (GtkWidget        *widget,
					    GdkScreen        *previous_screen);

static void     egg_toolbar_drag_leave  (GtkWidget      *widget,
					 GdkDragContext *context,
					 guint           time_);
static gboolean egg_toolbar_drag_motion (GtkWidget      *widget,
					 GdkDragContext *context,
					 gint            x,
					 gint            y,
					 guint           time_);

static void  egg_toolbar_add        (GtkContainer *container,
				     GtkWidget    *widget);
static void  egg_toolbar_remove     (GtkContainer *container,
				     GtkWidget    *widget);
static void  egg_toolbar_forall     (GtkContainer *container,
				     gboolean      include_internals,
				     GtkCallback   callback,
				     gpointer      callback_data);
static GType egg_toolbar_child_type (GtkContainer *container);


static void egg_toolbar_real_orientation_changed (EggToolbar      *toolbar,
						  GtkOrientation   orientation);
static void egg_toolbar_real_style_changed       (EggToolbar      *toolbar,
						  GtkToolbarStyle  style);

static gboolean             egg_toolbar_button_press         (GtkWidget      *button,
							      GdkEventButton *event,
							      EggToolbar     *toolbar);
static void                 egg_toolbar_arrow_button_press   (GtkWidget      *button,
							      GdkEventButton *event,
							      EggToolbar     *toolbar);
static void                 egg_toolbar_update_button_relief (EggToolbar     *toolbar);
static GtkReliefStyle       get_button_relief                (EggToolbar     *toolbar);
static gint                 get_space_size                   (EggToolbar     *toolbar);
static GtkToolbarSpaceStyle get_space_style                  (EggToolbar     *toolbar);

static GtkWidget *egg_toolbar_internal_insert_element (EggToolbar          *toolbar,
						       EggToolbarChildType  type,
						       GtkWidget           *widget,
						       const char          *text,
						       const char          *tooltip_text,
						       const char          *tooltip_private_text,
						       GtkWidget           *icon,
						       GtkSignalFunc        callback,
						       gpointer             user_data,
						       gint                 position,
						       gboolean             use_stock);


#define PRIVATE_KEY "egg-toolbar-private"

#define EGG_TOOLBAR_GET_PRIVATE(toolbar) (g_object_get_data (G_OBJECT (toolbar), PRIVATE_KEY))

typedef struct
{
  GList     *items;
  GList     *first_non_fitting_item;
  
  gint       total_button_maxw;
  gint       total_button_maxh;

  GtkWidget *button;
  GtkWidget *arrow;
  
  gboolean   show_arrow;

  gint       drop_index;
  GdkWindow *drag_highlight;
} EggToolbarPrivate;

static GtkContainerClass *parent_class = NULL;
static guint toolbar_signals [LAST_SIGNAL] = { 0 };

GType
egg_toolbar_get_type (void)
{
  static GtkType type = 0;

  if (!type)
    {
      static const GTypeInfo type_info =
	{
	  sizeof (EggToolbarClass),
	  (GBaseInitFunc) NULL,
	  (GBaseFinalizeFunc) NULL,
	  (GClassInitFunc) egg_toolbar_class_init,
	  (GClassFinalizeFunc) NULL,
	  NULL,        
	  sizeof (EggToolbar),
	  0, /* n_preallocs */
	  (GInstanceInitFunc) egg_toolbar_init,
	};

      type = g_type_register_static (GTK_TYPE_CONTAINER,
				     "EggToolbar",
				     &type_info, 0);
    }
  
  return type;
}

static void
egg_toolbar_class_init (EggToolbarClass *klass)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  parent_class = g_type_class_peek_parent (klass);
  
  gobject_class = (GObjectClass *)klass;
  widget_class = (GtkWidgetClass *)klass;
  container_class = (GtkContainerClass *)klass;
  
  gobject_class->set_property = egg_toolbar_set_property;
  gobject_class->get_property = egg_toolbar_get_property;

  widget_class->expose_event = egg_toolbar_expose;
  widget_class->size_request = egg_toolbar_size_request;
  widget_class->size_allocate = egg_toolbar_size_allocate;
  widget_class->style_set = egg_toolbar_style_set;
  widget_class->direction_changed = egg_toolbar_direction_changed;
  widget_class->focus = egg_toolbar_focus;
  widget_class->screen_changed = egg_toolbar_screen_changed;
  widget_class->realize = egg_toolbar_realize;
  widget_class->unrealize = egg_toolbar_unrealize;

  widget_class->drag_leave = egg_toolbar_drag_leave;
  widget_class->drag_motion = egg_toolbar_drag_motion;
  
  container_class->add    = egg_toolbar_add;
  container_class->remove = egg_toolbar_remove;
  container_class->forall = egg_toolbar_forall;
  container_class->child_type = egg_toolbar_child_type;
  
  klass->orientation_changed = egg_toolbar_real_orientation_changed;
  klass->style_changed = egg_toolbar_real_style_changed;

  toolbar_signals[ORIENTATION_CHANGED] =
    g_signal_new ("orientation_changed",
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (EggToolbarClass, orientation_changed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__ENUM,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_ORIENTATION);
  toolbar_signals[STYLE_CHANGED] =
    g_signal_new ("style_changed",
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (EggToolbarClass, style_changed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__ENUM,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_TOOLBAR_STYLE);

  toolbar_signals[POPUP_CONTEXT_MENU] =
    g_signal_new ("popup_context_menu",
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (EggToolbarClass, popup_context_menu),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  
  g_object_class_install_property (gobject_class,
				   PROP_ORIENTATION,
				   g_param_spec_enum ("orientation",
 						      _("Orientation"),
 						      _("The orientation of the toolbar"),
 						      GTK_TYPE_ORIENTATION,
 						      GTK_ORIENTATION_HORIZONTAL,
 						      G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
				   PROP_TOOLBAR_STYLE,
				   g_param_spec_enum ("toolbar_style",
 						      _("Toolbar Style"),
 						      _("How to draw the toolbar"),
 						      GTK_TYPE_TOOLBAR_STYLE,
 						      GTK_TOOLBAR_ICONS,
 						      G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_SHOW_ARROW,
				   g_param_spec_boolean ("show_arrow",
							 _("Show Arrow"),
							 _("If an arrow should be shown if the toolbar doesn't fit"),
							 FALSE,
							 G_PARAM_READWRITE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("space_size",
							     _("Spacer size"),
							     _("Size of spacers"),
							     0,
							     G_MAXINT,
                                                             DEFAULT_SPACE_SIZE,
							     G_PARAM_READABLE));
  
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("internal_padding",
							     _("Internal padding"),
							     _("Amount of border space between the toolbar shadow and the buttons"),
							     0,
							     G_MAXINT,
                                                             DEFAULT_IPADDING,
                                                             G_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_enum ("space_style",
							     _("Space style"),
							     _("Whether spacers are vertical lines or just blank"),
                                                              GTK_TYPE_TOOLBAR_SPACE_STYLE,
                                                              DEFAULT_SPACE_STYLE,
                                                              G_PARAM_READABLE));
  
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_enum ("button_relief",
							      _("Button relief"),
							      _("Type of bevel around toolbar buttons"),
                                                              GTK_TYPE_RELIEF_STYLE,
                                                              GTK_RELIEF_NONE,
                                                              G_PARAM_READABLE));
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_enum ("shadow_type",
                                                              _("Shadow type"),
                                                              _("Style of bevel around the toolbar"),
                                                              GTK_TYPE_SHADOW_TYPE,
                                                              GTK_SHADOW_OUT,
                                                              G_PARAM_READABLE));

  gtk_settings_install_property (g_param_spec_enum ("gtk-toolbar-style",
                                                    _("Toolbar style"),
                                                    _("Whether default toolbars have text only, text and icons, icons only, etc."),
                                                    GTK_TYPE_TOOLBAR_STYLE,
                                                    DEFAULT_TOOLBAR_STYLE,
                                                    G_PARAM_READWRITE));

  gtk_settings_install_property (g_param_spec_enum ("gtk-toolbar-icon-size",
                                                    _("Toolbar icon size"),
                                                    _("Size of icons in default toolbars"),
                                                    GTK_TYPE_ICON_SIZE,
                                                    DEFAULT_ICON_SIZE,
                                                    G_PARAM_READWRITE));  
}

static void
egg_toolbar_init (EggToolbar *toolbar)
{
  EggToolbarPrivate *priv;
  
  GTK_WIDGET_UNSET_FLAGS (toolbar, GTK_CAN_FOCUS);

  priv = g_new0 (EggToolbarPrivate, 1);
  g_object_set_data (G_OBJECT (toolbar), PRIVATE_KEY, priv);
  
  toolbar->orientation = GTK_ORIENTATION_HORIZONTAL;
  toolbar->style = GTK_TOOLBAR_ICONS;
  toolbar->tooltips = gtk_tooltips_new ();
  g_object_ref (toolbar->tooltips);
  gtk_object_sink (GTK_OBJECT (toolbar->tooltips));
  
  priv->button = gtk_toggle_button_new ();
  g_signal_connect (priv->button, "button_press_event",
		    G_CALLBACK (egg_toolbar_arrow_button_press), toolbar);
  gtk_button_set_relief (GTK_BUTTON (priv->button),
			 get_button_relief (toolbar));
  GTK_WIDGET_UNSET_FLAGS (priv->button, GTK_CAN_FOCUS);
  
  priv->arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
  gtk_widget_show (priv->arrow);
  gtk_container_add (GTK_CONTAINER (priv->button), priv->arrow);
  
  gtk_widget_set_parent (priv->button, GTK_WIDGET (toolbar));

  g_signal_connect (GTK_WIDGET (toolbar), "button_press_event",
      		    G_CALLBACK (egg_toolbar_button_press), toolbar);

  /* which child position a drop will occur at */
  priv->drop_index = -1;
  priv->drag_highlight = NULL;
}

static void
egg_toolbar_set_property (GObject     *object,
			  guint        prop_id,
			  const GValue *value,
			  GParamSpec   *pspec)
{
  EggToolbar *toolbar = EGG_TOOLBAR (object);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      egg_toolbar_set_orientation (toolbar, g_value_get_enum (value));
      break;
    case PROP_TOOLBAR_STYLE:
      egg_toolbar_set_style (toolbar, g_value_get_enum (value));
      break;
    case PROP_SHOW_ARROW:
      egg_toolbar_set_show_arrow (toolbar, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
egg_toolbar_get_property (GObject    *object,
			  guint       prop_id,
			  GValue     *value,
			  GParamSpec *pspec)
{
  EggToolbar *toolbar = EGG_TOOLBAR (object);
  EggToolbarPrivate *priv = EGG_TOOLBAR_GET_PRIVATE (toolbar);
  
  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, toolbar->orientation);
      break;
    case PROP_TOOLBAR_STYLE:
      g_value_set_enum (value, toolbar->style);
      break;
    case PROP_SHOW_ARROW:
      g_value_set_boolean (value, priv->show_arrow);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
egg_toolbar_paint_space_line (GtkWidget    *widget,
			      GdkRectangle *area,
			      EggToolItem  *item)
{
  EggToolbar *toolbar;
  GtkAllocation *allocation;
  gint space_size;
  
  g_return_if_fail (GTK_BIN (item)->child == NULL);

  toolbar = EGG_TOOLBAR (widget);

  allocation = &GTK_WIDGET (item)->allocation;
  space_size = get_space_size (toolbar);
  
  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
    gtk_paint_vline (widget->style, widget->window,
		     GTK_WIDGET_STATE (widget), area, widget,
		     "toolbar",
		     allocation->y +  allocation->height *
		     SPACE_LINE_START / SPACE_LINE_DIVISION,
		     allocation->y + allocation->height *
		     SPACE_LINE_END / SPACE_LINE_DIVISION,
		     allocation->x + (space_size-widget->style->xthickness)/2);
  else if (toolbar->orientation == GTK_ORIENTATION_VERTICAL)
    gtk_paint_hline (widget->style, widget->window,
		     GTK_WIDGET_STATE (widget), area, widget,
		     "toolbar",
		     allocation->x + allocation->width *
		     SPACE_LINE_START / SPACE_LINE_DIVISION,
		     allocation->x + allocation->width *
		     SPACE_LINE_END / SPACE_LINE_DIVISION,
		     allocation->y + (space_size-widget->style->ythickness)/2);
}

static void
egg_toolbar_realize (GtkWidget *widget)
{
  EggToolbar *toolbar = EGG_TOOLBAR (widget);
  GdkWindowAttr attributes;
  gint attributes_mask;
  gint border_width;

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  border_width = GTK_CONTAINER (widget)->border_width;

  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x + border_width;
  attributes.y = widget->allocation.y + border_width;
  attributes.width = widget->allocation.width - border_width * 2;
  attributes.height = widget->allocation.height - border_width * 2;
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_VISIBILITY_NOTIFY_MASK |
      			    GDK_EXPOSURE_MASK |
			    GDK_BUTTON_PRESS_MASK |
			    GDK_BUTTON_RELEASE_MASK |
			    GDK_ENTER_NOTIFY_MASK |
			    GDK_LEAVE_NOTIFY_MASK);

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
      				   &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, toolbar);
  gdk_window_set_background (widget->window, &widget->style->bg[GTK_WIDGET_STATE (widget)]);

  widget->style = gtk_style_attach (widget->style, widget->window);
}

static void
egg_toolbar_unrealize (GtkWidget *widget)
{
  EggToolbarPrivate *priv = EGG_TOOLBAR_GET_PRIVATE (widget);

  if (priv->drag_highlight)
    {
      gdk_window_set_user_data (priv->drag_highlight, NULL);
      gdk_window_destroy (priv->drag_highlight);
      priv->drag_highlight = NULL;
    }

  if (GTK_WIDGET_CLASS (parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static gint
egg_toolbar_expose (GtkWidget      *widget,
		    GdkEventExpose *event)
{
  EggToolbar *toolbar = EGG_TOOLBAR (widget);
  EggToolbarPrivate *priv = EGG_TOOLBAR_GET_PRIVATE (toolbar);
  
  GList *items;
  gint border_width;
  
  border_width = GTK_CONTAINER (widget)->border_width;
  
  if (GTK_WIDGET_DRAWABLE (widget))
    {
      GtkShadowType shadow_type;

      gtk_widget_style_get (widget, "shadow_type", &shadow_type, NULL);

      gtk_paint_box (widget->style,
		     widget->window,
                     GTK_WIDGET_STATE (widget),
                     shadow_type,
		     &event->area, widget, "toolbar",
		     border_width,
                     border_width,
		     widget->allocation.width - border_width,
                     widget->allocation.height - border_width);
    }

  items = priv->items;
  while (items)
    {
      EggToolItem *item = EGG_TOOL_ITEM (items->data);

      if (GTK_BIN (item)->child)
	gtk_container_propagate_expose (GTK_CONTAINER (widget),
					GTK_WIDGET (item),
					event);
      else if (GTK_WIDGET_MAPPED (item) && get_space_style (toolbar) == GTK_TOOLBAR_SPACE_LINE)
	egg_toolbar_paint_space_line (widget, &event->area, item);
      
      items = items->next;
    }

  gtk_container_propagate_expose (GTK_CONTAINER (widget),
				  priv->button,
				  event);

  return FALSE;
}

static void
egg_toolbar_size_request (GtkWidget      *widget,
			  GtkRequisition *requisition)
{
  EggToolbar *toolbar = EGG_TOOLBAR (widget);
  EggToolbarPrivate *priv = EGG_TOOLBAR_GET_PRIVATE (toolbar);
  GList *items;
  gint nbuttons, ipadding;
  gint button_maxw, button_maxh;
  gint total_button_maxw, total_button_maxh;
  gint space_size;
  GtkRequisition child_requisition;

  requisition->width = GTK_CONTAINER (toolbar)->border_width * 2;
  requisition->height = GTK_CONTAINER (toolbar)->border_width * 2;
  nbuttons = 0;
  button_maxw = 0;
  button_maxh = 0;
  total_button_maxw = 0;
  total_button_maxh = 0;
  items = priv->items;
  space_size = get_space_size (toolbar);
  
  if (priv->show_arrow)
    {
      /* When we enable the arrow we only want to be the
       * size of the arrows plus the size of any items that
       * are pack-end.
       */

      items = priv->items;

      while (items)
	{
	  EggToolItem *item = EGG_TOOL_ITEM (items->data);
	  
	  if (TOOLBAR_ITEM_VISIBLE (item))
	    {
	      gtk_widget_size_request (GTK_WIDGET (item), &child_requisition);

	      total_button_maxw = MAX (total_button_maxw, child_requisition.width);
	      total_button_maxh = MAX (total_button_maxh, child_requisition.height);

	      if (item->homogeneous)
		{
		  if (item->pack_end)
		    nbuttons++;
		  button_maxw = MAX (button_maxw, child_requisition.width);
		  button_maxh = MAX (button_maxh, child_requisition.height);
		}
	      else if (item->pack_end)
		{
		  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
		    requisition->width += child_requisition.width;
		  else
		    requisition->height += child_requisition.height;
		}
	      if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
		requisition->height = MAX (requisition->height, child_requisition.height);
	      else
		requisition->width = MAX (requisition->width, child_requisition.width);
	    }

	  items = items->next;
	}

      /* Add the arrow */
      gtk_widget_size_request (priv->button, &child_requisition);
      
      if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
	{
	  requisition->width += child_requisition.width;
	  requisition->height = MAX (requisition->height, child_requisition.height);	  
	}
      else
	{
	  requisition->height += child_requisition.height;
	  requisition->width = MAX (requisition->width, child_requisition.width);
	}
    }
  else
    {
      items = priv->items;

      while (items)
	{
	  EggToolItem *item = EGG_TOOL_ITEM (items->data);
	  
	  if (!TOOLBAR_ITEM_VISIBLE (item))
	    {
	      items = items->next;
	      continue;
	    }
	  
	  if (!GTK_BIN (item)->child)
	    {
	      if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
		requisition->width += space_size;
	      else
		requisition->height += space_size;
	    }
	  else
	    {
	      gtk_widget_size_request (GTK_WIDGET (item), &child_requisition);
	      
	      total_button_maxw = MAX (total_button_maxw, child_requisition.width);
	      total_button_maxh = MAX (total_button_maxh, child_requisition.height);
	      
	      if (item->homogeneous)
		{
		  nbuttons++;
		  button_maxw = MAX (button_maxw, child_requisition.width);
		  button_maxh = MAX (button_maxh, child_requisition.height);
		}
	      else
		{
		  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
		    {
		      requisition->width += child_requisition.width;
		      requisition->height = MAX (requisition->height, child_requisition.height);
		    }
		  else
		    {
		      requisition->height += child_requisition.height;
		      requisition->width = MAX (requisition->width, child_requisition.width);
		    }
		}
	    }

	  items = items->next;
	}
    }
  
  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      requisition->width += nbuttons * button_maxw;
      requisition->height = MAX (requisition->height, button_maxh);
    }
  else
    {
      requisition->width = MAX (requisition->width, button_maxw);
      requisition->height += nbuttons * button_maxh;
    }

  /* Extra spacing */
  gtk_widget_style_get (widget, "internal_padding", &ipadding, NULL);
  
  requisition->width += 2 * ipadding;
  requisition->height += 2 * ipadding;

  priv->total_button_maxw = total_button_maxw;
  priv->total_button_maxh = total_button_maxh;
  
  toolbar->button_maxw = button_maxw;
  toolbar->button_maxh = button_maxh;
}

static void
egg_toolbar_size_allocate (GtkWidget     *widget,
			   GtkAllocation *allocation)
{
  EggToolbar *toolbar = EGG_TOOLBAR (widget);
  EggToolbarPrivate *priv = EGG_TOOLBAR_GET_PRIVATE (toolbar);
  GList *items;
  GtkAllocation child_allocation;
  gint ipadding, space_size;
  gint border_width, edge_position;
  gint available_width, available_height;
  gint available_size, total_size;
  GtkRequisition child_requisition;
  gint remaining_size;
  gint number_expandable, expandable_size;
  gboolean first_expandable;
  gint child_x; 

  widget->allocation = *allocation;
  border_width = GTK_CONTAINER (widget)->border_width;
  total_size = 0;
  number_expandable = 0;
  space_size = get_space_size (toolbar);
  
  gtk_widget_style_get (widget, "internal_padding", &ipadding, NULL);
  border_width += ipadding;
  
  available_width  = allocation->width  - 2 * border_width;
  available_height = allocation->height - 2 * border_width;
  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      edge_position = allocation->x + allocation->width - border_width;
      available_size = available_width;
    }
  else
    {
      edge_position = allocation->height - border_width;
      available_size = available_height;
    }

  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (widget->window,
			      allocation->x + border_width,
			      allocation->y + border_width,
			      allocation->width - border_width * 2,
			      allocation->height - border_width * 2);
    }

  
  items = g_list_last (priv->items);
  
  while (items)
    {
      EggToolItem *item = EGG_TOOL_ITEM (items->data);
      
      if (!item->pack_end || !TOOLBAR_ITEM_VISIBLE (item))
	{
	  items = items->prev;
	  continue;
	}

      if (!GTK_BIN (item)->child)
	{
	  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
	    {
	      child_allocation.width = space_size;
	      child_allocation.height = available_height;
	      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR) 
		child_allocation.x = edge_position - child_allocation.width;
	      else
		child_allocation.x = allocation->x + allocation->width - edge_position;
		  
	      child_allocation.y = (allocation->height - child_allocation.height) / 2;


	      gtk_widget_size_allocate (GTK_WIDGET (item), &child_allocation);

	      edge_position -= child_allocation.width;
	      available_size -= child_allocation.width; 
	    }
	  else
	    {
	      child_allocation.width = available_width;
	      child_allocation.height = space_size;
	      child_allocation.x = allocation->x + (allocation->width - child_allocation.width) / 2;
	      child_allocation.y = edge_position - child_allocation.height;

	      gtk_widget_size_allocate (GTK_WIDGET (item), &child_allocation);

	      edge_position -= child_allocation.height;
	      available_size -= child_allocation.height; 
	    }
	}
      else
	{
	  gtk_widget_get_child_requisition (GTK_WIDGET (item), &child_requisition);
	  
	  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
	    {
	      if (item->homogeneous)
		child_allocation.width = toolbar->button_maxw;
	      else
		child_allocation.width = child_requisition.width;
	      child_allocation.height = available_height;
	      child_allocation.y = (allocation->height - child_allocation.height) / 2;
	      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR) 
		child_allocation.x = edge_position - child_allocation.width;
	      else
		child_allocation.x = allocation->x + allocation->width - edge_position;
	      
	      gtk_widget_size_allocate (GTK_WIDGET (item), &child_allocation);
	      
	      edge_position -= child_allocation.width;
	      available_size -= child_allocation.width; 
	    }
	  else
	    {
	      if (item->homogeneous)
		child_allocation.height = toolbar->button_maxh;
	      else
		child_allocation.height = child_requisition.height;

	      child_allocation.width = available_width;
	      child_allocation.x = allocation->x + (allocation->width - child_allocation.width) / 2;
	      child_allocation.y = edge_position - child_allocation.height;
	      
	      gtk_widget_size_allocate (GTK_WIDGET (item), &child_allocation);
	      
	      edge_position -= child_allocation.height;
	      available_size -= child_allocation.height;
	    }
	}

      items = items->prev;
    }

  /* Now go through the items and see if they fit */
  items = priv->items;

  while (items)
    {
      EggToolItem *item = EGG_TOOL_ITEM (items->data);

      if (item->pack_end || !TOOLBAR_ITEM_VISIBLE (item))
	{
	  items = items->next;
	  continue;
	}

      if (item->expandable)
	number_expandable += 1;

      if (!GTK_BIN (item)->child)
	{
	  total_size += space_size;
	}
      else
	{
	  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
	    {
	      gtk_widget_get_child_requisition (GTK_WIDGET (item), &child_requisition);
	      
	      if (item->homogeneous)
		total_size += toolbar->button_maxw;
	      else
		total_size += child_requisition.width;
	    }
	  else
	    {
	      gtk_widget_get_child_requisition (GTK_WIDGET (item), &child_requisition);
	      
	      if (item->homogeneous)
		total_size += toolbar->button_maxh;
	      else
		total_size += child_requisition.height;
	    }
	}
      items = items->next;
    }

  /* Check if we need to allocate and show the arrow */
  if (available_size < total_size)
    {
      if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
	{
	  gtk_widget_get_child_requisition (priv->button, &child_requisition);
	  available_size -= child_requisition.width;

	  child_allocation.width = child_requisition.width;
	  child_allocation.height = priv->total_button_maxh;
	  child_allocation.y = (allocation->height - child_allocation.height) / 2;
	  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR) 
	    child_allocation.x = edge_position - child_allocation.width;
	  else
	    child_allocation.x = allocation->x + allocation->width - edge_position;
	}
      else
	{
	  gtk_widget_get_child_requisition (priv->button, &child_requisition); 	  
	  available_size -= child_requisition.width;

	  child_allocation.height = child_requisition.height;
	  child_allocation.width = priv->total_button_maxw;
	  child_allocation.x = allocation->x + (allocation->width - child_allocation.width) / 2;
	  child_allocation.y = edge_position - child_allocation.height;
	}

      gtk_widget_size_allocate (priv->button, &child_allocation);      
      gtk_widget_show (priv->button);
    }
  else
    gtk_widget_hide (priv->button);
  
  /* Finally allocate the remaining items */
  items = priv->items;
  child_x = allocation->x + border_width;
  child_allocation.y = border_width;
  remaining_size = MAX (0, available_size - total_size);
  total_size = 0;
  first_expandable = TRUE;
  
  while (items)
    {
      EggToolItem *item = EGG_TOOL_ITEM (items->data);

      if (item->pack_end || !TOOLBAR_ITEM_VISIBLE (item))
	{
	  items = items->next;
	  continue;
	}
      
      if (!GTK_BIN (item)->child)
	{
	  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
	    {
	      child_allocation.width = space_size;
	      child_allocation.height = available_height;
	      child_allocation.y = (allocation->height - child_allocation.height) / 2;
	      total_size += child_allocation.width;

	      if (total_size > available_size)
		break;
	      
	      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR) 
		child_allocation.x = child_x;
	      else
		child_allocation.x = allocation->x + allocation->width 
		  - child_x - child_allocation.width;

	      gtk_widget_size_allocate (GTK_WIDGET (item), &child_allocation);
	      gtk_widget_map (GTK_WIDGET (item));
	      
	      child_x += child_allocation.width;
	    }
	  else
	    {
	      child_allocation.width = available_width;
	      child_allocation.height = space_size;
	      child_allocation.x = allocation->x + (allocation->width - child_allocation.width) / 2;
	      total_size += child_allocation.height;

	      if (total_size > available_size)
		break;
	      
	      gtk_widget_size_allocate (GTK_WIDGET (item), &child_allocation);
	      gtk_widget_map (GTK_WIDGET (item));
	  
	      child_allocation.y += child_allocation.height;
	    }
	}
      else
	{
	  gtk_widget_get_child_requisition (GTK_WIDGET (item), &child_requisition);
	  
	  if (item->expandable)
	    {
	      expandable_size = remaining_size / number_expandable;
	      
	      if (first_expandable)
		{
		  expandable_size += remaining_size % number_expandable;
		  first_expandable = FALSE;
		}
	    }
	  else
	    expandable_size = 0;
	  
	  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
	    {
	      if (item->homogeneous)
		child_allocation.width = toolbar->button_maxw;
	      else
		child_allocation.width = child_requisition.width;
	      
	      child_allocation.height = available_height;
	      child_allocation.width += expandable_size;
	      child_allocation.y = (allocation->height - child_allocation.height) / 2;
	      total_size += child_allocation.width;

	      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
		child_allocation.x = child_x;
	      else
		child_allocation.x = allocation->x + allocation->width 
		  - child_x - child_allocation.width;

	    }
	  else
	    {
	      if (item->homogeneous)
		child_allocation.height = toolbar->button_maxh;
	      else
		child_allocation.height = child_requisition.height;
	      
	      child_allocation.width = available_width;
	      child_allocation.height += expandable_size;
	      child_allocation.x = allocation->x + (allocation->width - child_allocation.width) / 2;
	      total_size += child_allocation.height;

	    }
	  
	  if (total_size > available_size)
	    break;
	  
	  gtk_widget_size_allocate (GTK_WIDGET (item), &child_allocation);
	  gtk_widget_map (GTK_WIDGET (item));
	  
	  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
	    child_x += child_allocation.width;
	  else
	    child_allocation.y += child_allocation.height;
	  
	}
      
      items = items->next;
    }
  
  /* Unmap the remaining items */
  priv->first_non_fitting_item = items;
  while (items)
    {
      EggToolItem *item = EGG_TOOL_ITEM (items->data);

      gtk_widget_unmap (GTK_WIDGET (item));
      items = items->next;      
    }
}

static void
egg_toolbar_style_set (GtkWidget *widget,
		       GtkStyle  *prev_style)
{
  if (GTK_WIDGET_REALIZED (widget))
      gtk_style_set_background (widget->style, widget->window, widget->state);

  if (prev_style)
    egg_toolbar_update_button_relief (EGG_TOOLBAR (widget));
}

static void 
egg_toolbar_direction_changed (GtkWidget        *widget,
		   	       GtkTextDirection  previous_dir)
{
  EggToolbar *toolbar = EGG_TOOLBAR (widget);
  EggToolbarPrivate *priv = EGG_TOOLBAR_GET_PRIVATE (toolbar);
  
  if (toolbar->orientation == GTK_ORIENTATION_VERTICAL)
    {
      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
	gtk_arrow_set (GTK_ARROW (priv->arrow), GTK_ARROW_RIGHT, GTK_SHADOW_NONE);
      else 
	gtk_arrow_set (GTK_ARROW (priv->arrow), GTK_ARROW_LEFT, GTK_SHADOW_NONE);
    }

  GTK_WIDGET_CLASS (parent_class)->direction_changed (widget, previous_dir);
}

static gboolean
egg_toolbar_focus (GtkWidget        *widget,
		   GtkDirectionType  dir)
{
  /* Focus can't go in toolbars */
  
  return FALSE;
}

static void
style_change_notify (EggToolbar *toolbar)
{
  if (!toolbar->style_set)
    {
      /* pretend it was set, then unset, thus reverting to new default */
      toolbar->style_set = TRUE; 
      egg_toolbar_unset_style (toolbar);
    }
}

static void
icon_size_change_notify (EggToolbar *toolbar)
{
  if (!toolbar->icon_size_set)
    {
      /* pretend it was set, then unset, thus reverting to new default */
      toolbar->icon_size_set = TRUE; 
      egg_toolbar_unset_icon_size (toolbar);
    }
}

static GtkSettings *
toolbar_get_settings (EggToolbar *toolbar)
{
  return g_object_get_data (G_OBJECT (toolbar), "egg-toolbar-settings");
}

static void
egg_toolbar_screen_changed (GtkWidget *widget,
			    GdkScreen *previous_screen)
{
  EggToolbar *toolbar = EGG_TOOLBAR (widget);
  GtkSettings *old_settings = toolbar_get_settings (toolbar);
  GtkSettings *settings;

  if (gtk_widget_has_screen (GTK_WIDGET (toolbar)))
    settings = gtk_widget_get_settings (GTK_WIDGET (toolbar));
  else
    settings = NULL;

  if (settings == old_settings)
    return;

  if (old_settings)
    {
      g_signal_handler_disconnect (old_settings, toolbar->style_set_connection);
      g_signal_handler_disconnect (old_settings, toolbar->icon_size_connection);

      g_object_unref (old_settings);
    }

  if (settings)
    {
      toolbar->style_set_connection =
	g_signal_connect_swapped (settings,
				  "notify::gtk-toolbar-style",
				  G_CALLBACK (style_change_notify),
				  toolbar);
      toolbar->icon_size_connection =
	g_signal_connect_swapped (settings,
				  "notify::gtk-toolbar-icon-size",
				  G_CALLBACK (icon_size_change_notify),
				  toolbar);

      g_object_ref (settings);
      g_object_set_data (G_OBJECT (toolbar), "egg-toolbar-settings", settings);
    }
  else
    g_object_set_data (G_OBJECT (toolbar), "egg-toolbar-settings", NULL);

  style_change_notify (toolbar);
  icon_size_change_notify (toolbar);
}

static void
find_drop_pos(EggToolbar *toolbar, gint x, gint y,
	      gint *drop_index, gint *drop_pos)
{
  EggToolbarPrivate *priv = EGG_TOOLBAR_GET_PRIVATE (toolbar);
  GtkOrientation orientation;
  GtkTextDirection direction;
  GList *items;
  EggToolItem *item;
  gint border_width, ipadding;
  gint best_distance, best_pos, best_index, index;

  orientation = toolbar->orientation;
  direction = gtk_widget_get_direction (GTK_WIDGET (toolbar));
  border_width = GTK_CONTAINER (toolbar)->border_width;
  gtk_widget_style_get (GTK_WIDGET (toolbar), "internal_padding",
			&ipadding, NULL);
  border_width += ipadding;

  items = priv->items;
  if (!items)
    {
      *drop_index = 0;
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
	{
	  if (direction == GTK_TEXT_DIR_LTR) 
	    *drop_pos = border_width;
	  else
	    *drop_pos = GTK_WIDGET (toolbar)->allocation.width - border_width;
	}
      else
	{
	  *drop_pos = border_width;
	}
      return;
    }

  /* initial conditions */
  item = EGG_TOOL_ITEM (items->data);
  best_index = 0;
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (direction == GTK_TEXT_DIR_LTR)
	best_pos = GTK_WIDGET (item)->allocation.x;
      else
	best_pos = GTK_WIDGET (item)->allocation.x +
	  GTK_WIDGET (item)->allocation.width;
      best_distance = ABS (best_pos - x);
    }
  else
    {
      best_pos = GTK_WIDGET (item)->allocation.y;
      best_distance = ABS (best_pos - y);
    }

  index = 0;
  while (items)
    {
      item = EGG_TOOL_ITEM (items->data);
      index++;
      if (GTK_WIDGET_DRAWABLE (item) && !item->pack_end)
	{
	  gint pos, distance;

	  if (orientation == GTK_ORIENTATION_HORIZONTAL)
	    {
	      if (direction == GTK_TEXT_DIR_LTR)
		pos = GTK_WIDGET (item)->allocation.x +
		  GTK_WIDGET (item)->allocation.width;
	      else
		pos = GTK_WIDGET (item)->allocation.x;
	      distance = ABS (pos - x);
	    }
	  else
	    {
	      pos = GTK_WIDGET (item)->allocation.y +
		GTK_WIDGET (item)->allocation.height;
	      distance = ABS (pos - y);
	    }
	  if (distance < best_distance)
	    {
	      best_index = index;
	      best_pos = pos;
	      best_distance = distance;
	    }
	}
      items = items->next;
    }
  *drop_index = best_index;
  *drop_pos = best_pos;
}

static void
egg_toolbar_drag_leave (GtkWidget      *widget,
			GdkDragContext *context,
			guint           time_)
{
  EggToolbar *toolbar = EGG_TOOLBAR (widget);
  EggToolbarPrivate *priv = EGG_TOOLBAR_GET_PRIVATE (toolbar);

  if (priv->drag_highlight)
    {
      gdk_window_set_user_data (priv->drag_highlight, NULL);
      gdk_window_destroy (priv->drag_highlight);
      priv->drag_highlight = NULL;
    }

  priv->drop_index = -1;
}

static gboolean
egg_toolbar_drag_motion (GtkWidget      *widget,
			 GdkDragContext *context,
			 gint            x,
			 gint            y,
			 guint           time_)
{
  EggToolbar *toolbar = EGG_TOOLBAR (widget);
  EggToolbarPrivate *priv = EGG_TOOLBAR_GET_PRIVATE (toolbar);
  gint new_index, new_pos;

  find_drop_pos(toolbar, x, y, &new_index, &new_pos);

  if (!priv->drag_highlight)
    {
      GdkWindowAttr attributes;
      guint attributes_mask;

      attributes.window_type = GDK_WINDOW_CHILD;
      attributes.wclass = GDK_INPUT_OUTPUT;
      attributes.visual = gtk_widget_get_visual (widget);
      attributes.colormap = gtk_widget_get_colormap (widget);
      attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK | GDK_EXPOSURE_MASK | GDK_POINTER_MOTION_MASK;
      attributes_mask = GDK_WA_VISUAL | GDK_WA_COLORMAP;
      priv->drag_highlight = gdk_window_new (widget->window,
					     &attributes, attributes_mask);
      gdk_window_set_user_data (priv->drag_highlight, widget);
      gdk_window_set_background (priv->drag_highlight,
      				 &widget->style->fg[widget->state]);
    }

  if (priv->drop_index < 0 ||
      priv->drop_index != new_index)
    {
      gint border_width = GTK_CONTAINER (toolbar)->border_width;
      priv->drop_index = new_index;
      if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
	{
	  gdk_window_move_resize (priv->drag_highlight,
				  new_pos - 1, border_width,
				  2, widget->allocation.height-border_width*2);
	}
      else
	{
	  gdk_window_move_resize (priv->drag_highlight,
				  border_width, new_pos - 1,
				  widget->allocation.width-border_width*2, 2);
	}
    }

  gdk_window_show (priv->drag_highlight);
  gdk_window_raise (priv->drag_highlight);

  gdk_drag_status (context, context->suggested_action, time_);

  return TRUE;
}


static void
egg_toolbar_add (GtkContainer *container,
		 GtkWidget *widget)
{
  g_return_if_fail (EGG_IS_TOOLBAR (container));
  g_return_if_fail (EGG_IS_TOOL_ITEM (widget));

  egg_toolbar_append_tool_item (EGG_TOOLBAR (container),
				EGG_TOOL_ITEM (widget));
}

static void
egg_toolbar_remove (GtkContainer *container,
		    GtkWidget    *widget)
{
  g_return_if_fail (EGG_IS_TOOLBAR (container));
  g_return_if_fail (EGG_IS_TOOL_ITEM (widget));
  
  egg_toolbar_remove_tool_item (EGG_TOOLBAR (container),
				EGG_TOOL_ITEM (widget));
}

static void
egg_toolbar_forall (GtkContainer *container,
		    gboolean	  include_internals,
		    GtkCallback   callback,
		    gpointer      callback_data)
{
  EggToolbar *toolbar = EGG_TOOLBAR (container);
  EggToolbarPrivate *priv = EGG_TOOLBAR_GET_PRIVATE (toolbar);
  GList *items;

  g_return_if_fail (callback != NULL);

  items = priv->items;

  while (items)
    {
      EggToolItem *item = EGG_TOOL_ITEM (items->data);
      
      items = items->next;
      (*callback) (GTK_WIDGET (item), callback_data);
    }

  if (include_internals)
    (* callback) (priv->button, callback_data);
}

static GType
egg_toolbar_child_type (GtkContainer *container)
{
  return EGG_TYPE_TOOL_ITEM;
}

static void
egg_toolbar_real_orientation_changed (EggToolbar    *toolbar,
				      GtkOrientation orientation)
{
  EggToolbarPrivate *priv = EGG_TOOLBAR_GET_PRIVATE (toolbar);
  
  GList *items;
  
  if (toolbar->orientation != orientation)
    {
      toolbar->orientation = orientation;

      items = priv->items;
      while (items)
	{
	  EggToolItem *item = EGG_TOOL_ITEM (items->data);

	  egg_tool_item_set_orientation (item, orientation);

	  items = items->next;
	}

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
	gtk_arrow_set (GTK_ARROW (priv->arrow), GTK_ARROW_DOWN, GTK_SHADOW_NONE);
      else if (gtk_widget_get_direction (GTK_WIDGET (toolbar)) == GTK_TEXT_DIR_LTR)
	gtk_arrow_set (GTK_ARROW (priv->arrow), GTK_ARROW_RIGHT, GTK_SHADOW_NONE);
      else 
	gtk_arrow_set (GTK_ARROW (priv->arrow), GTK_ARROW_LEFT, GTK_SHADOW_NONE);
      
      gtk_widget_queue_resize (GTK_WIDGET (toolbar));
      g_object_notify (G_OBJECT (toolbar), "orientation");
    }
}

static void
egg_toolbar_real_style_changed (EggToolbar     *toolbar,
				GtkToolbarStyle style)
{
  EggToolbarPrivate *priv;
  GList *items;

  priv = EGG_TOOLBAR_GET_PRIVATE (toolbar);
  
  if (toolbar->style != style)
    {
      toolbar->style = style;

      items = priv->items;
      
      while (items)
	{
	  EggToolItem *item = EGG_TOOL_ITEM (items->data);
	  
	  egg_tool_item_set_toolbar_style (item, style);
	  
	  items = items->next;
	}
      
      gtk_widget_queue_resize (GTK_WIDGET (toolbar));
      g_object_notify (G_OBJECT (toolbar), "toolbar_style");
    }
}

static void
menu_position_func (GtkMenu  *menu,
		    gint     *x,
		    gint     *y,
		    gboolean *push_in,
		    gpointer  user_data)
{
  EggToolbar *toolbar = EGG_TOOLBAR (user_data);
  EggToolbarPrivate *priv = EGG_TOOLBAR_GET_PRIVATE (toolbar);
  GtkRequisition req;
  GtkRequisition menu_req;
  
  gdk_window_get_origin (GTK_BUTTON (priv->button)->event_window, x, y);
  gtk_widget_size_request (priv->button, &req);
  gtk_widget_size_request (GTK_WIDGET (menu), &menu_req);
  
  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      *y += priv->button->allocation.height;
      if (gtk_widget_get_direction (GTK_WIDGET (toolbar)) == GTK_TEXT_DIR_LTR) 
	*x += priv->button->allocation.width - req.width;
      else 
	*x += req.width - menu_req.width;
    }
  else 
    {
      if (gtk_widget_get_direction (GTK_WIDGET (toolbar)) == GTK_TEXT_DIR_LTR) 
	*x += priv->button->allocation.width;
      else 
	*x -= menu_req.width;
      *y += priv->button->allocation.height - req.height;      
    }
  
  *push_in = TRUE;
}

static void
menu_deactivated (GtkWidget *menu, GtkWidget *button)
{
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), FALSE);
}

static void
egg_toolbar_arrow_button_press (GtkWidget      *button,
    				GdkEventButton *event,
				EggToolbar     *toolbar)
{
  EggToolbarPrivate *priv = EGG_TOOLBAR_GET_PRIVATE (toolbar);  
  GtkWidget *menu;
  GtkWidget *menu_item;
  GList *items;
  
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
  
  menu = gtk_menu_new ();
  g_signal_connect (menu, "deactivate", G_CALLBACK (menu_deactivated), button);

  items = priv->first_non_fitting_item;
  while (items)
    {
      EggToolItem *item = EGG_TOOL_ITEM (items->data);

      if (TOOLBAR_ITEM_VISIBLE (item) && !item->pack_end)
	{
	  menu_item = NULL;
	  g_signal_emit_by_name (item, "create_menu_proxy", &menu_item);
	  
	  if (menu_item)
	    {
	      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
	    }
	}
      items = items->next;
    }

  gtk_widget_show_all (menu);

  gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
		  menu_position_func, toolbar,
		  event->button, event->time);
}

static gboolean
egg_toolbar_button_press (GtkWidget      *button,
    			  GdkEventButton *event,
			  EggToolbar     *toolbar)
{
  if (event->button == 3)
    {
      g_signal_emit (toolbar, toolbar_signals[POPUP_CONTEXT_MENU], 0, NULL);
      return FALSE;
    }

  return FALSE;
}

static void
egg_toolbar_update_button_relief (EggToolbar *toolbar)
{
  EggToolbarPrivate *priv = EGG_TOOLBAR_GET_PRIVATE (toolbar);
  GtkReliefStyle relief;
  GList *items;
  
  relief = get_button_relief (toolbar);

  items = priv->items;
  while (items)
    {
      EggToolItem *item = EGG_TOOL_ITEM (items->data);

      egg_tool_item_set_relief_style (item, relief);
      
      items = items->next;
    }

  gtk_button_set_relief (GTK_BUTTON (priv->button), relief);
}

static GtkReliefStyle
get_button_relief (EggToolbar *toolbar)
{
  GtkReliefStyle button_relief = GTK_RELIEF_NORMAL;

  gtk_widget_ensure_style (GTK_WIDGET (toolbar));
  
  gtk_widget_style_get (GTK_WIDGET (toolbar),
                        "button_relief", &button_relief,
                        NULL);

  return button_relief;
}

static gint
get_space_size (EggToolbar *toolbar)
{
  gint space_size = DEFAULT_SPACE_SIZE;

  gtk_widget_style_get (GTK_WIDGET (toolbar),
                        "space_size", &space_size,
                        NULL);

  return space_size;
}

static GtkToolbarSpaceStyle
get_space_style (EggToolbar *toolbar)
{
  GtkToolbarSpaceStyle space_style = DEFAULT_SPACE_STYLE;

  gtk_widget_style_get (GTK_WIDGET (toolbar),
                        "space_style", &space_style,
                        NULL);


  return space_style;  
}

GtkWidget *
egg_toolbar_new (void)
{
  EggToolbar *toolbar;

  toolbar = g_object_new (EGG_TYPE_TOOLBAR, NULL);

  return GTK_WIDGET (toolbar);
}

void
egg_toolbar_append_tool_item (EggToolbar  *toolbar,
			      EggToolItem *item)
{
  g_return_if_fail (EGG_IS_TOOLBAR (toolbar));
  g_return_if_fail (EGG_IS_TOOL_ITEM (item));

  egg_toolbar_insert_tool_item (toolbar, item, toolbar->num_children);
}

void
egg_toolbar_prepend_tool_item (EggToolbar  *toolbar,
			       EggToolItem *item)
{
  g_return_if_fail (EGG_IS_TOOLBAR (toolbar));
  g_return_if_fail (EGG_IS_TOOL_ITEM (item));

  egg_toolbar_insert_tool_item (toolbar, item, 0);
}

void
egg_toolbar_remove_tool_item (EggToolbar  *toolbar,
			      EggToolItem *item)
{
  EggToolbarPrivate *priv;
  GList *tmp;

  g_return_if_fail (EGG_IS_TOOLBAR (toolbar));
  g_return_if_fail (EGG_IS_TOOL_ITEM (item));
  
  priv = EGG_TOOLBAR_GET_PRIVATE (toolbar);
  
  for (tmp = priv->items; tmp != NULL; tmp = tmp->next)
    {
      GtkWidget *child = tmp->data;
      
      if (child == GTK_WIDGET (item))
	{
	  gboolean was_visible;
	  
	  was_visible = GTK_WIDGET_VISIBLE (item);
	  gtk_widget_unparent (GTK_WIDGET (item));

	  priv->items = g_list_remove_link (priv->items, tmp);
	  toolbar->num_children--;

	  if (was_visible && GTK_WIDGET_VISIBLE (toolbar))
	    gtk_widget_queue_resize (GTK_WIDGET (toolbar));

	  break;
	}
    }
}

void
egg_toolbar_insert_tool_item (EggToolbar  *toolbar,
			      EggToolItem *item,
			      gint pos)
{
  EggToolbarPrivate *priv;
  
  g_return_if_fail (EGG_IS_TOOLBAR (toolbar));
  g_return_if_fail (EGG_IS_TOOL_ITEM (item));

  priv = EGG_TOOLBAR_GET_PRIVATE (toolbar);
  
  priv->items = g_list_insert (priv->items, item, pos);
  toolbar->num_children++;

  egg_tool_item_set_orientation (item, toolbar->orientation);
  egg_tool_item_set_toolbar_style (item, toolbar->style);
  egg_tool_item_set_relief_style (item, get_button_relief (toolbar));
  
  gtk_widget_set_parent (GTK_WIDGET (item), GTK_WIDGET (toolbar));
  GTK_WIDGET_UNSET_FLAGS (item, GTK_CAN_FOCUS);  
}

gint
egg_toolbar_get_item_index (EggToolbar  *toolbar,
			    EggToolItem *item)
{
  EggToolbarPrivate *priv;

  g_return_val_if_fail (EGG_IS_TOOLBAR (toolbar), -1);
  g_return_val_if_fail (EGG_IS_TOOL_ITEM (item), -1);

  priv = EGG_TOOLBAR_GET_PRIVATE (toolbar);
  g_return_val_if_fail (g_list_find (priv->items, item) != NULL, -1);

  return g_list_index (priv->items, item);
}

void
egg_toolbar_set_orientation (EggToolbar     *toolbar,
			     GtkOrientation  orientation)
{
  g_return_if_fail (EGG_IS_TOOLBAR (toolbar));

  g_signal_emit (toolbar, toolbar_signals[ORIENTATION_CHANGED], 0, orientation);
}

GtkOrientation
egg_toolbar_get_orientation (EggToolbar *toolbar)
{
  g_return_val_if_fail (EGG_IS_TOOLBAR (toolbar), GTK_ORIENTATION_HORIZONTAL);

  return toolbar->orientation;
}

void
egg_toolbar_set_style (EggToolbar      *toolbar,
		       GtkToolbarStyle  style)
{
  g_return_if_fail (EGG_IS_TOOLBAR (toolbar));

  toolbar->style_set = TRUE;  
  g_signal_emit (toolbar, toolbar_signals[STYLE_CHANGED], 0, style);
}

GtkToolbarStyle
egg_toolbar_get_style (EggToolbar *toolbar)
{
  g_return_val_if_fail (EGG_IS_TOOLBAR (toolbar), DEFAULT_TOOLBAR_STYLE);

  return toolbar->style;
}

void
egg_toolbar_unset_style (EggToolbar *toolbar)
{
  GtkToolbarStyle style;

  g_return_if_fail (EGG_IS_TOOLBAR (toolbar));

  if (toolbar->style_set)
    {
      GtkSettings *settings = toolbar_get_settings (toolbar);

      if (settings)
	g_object_get (settings,
		      "gtk-toolbar-style", &style,
		      NULL);
      else
	style = DEFAULT_TOOLBAR_STYLE;

      if (style != toolbar->style)
	g_signal_emit (toolbar, toolbar_signals[STYLE_CHANGED], 0, style);

      toolbar->style_set = FALSE;
    }
}

void
egg_toolbar_set_tooltips (EggToolbar *toolbar,
			  gboolean    enable)
{
  g_return_if_fail (EGG_IS_TOOLBAR (toolbar));

  if (enable)
    gtk_tooltips_enable (toolbar->tooltips);
  else
    gtk_tooltips_disable (toolbar->tooltips);
}

gboolean
egg_toolbar_get_tooltips (EggToolbar *toolbar)
{
  g_return_val_if_fail (EGG_IS_TOOLBAR (toolbar), FALSE);

  return toolbar->tooltips->enabled;
}

GList*
egg_toolbar_get_tool_items (EggToolbar *toolbar)
{
  EggToolbarPrivate *priv = EGG_TOOLBAR_GET_PRIVATE (toolbar);

  g_return_val_if_fail (EGG_IS_TOOLBAR (toolbar), NULL);

  return priv->items;
}

void
egg_toolbar_set_icon_size (EggToolbar  *toolbar,
			   GtkIconSize  icon_size)
{
  GList *items;
  EggToolbarPrivate *priv = EGG_TOOLBAR_GET_PRIVATE (toolbar);
  
  g_return_if_fail (EGG_IS_TOOLBAR (toolbar));

  toolbar->icon_size_set = TRUE;

  if (toolbar->icon_size == icon_size)
    return;

  toolbar->icon_size = icon_size;

  items = priv->items;

  while (items)
    {
      EggToolItem *item = EGG_TOOL_ITEM (items->data);
	  
      egg_tool_item_set_icon_size (item, icon_size);
      
      items = items->next;
    }

  gtk_widget_queue_resize (GTK_WIDGET (toolbar));
}

GtkIconSize
egg_toolbar_get_icon_size (EggToolbar *toolbar)
{
  g_return_val_if_fail (EGG_IS_TOOLBAR (toolbar), DEFAULT_ICON_SIZE);

  return toolbar->icon_size;
}

void
egg_toolbar_unset_icon_size (EggToolbar *toolbar)
{
  GtkIconSize size;

  if (toolbar->icon_size_set)
    {
      GtkSettings *settings = toolbar_get_settings (toolbar);

      if (settings)
	g_object_get (settings,
		      "gtk-toolbar-icon-size", &size,
		      NULL);
      else
	size = DEFAULT_ICON_SIZE;

      if (size != toolbar->icon_size)
	egg_toolbar_set_icon_size (toolbar, size);

      toolbar->icon_size_set = FALSE;
    }
}


void
egg_toolbar_set_show_arrow (EggToolbar *toolbar,
			    gboolean    show_arrow)
{
  EggToolbarPrivate *priv;
  
  g_return_if_fail (EGG_IS_TOOLBAR (toolbar));

  priv = EGG_TOOLBAR_GET_PRIVATE (toolbar);
  show_arrow = show_arrow != FALSE;

  if (priv->show_arrow != show_arrow)
    {
      priv->show_arrow = show_arrow;
      
      if (!priv->show_arrow)
	gtk_widget_hide (priv->button);
      
      gtk_widget_queue_resize (GTK_WIDGET (toolbar));      
      g_object_notify (G_OBJECT (toolbar), "show_arrow");
    }
}

gboolean
egg_toolbar_get_show_arrow (EggToolbar *toolbar)
{
  EggToolbarPrivate *priv;

  g_return_val_if_fail (EGG_IS_TOOLBAR (toolbar), FALSE);

  priv = EGG_TOOLBAR_GET_PRIVATE (toolbar);
  
  return priv->show_arrow;
}

gint
egg_toolbar_get_drop_index (EggToolbar *toolbar,
			    gint        x,
			    gint        y)
{
  gint drop_index, drop_pos;

  g_return_val_if_fail (EGG_IS_TOOLBAR (toolbar), FALSE);

  find_drop_pos (toolbar, x, y, &drop_index, &drop_pos);

  return drop_index;
}

GtkWidget *
egg_toolbar_append_item (EggToolbar    *toolbar,
			 const char    *text,
			 const char    *tooltip_text,
			 const char    *tooltip_private_text,
			 GtkWidget     *icon,
			 GtkSignalFunc  callback,
			 gpointer       user_data)
{
  return egg_toolbar_insert_element (toolbar, EGG_TOOLBAR_CHILD_BUTTON,
				     NULL, text,
				     tooltip_text, tooltip_private_text,
				     icon, callback, user_data,
				     toolbar->num_children);
}

GtkWidget *
egg_toolbar_prepend_item (EggToolbar    *toolbar,
			  const char    *text,
			  const char    *tooltip_text,
			  const char    *tooltip_private_text,
			  GtkWidget     *icon,
			  GtkSignalFunc  callback,
			  gpointer       user_data)
{
  return egg_toolbar_insert_element (toolbar, EGG_TOOLBAR_CHILD_BUTTON,
				     NULL, text,
				     tooltip_text, tooltip_private_text,
				     icon, callback, user_data,
				     0);
}

GtkWidget *
egg_toolbar_insert_item (EggToolbar    *toolbar,
			 const char    *text,
			 const char    *tooltip_text,
			 const char    *tooltip_private_text,
			 GtkWidget     *icon,
			 GtkSignalFunc  callback,
			 gpointer       user_data,
			 gint           position)
{
  return egg_toolbar_insert_element (toolbar, EGG_TOOLBAR_CHILD_BUTTON,
				     NULL, text,
				     tooltip_text, tooltip_private_text,
				     icon, callback, user_data,
				     position);
}

GtkWidget*
egg_toolbar_insert_stock (EggToolbar      *toolbar,
			  const gchar     *stock_id,
			  const char      *tooltip_text,
			  const char      *tooltip_private_text,
			  GtkSignalFunc    callback,
			  gpointer         user_data,
			  gint             position)
{
  return egg_toolbar_internal_insert_element (toolbar, EGG_TOOLBAR_CHILD_BUTTON,
					      NULL, stock_id,
					      tooltip_text, tooltip_private_text,
					      NULL, callback, user_data,
					      position, TRUE);
}

void
egg_toolbar_append_space (EggToolbar *toolbar)
{
  egg_toolbar_insert_element (toolbar, EGG_TOOLBAR_CHILD_SPACE,
			      NULL, NULL,
			      NULL, NULL,
			      NULL, NULL, NULL,
			      toolbar->num_children);
}

void
egg_toolbar_prepend_space (EggToolbar *toolbar)
{
  egg_toolbar_insert_element (toolbar, EGG_TOOLBAR_CHILD_SPACE,
			      NULL, NULL,
			      NULL, NULL,
			      NULL, NULL, NULL,
			      0);
}

void
egg_toolbar_insert_space (EggToolbar *toolbar,
			  gint        position)
{
  egg_toolbar_insert_element (toolbar, EGG_TOOLBAR_CHILD_SPACE,
			      NULL, NULL,
			      NULL, NULL,
			      NULL, NULL, NULL,
			      position);
}

void
egg_toolbar_append_widget (EggToolbar  *toolbar,
			   GtkWidget   *widget,
			   const gchar *tooltip_text,
			   const gchar *tooltip_private_text)
{
  egg_toolbar_insert_element (toolbar, EGG_TOOLBAR_CHILD_WIDGET,
			      widget, NULL,
			      tooltip_text, tooltip_private_text,
			      NULL, NULL, NULL,
			      toolbar->num_children);
}

void
egg_toolbar_prepend_widget (EggToolbar  *toolbar,
			    GtkWidget   *widget,
			    const gchar *tooltip_text,
			    const gchar *tooltip_private_text)
{
  egg_toolbar_insert_element (toolbar, EGG_TOOLBAR_CHILD_WIDGET,
			      widget, NULL,
			      tooltip_text, tooltip_private_text,
			      NULL, NULL, NULL,
			      0);
}

void
egg_toolbar_insert_widget (EggToolbar *toolbar,
			   GtkWidget  *widget,
			   const char *tooltip_text,
			   const char *tooltip_private_text,
			   gint        position)
{
  egg_toolbar_insert_element (toolbar, EGG_TOOLBAR_CHILD_WIDGET,
			      widget, NULL,
			      tooltip_text, tooltip_private_text,
			      NULL, NULL, NULL,
			      position);
}

GtkWidget*
egg_toolbar_append_element (EggToolbar          *toolbar,
			    GtkToolbarChildType  type,
			    GtkWidget           *widget,
			    const char          *text,
			    const char          *tooltip_text,
			    const char          *tooltip_private_text,
			    GtkWidget           *icon,
			    GtkSignalFunc        callback,
			    gpointer             user_data)
{
  return egg_toolbar_insert_element (toolbar, type, widget, text,
				     tooltip_text, tooltip_private_text,
				     icon, callback, user_data,
				     toolbar->num_children);
}

GtkWidget *
egg_toolbar_prepend_element (EggToolbar          *toolbar,
			     GtkToolbarChildType  type,
			     GtkWidget           *widget,
			     const char          *text,
			     const char          *tooltip_text,
			     const char          *tooltip_private_text,
			     GtkWidget           *icon,
			     GtkSignalFunc        callback,
			     gpointer             user_data)
{
  return egg_toolbar_insert_element (toolbar, type, widget, text,
				     tooltip_text, tooltip_private_text,
				     icon, callback, user_data, 0);
}

GtkWidget *
egg_toolbar_insert_element (EggToolbar          *toolbar,
			    EggToolbarChildType  type,
			    GtkWidget           *widget,
			    const char          *text,
			    const char          *tooltip_text,
			    const char          *tooltip_private_text,
			    GtkWidget           *icon,
			    GtkSignalFunc        callback,
			    gpointer             user_data,
			    gint                 position)
{
  return egg_toolbar_internal_insert_element (toolbar, type, widget, text,
					      tooltip_text, tooltip_private_text,
					      icon, callback, user_data, position, FALSE);
}

static GtkWidget *
egg_toolbar_internal_insert_element (EggToolbar          *toolbar,
				     EggToolbarChildType  type,
				     GtkWidget           *widget,
				     const char          *text,
				     const char          *tooltip_text,
				     const char          *tooltip_private_text,
				     GtkWidget           *icon,
				     GtkSignalFunc        callback,
				     gpointer             user_data,
				     gint                 position,
				     gboolean             use_stock)
{
  EggToolbarChild *child;
  EggToolItem *item = NULL;
  
  g_return_val_if_fail (EGG_IS_TOOLBAR (toolbar), NULL);
  
  if (type == EGG_TOOLBAR_CHILD_WIDGET)
    g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  else if (type != EGG_TOOLBAR_CHILD_RADIOBUTTON)
    g_return_val_if_fail (widget == NULL, NULL);

  child = g_new (EggToolbarChild, 1);

  child->type = type;
  child->icon = NULL;
  child->label = NULL;

  switch (type)
    {
    case EGG_TOOLBAR_CHILD_SPACE:
      item = egg_separator_tool_item_new ();
      child->widget = NULL;
      break;

    case EGG_TOOLBAR_CHILD_WIDGET:
      item = egg_tool_item_new ();
      child->widget = widget;
      gtk_container_add (GTK_CONTAINER (item), child->widget);
      
      break;

    case EGG_TOOLBAR_CHILD_BUTTON:
      item = egg_tool_button_new ();
      child->widget = EGG_TOOL_BUTTON (item)->button;
      break;
      
    case EGG_TOOLBAR_CHILD_TOGGLEBUTTON:
      item = egg_toggle_tool_button_new ();
      child->widget = EGG_TOOL_BUTTON (item)->button;
      break;

    case EGG_TOOLBAR_CHILD_RADIOBUTTON:
      item = egg_radio_tool_button_new (widget
					? gtk_radio_button_get_group (GTK_RADIO_BUTTON (widget))
					: NULL);
      child->widget = EGG_TOOL_BUTTON (item)->button;
      break;
    }

  /*
   * We need to connect to the button's clicked callback because some
   * programs may rely on that the widget in the callback is a GtkButton
   */
  if (callback)
    g_signal_connect (child->widget, "clicked",
		      callback, user_data);
  
  if (type == EGG_TOOLBAR_CHILD_BUTTON ||
      type == EGG_TOOLBAR_CHILD_RADIOBUTTON ||
      type == EGG_TOOLBAR_CHILD_TOGGLEBUTTON)
    {
      if (text)
	{
	  child->label = EGG_TOOL_BUTTON (item)->label;

	  if (use_stock)
	    g_object_set (G_OBJECT (item), "stock_id", text, NULL);
	  else
	    egg_tool_button_set_label (EGG_TOOL_BUTTON (item), text);	    
	}

      if (icon)
	{
	  child->icon = icon;
	  egg_tool_button_set_icon_widget (EGG_TOOL_BUTTON (item), icon);
	}

    }

  if ((type != GTK_TOOLBAR_CHILD_SPACE) && tooltip_text)
    egg_tool_item_set_tooltip (item, toolbar->tooltips,
			       tooltip_text, tooltip_private_text);
  
  toolbar->children = g_list_insert (toolbar->children, child, position);
  egg_toolbar_insert_tool_item (toolbar, item, position);

  return child->widget;
}
