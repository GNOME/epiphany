/*
 *  Copyright © 2000-2002 Marco Pesenti Gritti
 *  Copyright © 2006, 2008 Christian Persch
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

#include "ephy-shell.h"
#include "ephy-file-helpers.h"
#include "ephy-object-helpers.h"
#include "ephy-state.h"
#include "ephy-debug.h"
#include "ephy-stock-icons.h"
#include "eel-gconf-extensions.h"
#include "ephy-dbus-client-bindings.h"
#include "ephy-activation.h"
#include "ephy-session.h"
#include "ephy-shell.h"
#include "ephy-prefs.h"
#include "ephy-profile-migration.h"
#include "ephy-debug.h"
#include "eggsmclient.h"

#include <libxml/xmlversion.h>

#include <glib/gi18n.h>

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <errno.h>
#include <string.h>

#ifdef ENABLE_INTROSPECTION
#include <girepository.h>
#endif

#ifdef HAVE_LIBNOTIFY
#include <libnotify/notify.h>
#endif

static GQuark startup_error_quark = 0;
#define STARTUP_ERROR_QUARK	(startup_error_quark)

static gboolean open_in_new_tab = FALSE;
static gboolean open_in_new_window = FALSE;
static gboolean open_as_bookmarks_editor = FALSE;
/*static gboolean reload_plugins = FALSE;*/

static char *session_filename = NULL;
static char *bookmark_url = NULL;
static char *bookmarks_file = NULL;
static char **arguments = NULL;

/* Only set from options in debug builds */
static gboolean private_instance = FALSE;
static gboolean keep_temp_directory = FALSE;
static char *profile_directory = NULL;

static gboolean
option_version_cb (const gchar *option_name,
                   const gchar *value,
                   gpointer     data,
                   GError     **error)
{
  g_print ("%s %s\n", _("GNOME Web Browser"), VERSION);

  exit (EXIT_SUCCESS);
 return FALSE;
}
 
static const GOptionEntry option_entries[] =
{
	{ "new-tab", 'n', 0, G_OPTION_ARG_NONE, &open_in_new_tab,
	  N_("Open a new tab in an existing browser window"), NULL },
	{ "new-window", 0, 0, G_OPTION_ARG_NONE, &open_in_new_window,
	  N_("Open a new browser window"), NULL },
	{ "bookmarks-editor", 'b', 0, G_OPTION_ARG_NONE, &open_as_bookmarks_editor,
	  N_("Launch the bookmarks editor"), NULL },
	{ "import-bookmarks", '\0', 0, G_OPTION_ARG_FILENAME, &bookmarks_file,
	  N_("Import bookmarks from the given file"), N_("FILE") },
	{ "load-session", 'l', 0, G_OPTION_ARG_FILENAME, &session_filename,
	  N_("Load the given session file"), N_("FILE") },
	{ "add-bookmark", 't', 0, G_OPTION_ARG_STRING, &bookmark_url,
	  N_("Add a bookmark"), N_("URL") },
	{ "private-instance", 'p', 0, G_OPTION_ARG_NONE, &private_instance,
	  N_("Start a private instance"), NULL },
	{ "profile", 0, 0, G_OPTION_ARG_STRING, &profile_directory,
	  N_("Profile directory to use in the private instance"), N_("DIR") },
	{ G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &arguments,
	  "", N_("URL …")},
	{ "version", 0, G_OPTION_FLAG_NO_ARG | G_OPTION_FLAG_HIDDEN, 
	  G_OPTION_ARG_CALLBACK, option_version_cb, NULL, NULL },
	{ NULL }
};

#ifdef GNOME_ENABLE_DEBUG
static GOptionEntry debug_option_entries[] =
{
	{ "keep-tempdir", 0, 0, G_OPTION_ARG_NONE, &keep_temp_directory,
	  "Don't delete the temporary directory on exit", NULL },
	{ NULL }
};
#endif /* GNOME_ENABLE_DEBUG */
 
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

static void
handle_url (GtkAboutDialog *about,
	    const char *link,
	    gpointer data)
{
	ephy_shell_new_tab (ephy_shell_get_default (),
			    NULL, NULL, link,
			    EPHY_NEW_TAB_OPEN_PAGE);
}

static void
handle_email (GtkAboutDialog *about,
	      const char *link,
	      gpointer data)
{
	char *command, *handler;
	GAppInfo *appinfo;

	if (eel_gconf_get_boolean ("/desktop/gnome/url-handlers/mailto/enabled") == FALSE)
	{
		return;
	}
	/* FIXME: better use g_app_info_get_default_for_uri_scheme () when it is
	 * implemented.
	 */
	handler = eel_gconf_get_string ("/desktop/gnome/url-handlers/mailto/command");
	command = g_strconcat (handler, "mailto:", link, NULL);
	appinfo = g_app_info_create_from_commandline (command, NULL, 0, NULL);
	ephy_file_launch_application (appinfo, NULL,
				      gtk_get_current_event_time (),
				      GTK_WIDGET (about));
	g_free (handler);
	g_free (command);
}

static void
unref_proxy_reply_cb (DBusGProxy *proxy,
		      GError *error,
		      gpointer user_data)
{
	if (error != NULL)
	{
		g_warning ("An error occurred while calling remote method: %s", error->message);
		g_error_free (error);
	}

	g_object_unref (proxy);

	if (gtk_main_level ())
	{
		gtk_main_quit ();
	}
}

static gboolean
open_urls (DBusGProxy *proxy,
	   guint32 user_time,
	   GError **error)
{
	static const char *empty_arguments[] = { "", NULL };
	GString *options;
	char **uris;

	options = g_string_sized_new (64);

	if (open_in_new_window)
	{
		g_string_append (options, "new-window,");
	}
	if (open_in_new_tab)
	{
		g_string_append (options, "new-tab,");
	}

	if (arguments == NULL)
	{
		uris = (char **) empty_arguments;
	}
	else
	{
		uris = (char **) arguments;
	}

	org_gnome_Epiphany_load_ur_ilist_async
		(proxy, (const char **) uris, options->str, user_time,
		 unref_proxy_reply_cb, NULL);
	
	if (arguments != NULL)
	{
		g_strfreev (arguments);
		arguments = NULL;
	}

	g_string_free (options, TRUE);

	return TRUE;
}

static gboolean
call_dbus_proxy (DBusGProxy *proxy,
		 guint32 user_time,
		 GError **error)
{
	EphyShell *shell;
	gboolean retval = TRUE;

	shell = ephy_shell_get_default ();

	if (open_as_bookmarks_editor)
	{
		org_gnome_Epiphany_open_bookmarks_editor_async
			(proxy, user_time,
			 unref_proxy_reply_cb, shell);
	}
	else if (session_filename != NULL)
	{
		org_gnome_Epiphany_load_session_async
			(proxy, session_filename, user_time,
			 unref_proxy_reply_cb, shell);

		g_free (session_filename);
		session_filename = NULL;
	}
	else
	{
		retval = open_urls (proxy, user_time, error);
	}

	/* FIXME why? */
	dbus_g_connection_flush (ephy_dbus_get_bus (ephy_dbus_get_default (), EPHY_DBUS_SESSION));

	return retval;
}

static void
queue_commands (guint32 user_time)
{
	EphyShell *shell;
	EphySession *session;

	shell = ephy_shell_get_default ();
	g_assert (shell != NULL);

	session = EPHY_SESSION (ephy_shell_get_session (shell));
	g_assert (session != NULL);

	/* We only get here when starting a new instance, so we 
	 * first need to autoresume!
	 */
	ephy_session_queue_command (session,
				    EPHY_SESSION_CMD_RESUME_SESSION,
				    NULL, NULL, user_time, TRUE);

	if (open_as_bookmarks_editor)
	{
		ephy_session_queue_command (session,
					    EPHY_SESSION_CMD_OPEN_BOOKMARKS_EDITOR,
					    NULL, NULL, user_time, FALSE);
	}
	else if (session_filename != NULL)
	{
		ephy_session_queue_command (session,
					    EPHY_SESSION_CMD_LOAD_SESSION,
					    session_filename, NULL,
					    user_time, FALSE);

		g_free (session_filename);
		session_filename = NULL;
	}
	/* Don't queue any window openings if no extra arguments given,
	 * since session autoresume will open one for us.
	 */
	else if (arguments != NULL)
	{
		GString *options;

		options = g_string_sized_new (64);

		if (open_in_new_window)
		{
			g_string_append (options, "new-window,");
		}
		if (open_in_new_tab)
		{
			g_string_append (options, "new-tab,external,");
		}

		ephy_session_queue_command (session,
					    EPHY_SESSION_CMD_OPEN_URIS,
					    options->str,
					    arguments,
					    user_time, FALSE);

		g_strfreev (arguments);
		arguments = NULL;
	}
}

static void
show_error_message (GError **error)
{
	GtkWidget *dialog;

	/* FIXME better texts!!! */
	dialog = gtk_message_dialog_new (NULL,
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_CLOSE,
					 _("Could not start GNOME Web Browser"));
	gtk_message_dialog_format_secondary_text
		(GTK_MESSAGE_DIALOG (dialog),
		 _("Startup failed because of the following error:\n%s"),
		 (*error)->message);

	g_clear_error (error);

	gtk_dialog_run (GTK_DIALOG (dialog));
}

static gchar *
get_accels_filename (void)
{
        const char *home;

        home = g_get_home_dir();
        if (!home)
                return NULL;
        return g_build_filename (home, GNOME_DOT_GNOME, "accels", PACKAGE, NULL);
}

static void
load_accels (void)
{
        char *filename;

        filename = get_accels_filename ();
        if (!filename)
                return;

        gtk_accel_map_load (filename);
        g_free (filename);
}

static void
save_accels (void)
{
        char *filename;

        filename = get_accels_filename ();
        if (!filename)
                return;

        gtk_accel_map_save (filename);
        g_free (filename);
}

static void
shell_quit_cb (EphyShell *shell, gpointer data)
{
	gtk_main_quit ();
}

int
main (int argc,
      char *argv[])
{
	GOptionContext *option_context;
	GOptionGroup *option_group;
	DBusGProxy *proxy;
	GError *error = NULL;
	guint32 user_time;

#ifdef ENABLE_NLS
	/* Initialize the i18n stuff */
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif

	/* Threads have to be initialised before calling ANY glib function */
	g_thread_init (NULL);
	dbus_g_thread_init ();

	/* check libxml2 API version epiphany was compiled with against the
	 * version we're running with.
	 */
	LIBXML_TEST_VERSION;

	/* sets name to help matching with the .desktop file */
	g_set_prgname ("epiphany");

	/* If we're given -remote arguments, translate them */
	if (argc >= 2 &&
	    strcmp (argv[1], "-remote") == 0)
	{
		const char *opening, *closing;
		char *command, *argument;
		char **arguments;

		if (argc != 3)
		{
			g_print ("-remote allows exactly one argument\n");
			exit (1);
		}

		opening = strchr (argv[2], '(');
		closing = strchr (argv[2], ')');

		if (opening == NULL ||
		    closing == NULL ||
		    opening == argv[2] ||
		    opening + 1 >= closing)
		{
			g_print ("Invalid argument for -remote\n");
			exit (1);
		}

		command = g_strstrip (g_strndup (argv[2], opening - argv[2]));

		/* See http://lxr.mozilla.org/seamonkey/source/xpfe/components/xremote/src/XRemoteService.cpp
		 * for the commands that mozilla supports; we'll just support openURL here.
		 */
		if (g_ascii_strcasecmp (command, "openURL") != 0)
		{
			g_print ("-remote command \"%s\" not supported\n", command);
			g_free (command);
			exit (1);
		}

		g_free (command);

		argument = g_strstrip (g_strndup (opening + 1, closing - opening - 1));
		arguments = g_strsplit (argument, ",", -1);
		g_free (argument);
		if (arguments == NULL)
		{
			g_print ("Invalid argument for -remote\n");

			exit (1);
		}

		/* replace arguments */
		argv[1] = g_strstrip (g_strdup (arguments[0]));
		argc = 2;

		g_strfreev (arguments);
	}

	/* Initialise our debug helpers */
	ephy_debug_init ();

	/* get this early, since gdk will unset the env var */
	user_time = get_startup_id ();

	option_context = g_option_context_new ("");
	option_group = g_option_group_new ("epiphany",
					   N_("GNOME Web Browser"),
					   N_("GNOME Web Browser options"),
					   NULL, NULL);

	g_option_group_set_translation_domain (option_group, GETTEXT_PACKAGE);

	g_option_group_add_entries (option_group, option_entries);

	g_option_context_set_main_group (option_context, option_group);

#ifdef ENABLE_INTROSPECTION
	g_option_context_add_group (option_context, g_irepository_get_option_group ());
#endif

        g_option_context_add_group (option_context, gtk_get_option_group (TRUE));
        g_option_context_add_group (option_context, egg_sm_client_get_option_group ());

#ifdef GNOME_ENABLE_DEBUG
	option_group = g_option_group_new ("debug",
					   "Epiphany debug options",
					   "Epiphany debug options",
					   NULL, NULL);
	g_option_group_add_entries (option_group, debug_option_entries);
	g_option_context_add_group (option_context, option_group);
#endif /* GNOME_ENABLE_DEBUG */

        if (!g_option_context_parse (option_context, &argc, &argv, &error))
        {
                g_print ("Failed to parse arguments: %s\n", error->message);
                g_error_free (error);
                g_option_context_free (option_context);
                exit (1);
        }
        
        g_option_context_free (option_context);

	/* Some argument sanity checks*/
	if (arguments != NULL && (session_filename != NULL || open_as_bookmarks_editor))
	{
		g_print ("Cannot use --bookmarks-editor or --load-session with URL arguments\n");
		exit (1);
	}

	if (profile_directory != NULL && private_instance == FALSE)
	{
		g_print ("--profile can only be used in combination with --private-instance\n");
		exit (1);
	}

	if (arguments != NULL &&
	    eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_ARBITRARY_URL))
	{
		g_print ("URL loading is locked down\n");
		exit (1);
	}

	/* Make URIs from arguments, to support filename args */
	if (arguments != NULL)
	{
		guint i;

		for (i = 0; arguments[i] != NULL; ++i)
		{
			char *uri, *path;
#ifdef PATH_MAX
			char rpath[PATH_MAX];
#else
			char *rpath = NULL;
#endif

			path = realpath (arguments[i], rpath);
			if (path != NULL)
			{
				uri = g_locale_to_utf8 (path, -1, 
							NULL, NULL, &error);
#ifndef PATH_MAX
				free (path);
#endif
			}
			else
			{
				uri = g_locale_to_utf8 (arguments[i], -1, 
							NULL, NULL, &error);
			}

			if (uri != NULL)
			{
				g_free (arguments[i]);

				/* If it's a file, use g_file_new_for_commandline_arg,
				 * so we get the right escaping.
				 */
				if (path != NULL)
				{
					GFile *file;
					file = g_file_new_for_commandline_arg (uri);
					arguments[i] = g_file_get_uri (file);
					g_object_unref (file);
					g_free (uri);
				}
				else
				{
					arguments[i] = uri;
				}
			}
			else
			{
				g_print ("Could not convert '%s' to UTF-8: %s!\n",
					 arguments[i], error->message);
				g_error_free (error);
				exit (1);
			}
		}
	}

	/* Get a timestamp manually if need be */
	if (user_time == 0)
	{
		user_time = slowly_and_stupidly_obtain_timestamp (gdk_display);
	}

	/* sets the name to appear in the window list applet when grouping windows */
	g_set_application_name (_("Web Browser"));

	/* Set default window icon */
	gtk_window_set_default_icon_name (EPHY_STOCK_EPHY);

	startup_error_quark = g_quark_from_static_string ("epiphany-startup-error");

	if (!_ephy_dbus_startup (!private_instance, &error))
	{
		_ephy_dbus_release ();

		show_error_message (&error);

		exit (1);
	}

	/* If we're remoting, no need to start up any further services,
	 * just forward the call.
	 */
	if (!private_instance &&
	    !_ephy_dbus_is_name_owner ())
	{
		/* Create DBUS proxy */
		proxy = ephy_dbus_get_proxy (ephy_dbus_get_default (), EPHY_DBUS_SESSION);
		if (proxy == NULL)
		{
			error = g_error_new (STARTUP_ERROR_QUARK,
					     0,
					     "Unable to get DBus proxy; aborting activation."); /* FIXME i18n */

			_ephy_dbus_release ();

			show_error_message (&error);

			exit (1);
		}

		if (!call_dbus_proxy (proxy, user_time, &error))
		{
			_ephy_dbus_release ();

			show_error_message (&error);

			exit (1);
		}

		/* Wait for the response */
		gtk_main ();

		_ephy_dbus_release ();

		gdk_notify_startup_complete ();

		exit (0);
	}

	/* We're not remoting; start our services */
	if (!ephy_file_helpers_init (profile_directory,
				     private_instance,
				     keep_temp_directory || profile_directory,
				     &error))
	{
		_ephy_dbus_release ();

		show_error_message (&error);

		exit (1);
	}

	/* Migrate profile if we are not running a private instance */
        if (ephy_has_private_profile () == FALSE)
          _ephy_profile_migrate ();

	eel_gconf_monitor_add ("/apps/epiphany/general");
	ephy_stock_icons_init ();
	load_accels ();

	/* Extensions may want these, so don't initialize in window-cmds */
	gtk_about_dialog_set_url_hook (handle_url, NULL, NULL);
	gtk_about_dialog_set_email_hook (handle_email, NULL, NULL);

	/* Work-around Flash Player crash */
	g_setenv ("XLIB_SKIP_ARGB_VISUALS", "1", FALSE);

	/* Now create the shell */
	_ephy_shell_create_instance ();
	g_signal_connect (ephy_shell, "quit", G_CALLBACK (shell_quit_cb), NULL);

	queue_commands (user_time);

#ifdef HAVE_LIBNOTIFY	
	/* Init notifications for the download manager */
	notify_init (PACKAGE);
#endif

	gtk_main ();

	/* Shutdown */
	g_object_unref (ephy_shell);

#ifdef HAVE_LIBNOTIFY	
	if (notify_is_initted ())
		notify_uninit ();
#endif
	eel_gconf_monitor_remove ("/apps/epiphany/general");
	save_accels ();
	ephy_state_save ();
	ephy_file_helpers_shutdown ();
	xmlCleanupParser ();

	_ephy_dbus_release ();

	return 0;
}
