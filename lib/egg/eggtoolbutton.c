/* eggtoolbutton.c
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

#include "eggtoolbutton.h"
#include <gtk/gtkbutton.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkiconfactory.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkimagemenuitem.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkvbox.h>

#include <string.h>

#ifndef _
#  define _(s) (s)
#endif

enum {
  PROP_0,
  PROP_LABEL,
  PROP_USE_UNDERLINE,
  PROP_STOCK_ID,
  PROP_ICON_SET,
  PROP_ICON_WIDGET,
};

static void egg_tool_button_init       (EggToolButton      *button,
					EggToolButtonClass *klass);
static void egg_tool_button_class_init (EggToolButtonClass *klass);


static void egg_tool_button_set_property (GObject      *object,
					  guint         prop_id,
					  const GValue *value,
					  GParamSpec   *pspec);
static void egg_tool_button_get_property (GObject      *object,
					  guint         prop_id,
					  GValue       *value,
					  GParamSpec   *pspec);
static void egg_tool_button_finalize     (GObject      *object);

static void egg_tool_button_show_all (GtkWidget *widget);

static GtkWidget *egg_tool_button_create_menu_proxy (EggToolItem     *item);
static void       egg_tool_button_set_orientation   (EggToolItem     *tool_item,
						     GtkOrientation   orientation);
static void       egg_tool_button_set_icon_size     (EggToolItem     *tool_item,
						     GtkIconSize      icon_size);
static void       egg_tool_button_set_toolbar_style (EggToolItem     *tool_item,
						     GtkToolbarStyle  style);
static void       egg_tool_button_set_relief_style  (EggToolItem     *tool_item,
						     GtkReliefStyle   style);
static void       egg_tool_button_set_tooltip       (EggToolItem     *tool_item,
						     GtkTooltips     *tooltips,
						     const gchar     *tip_text,
						     const gchar     *tip_private);
static void       button_clicked                    (GtkWidget       *widget,
						     EggToolButton   *button);

static GObjectClass *parent_class = NULL;

GType
egg_tool_button_get_type (void)
{
  static GtkType type = 0;

  if (!type)
    {
      static const GTypeInfo type_info =
	{
	  sizeof (EggToolButtonClass),
	  (GBaseInitFunc) NULL,
	  (GBaseFinalizeFunc) NULL,
	  (GClassInitFunc) egg_tool_button_class_init,
	  (GClassFinalizeFunc) NULL,
	  NULL,
	  sizeof (EggToolButton),
	  0, /* n_preallocs */
	  (GInstanceInitFunc) egg_tool_button_init,
	};

      type = g_type_register_static (EGG_TYPE_TOOL_ITEM,
				     "EggToolButton",
				     &type_info, 0);
    }
  return type;
}

static void
egg_tool_button_class_init (EggToolButtonClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  EggToolItemClass *tool_item_class;
  
  parent_class = g_type_class_peek_parent (klass);
  
  object_class = (GObjectClass *)klass;
  widget_class = (GtkWidgetClass *)klass;
  tool_item_class = (EggToolItemClass *)klass;
  
  object_class->set_property = egg_tool_button_set_property;
  object_class->get_property = egg_tool_button_get_property;
  object_class->finalize = egg_tool_button_finalize;

  widget_class->show_all = egg_tool_button_show_all;

  tool_item_class->create_menu_proxy = egg_tool_button_create_menu_proxy;
  tool_item_class->set_orientation = egg_tool_button_set_orientation;
  tool_item_class->set_icon_size = egg_tool_button_set_icon_size;
  tool_item_class->set_toolbar_style = egg_tool_button_set_toolbar_style;
  tool_item_class->set_relief_style = egg_tool_button_set_relief_style;
  tool_item_class->set_tooltip = egg_tool_button_set_tooltip;
  
  klass->button_type = GTK_TYPE_BUTTON;

  g_object_class_install_property (object_class,
				   PROP_LABEL,
				   g_param_spec_string ("label",
							_("Label"),
							_("Text to show in the item."),
							NULL,
							G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
				   PROP_USE_UNDERLINE,
				   g_param_spec_boolean ("use_underline",
							 _("Use underline"),
							 _("Interpret underlines in the item label."),
							 FALSE,
							 G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
				   PROP_STOCK_ID,
				   g_param_spec_string ("stock_id",
							_("Stock Id"),
							_("The stock icon displayed on the item."),
							NULL,
							G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
				   PROP_ICON_SET,
				   g_param_spec_boxed ("icon_set",
						       _("Icon set"),
						       _("Icon set to use to draw the item's icon."),
						       GTK_TYPE_ICON_SET,
						       G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
				   PROP_ICON_WIDGET,
				   g_param_spec_object ("icon_widget",
							_("Icon widget"),
							_("Icon widget to display in the item."),
							GTK_TYPE_WIDGET,
							G_PARAM_READWRITE));
}

static void
egg_tool_button_init (EggToolButton *button, EggToolButtonClass *klass)
{
  EggToolItem *toolitem = EGG_TOOL_ITEM (button);
  
  toolitem->homogeneous = TRUE;

  /* create button */
  button->button = g_object_new (klass->button_type, NULL);
  GTK_WIDGET_UNSET_FLAGS (button->button, GTK_CAN_FOCUS);
  g_signal_connect_object (button->button, "clicked",
			   G_CALLBACK (button_clicked), button, 0);

  button->box = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (button->button), button->box);
  gtk_widget_show (button->box);

#if 0
  button->icon = gtk_image_new();
  gtk_box_pack_start (GTK_BOX (button->box), button->icon, TRUE, TRUE, 0);
  gtk_widget_show (button->icon);
#endif

  button->label = gtk_label_new (NULL);
  gtk_label_set_use_underline (GTK_LABEL (button->label), TRUE);
  gtk_box_pack_start (GTK_BOX (button->box), button->label, FALSE, TRUE, 0);
  gtk_widget_show (button->label);

  gtk_container_add (GTK_CONTAINER (button), button->button);
  gtk_widget_show (button->button);
}

static gchar *
elide_underscores (const gchar *original)
{
  gchar *q, *result;
  const gchar *p;
  gboolean last_underscore;

  q = result = g_malloc (strlen (original) + 1);
  last_underscore = FALSE;
  
  for (p = original; *p; p++)
    {
      if (!last_underscore && *p == '_')
	last_underscore = TRUE;
      else
	{
	  last_underscore = FALSE;
	  *q++ = *p;
	}
    }
  
  *q = '\0';
  
  return result;
}

static void
egg_tool_button_set_property (GObject         *object,
			      guint            prop_id,
			      const GValue    *value,
			      GParamSpec      *pspec)
{
  EggToolButton *button = EGG_TOOL_BUTTON (object);
  GtkStockItem stock_item;
  GtkIconSet *icon_set;
  gchar *label_no_mnemonic = NULL;
  
  switch (prop_id)
    {
    case PROP_LABEL:
      egg_tool_button_set_label (button, g_value_get_string (value));
      break;
    case PROP_USE_UNDERLINE:
      gtk_label_set_use_underline (GTK_LABEL (button->label),
				   g_value_get_boolean (value));
      break;
    case PROP_STOCK_ID:
      g_free (button->stock_id);
      button->stock_id = g_value_dup_string (value);
      if (!button->label_set)
	{
	  if (gtk_stock_lookup (button->stock_id, &stock_item))
	    {
	      label_no_mnemonic = elide_underscores (stock_item.label);
	      gtk_label_set_label (GTK_LABEL (button->label), label_no_mnemonic);
	      g_free (label_no_mnemonic);
	    }
	}
      if (!button->icon_set)
	{
	  if (button->icon && !GTK_IS_IMAGE (button->icon))
	    {
	      gtk_container_remove (GTK_CONTAINER (button->box), button->icon);
	      button->icon = NULL;
	    }
	  if (!button->icon)
	    {
	      button->icon = gtk_image_new ();
	      gtk_box_pack_start (GTK_BOX (button->box), button->icon,
				  TRUE, TRUE, 0);
	      gtk_box_reorder_child (GTK_BOX (button->box), button->icon, 0);
	    }
	  gtk_image_set_from_stock (GTK_IMAGE (button->icon), button->stock_id,
				    EGG_TOOL_ITEM (button)->icon_size);
          if (EGG_TOOL_ITEM (button)->style != GTK_TOOLBAR_TEXT)
            gtk_widget_show (button->icon);
	}
      break;
    case PROP_ICON_SET:
      if (button->icon && !GTK_IS_IMAGE (button->icon))
	{
	  gtk_container_remove (GTK_CONTAINER (button->box), button->icon);
	  button->icon = NULL;
	}
      if (!button->icon)
	{
	  button->icon = gtk_image_new ();
	  gtk_box_pack_start (GTK_BOX (button->box), button->icon,
			      TRUE, TRUE, 0);
	  gtk_box_reorder_child (GTK_BOX (button->box), button->icon, 0);
	}
      icon_set = g_value_get_boxed (value);
      button->icon_set = (icon_set != NULL);
      if (!button->icon_set && button->stock_id)
	gtk_image_set_from_stock (GTK_IMAGE (button->icon), button->stock_id,
				  EGG_TOOL_ITEM (button)->icon_size);
      else
	gtk_image_set_from_icon_set (GTK_IMAGE (button->icon), icon_set,
				     EGG_TOOL_ITEM (button)->icon_size);
      break;
    case PROP_ICON_WIDGET:
      egg_tool_button_set_icon_widget (button, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
egg_tool_button_get_property (GObject         *object,
			      guint            prop_id,
			      GValue          *value,
			      GParamSpec      *pspec)
{
  EggToolButton *button = EGG_TOOL_BUTTON (object);

  switch (prop_id)
    {
    case PROP_LABEL:
      if (button->label_set)
	g_value_set_string (value,
			    gtk_label_get_label (GTK_LABEL (button->label)));
      break;
    case PROP_USE_UNDERLINE:
      g_value_set_boolean (value,
		gtk_label_get_use_underline (GTK_LABEL (button->label)));
      break;
    case PROP_STOCK_ID:
      g_value_set_string (value, button->stock_id);
      break;
    case PROP_ICON_SET:
      if (GTK_IS_IMAGE (button->icon) &&
	  GTK_IMAGE (button->icon)->storage_type == GTK_IMAGE_ICON_SET)
	{
	  GtkIconSet *icon_set;
	  gtk_image_get_icon_set (GTK_IMAGE (button->icon), &icon_set, NULL);
	  g_value_set_boxed (value, icon_set);
	}
      else
	g_value_set_boxed (value, NULL);
      break;
    case PROP_ICON_WIDGET:
      g_value_set_object (value, button->icon);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
egg_tool_button_finalize (GObject *object)
{
  EggToolButton *button = EGG_TOOL_BUTTON (object);

  g_free (button->stock_id);
  button->stock_id = NULL;

  parent_class->finalize (object);
}

static void
egg_tool_button_show_all (GtkWidget *widget)
{
  EggToolButton *button = EGG_TOOL_BUTTON (widget);

  switch (EGG_TOOL_ITEM (widget)->style)
    {
    case GTK_TOOLBAR_ICONS:
      if (button->icon) gtk_widget_show_all (button->icon);
      gtk_widget_hide (button->label);
      gtk_widget_show (button->box);
      gtk_widget_show (button->button);
      break;
    case GTK_TOOLBAR_TEXT:
      if (button->icon) gtk_widget_hide (button->icon);
      gtk_widget_show_all (button->label);
      gtk_widget_show (button->box);
      gtk_widget_show (button->button);
      break;
    case GTK_TOOLBAR_BOTH:
    case GTK_TOOLBAR_BOTH_HORIZ:
      gtk_widget_show_all (button->button);
    }

  gtk_widget_show (GTK_WIDGET (button));
}

static GtkWidget *
egg_tool_button_create_menu_proxy (EggToolItem *item)
{
  EggToolButton *button = EGG_TOOL_BUTTON (item);
  GtkWidget *menu_item;
  GtkWidget *image;
  const char *label;

  label = gtk_label_get_text (GTK_LABEL (button->label));
  
  menu_item = gtk_image_menu_item_new_with_label (label);

  if (GTK_IS_IMAGE (button->icon))
    {
      image = gtk_image_new ();

      if (GTK_IMAGE (button->icon)->storage_type == GTK_IMAGE_STOCK)
	{
	  gchar *stock_id;

	  gtk_image_get_stock (GTK_IMAGE (button->icon),
			       &stock_id, NULL);
	  gtk_image_set_from_stock (GTK_IMAGE (image), stock_id,
				    GTK_ICON_SIZE_MENU);
	}
      else if (GTK_IMAGE (button->icon)->storage_type == GTK_IMAGE_ICON_SET)
	{
	  GtkIconSet *icon_set;

	  gtk_image_get_icon_set (GTK_IMAGE (button->icon), &icon_set, NULL);
	  gtk_image_set_from_icon_set (GTK_IMAGE (image), icon_set,
				       GTK_ICON_SIZE_MENU);
	}
      else
	{
	  g_warning ("FIXME: Add more cases here");
	}
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item), image);
    }

  g_signal_connect_object (menu_item, "activate",
			   G_CALLBACK (gtk_button_clicked),
			   EGG_TOOL_BUTTON (button)->button,
			   G_CONNECT_SWAPPED);

  return menu_item;
}

static void
egg_tool_button_set_orientation (EggToolItem   *tool_item,
				 GtkOrientation orientation)
{
  if (tool_item->orientation != orientation)
    {
      tool_item->orientation = orientation;
    }
}

static void
egg_tool_button_set_icon_size (EggToolItem  *tool_item,
			       GtkIconSize   icon_size)
{
  if (tool_item->icon_size != icon_size)
    {
      EggToolButton *button = EGG_TOOL_BUTTON (tool_item);
      char *stock_id;

      if (button->icon && GTK_IS_IMAGE (button->icon) &&
	  gtk_image_get_storage_type (GTK_IMAGE (button->icon)) == GTK_IMAGE_STOCK)
	{
	  gtk_image_get_stock (GTK_IMAGE (button->icon), &stock_id, NULL);
	  stock_id = g_strdup (stock_id);
	  gtk_image_set_from_stock (GTK_IMAGE (button->icon),
				    stock_id,
				    icon_size);
	  g_free (stock_id);
	}
      tool_item->icon_size = icon_size;
    }
}

static void
egg_tool_button_set_toolbar_style (EggToolItem    *tool_item,
				   GtkToolbarStyle style)
{
  EggToolButton *button = EGG_TOOL_BUTTON (tool_item);
  
  if (tool_item->style != style)
    {
      tool_item->style = style;

      switch (tool_item->style)
	{
	case GTK_TOOLBAR_ICONS:
	  gtk_widget_hide (button->label);
	  if (button->icon) {
	    gtk_box_set_child_packing (GTK_BOX (button->box), button->icon,
				       TRUE, TRUE, 0, GTK_PACK_START);
	    gtk_widget_show (button->icon);
	  }
	  break;
	case GTK_TOOLBAR_TEXT:
	  gtk_box_set_child_packing (GTK_BOX (button->box), button->label,
				     TRUE, TRUE, 0, GTK_PACK_START);
	  gtk_widget_show (button->label);
	  if (button->icon)
	    gtk_widget_hide (button->icon);
	  break;
	case GTK_TOOLBAR_BOTH:
	  if (GTK_IS_HBOX (button->box))
	    {
	      GtkWidget *vbox;
	      
              vbox = gtk_vbox_new (FALSE, 0);
	      gtk_widget_show (vbox);

	      if (button->icon)
		{
		  g_object_ref (button->icon);
		  gtk_container_remove (GTK_CONTAINER (button->box), button->icon);
		  gtk_box_pack_start (GTK_BOX (vbox), button->icon,
				      TRUE, TRUE, 0);
		  g_object_unref (button->icon);
		}
	      
	      g_object_ref (button->label);
	      gtk_container_remove (GTK_CONTAINER (button->box), button->label);
	      gtk_box_pack_start (GTK_BOX (vbox), button->label, FALSE, TRUE, 0);
	      g_object_unref (button->label);
	      
	      gtk_container_remove (GTK_CONTAINER (button->button), button->box);
	      button->box = vbox;
	      gtk_container_add (GTK_CONTAINER (button->button), button->box);
	    }
	  
	  gtk_box_set_child_packing (GTK_BOX (button->box), button->label,
				     FALSE, TRUE, 0, GTK_PACK_START);
	  gtk_widget_show (button->label);
	  if (button->icon) {
	    gtk_box_set_child_packing (GTK_BOX (button->box), button->icon,
				       TRUE, TRUE, 0, GTK_PACK_START);
	    gtk_widget_show (button->icon);
	  }
	  break;
	case GTK_TOOLBAR_BOTH_HORIZ:
	  if (GTK_IS_VBOX (button->box))
	    {
	      GtkWidget *hbox;
	      
              hbox = gtk_hbox_new (FALSE, 0);
	      gtk_widget_show (hbox);

	      if (button->icon)
		{
		  g_object_ref (button->icon);
		  gtk_container_remove (GTK_CONTAINER (button->box), button->icon);
		  gtk_box_pack_start (GTK_BOX (hbox), button->icon,
				      TRUE, TRUE, 0);
		  g_object_unref (button->icon);
		}
	      
	      g_object_ref (button->label);
	      gtk_container_remove (GTK_CONTAINER (button->box), button->label);
	      gtk_box_pack_start (GTK_BOX (hbox), button->label, FALSE, TRUE, 0);
	      g_object_unref (button->label);
	      
	      gtk_container_remove (GTK_CONTAINER (button->button), button->box);
	      button->box = hbox;
	      gtk_container_add (GTK_CONTAINER (button->button), button->box);
	    }
	  
	  gtk_box_set_child_packing (GTK_BOX (button->box), button->label,
				     TRUE, TRUE, 0, GTK_PACK_START);
	  gtk_widget_show (button->label);
	  if (button->icon) {
	    gtk_box_set_child_packing (GTK_BOX (button->box), button->icon,
				       FALSE, TRUE, 0, GTK_PACK_START);
	    gtk_widget_show (button->icon);
	  }
	  break;
	}
    }
}

static void
egg_tool_button_set_relief_style (EggToolItem   *tool_item,
				  GtkReliefStyle style)
{
  gtk_button_set_relief (GTK_BUTTON (EGG_TOOL_BUTTON (tool_item)->button), style);
}

static void
egg_tool_button_set_tooltip (EggToolItem *tool_item,
			     GtkTooltips *tooltips,
			     const gchar *tip_text,
			     const gchar *tip_private)
{
  gtk_tooltips_set_tip (tooltips, EGG_TOOL_BUTTON (tool_item)->button,
			tip_text, tip_private);
}

static void
button_clicked (GtkWidget *widget, EggToolButton *button)
{
  g_signal_emit_by_name (button, "clicked");
}

EggToolItem *
egg_tool_button_new_from_stock (const gchar *stock_id)
{
  EggToolButton *button;

  button = g_object_new (EGG_TYPE_TOOL_BUTTON,
			 "stock_id", stock_id,
			 "use_underline", TRUE,
			 NULL);

  return EGG_TOOL_ITEM (button);
}

EggToolItem *
egg_tool_button_new (void)
{
  EggToolButton *button;
  
  button = g_object_new (EGG_TYPE_TOOL_BUTTON,
			 NULL);

  return EGG_TOOL_ITEM (button);  
}

void
egg_tool_button_set_icon_widget (EggToolButton *button,
				 GtkWidget     *icon)
{
  g_return_if_fail (EGG_IS_TOOL_BUTTON (button));
  g_return_if_fail (GTK_IS_WIDGET (icon));

  if (button->icon)
    gtk_container_remove (GTK_CONTAINER (button->box), button->icon);
  button->icon = NULL;

  button->icon_set = (icon != NULL);
  if (icon)
    {
      button->icon = icon;
      gtk_box_pack_start (GTK_BOX (button->box), button->icon,
			  TRUE, TRUE, 0);
      gtk_box_reorder_child (GTK_BOX (button->box), button->icon, 0);
    }
  else if (button->stock_id)
    {
      button->icon = gtk_image_new_from_stock (button->stock_id,
					     EGG_TOOL_ITEM (button)->icon_size);
      gtk_box_pack_start (GTK_BOX (button->box), button->icon,
			  TRUE, TRUE, 0);
    }
}

void
egg_tool_button_set_label (EggToolButton *button,
			   const gchar   *label)
{
  gchar *label_no_mnemonic = NULL;
  GtkStockItem stock_item;  
  
  g_return_if_fail (EGG_IS_TOOL_BUTTON (button));
  g_return_if_fail (label != NULL);

  button->label_set = (label != NULL);
      
  if (label)
    label_no_mnemonic = elide_underscores (label);
  else if (button->stock_id && gtk_stock_lookup (button->stock_id, &stock_item)) 
    label_no_mnemonic = elide_underscores (stock_item.label);
	  
  gtk_label_set_label (GTK_LABEL (button->label), label_no_mnemonic);
  g_free (label_no_mnemonic);
}

