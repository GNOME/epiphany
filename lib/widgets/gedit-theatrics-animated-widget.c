/*
 * gedit-theatrics-animated-widget.c
 * This file is part of gedit
 *
 * Copyright (C) 2010 - Ignacio Casal Quinteiro
 *
 * Based on Scott Peterson <lunchtimemama@gmail.com> work.
 *
 * gedit is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * gedit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gedit; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */

#include "gedit-theatrics-animated-widget.h"

#define GEDIT_THEATRICS_ANIMATED_WIDGET_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), GEDIT_TYPE_THEATRICS_ANIMATED_WIDGET, GeditTheatricsAnimatedWidgetPrivate))

struct _GeditTheatricsAnimatedWidgetPrivate
{
	GtkWidget *widget;
	GtkOrientation orientation;
	GtkAllocation widget_alloc;
};

enum
{
	PROP_0,
	PROP_WIDGET,
	PROP_ORIENTATION
};

G_DEFINE_TYPE_EXTENDED (GeditTheatricsAnimatedWidget,
			gedit_theatrics_animated_widget,
			GTK_TYPE_BIN,
			0,
			G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE,
					       NULL))

static void
gedit_theatrics_animated_widget_get_property (GObject	 *object,
					      guint	  prop_id,
					      GValue	 *value,
					      GParamSpec *pspec)
{
	GeditTheatricsAnimatedWidget *aw = GEDIT_THEATRICS_ANIMATED_WIDGET (object);

	switch (prop_id)
	{
		case PROP_WIDGET:
			g_value_set_object (value, aw->priv->widget);
			break;
		case PROP_ORIENTATION:
			g_value_set_enum (value, aw->priv->orientation);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_theatrics_animated_widget_set_property (GObject	   *object,
					      guint	    prop_id,
					      const GValue *value,
					      GParamSpec   *pspec)
{
	GeditTheatricsAnimatedWidget *aw = GEDIT_THEATRICS_ANIMATED_WIDGET (object);

	switch (prop_id)
	{
		case PROP_WIDGET:
		{
			gtk_container_add (GTK_CONTAINER (aw),
					   g_value_get_object (value));
			break;
		}
		case PROP_ORIENTATION:
			aw->priv->orientation = g_value_get_enum (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_theatrics_animated_widget_realize (GtkWidget *widget)
{
	GdkWindowAttr attributes;
	GdkWindow *parent_window;
	GdkWindow *window;
	GtkStyleContext *context;

	gtk_widget_set_realized (widget, TRUE);

	parent_window = gtk_widget_get_parent_window (widget);
	context = gtk_widget_get_style_context (widget);

	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.event_mask = GDK_EXPOSURE_MASK;

	window = gdk_window_new (parent_window, &attributes, 0);
	gdk_window_set_user_data (window, widget);
	gtk_widget_set_window (widget, window);
	gtk_style_context_set_state (context, GTK_STATE_FLAG_NORMAL);
	gtk_style_context_set_background (context, window);
}

static void
gedit_theatrics_animated_widget_get_preferred_width (GtkWidget *widget,
						     gint      *minimum,
						     gint      *natural)
{
	GeditTheatricsAnimatedWidget *aw = GEDIT_THEATRICS_ANIMATED_WIDGET (widget);
	gint width;

	if (aw->priv->widget != NULL)
	{
		gint child_min, child_nat;

		gtk_widget_get_preferred_width (aw->priv->widget,
						&child_min, &child_nat);
		aw->priv->widget_alloc.width = child_min;
	}

	width = aw->priv->widget_alloc.width;
	*minimum = *natural = width;
}

static void
gedit_theatrics_animated_widget_get_preferred_height (GtkWidget *widget,
						      gint	*minimum,
						      gint	*natural)
{
	GeditTheatricsAnimatedWidget *aw = GEDIT_THEATRICS_ANIMATED_WIDGET (widget);
	gint height;

	if (aw->priv->widget != NULL)
	{
		gint child_min, child_nat;

		gtk_widget_get_preferred_height (aw->priv->widget,
						 &child_min, &child_nat);
		aw->priv->widget_alloc.height = child_min;
	}

	height = aw->priv->widget_alloc.height;
	*minimum = *natural = height;
}

static void
gedit_theatrics_animated_widget_size_allocate (GtkWidget     *widget,
					       GtkAllocation *allocation)
{
	GeditTheatricsAnimatedWidget *aw = GEDIT_THEATRICS_ANIMATED_WIDGET (widget);

	GTK_WIDGET_CLASS (gedit_theatrics_animated_widget_parent_class)->size_allocate (widget, allocation);

	if (aw->priv->widget != NULL)
	{
		if (aw->priv->orientation == GTK_ORIENTATION_HORIZONTAL)
		{
			aw->priv->widget_alloc.height = allocation->height;
			aw->priv->widget_alloc.x = 0;
		}

		if (aw->priv->widget_alloc.height > 0 && aw->priv->widget_alloc.width > 0)
		{
			gtk_widget_size_allocate (aw->priv->widget,
						  &aw->priv->widget_alloc);
		}
	}
}

static void
gedit_theatrics_animated_widget_add (GtkContainer *container,
				     GtkWidget	  *widget)
{
	GeditTheatricsAnimatedWidget *aw = GEDIT_THEATRICS_ANIMATED_WIDGET (container);

	aw->priv->widget = widget;

	GTK_CONTAINER_CLASS (gedit_theatrics_animated_widget_parent_class)->add (container, widget);
}

static void
gedit_theatrics_animated_widget_remove (GtkContainer *container,
					GtkWidget    *widget)
{
	GeditTheatricsAnimatedWidget *aw = GEDIT_THEATRICS_ANIMATED_WIDGET (container);

	aw->priv->widget = NULL;

	GTK_CONTAINER_CLASS (gedit_theatrics_animated_widget_parent_class)->remove (container, widget);
}

static void
gedit_theatrics_animated_widget_class_init (GeditTheatricsAnimatedWidgetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
	
	object_class->get_property = gedit_theatrics_animated_widget_get_property;
	object_class->set_property = gedit_theatrics_animated_widget_set_property;

	widget_class->realize = gedit_theatrics_animated_widget_realize;
	widget_class->get_preferred_width = gedit_theatrics_animated_widget_get_preferred_width;
	widget_class->get_preferred_height = gedit_theatrics_animated_widget_get_preferred_height;
	widget_class->size_allocate = gedit_theatrics_animated_widget_size_allocate;

	container_class->add = gedit_theatrics_animated_widget_add;
	container_class->remove = gedit_theatrics_animated_widget_remove;

	g_object_class_install_property (object_class, PROP_WIDGET,
					 g_param_spec_object ("widget",
							      "Widget",
							      "The Widget",
							      GTK_TYPE_WIDGET,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY |
							      G_PARAM_STATIC_STRINGS));

	g_object_class_override_property (object_class,
					  PROP_ORIENTATION,
					  "orientation");

	g_type_class_add_private (object_class, sizeof (GeditTheatricsAnimatedWidgetPrivate));
}

static void
gedit_theatrics_animated_widget_init (GeditTheatricsAnimatedWidget *aw)
{
	aw->priv = GEDIT_THEATRICS_ANIMATED_WIDGET_GET_PRIVATE (aw);

	gtk_widget_set_has_window (GTK_WIDGET (aw), TRUE);

	aw->priv->orientation = GTK_ORIENTATION_HORIZONTAL;
}

GeditTheatricsAnimatedWidget *
gedit_theatrics_animated_widget_new (GtkWidget				*widget,
				     GtkOrientation			 orientation)
{
	return g_object_new (GEDIT_TYPE_THEATRICS_ANIMATED_WIDGET,
			     "widget", widget,
			     "orientation", orientation,
			     NULL);
}

/* ex:set ts=8 noet: */
