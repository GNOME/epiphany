/*
 *  Copyright (C) 2000-2002 Marco Pesenti Gritti
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#include "config.h"

#undef GNOME_DISABLE_DEPRECATED

#include "ephy-shell.h"
#include "ephy-file-helpers.h"
#include "ephy-object-helpers.h"
#include "ephy-state.h"
#include "ephy-debug.h"
#include "ephy-stock-icons.h"
#include "eel-gconf-extensions.h"

#include <libgnomeui/gnome-ui-init.h>
#include <libgnomeui/gnome-app-helper.h>
#include <gtk/gtkaboutdialog.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkmessagedialog.h>
#include <gdk/gdkx.h>
#include <libgnome/gnome-program.h>
#include <glib/gi18n.h>
#include <libgnomevfs/gnome-vfs-init.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libxml/xmlversion.h>
#include <errno.h>
#include <string.h>

static gboolean open_in_existing = FALSE;
static gboolean open_in_new_tab = FALSE;
static gboolean open_fullscreen = FALSE;
static gboolean open_as_bookmarks_editor = FALSE;
static gboolean reload_plugins = FALSE;

static char *session_filename = NULL;
static char *bookmark_url = NULL;
static char *bookmarks_file = NULL;

static struct poptOption popt_options[] =
{
	{ "new-tab", 'n', POPT_ARG_NONE, &open_in_new_tab, 0,
	  N_("Open a new tab in an existing window"),
	  NULL },
	{ "fullscreen", 'f', POPT_ARG_NONE, &open_fullscreen, 0,
	  N_("Run in full screen mode"),
	  NULL },
	{ "load-session", 'l', POPT_ARG_STRING, &session_filename, 0,
	  N_("Load the given session file"),
	  N_("FILE") },
	{ "add-bookmark", 't', POPT_ARG_STRING, &bookmark_url,
	  0, N_("Add a bookmark (don't open any window)"),
	  N_("URL")},
	{ "import-bookmarks", '\0', POPT_ARG_STRING, &bookmarks_file,
	  0, N_("Import bookmarks from the given file"),
	  N_("FILE") },
	{ "bookmarks-editor", 'b', POPT_ARG_NONE, &open_as_bookmarks_editor, 0,
	  N_("Launch the bookmarks editor"),
	  NULL },
	{ "reload-plugins", '\0', POPT_ARG_NONE, &reload_plugins, 0, NULL, NULL },
	{ NULL, 0, 0, NULL, 0, NULL, NULL }
};

/* adapted from gtk+/gdk/x11/gdkdisplay-x11.c */
static guint32
get_startup_id (void)
{
	const char *startup_id, *time_str;
	guint32 retval = 0;

	startup_id = g_getenv ("DESKTOP_STARTUP_ID");
	if (startup_id == NULL) return 0;

	/* Find the launch time from the startup_id, if it's there.  Newer spec
	* states that the startup_id is of the form <unique>_TIME<timestamp>
	*/
	time_str = g_strrstr (startup_id, "_TIME");
	if (time_str != NULL)
	{
		gulong value;
		gchar *end;
		errno = 0;
	
		/* Skip past the "_TIME" part */
		time_str += 5;
	
		value = strtoul (time_str, &end, 0);
		if (end != time_str && errno == 0)
		{
			retval = (guint32) value;
		}
	}

	return retval;
}

static void
handle_url (GtkAboutDialog *about,
	    const char *link,
	    gpointer data)
{
	ephy_shell_new_tab (ephy_shell, NULL, NULL, link,
			    EPHY_NEW_TAB_OPEN_PAGE);
}

static void
handle_email (GtkAboutDialog *about,
	      const char *link,
	      gpointer data)
{
	char *address;

	address = g_strdup_printf ("mailto:%s\n", link);
	gnome_vfs_url_show (address);
	g_free (address);
}

static void
shell_weak_notify (gpointer data,
                   GObject *where_the_object_was)
{
	gtk_main_quit ();
}

/* Copied from libnautilus/nautilus-program-choosing.c; Needed in case
 * we have no DESKTOP_STARTUP_ID (with its accompanying timestamp).
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
				       CopyFromParent,
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
				 8, PropModeReplace, (unsigned char *)name, strlen (name));
	}
	
	XWindowEvent (xdisplay,
		      xwindow,
		      PropertyChangeMask,
		      &event);
	
	XDestroyWindow(xdisplay, xwindow);
	
	return event.xproperty.time;
}

int
main (int argc, char *argv[])
{
	poptContext context;
        GValue context_as_value = { 0 };
	GnomeProgram *program;
	EphyShellStartupFlags startup_flags;
	const char **args, *string_arg;
	guint32 user_time;
	gboolean new_instance;
	GError *err = NULL;

#ifdef ENABLE_NLS
	/* Initialize the i18n stuff */
	bindtextdomain(GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);
#endif

	/* check libxml2 API version epiphany was compiled with against the
	 * version we're running with.
	 */
	LIBXML_TEST_VERSION

	/* get this early, since gdk will unset the env var */
	user_time = get_startup_id ();

	program = gnome_program_init (PACKAGE, VERSION,
                                      LIBGNOMEUI_MODULE, argc, argv,
                                      GNOME_PARAM_POPT_TABLE, popt_options,
                                      GNOME_PARAM_HUMAN_READABLE_NAME, _("Web Browser"),
				      GNOME_PARAM_APP_DATADIR, DATADIR,
                                      NULL);

	/* Get a timestamp manually if need be */
	if (user_time == 0)
	{
		user_time = slowly_and_stupidly_obtain_timestamp (gdk_display);
	}

	/* sets the name to appear in the window list applet when grouping windows */
	g_set_application_name (_("Web Browser"));

	/* Set default window icon */
	gtk_window_set_default_icon_name ("web-browser");

	g_object_get_property (G_OBJECT (program),
                               GNOME_PARAM_POPT_CONTEXT,
                               g_value_init (&context_as_value, G_TYPE_POINTER));
        context = g_value_get_pointer (&context_as_value);
        args = poptGetArgs (context);

	startup_flags = 0;
	string_arg = NULL;
	if (open_in_new_tab)
	{
		startup_flags |= EPHY_SHELL_STARTUP_TABS;
	}
	else if (open_fullscreen)
	{
		startup_flags |= EPHY_SHELL_STARTUP_FULLSCREEN;
	}
	else if (open_in_existing)
	{
		startup_flags |= EPHY_SHELL_STARTUP_EXISTING_WINDOW;
	}
	else if (open_as_bookmarks_editor)
	{
		startup_flags |= EPHY_SHELL_STARTUP_BOOKMARKS_EDITOR;
	}
	else if (session_filename != NULL)
	{
		startup_flags |= EPHY_SHELL_STARTUP_SESSION;
		string_arg = session_filename;
	}
	else if (bookmarks_file != NULL)
	{
		startup_flags |= EPHY_SHELL_STARTUP_IMPORT_BOOKMARKS;
		string_arg = bookmarks_file;
	}
	else if (bookmark_url != NULL)
	{
		startup_flags |= EPHY_SHELL_STARTUP_ADD_BOOKMARK;
		string_arg = bookmark_url;
	}

	gnome_vfs_init ();
	ephy_debug_init ();
	ephy_file_helpers_init ();
	ephy_stock_icons_init ();
	eel_gconf_monitor_add ("/apps/epiphany/general");
	eel_gconf_monitor_add ("/apps/epiphany/lockdown");
	eel_gconf_monitor_add ("/desktop/gnome/lockdown");

	/* Extensions may want these, so don't initialize in window-cmds */
	gtk_about_dialog_set_url_hook (handle_url, NULL, NULL);
	gtk_about_dialog_set_email_hook (handle_email, NULL, NULL);

	ephy_shell_new ();
	g_assert (ephy_shell != NULL);
	new_instance = ephy_shell_startup (ephy_shell, startup_flags,
					   user_time,
					   args, string_arg, &err);

	if (err != NULL)
	{
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
						GTK_BUTTONS_CLOSE, 
						GTK_MESSAGE_ERROR, err->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
	}
	else if (new_instance && ephy_shell)
	{
		g_object_weak_ref (G_OBJECT (ephy_shell), shell_weak_notify, NULL);
		ephy_object_idle_unref (ephy_shell);

		gtk_main ();
	}

	eel_gconf_monitor_remove ("/apps/epiphany/general");
	eel_gconf_monitor_remove ("/apps/epiphany/lockdown");
	eel_gconf_monitor_remove ("/desktop/gnome/lockdown");
	gnome_accelerators_sync ();
	ephy_state_save ();
	ephy_file_helpers_shutdown ();
	gnome_vfs_shutdown ();
	xmlCleanupParser ();
	poptFreeContext (context);

	return 0;
}
