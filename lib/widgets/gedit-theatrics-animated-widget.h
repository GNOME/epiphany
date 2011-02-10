/*
 * gedit-theatrics-animated-widget.h
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gedit; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */

#ifndef __GEDIT_THEATRICS_ANIMATED_WIDGET_H__
#define __GEDIT_THEATRICS_ANIMATED_WIDGET_H__

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GEDIT_TYPE_THEATRICS_ANIMATED_WIDGET		(gedit_theatrics_animated_widget_get_type ())
#define GEDIT_THEATRICS_ANIMATED_WIDGET(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_THEATRICS_ANIMATED_WIDGET, GeditTheatricsAnimatedWidget))
#define GEDIT_THEATRICS_ANIMATED_WIDGET_CONST(obj)	(G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_THEATRICS_ANIMATED_WIDGET, GeditTheatricsAnimatedWidget const))
#define GEDIT_THEATRICS_ANIMATED_WIDGET_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), GEDIT_TYPE_THEATRICS_ANIMATED_WIDGET, GeditTheatricsAnimatedWidgetClass))
#define GEDIT_IS_THEATRICS_ANIMATED_WIDGET(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GEDIT_TYPE_THEATRICS_ANIMATED_WIDGET))
#define GEDIT_IS_THEATRICS_ANIMATED_WIDGET_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GEDIT_TYPE_THEATRICS_ANIMATED_WIDGET))
#define GEDIT_THEATRICS_ANIMATED_WIDGET_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GEDIT_TYPE_THEATRICS_ANIMATED_WIDGET, GeditTheatricsAnimatedWidgetClass))

typedef struct _GeditTheatricsAnimatedWidget		GeditTheatricsAnimatedWidget;
typedef struct _GeditTheatricsAnimatedWidgetClass	GeditTheatricsAnimatedWidgetClass;
typedef struct _GeditTheatricsAnimatedWidgetPrivate	GeditTheatricsAnimatedWidgetPrivate;

struct _GeditTheatricsAnimatedWidget
{
	GtkBin parent;
	
	GeditTheatricsAnimatedWidgetPrivate *priv;
};

struct _GeditTheatricsAnimatedWidgetClass
{
	GtkBinClass parent_class;
};

GType				 gedit_theatrics_animated_widget_get_type	(void) G_GNUC_CONST;

GeditTheatricsAnimatedWidget	*gedit_theatrics_animated_widget_new		(GtkWidget                          *widget,
										 GtkOrientation                      orientation);

G_END_DECLS

#endif /* __GEDIT_THEATRICS_ANIMATED_WIDGET_H__ */
