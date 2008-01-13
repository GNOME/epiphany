/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-mount-operation.c - Gtk+ implementation for GAppLaunchContext

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

#include <config.h>
#include "eel-app-launch-context.h"

#include <gio/gio.h>
#include <glib/gurifuncs.h>
#include <string.h>

#ifdef HAVE_STARTUP_NOTIFICATION
#define SN_API_NOT_YET_FROZEN
#include <libsn/sn.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtkmain.h>
#endif

G_DEFINE_TYPE (EelAppLaunchContext, eel_app_launch_context, G_TYPE_APP_LAUNCH_CONTEXT);

struct EelAppLaunchContextPrivate {
	GdkDisplay *display;
	GdkScreen *screen;
	guint32 timestamp;
	GIcon *icon;
	char *icon_name;
};
	
static void
eel_app_launch_context_finalize (GObject *object)
{
	EelAppLaunchContext *context;
	EelAppLaunchContextPrivate *priv;

	context = EEL_APP_LAUNCH_CONTEXT (object);

	priv = context->priv;
	
	if (priv->display) {
		g_object_unref (priv->display);
	} 
	if (priv->screen) {
		g_object_unref (priv->screen);
	}
	if (priv->icon) {
		g_object_unref (priv->screen);
	}
	g_free (priv->icon_name);
 
	(*G_OBJECT_CLASS (eel_app_launch_context_parent_class)->finalize) (object);
}

static char *
get_display (GAppLaunchContext *context,
	     GAppInfo *info,
	     GList *files)
{
	GdkDisplay *display;
	EelAppLaunchContextPrivate *priv;

	priv = EEL_APP_LAUNCH_CONTEXT (context)->priv;
	
	if (priv->screen) {
		return gdk_screen_make_display_name (priv->screen);
	}
	
	if (priv->display) {
		display = priv->display;
	} else {
		display = gdk_display_get_default ();
	}
	
	return g_strdup (gdk_display_get_name (display));
}

#ifdef HAVE_STARTUP_NOTIFICATION
static void
sn_error_trap_push (SnDisplay *display,
		    Display   *xdisplay)
{
	gdk_error_trap_push ();
}

static void
sn_error_trap_pop (SnDisplay *display,
		   Display   *xdisplay)
{
	gdk_error_trap_pop ();
}

static char *
get_display_name (GFile *file)
{
        GFileInfo *info;
        char *name, *tmp;

        /* This does sync i/o, which isn't ideal.
         * It should probably use the NautilusFile machinery
         */
        
        name = NULL;
        info = g_file_query_info (file,
                                  G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
                                  0, NULL, NULL);
        if (info) {
                name = g_strdup (g_file_info_get_display_name (info));
                g_object_unref (info);
        }

        if (name == NULL) {
                name = g_file_get_basename (file);
                if (!g_utf8_validate (name, -1, NULL)) {
			tmp = name;
                        name = g_uri_escape_string (name, G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, TRUE);
                        g_free (tmp);
                }
        }
        return name;
}

static GIcon *
get_icon (GFile *file)
{
        GFileInfo *info;
	GIcon *icon;

        icon = NULL;
        info = g_file_query_info (file,
                                  G_FILE_ATTRIBUTE_STANDARD_ICON,
                                  0, NULL, NULL);
        if (info) {
                icon = g_file_info_get_icon (info);
		if (icon) {
			g_object_ref (icon);
		}
                g_object_unref (info);
        }

        return icon;
	
}

static char *
gicon_to_string (GIcon *icon)
{
	GFile *file;
	const char * const *names;
	
	if (G_IS_FILE_ICON (icon)) {
		file = g_file_icon_get_file (G_FILE_ICON (icon));
		if (file) {
			return g_file_get_path (file);
		}
	} else if (G_IS_THEMED_ICON (icon)) {
		names = g_themed_icon_get_names (G_THEMED_ICON (icon));
		if (names) {
			return g_strdup (names[0]);
		}
	}
	
	return NULL;
}

/* FIXME: This is the wrong way to do this; there should be some event
 * (e.g. button press) available with a good time.  A function like
 * this should not be needed.
 */
static Time
slowly_and_stupidly_obtain_timestamp (Display *xdisplay)
{
	Window xwindow;
	XEvent event;
	
	{
		XSetWindowAttributes attrs;
		Atom atom_name;
		Atom atom_type;
		char* name;
		
		attrs.override_redirect = True;
		attrs.event_mask = PropertyChangeMask | StructureNotifyMask;
		
		xwindow =
			XCreateWindow (xdisplay,
				       RootWindow (xdisplay, 0),
				       -100, -100, 1, 1,
				       0,
				       CopyFromParent,
				       CopyFromParent,
				       (Visual *)CopyFromParent,
				       CWOverrideRedirect | CWEventMask,
				       &attrs);
		
		atom_name = XInternAtom (xdisplay, "WM_NAME", TRUE);
		g_assert (atom_name != None);
		atom_type = XInternAtom (xdisplay, "STRING", TRUE);
		g_assert (atom_type != None);
		
		name = "Fake Window";
		XChangeProperty (xdisplay, 
				 xwindow, atom_name,
				 atom_type,
				 8, PropModeReplace, name, strlen (name));
	}
	
	XWindowEvent (xdisplay,
		      xwindow,
		      PropertyChangeMask,
		      &event);
	
	XDestroyWindow(xdisplay, xwindow);
	
	return event.xproperty.time;
}

/* This should be fairly long, as it's confusing to users if a startup
 * ends when it shouldn't (it appears that the startup failed, and
 * they have to relaunch the app). Also the timeout only matters when
 * there are bugs and apps don't end their own startup sequence.
 *
 * This timeout is a "last resort" timeout that ignores whether the
 * startup sequence has shown activity or not.  Metacity and the
 * tasklist have smarter, and correspondingly able-to-be-shorter
 * timeouts. The reason our timeout is dumb is that we don't monitor
 * the sequence (don't use an SnMonitorContext)
 */
#define STARTUP_TIMEOUT_LENGTH (30 /* seconds */ * 1000)

typedef struct
{
	GSList *contexts;
	guint timeout_id;
} StartupTimeoutData;

static void
free_startup_timeout (void *data)
{
	StartupTimeoutData *std;

	std = data;

	g_slist_foreach (std->contexts,
			 (GFunc) sn_launcher_context_unref,
			 NULL);
	g_slist_free (std->contexts);

	if (std->timeout_id != 0) {
		g_source_remove (std->timeout_id);
		std->timeout_id = 0;
	}

	g_free (std);
}

static gboolean
startup_timeout (void *data)
{
	StartupTimeoutData *std;
	GSList *tmp;
	GTimeVal now;
	int min_timeout;

	std = data;

	min_timeout = STARTUP_TIMEOUT_LENGTH;
	
	g_get_current_time (&now);
	
	tmp = std->contexts;
	while (tmp != NULL) {
		SnLauncherContext *sn_context;
		GSList *next;
		long tv_sec, tv_usec;
		double elapsed;
		
		sn_context = tmp->data;
		next = tmp->next;
		
		sn_launcher_context_get_last_active_time (sn_context,
							  &tv_sec, &tv_usec);

		elapsed =
			((((double)now.tv_sec - tv_sec) * G_USEC_PER_SEC +
			  (now.tv_usec - tv_usec))) / 1000.0;

		if (elapsed >= STARTUP_TIMEOUT_LENGTH) {
			std->contexts = g_slist_remove (std->contexts,
							sn_context);
			sn_launcher_context_complete (sn_context);
			sn_launcher_context_unref (sn_context);
		} else {
			min_timeout = MIN (min_timeout, (STARTUP_TIMEOUT_LENGTH - elapsed));
		}
		
		tmp = next;
	}

	if (std->contexts == NULL) {
		std->timeout_id = 0;
	} else {
		std->timeout_id = g_timeout_add (min_timeout,
						 startup_timeout,
						 std);
	}

	/* always remove this one, but we may have reinstalled another one. */
	return FALSE;
}

static void
add_startup_timeout (GdkScreen         *screen,
		     SnLauncherContext *sn_context)
{
	StartupTimeoutData *data;

	data = g_object_get_data (G_OBJECT (screen), "appinfo-startup-data");
	if (data == NULL) {
		data = g_new (StartupTimeoutData, 1);
		data->contexts = NULL;
		data->timeout_id = 0;
		
		g_object_set_data_full (G_OBJECT (screen), "appinfo-startup-data",
					data, free_startup_timeout);		
	}

	sn_launcher_context_ref (sn_context);
	data->contexts = g_slist_prepend (data->contexts, sn_context);
	
	if (data->timeout_id == 0) {
		data->timeout_id = g_timeout_add (STARTUP_TIMEOUT_LENGTH,
						  startup_timeout,
						  data);		
	}
}

#endif


static char *
get_startup_notify_id (GAppLaunchContext *context,
		       GAppInfo *info,
		       GList *files)
{
#ifdef HAVE_STARTUP_NOTIFICATION
	EelAppLaunchContextPrivate *priv;
	SnLauncherContext *sn_context;
	SnDisplay *sn_display;
	GdkDisplay *display;
	GdkScreen *screen;
	int files_count;
	char *name, *description, *icon_name;
	const char *binary_name;
	GIcon *icon;
	guint32 timestamp;
	char *id;

	priv = EEL_APP_LAUNCH_CONTEXT (context)->priv;

	if (priv->screen) {
		screen = priv->screen;
		display = gdk_screen_get_display (priv->screen);
	} else if (priv->display) {
		display = priv->display;
		screen = gdk_display_get_default_screen (display);
	} else {
		display = gdk_display_get_default ();
		screen = gdk_display_get_default_screen (display);
	}

	sn_display = sn_display_new (gdk_display,
				     sn_error_trap_push,
				     sn_error_trap_pop);
	

	sn_context = sn_launcher_context_new (sn_display,
					      gdk_screen_get_number (screen));


	files_count = g_list_length (files);
	if (files_count == 1) {
		name = get_display_name (files->data);
		description = g_strdup_printf (_("Opening %s"), name);
	} else {
		name = NULL;
		description = g_strdup_printf (ngettext ("Opening %d Item",
							 "Opening %d Items",
							 files_count),
					       files_count);
	}

	if (name != NULL) {
		sn_launcher_context_set_name (sn_context, name);
		g_free (name);
	}

	if (description != NULL) {
		sn_launcher_context_set_description (sn_context, description);
		g_free (description);
	}

	icon_name = NULL;
	if (priv->icon_name) {
		icon_name = g_strdup (priv->icon_name);
	} else {
		icon = NULL;

		if (priv->icon != NULL) {
			icon = g_object_ref (priv->icon);
		} else 	if (files_count == 1) {
			icon = get_icon (files->data);
		}

		if (icon == NULL) {
			icon = g_app_info_get_icon (info);
			g_object_ref (icon);
		}

		if (icon) {
			icon_name = gicon_to_string (icon);
		}
		g_object_unref (icon);
	}
	
	if (icon_name) {
		sn_launcher_context_set_icon_name (sn_context, icon_name);
		g_free (icon_name);
	}
	

	binary_name = g_app_info_get_executable (info);
	sn_launcher_context_set_binary_name (sn_context, binary_name);

	timestamp = priv->timestamp;
	if (timestamp == GDK_CURRENT_TIME) {
		timestamp = gtk_get_current_event_time ();
	}
	if (timestamp == GDK_CURRENT_TIME) {
		timestamp = slowly_and_stupidly_obtain_timestamp (GDK_SCREEN_XDISPLAY (screen));
	}

	sn_launcher_context_initiate (sn_context,
				      g_get_application_name () ? g_get_application_name () : "unknown",
				      binary_name,
				      timestamp);

	id = g_strdup (sn_launcher_context_get_startup_id (sn_context));

	add_startup_timeout (screen, sn_context);
	
	sn_launcher_context_unref (sn_context);

	sn_display_unref (sn_display);
	
	return id;
#else
	return NULL;
#endif
}

static void
launch_failed (GAppLaunchContext *context,
	       const char *startup_notify_id)
{
#ifdef HAVE_STARTUP_NOTIFICATION
	EelAppLaunchContextPrivate *priv;
	GdkScreen *screen;
	StartupTimeoutData *data;
	SnLauncherContext *sn_context;
	GSList *l;

	priv = EEL_APP_LAUNCH_CONTEXT (context)->priv;

	if (priv->screen) {
		screen = priv->screen;
	} else if (priv->display) {
		screen = gdk_display_get_default_screen (priv->display);
	} else {
		screen = gdk_display_get_default_screen (gdk_display_get_default ());
	}

	data = g_object_get_data (G_OBJECT (screen), "appinfo-startup-data");

	if (data) {
		for (l = data->contexts; l != NULL; l = l->next) {
			sn_context = l->data;
			if (strcmp (startup_notify_id, sn_launcher_context_get_startup_id (sn_context)) == 0) {
				data->contexts = g_slist_remove (data->contexts,
								 sn_context);
				sn_launcher_context_complete (sn_context);
				sn_launcher_context_unref (sn_context);
				break;
			}
		}
		
		if (data->contexts == NULL) {
			g_source_remove (data->timeout_id);
			data->timeout_id = 0;
		}
	}
	
#endif

}


static void
eel_app_launch_context_class_init (EelAppLaunchContextClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GAppLaunchContextClass *context_class = G_APP_LAUNCH_CONTEXT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (EelAppLaunchContextPrivate));
	
	gobject_class->finalize = eel_app_launch_context_finalize;
	
	context_class->get_display = get_display;
	context_class->get_startup_notify_id = get_startup_notify_id;
	context_class->launch_failed = launch_failed;
}

static void
eel_app_launch_context_init (EelAppLaunchContext *context)
{
	context->priv = G_TYPE_INSTANCE_GET_PRIVATE (context,
						     EEL_TYPE_APP_LAUNCH_CONTEXT,
						     EelAppLaunchContextPrivate);
}

void
eel_app_launch_context_set_display (EelAppLaunchContext *context,
				    GdkDisplay          *display)
{
	if (context->priv->display) {
		g_object_unref (context->priv->display);
		context->priv->display = NULL;		
	}

	if (display) {
		context->priv->display = g_object_ref (display);
	}
}

void
eel_app_launch_context_set_screen (EelAppLaunchContext *context,
				   GdkScreen           *screen)
{
	if (context->priv->screen) {
		g_object_unref (context->priv->screen);
		context->priv->screen = NULL;		
	}

	if (screen) {
		context->priv->screen = g_object_ref (screen);
	}
}

void
eel_app_launch_context_set_timestamp (EelAppLaunchContext *context,
				      guint32              timestamp)
{
	context->priv->timestamp = timestamp;
}
	
void
eel_app_launch_context_set_icon (EelAppLaunchContext *context,
				 GIcon               *icon)
{
	if (context->priv->icon) {
		g_object_unref (context->priv->icon);
		context->priv->icon = NULL;		
	}

	if (icon) {
		context->priv->icon = g_object_ref (icon);
	}
}

void
eel_app_launch_context_set_icon_name (EelAppLaunchContext *context,
				      const char          *icon_name)
{
	g_free (context->priv->icon_name);
	context->priv->icon_name = g_strdup (icon_name);
}

EelAppLaunchContext *
eel_app_launch_context_new (void)
{
	EelAppLaunchContext *context;

	context = g_object_new (eel_app_launch_context_get_type (), NULL);
	return context;
}
