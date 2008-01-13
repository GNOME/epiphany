/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-app-launch-context.h - Gtk+ implementation for GAppLaunchContext

   Copyright (C) 2007 Red Hat, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Alexander Larsson <alexl@redhat.com>
*/

#ifndef EEL_APP_LAUCH_CONTEXT_H
#define EEL_APP_LAUCH_CONTEXT_H

#include <glib.h>
#include <gio/gio.h>
#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define EEL_TYPE_APP_LAUNCH_CONTEXT         (eel_app_launch_context_get_type ())
#define EEL_APP_LAUNCH_CONTEXT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EEL_TYPE_APP_LAUNCH_CONTEXT, EelAppLaunchContext))
#define EEL_APP_LAUNCH_CONTEXT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EEL_TYPE_APP_LAUNCH_CONTEXT, EelAppLaunchContextClass))
#define EEL_IS_APP_LAUNCH_CONTEXT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EEL_TYPE_APP_LAUNCH_CONTEXT))
#define EEL_IS_APP_LAUNCH_CONTEXT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EEL_TYPE_APP_LAUNCH_CONTEXT))
#define EEL_APP_LAUNCH_CONTEXT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EEL_TYPE_APP_LAUNCH_CONTEXT, EelAppLaunchContextClass))

typedef struct EelAppLaunchContext	      EelAppLaunchContext;
typedef struct EelAppLaunchContextClass       EelAppLaunchContextClass;
typedef struct EelAppLaunchContextPrivate     EelAppLaunchContextPrivate;

struct EelAppLaunchContext
{
	GAppLaunchContext parent_instance;
	
	EelAppLaunchContextPrivate *priv;
};

struct EelAppLaunchContextClass 
{
	GAppLaunchContextClass parent_class;


};

GType eel_app_launch_context_get_type (void);

EelAppLaunchContext *eel_app_launch_context_new           (void);
void                 eel_app_launch_context_set_display   (EelAppLaunchContext *context,
							   GdkDisplay          *display);
void                 eel_app_launch_context_set_screen    (EelAppLaunchContext *context,
							   GdkScreen           *screen);
void                 eel_app_launch_context_set_timestamp (EelAppLaunchContext *context,
							   guint32              timestamp);
void                 eel_app_launch_context_set_icon      (EelAppLaunchContext *context,
							   GIcon               *icon);
void                 eel_app_launch_context_set_icon_name (EelAppLaunchContext *context,
							   const char          *icon_name);

G_END_DECLS

#endif /* EEL_APP_LAUNCH_CONTEXT_H */


