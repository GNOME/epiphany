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

#include "eggtoolitem.h"
#include "eggmarshalers.h"

#ifndef _
#  define _(s) (s)
#endif

enum {
  CLICKED, 
  CREATE_MENU_PROXY,
  SET_ORIENTATION,
  SET_ICON_SIZE,
  SET_TOOLBAR_STYLE,
  SET_RELIEF_STYLE,
  SET_TOOLTIP,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_VISIBLE_HORIZONTAL,
  PROP_VISIBLE_VERTICAL,
  PROP_HOMOGENEOUS,
  PROP_ORIENTATION
};

static void egg_tool_item_init       (EggToolItem *toolitem);
static void egg_tool_item_class_init (EggToolItemClass *class);

static void egg_tool_item_set_property (GObject         *object,
					guint            prop_id,
					const GValue    *value,
					GParamSpec      *pspec);
static void egg_tool_item_get_property (GObject         *object,
					guint            prop_id,
					GValue          *value,
					GParamSpec      *pspec);

static void egg_tool_item_size_request  (GtkWidget      *widget,
					 GtkRequisition *requisition);
static void egg_tool_item_size_allocate (GtkWidget      *widget,
					 GtkAllocation  *allocation);

static GtkWidget *egg_tool_item_create_menu_proxy (EggToolItem *item);


static GObjectClass *parent_class = NULL;
static guint         toolitem_signals[LAST_SIGNAL] = { 0 };

GType
egg_tool_item_get_type (void)
{
  static GtkType type = 0;

  if (!type)
    {
      static const GTypeInfo type_info =
	{
	  sizeof (EggToolItemClass),
	  (GBaseInitFunc) NULL,
	  (GBaseFinalizeFunc) NULL,
	  (GClassInitFunc) egg_tool_item_class_init,
	  (GClassFinalizeFunc) NULL,
	  NULL,
        
	  sizeof (EggToolItem),
	  0, /* n_preallocs */
	  (GInstanceInitFunc) egg_tool_item_init,
	};

      type = g_type_register_static (GTK_TYPE_BIN,
				     "EggToolItem",
				     &type_info, 0);
    }
  return type;
}

static gboolean
create_proxy_accumulator (GSignalInvocationHint *hint,
			  GValue *return_accumulator,
			  const GValue *handler_return,
			  gpointer user_data)
{
  GObject *proxy;
  gboolean continue_emission;

  proxy = g_value_get_object(handler_return);
  g_value_set_object(return_accumulator, proxy);
  continue_emission = (proxy == NULL);

  return continue_emission;
}

static void
egg_tool_item_class_init (EggToolItemClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  
  parent_class = g_type_class_peek_parent (klass);
  object_class = (GObjectClass *)klass;
  widget_class = (GtkWidgetClass *)klass;
  
  object_class->set_property = egg_tool_item_set_property;
  object_class->get_property = egg_tool_item_get_property;

  widget_class->size_request = egg_tool_item_size_request;
  widget_class->size_allocate = egg_tool_item_size_allocate;

  klass->create_menu_proxy = egg_tool_item_create_menu_proxy;
  
  g_object_class_install_property (object_class,
				   PROP_VISIBLE_HORIZONTAL,
				   g_param_spec_boolean ("visible_horizontal",
							 _("Visible when horizontal"),
							 _("Whether the toolbar item is visible when the toolbar is in a horizontal orientation."),
							 TRUE,
							 G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
				   PROP_VISIBLE_VERTICAL,
				   g_param_spec_boolean ("visible_vertical",
							 _("Visible when vertical"),
							 _("Whether the toolbar item is visible when the toolbar is in a vertical orientation."),
							 TRUE,
							 G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
				   PROP_HOMOGENEOUS,
				   g_param_spec_boolean ("homogeneous",
							 _("Homogeneous size"),
							 _("Whether the toolbar item should be the same size as other homogeneous items."),
							 FALSE,
							 G_PARAM_READWRITE));

  toolitem_signals[CLICKED] =
    g_signal_new ("clicked",
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (EggToolItemClass, clicked),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  
  toolitem_signals[CREATE_MENU_PROXY] =
    g_signal_new ("create_menu_proxy",
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (EggToolItemClass, create_menu_proxy),
		  create_proxy_accumulator, NULL,
		  _egg_marshal_OBJECT__VOID,
		  GTK_TYPE_WIDGET, 0);
  toolitem_signals[SET_ORIENTATION] =
    g_signal_new ("set_orientation",
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (EggToolItemClass, set_orientation),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__ENUM,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_ORIENTATION);
  toolitem_signals[SET_ICON_SIZE] =
    g_signal_new ("set_icon_size",
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (EggToolItemClass, set_icon_size),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__ENUM,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_ICON_SIZE);
  toolitem_signals[SET_TOOLBAR_STYLE] =
    g_signal_new ("set_toolbar_style",
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (EggToolItemClass, set_toolbar_style),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__ENUM,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_TOOLBAR_STYLE);
  toolitem_signals[SET_RELIEF_STYLE] =
    g_signal_new ("set_relief_style",
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (EggToolItemClass, set_relief_style),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__ENUM,
		  G_TYPE_NONE, 1, 
		  GTK_TYPE_RELIEF_STYLE);
  toolitem_signals[SET_TOOLTIP] =
    g_signal_new ("set_tooltip",
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (EggToolItemClass, set_tooltip),
		  NULL, NULL,
		  _egg_marshal_VOID__OBJECT_STRING_STRING,
		  G_TYPE_NONE, 3,
		  GTK_TYPE_TOOLTIPS,
		  G_TYPE_STRING,
		  G_TYPE_STRING);		  
		  
}

static void
egg_tool_item_init (EggToolItem *toolitem)
{
  toolitem->visible_horizontal = TRUE;
  toolitem->visible_vertical = TRUE;
  toolitem->homogeneous = FALSE;

  toolitem->orientation = GTK_ORIENTATION_HORIZONTAL;
  toolitem->icon_size = GTK_ICON_SIZE_LARGE_TOOLBAR;
  toolitem->style = GTK_TOOLBAR_BOTH;
}

static void
egg_tool_item_set_property (GObject         *object,
			    guint            prop_id,
			    const GValue    *value,
			    GParamSpec      *pspec)
{
  EggToolItem *toolitem = EGG_TOOL_ITEM (object);

  switch (prop_id)
    {
    case PROP_VISIBLE_HORIZONTAL:
      toolitem->visible_horizontal = g_value_get_boolean (value);
      break;
    case PROP_VISIBLE_VERTICAL:
      toolitem->visible_vertical = g_value_get_boolean (value);
      break;
    case PROP_HOMOGENEOUS:
      toolitem->homogeneous = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
egg_tool_item_get_property (GObject         *object,
			    guint            prop_id,
			    GValue          *value,
			    GParamSpec      *pspec)
{
  EggToolItem *toolitem = EGG_TOOL_ITEM (object);

  switch (prop_id)
    {
    case PROP_VISIBLE_HORIZONTAL:
      g_value_set_boolean (value, toolitem->visible_horizontal);
      break;
    case PROP_VISIBLE_VERTICAL:
      g_value_set_boolean (value, toolitem->visible_vertical);
      break;
    case PROP_HOMOGENEOUS:
      g_value_set_boolean (value, toolitem->homogeneous);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
egg_tool_item_size_request (GtkWidget      *widget,
			    GtkRequisition *requisition)
{
  GtkBin *bin = GTK_BIN (widget);

  if (bin->child)
    gtk_widget_size_request (bin->child, requisition);
  
  requisition->width += GTK_CONTAINER (widget)->border_width * 2;
  requisition->height += GTK_CONTAINER (widget)->border_width * 2;  
}

static void
egg_tool_item_size_allocate (GtkWidget     *widget,
			     GtkAllocation *allocation)
{
  GtkBin *bin = GTK_BIN (widget);
  GtkAllocation child_allocation;

  widget->allocation = *allocation;

  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      child_allocation.x = allocation->x + GTK_CONTAINER (widget)->border_width;
      child_allocation.y = allocation->y + GTK_CONTAINER (widget)->border_width;
      child_allocation.width = allocation->width - GTK_CONTAINER (widget)->border_width * 2;
      child_allocation.height = allocation->height - GTK_CONTAINER (widget)->border_width * 2;
      
      gtk_widget_size_allocate (bin->child, &child_allocation);
    }
}

static GtkWidget *
egg_tool_item_create_menu_proxy (EggToolItem *item)
{
  if (GTK_BIN (item)->child)
    return NULL;
  else
    return gtk_separator_menu_item_new ();
}

EggToolItem *
egg_tool_item_new (void)
{
  EggToolItem *item;

  item = g_object_new (EGG_TYPE_TOOL_ITEM, NULL);

  return item;
}

void
egg_tool_item_set_orientation (EggToolItem *tool_item,
			       GtkOrientation orientation)
{
  g_return_if_fail (EGG_IS_TOOL_ITEM (tool_item));
  
  g_signal_emit (tool_item, toolitem_signals[SET_ORIENTATION], 0, orientation);
}

void
egg_tool_item_set_icon_size (EggToolItem *tool_item,
			     GtkIconSize icon_size)
{
  g_return_if_fail (EGG_IS_TOOL_ITEM (tool_item));
    
  g_signal_emit (tool_item, toolitem_signals[SET_ICON_SIZE], 0, icon_size);
}

void
egg_tool_item_set_toolbar_style (EggToolItem    *tool_item,
				 GtkToolbarStyle style)
{
  g_return_if_fail (EGG_IS_TOOL_ITEM (tool_item));
    
  g_signal_emit (tool_item, toolitem_signals[SET_TOOLBAR_STYLE], 0, style);
}

void
egg_tool_item_set_relief_style (EggToolItem   *tool_item,
				GtkReliefStyle style)
{
  g_return_if_fail (EGG_IS_TOOL_ITEM (tool_item));
    
  g_signal_emit (tool_item, toolitem_signals[SET_RELIEF_STYLE], 0, style);
}

void
egg_tool_item_set_expandable (EggToolItem *tool_item,
			      gboolean     expandable)
{
  if ((expandable && tool_item->expandable) ||
      (!expandable && !tool_item->expandable))
    return;

  tool_item->expandable = expandable;
  gtk_widget_queue_resize (GTK_WIDGET (tool_item));
}

void
egg_tool_item_set_pack_end (EggToolItem *tool_item,
			    gboolean pack_end)
{
  if ((pack_end && tool_item->pack_end) ||
      (!pack_end && !tool_item->pack_end))
    return;

  tool_item->pack_end = pack_end;
  gtk_widget_queue_resize (GTK_WIDGET (tool_item));
}

void
egg_tool_item_set_homogeneous (EggToolItem *tool_item,
			       gboolean     homogeneous)
{
  if ((homogeneous && tool_item->homogeneous) ||
      (!homogeneous && !tool_item->homogeneous))
    return;

  tool_item->homogeneous = homogeneous;
  gtk_widget_queue_resize (GTK_WIDGET (tool_item));
}

void
egg_tool_item_set_tooltip (EggToolItem *tool_item,
			   GtkTooltips *tooltips,
			   const gchar *tip_text,
			   const gchar *tip_private)
{
  g_return_if_fail (EGG_IS_TOOL_ITEM (tool_item));

  g_signal_emit (tool_item, toolitem_signals[SET_TOOLTIP], 0,
		 tooltips, tip_text, tip_private);
}
