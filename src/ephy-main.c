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
#include "ephy-debug.h"

#include <libxml/xmlversion.h>

#include <glib/gi18n.h>

#include <gdk/gdkx.h>
#include <gtk/gtkaboutdialog.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkmessagedialog.h>

#include <libgnome/gnome-program.h>
#include <libgnomeui/gnome-client.h>

/* libgnome < 2.13 compat */
#ifndef GNOME_PARAM_GOPTION_CONTEXT
#include <libgnomeui/gnome-ui-init.h>
#endif

#include <libgnomevfs/gnome-vfs-init.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomeui/gnome-app-helper.h>

#include <errno.h>
#include <string.h>

static GQuark startup_error_quark = 0;
#define STARTUP_ERROR_QUARK	(startup_error_quark)

static gboolean open_in_new_tab = FALSE;
static gboolean open_in_new_window = FALSE;
static gboolean open_as_bookmarks_editor = FALSE;
//static gboolean reload_plugins = FALSE;

static char *session_filename = NULL;
static char *bookmark_url = NULL;
static char *bookmarks_file = NULL;
static char **remaining_arguments = NULL;

static const GOptionEntry option_entries[] =
{
	{ "new-tab", 'n', 0, G_OPTION_ARG_NONE, &open_in_new_tab,
	  N_("Open a new tab in an existing Epiphany window"), NULL },
	{ "new-window", 0, 0, G_OPTION_ARG_NONE, &open_in_new_window,
	  N_("Open a new tab in an existing Epiphany window"), NULL },
	{ "bookmarks-editor", 'b', 0, G_OPTION_ARG_NONE, &open_as_bookmarks_editor,
	  N_("Launch the bookmarks editor"), NULL },
	{ "import-bookmarks", '\0', 0, G_OPTION_ARG_FILENAME, &bookmarks_file,
	  N_("Import bookmarks from the given file"), N_("FILE") },
	{ "load-session", 'l', 0, G_OPTION_ARG_FILENAME, &session_filename,
	  N_("Load the given session file"), N_("FILE") },
	{ "add-bookmark", 't', 0, G_OPTION_ARG_STRING, &bookmark_url,
	  N_("Add a bookmark"), N_("URL") },
	{ G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_STRING_ARRAY, &remaining_arguments,
	  "", "" },
	{ NULL }
};

#ifndef GNOME_PARAM_GOPTION_CONTEXT
/* libgnome < 2.13 compat */
static char *sm_client_id = NULL;
static char *sm_config_prefix = NULL;
static gboolean sm_disable = FALSE;
static gboolean disable_crash_dialog = FALSE;

static const GOptionEntry libgnome_option_entries[] =
{
	{ "sm-client-id", 0, 0, G_OPTION_ARG_STRING, &sm_client_id,
	  "Specify session management ID", "ID" },
	{ "sm-config-prefix", 0, 0, G_OPTION_ARG_STRING, &sm_config_prefix,
	  "Specify prefix of saved configuration", "PREFIX" },
	{ "sm-disable", 0, 0, G_OPTION_ARG_NONE, &sm_disable,
	  "Disable connection to session manager", NULL },
	{ "disable-crash-dialog", 0, 0, G_OPTION_ARG_NONE, &disable_crash_dialog,
	  "Disable Crash Dialog", NULL },
	{ NULL }
};
#endif /* !GNOME_PARAM_GOPTION_CONTEXT */

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
	char *address;

	address = g_strdup_printf ("mailto:%s", link);
	gnome_vfs_url_show (address);
	g_free (address);
}

static void
shell_weak_notify (gpointer data,
                   GObject *zombie)
{
	if (gtk_main_level ())
	{
		gtk_main_quit ();
	}
}

static void
dbus_g_proxy_finalized_cb (EphyShell *shell,
			   GObject *zombie)
{
	LOG ("dbus_g_proxy_finalized_cb");

	g_object_unref (shell);
}

/* Gnome session client */

static gboolean
save_yourself_cb (GnomeClient *client,
		  gint phase,
		  GnomeSaveStyle save_style,
		  gboolean shutdown,
		  GnomeInteractStyle interact_style,
		  gboolean fast,
		  gpointer user_data)
{
	EphyShell *shell;
	EphySession *session;
	char *argv[] = { NULL, "--load-session", NULL };
	char *discard_argv[] = { "rm", "-f", NULL };
	char *tmp, *save_to;

	LOG ("save_yourself_cb");

	/* FIXME FIXME */
	if (!ephy_shell_get_default ()) return FALSE;

	tmp = g_build_filename (ephy_dot_dir (),
				"session_gnome-XXXXXX",
				NULL);
	save_to = ephy_file_tmp_filename (tmp, "xml");
	g_free (tmp);

	shell = ephy_shell_get_default ();
	g_assert (shell != NULL);

	session = EPHY_SESSION (ephy_shell_get_session (shell));
	g_assert (session != NULL);

	argv[0] = g_get_prgname ();
	argv[2] = save_to;
	gnome_client_set_restart_command
		(client, 3, argv);

	discard_argv[2] = save_to;
	gnome_client_set_discard_command (client, 3,
					  discard_argv);

	ephy_session_save (session, save_to);

	g_free (save_to);

	return TRUE;
}

static void
die_cb (GnomeClient* client,
	gpointer user_data)
	
{
	EphyShell *shell;
	EphySession *session;

	LOG ("die_cb");

	/* FIXME FIXME */
	if (!ephy_shell_get_default ()) return;

	shell = ephy_shell_get_default ();
	g_assert (shell != NULL);

	session = EPHY_SESSION (ephy_shell_get_session (shell));
	g_assert (session != NULL);

	ephy_session_close (session);
}

static void
gnome_session_init (void)
{
	GnomeClient *client;

	client = gnome_master_client ();

	g_signal_connect (client, "save_yourself",
			  G_CALLBACK (save_yourself_cb), NULL);
	g_signal_connect (client, "die",
			  G_CALLBACK (die_cb), NULL);
}

#if 0
static char *
path_from_command_line_arg (const char *arg)
{
	char path[PATH_MAX];

	if (realpath (arg, path) != NULL)
	{
		return g_strdup (path);
	}
	else
	{
		return g_strdup (arg);
	}
}
#endif

static void
unref_proxy_reply_cb (DBusGProxy *proxy,
		      GError *error,
		      gpointer user_data)
{
	if (error != NULL)
	{
		g_warning ("An error occured while calling remote method: %s", error->message);
		g_error_free (error);
	}

	g_object_unref (proxy);
}

static gboolean
open_urls (DBusGProxy *proxy,
	   guint32 user_time,
	   GError **error)
{
	EphyShell *shell;
	const char *options = "new-window";
	int i;
	
	shell = ephy_shell_get_default ();

	if (open_in_new_window)
	{
		options = "new-window";
	}
	else if (open_in_new_tab)
	{
		options = "new-tab";
	}

	if (remaining_arguments == NULL)
	{
		/* Homepage or resume */
		org_gnome_Epiphany_load_url_async
			(proxy, "", options, user_time,
			 unref_proxy_reply_cb, NULL /* FIXME! */);
	}
	else
	{
		for (i = 0; remaining_arguments[i] != NULL; ++i)
		{
			char *path;

			path = remaining_arguments[i];
			//path = path_from_command_line_arg (args[i]);

			org_gnome_Epiphany_load_url_async
				(proxy, path, options, user_time,
				 unref_proxy_reply_cb, NULL /* FIXME */);

			//g_free (path);
		}

		g_strfreev (remaining_arguments);
	}

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
	}
	else
	{
		/* no need to open the homepage if autoresume returns TRUE;
		 * we already opened session windows */
		if (!_ephy_dbus_is_name_owner () ||
		    (ephy_session_autoresume
			(EPHY_SESSION (ephy_shell_get_session (shell)),
			 user_time) == FALSE))
		{
			retval = open_urls (proxy, user_time, error);
		}
	}

	/* FIXME why? */
	dbus_g_connection_flush (ephy_dbus_get_bus (ephy_dbus_get_default (), EPHY_DBUS_SESSION));

	return retval;
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

int
main (int argc,
      char *argv[])
{
	GOptionContext *option_context;
	GOptionGroup *option_group;
	DBusGProxy *proxy;
	GError *error = NULL;
	guint32 user_time;
#ifndef GNOME_PARAM_GOPTION_CONTEXT
	GPtrArray *fake_argv_array;
#endif

#ifdef ENABLE_NLS
	/* Initialize the i18n stuff */
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif

	/* check libxml2 API version epiphany was compiled with against the
	 * version we're running with.
	 */
	LIBXML_TEST_VERSION

	/* Initialise our debug helpers */
	ephy_debug_init ();

	/* get this early, since gdk will unset the env var */
	user_time = get_startup_id ();

	option_context = g_option_context_new (_("GNOME Web Browser"));
	option_group = g_option_group_new ("epiphany",
					   N_("GNOME Web Browser"),
					   N_("GNOME Web Browser options"),
					   NULL, NULL);
	g_option_group_add_entries (option_group, option_entries);

	g_option_context_set_main_group (option_context, option_group);

#ifdef GNOME_PARAM_GOPTION_CONTEXT
	gnome_program_init (PACKAGE, VERSION,
			    LIBGNOMEUI_MODULE, argc, argv,
			    GNOME_PARAM_GOPTION_CONTEXT, option_context,
			    GNOME_PARAM_HUMAN_READABLE_NAME, _("Web Browser"),
			    GNOME_PARAM_APP_DATADIR, DATADIR,
			    NULL);

#else /* !GNOME_PARAM_GOPTION_CONTEXT */

	option_group = g_option_group_new ("gnome-compat", "GNOME GUI Library",
					   "Show GNOME GUI options", NULL, NULL);
	g_option_group_set_translation_domain (option_group, "libgnomeui-2.0");
	g_option_group_add_entries (option_group, libgnome_option_entries);
	g_option_context_add_group (option_context, option_group);

	/* Add the gtk+ option group, but don't open the default display! */
	option_group = gtk_get_option_group (FALSE);
	g_option_context_add_group (option_context, option_group);

	if (!g_option_context_parse (option_context, &argc, &argv, &error))
	{
		g_print ("%s\n", error->message);
		g_error_free (error);
		exit (1);
	}

	fake_argv_array = g_ptr_array_new ();
	
	g_ptr_array_add (fake_argv_array, g_strdup (g_get_prgname ()));
	if (sm_disable)
	{
		g_ptr_array_add (fake_argv_array, g_strdup ("--sm-disable"));
	}
	if (sm_client_id != NULL)
	{
		g_ptr_array_add (fake_argv_array, g_strdup ("--sm-client-id"));
		g_ptr_array_add (fake_argv_array, sm_client_id);
	}
	if (sm_config_prefix != NULL)
	{
		g_ptr_array_add (fake_argv_array, g_strdup ("--sm-config-prefix"));
		g_ptr_array_add (fake_argv_array, sm_config_prefix);
	}
	if (disable_crash_dialog)
	{
		g_ptr_array_add (fake_argv_array, g_strdup ("--disable-crash-dialog"));
	}

	gnome_program_init (PACKAGE, VERSION,
			    LIBGNOMEUI_MODULE,
			    fake_argv_array->len,
			    (char**) fake_argv_array->pdata,
			    GNOME_PARAM_HUMAN_READABLE_NAME, _("Web Browser"),
			    GNOME_PARAM_APP_DATADIR, DATADIR,
			    NULL);

	g_ptr_array_add (fake_argv_array, NULL);
	g_strfreev ((char**) g_ptr_array_free (fake_argv_array, FALSE));

#endif /* GNOME_PARAM_GOPTION_CONTEXT */

	/* Get a timestamp manually if need be */
	if (user_time == 0)
	{
		user_time = slowly_and_stupidly_obtain_timestamp (gdk_display);
	}

	/* sets the name to appear in the window list applet when grouping windows */
	g_set_application_name (_("Web Browser"));

	/* Set default window icon */
	gtk_window_set_default_icon_name ("web-browser");

	startup_error_quark = g_quark_from_static_string ("epiphany-startup-error");

	if (!_ephy_dbus_startup (&error))
	{
		_ephy_dbus_release ();

		gdk_notify_startup_complete ();
		show_error_message (&error);

		exit (1);
	}

	/* If we're remoting, no need to start up any further services,
	 * just forward the call.
	 */
	if (!_ephy_dbus_is_name_owner ())
	{
		/* FIXME */
		proxy = ephy_dbus_get_proxy (ephy_dbus_get_default (), EPHY_DBUS_SESSION);
		if (proxy == NULL)
		{
			error = g_error_new (STARTUP_ERROR_QUARK,
					     0,
					     "Unable to get DBus proxy; aborting activation."); /* FIXME i18n */	
		}

		if (proxy != NULL &&
		    call_dbus_proxy (proxy, user_time, &error))
		{
			_ephy_dbus_release ();

			gdk_notify_startup_complete ();
			exit (0);
		}

		_ephy_dbus_release ();

		gdk_notify_startup_complete ();
		show_error_message (&error);

		exit (1);
	}

	/* We're not remoting; start our services */

	if (!ephy_file_helpers_init (&error))
	{
		_ephy_dbus_release ();

		gdk_notify_startup_complete ();
		show_error_message (&error);

		exit (1);
	}

	/* init the session manager up here so we can quit while the resume dialogue is shown */
	gnome_session_init ();

	eel_gconf_monitor_add ("/apps/epiphany/general");
	gnome_vfs_init ();
	ephy_stock_icons_init ();

	/* Extensions may want these, so don't initialize in window-cmds */
	gtk_about_dialog_set_url_hook (handle_url, NULL, NULL);
	gtk_about_dialog_set_email_hook (handle_email, NULL, NULL);

	/* Now create the shell */
	_ephy_shell_create_instance ();

	/* Create DBUS proxy */
	proxy = ephy_dbus_get_proxy (ephy_dbus_get_default (), EPHY_DBUS_SESSION);
	if (proxy == NULL)
	{
		error = g_error_new (STARTUP_ERROR_QUARK,
				     0,
				     "Unable to get DBus proxy; aborting activation."); /* FIXME i18n */

		g_object_unref (ephy_shell_get_default ());

		_ephy_dbus_release ();

		gdk_notify_startup_complete ();
		show_error_message (&error);

		exit (1);
	}

	g_object_weak_ref (G_OBJECT (proxy),
			   (GWeakNotify) dbus_g_proxy_finalized_cb,
			   g_object_ref (ephy_shell_get_default ()));

	if (!call_dbus_proxy (proxy, user_time, &error))
	{
		g_object_unref (ephy_shell_get_default ());

		_ephy_dbus_release ();

		gdk_notify_startup_complete ();
		show_error_message (&error);

		exit (1);
	}

	/* We'll release the initial reference on idle */
	g_object_weak_ref (G_OBJECT (ephy_shell), shell_weak_notify, NULL);
	ephy_object_idle_unref (ephy_shell);

	gtk_main ();

	/* Shutdown */
	eel_gconf_monitor_remove ("/apps/epiphany/general");
	gnome_accelerators_sync ();
	ephy_state_save ();
	ephy_file_helpers_shutdown ();
	gnome_vfs_shutdown ();
	xmlCleanupParser ();

	_ephy_dbus_release ();

	return 0;
}
