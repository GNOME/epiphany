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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ephy-shell.h"
#include "ephy-automation.h"
#include "ephy-window.h"
#include "ephy-file-helpers.h"
#include "EphyAutomation.h"

#include <libbonoboui.h>
#include <libgnome/gnome-program.h>
#include <libgnomeui/gnome-ui-init.h>
#include <gtk/gtkwindow.h>
#include <libgnomevfs/gnome-vfs-init.h>
#include <glade/glade-init.h>

#define EPHY_FACTORY_OAFIID "OAFIID:GNOME_Epiphany_Automation_Factory"

static gboolean
ephy_main_automation_init (void);
static gint
ephy_main_translate_url_arguments (poptContext context, gchar ***urls);
static gboolean
ephy_main_start (gpointer data);

GnomeProgram *program;
CORBA_Environment corba_env;			/* Global for downloader	*/
static gboolean open_in_existing      = FALSE;  /* load in existing window?     */
static gboolean open_in_new_tab       = FALSE;  /* force open in a new tab?     */
static gboolean noraise               = FALSE;  /* no raise                     */
static gboolean open_in_new_window    = FALSE;  /* force open in a new window?  */
static gboolean open_fullscreen       = FALSE;  /* open ephy in full screen ? */
static gchar   *session_filename      = NULL;   /* the session filename         */
static gchar   *geometry_string       = NULL;   /* the geometry string          */
static gchar   *bookmark_url          = NULL;   /* the temp bookmark to add     */
static gboolean close_option          = FALSE;  /* --close                      */
static gboolean quit_option           = FALSE;  /* --quit                       */
static gboolean ephy_server_mode    = FALSE;
static gboolean open_as_nautilus_view = FALSE;

static BonoboObject *automation_object;
static gint n_urls;
static gchar **url;
static gboolean first_instance;

/* command line argument parsing structure */
static struct poptOption popt_options[] =
{
	{ NULL, '\0', POPT_ARG_INCLUDE_TABLE, &bonobo_activation_popt_options, 0, NULL,
	  NULL },
	{ "new-tab", 'n', POPT_ARG_NONE, &open_in_new_tab, 0,
	  N_("Open a new tab in an existing Ephy window"),
	  NULL },
	{ "new-window", 'w', POPT_ARG_NONE, &open_in_new_window, 0,
	  N_("Open a new window in an existing Ephy process"),
	  NULL },
	{ "noraise", '\0', POPT_ARG_NONE, &noraise, 0,
	  N_("Do not raise the window when opening a page in an existing Ephy process"),
	  NULL },
	{ "fullscreen", 'f', POPT_ARG_NONE, &open_fullscreen, 0,
	  N_("Run Ephy in full screen mode"),
	  NULL },
	{ "existing", 'x', POPT_ARG_NONE, &open_in_existing, 0,
	  N_("Attempt to load URL in existing Ephy window"),
	  NULL },
	{ "load-session", 'l', POPT_ARG_STRING, &session_filename, 0,
	  N_("Load the given session file"),
	  N_("FILE") },
	{ "server", 's', POPT_ARG_NONE, &ephy_server_mode, 0,
	  N_("Don't open any windows; instead act as a server "
	     "for quick startup of new Ephy instances"),
	  NULL },
	{ "add-bookmark", 't', POPT_ARG_STRING, &bookmark_url,
	  0, N_("Add a bookmark (don't open any window)"),
	  N_("URL")},
	{ "geometry", 'g', POPT_ARG_STRING, &geometry_string,
	  0, N_("Create the initial window with the given geometry.\n"
		"see X(1) for the GEOMETRY format"),
	  N_("GEOMETRY")},
	{ "close", 'c', POPT_ARG_NONE, &close_option, 0,
	  N_("Close all Ephy windows"),
	  NULL },
	{ "quit", 'q', POPT_ARG_NONE, &quit_option, 0,
	  N_("Same as --close, but exits server mode too"),
	  NULL },
	{ "nautilus-view", 'v', POPT_ARG_NONE, &open_as_nautilus_view, 0,
	  N_("Used internally by the nautilus view"),
	  NULL },

	/* terminator, must be last */
	{ NULL, 0, 0, NULL, 0, NULL, NULL }
};

int
main (int argc, char *argv[])
{
	poptContext context;
        GValue context_as_value = { 0 };
	GnomeProgram *program;
	char *file;

#ifdef ENABLE_NLS
	/* Initialize the i18n stuff */
	bindtextdomain(GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);
#endif
	g_set_application_name (_("Epiphany Web Browser"));

	program = gnome_program_init (PACKAGE, VERSION,
                                      LIBGNOMEUI_MODULE, argc, argv,
                                      GNOME_PARAM_POPT_TABLE, popt_options,
                                      GNOME_PARAM_HUMAN_READABLE_NAME, _("Ephy"),
				      GNOME_PARAM_APP_DATADIR, DATADIR,
                                      NULL);

        g_object_get_property (G_OBJECT (program),
                               GNOME_PARAM_POPT_CONTEXT,
                               g_value_init (&context_as_value, G_TYPE_POINTER));

        context = g_value_get_pointer (&context_as_value);

	/* load arguments that aren't regular options (urls to load) */
        n_urls = ephy_main_translate_url_arguments (context, &url);

	first_instance = ephy_main_automation_init ();

	if (first_instance)
	{
		gnome_vfs_init ();

		glade_gnome_init ();

		ephy_shell_new ();

		file = gnome_program_locate_file
			(NULL, GNOME_FILE_DOMAIN_APP_PIXMAP,
			 "epiphany.png", TRUE, NULL);
		gtk_window_set_default_icon_from_file (file, NULL);
		g_free (file);

		g_idle_add ((GSourceFunc) ephy_main_start, NULL);

		bonobo_main ();

		gnome_vfs_shutdown ();
	}

	return 0;
}

static gboolean
ephy_main_start (gpointer data)
{
	GNOME_EphyAutomation gaserver;
	int i;

	CORBA_exception_init (&corba_env);

	gaserver = bonobo_activation_activate_from_id ("OAFIID:GNOME_Epiphany_Automation",
						       0, NULL, &corba_env);

	if (gaserver == NULL)
	{
		GtkWidget *dialog;
		dialog = gtk_message_dialog_new
			(NULL,
                         GTK_DIALOG_MODAL,
                         GTK_MESSAGE_ERROR,
                         GTK_BUTTONS_CLOSE,
                         "Ephy can't be used now. "
                         "Running the command \"bonobo-slay\" "
                         "from the console may fix the problem. If not, "
                         "you can try rebooting the computer or "
                         "installing Ephy again.\n\n"
                         "Bonobo couldn't locate the GNOME_Epiphany_Automation.server. ");
		gtk_dialog_run (GTK_DIALOG (dialog));

	}
	/* FIXME ephy --server doesnt work when not first istance */
	/* Server mode */
	else if (ephy_server_mode)
	{
		g_object_ref (G_OBJECT(ephy_shell));
	}
	/* load the session if requested */
	else if (session_filename)
	{
		GNOME_EphyAutomation_loadSession
			(gaserver, session_filename, &corba_env);
	}
	/* if found and we're given a bookmark to add... */
	else if (bookmark_url != NULL)
	{
		GNOME_EphyAutomation_addBookmark
			(gaserver, bookmark_url, &corba_env);
	}
	else if (close_option || quit_option)
	{
		GNOME_EphyAutomation_quit
			(gaserver, quit_option, &corba_env);
	}
	/* provided with urls? */
	else if (n_urls == 0 &&
		 !open_as_nautilus_view)
	{
		/* no, open a default window */
		GNOME_EphyAutomation_loadurl
			(gaserver, "",
			 geometry_string ?
			 geometry_string : "",
			 open_fullscreen,
			 open_in_existing,
			 open_in_new_window,
			 open_in_new_tab,
			 !noraise,
			 &corba_env);
	}
	else
	{
		/* open all of the urls */
		for (i = 0; i < n_urls; i++)
		{
			GNOME_EphyAutomation_loadurl
				(gaserver, url[i],
				 geometry_string ?
				 geometry_string : "",
				 open_fullscreen,
				 open_in_existing,
				 open_in_new_window,
				 open_in_new_tab,
				 !noraise,
				 &corba_env);
		}
	}

	/* Unref so it will exit if no more used */
	if (first_instance)
	{
		g_object_unref (G_OBJECT(ephy_shell));
	}

	if (gaserver)
	{
		bonobo_object_release_unref (gaserver, &corba_env);
	}

	CORBA_exception_free (&corba_env);

	gdk_notify_startup_complete ();

	return FALSE;
}

static gboolean
ephy_main_automation_init (void)
{
	CORBA_Object factory;

	factory = bonobo_activation_activate_from_id
		(EPHY_FACTORY_OAFIID,
		 Bonobo_ACTIVATION_FLAG_EXISTING_ONLY,
		 NULL, NULL);

	if (!factory)
	{
		automation_object = ephy_automation_new ();
		return TRUE;
	}
	else
	{
		ephy_main_start (NULL);
		g_message (_("Ephy already running, using existing process"));
		return FALSE;
	}
}

/**
 * translate_url_arguments: gather URL arguments and expand them fully
 * with realpath if they're filenames
 */
static gint
ephy_main_translate_url_arguments (poptContext context, gchar ***urls)
{
        gchar buffer[PATH_MAX];
        gchar **args;
        gint i, n;

        /* any context remaining? */
        if (context == NULL)
        {
                *urls = NULL;
                return 0;
        }

        /* get the args and check */
        args = (gchar **) poptGetArgs (context);
        if (args == NULL)
        {
                poptFreeContext (context);
                *urls = NULL;
                return 0;
        }

        /* count args */
        for (n = 0; args[n] != NULL; n++)
                /* nothing */;

        /* allocate pointer array */
        *urls = g_new0 (gchar *, n + 1);

        /* translate each one */
        for (i = 0; i < n; i++)
        {
                /* try to expand as files */
                if (realpath (args[i], buffer) != NULL)
                {
                        (*urls)[i] = g_strconcat ("file://", buffer, NULL);
                }
                else
                {
                        (*urls)[i] = g_strdup (args[i]);
                }
        }
        poptFreeContext (context);
        (*urls)[i] = NULL;

        /* return the number of urls */
        return n;
}
