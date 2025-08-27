/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2000-2002 Marco Pesenti Gritti
 *  Copyright © 2006, 2008 Christian Persch
 *  Copyright © 2011, 2012 Igalia S.L.
 *
 *  This file is part of Epiphany.
 *
 *  Epiphany is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Epiphany is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Epiphany.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "ephy-debug.h"
#include "ephy-embed-utils.h"
#include "ephy-file-helpers.h"
#include "ephy-flatpak-utils.h"
#include "ephy-profile-utils.h"
#include "ephy-session.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-string.h"
#include "ephy-web-app-utils.h"

#include <errno.h>
#include <glib/gi18n.h>
#include <glib-unix.h>
#include <gtk/gtk.h>
#include <libxml/xmlreader.h>
#include <libxml/xmlversion.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <sys/resource.h>

static gboolean open_in_new_window = FALSE;

static char *session_filename = NULL;
static char **arguments = NULL;
static char *application_to_delete = NULL;

static gboolean private_instance = FALSE;
static gboolean incognito_mode = FALSE;
static gboolean application_mode = FALSE;
static gboolean automation_mode = FALSE;
static char *desktop_file_basename = NULL;
static char *profile_directory = NULL;
static char *search_term = NULL;
static gboolean kiosk_mode = FALSE;

static EphyShell *ephy_shell = NULL;
static int shutdown_signum = 0;

static gboolean
handle_shutdown_signal (gpointer user_data)
{
  shutdown_signum = GPOINTER_TO_INT (user_data);

  /* Note that this function executes on the main loop AFTER the signal handler
   * has returned, so we don't have to worry about async signal safety.
   */
  g_assert (ephy_shell);
  ephy_shell_try_quit (ephy_shell);

  /* Goals:
   *
   * (1) Shutdown safely and cleanly if signal is received once.
   * (2) Shutdown unsafely but immediately if signal is received twice.
   * (3) Always re-raise the signal so the parent process knows what happened.
   *
   * Removing this source is required by goals (2) and (3).
   */
  return G_SOURCE_REMOVE;
}

static gboolean
application_mode_cb (const gchar  *option_name,
                     const gchar  *value,
                     gpointer      data,
                     GError      **error)
{
  application_mode = TRUE;
  desktop_file_basename = g_strdup (value);
  return TRUE;
}

static gboolean
option_version_cb (const gchar  *option_name,
                   const gchar  *value,
                   gpointer      data,
                   GError      **error)
{
  g_print ("%s %s\n", _("Web"), VERSION);

  exit (EXIT_SUCCESS);
  return FALSE;
}

/* If you're modifying this array then you need to update the manpage. */
static const GOptionEntry option_entries[] = {
  {
    "new-window", 0, 0, G_OPTION_ARG_NONE, &open_in_new_window,
    N_("Open a new browser window instead of a new tab"), NULL
  },
  {
    "load-session", 'l', 0, G_OPTION_ARG_FILENAME, &session_filename,
    N_("Load the given session state file"), N_("FILE")
  },
  {
    "incognito-mode", 'i', 0, G_OPTION_ARG_NONE, &incognito_mode,
    N_("Start an instance with user data read-only"), NULL
  },
  {
    "private-instance", 'p', 0, G_OPTION_ARG_NONE, &private_instance,
    N_("Start a private instance with separate user data"), NULL
  },
  {
    "application-mode", 'a', G_OPTION_FLAG_FILENAME | G_OPTION_FLAG_OPTIONAL_ARG,
    G_OPTION_ARG_CALLBACK, application_mode_cb,
    N_("Start a private instance in web application mode"), NULL
  },
  {
    "automation-mode", 0, 0, G_OPTION_ARG_NONE, &automation_mode,
    N_("Start a private instance for WebDriver control"), NULL
  },
  {
    "profile", 0, 0, G_OPTION_ARG_STRING, &profile_directory,
    N_("Custom profile directory for private instance"), N_("DIR")
  },
  {
    G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &arguments,
    "", N_("URL …")
  },
  {
    "version", 0, G_OPTION_FLAG_NO_ARG | G_OPTION_FLAG_HIDDEN,
    G_OPTION_ARG_CALLBACK, option_version_cb, NULL, NULL
  },
  {
    "delete-application", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING,
    &application_to_delete, NULL, NULL
  },
  { "search", 0, 0, G_OPTION_ARG_STRING, &search_term, NULL, NULL},
  { "kiosk-mode", 0, 0, G_OPTION_ARG_NONE, &kiosk_mode, NULL, NULL},
  { NULL }
};

static void
maximize_fd_limit (void)
{
  /* We need a relatively high number of file descriptors per web process. The
   * default limit of 1024 will be exceeded with relatively small numbers of
   * open tabs. Let's increase as far as we can. Note this code cannot safely
   * go into WebKit because it will break applications that use select().
   *
   * https://gitlab.gnome.org/GNOME/epiphany/-/issues/2010
   */
  struct rlimit rlim;

  if (getrlimit (RLIMIT_NOFILE, &rlim) == -1) {
    g_warning ("Failed to read file descriptor limit: %s", g_strerror (errno));
    return;
  }

  rlim.rlim_cur = rlim.rlim_max;
  if (setrlimit (RLIMIT_NOFILE, &rlim) == -1)
    g_warning ("Failed to set file descriptor limit: %s", g_strerror (errno));
}

int
main (int   argc,
      char *argv[])
{
  GOptionContext *option_context;
  GOptionGroup *option_group;
  GError *error = NULL;
  gboolean arbitrary_url;
  EphyShellStartupContext *ctx;
  EphyEmbedShellMode mode;
  int status;
  EphyFileHelpersFlags flags;
  GDesktopAppInfo *desktop_info = NULL;

#if DEVELOPER_MODE
  g_setenv ("GSETTINGS_SCHEMA_DIR", BUILD_ROOT "/data", FALSE);
#endif

  g_setenv ("PULSE_PROP_application.icon_name", APPLICATION_ID, TRUE);

  /* Initialize the i18n stuff */
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  /* check libxml2 API version epiphany was compiled with against the
   * version we're running with.
   */
  LIBXML_TEST_VERSION;

  ephy_debug_set_fatal_criticals ();

  /* If we're given -remote arguments, translate them */
  if (argc >= 2 && strcmp (argv[1], "-remote") == 0) {
    const char *opening, *closing;
    char *command, *argument;
    char **arg_list;

    if (argc != 3) {
      g_print ("-remote allows exactly one argument\n");
      exit (1);
    }

    opening = strchr (argv[2], '(');
    closing = strchr (argv[2], ')');

    if (!opening ||
        !closing ||
        opening == argv[2] ||
        opening + 1 >= closing) {
      g_print ("Invalid argument for -remote\n");
      exit (1);
    }

    command = g_strstrip (g_strndup (argv[2], opening - argv[2]));

    /* See http://lxr.mozilla.org/seamonkey/source/xpfe/components/xremote/src/XRemoteService.cpp
     * for the commands that mozilla supports; we'll just support openURL here.
     */
    if (g_ascii_strcasecmp (command, "openURL") != 0) {
      g_print ("-remote command \"%s\" not supported\n", command);
      g_free (command);
      exit (1);
    }

    g_free (command);

    argument = g_strstrip (g_strndup (opening + 1, closing - opening - 1));
    arg_list = g_strsplit (argument, ",", -1);
    g_free (argument);
    if (!arg_list) {
      g_print ("Invalid argument for -remote\n");

      exit (1);
    }

    /* replace arguments */
    argv[1] = g_strstrip (g_strdup (arg_list[0]));
    argc = 2;

    g_strfreev (arg_list);
  }

  /* Initialize our debug helpers */
  ephy_debug_init ();

  option_context = g_option_context_new ("");
  option_group = g_option_group_new ("epiphany",
                                     N_("Web"),
                                     N_("Web options"),
                                     NULL, NULL);

  g_option_group_set_translation_domain (option_group, GETTEXT_PACKAGE);

  g_option_group_add_entries (option_group, option_entries);

  g_option_context_set_main_group (option_context, option_group);

  if (!g_option_context_parse (option_context, &argc, &argv, &error)) {
    g_print ("Failed to parse arguments: %s\n", error->message);
    g_error_free (error);
    g_option_context_free (option_context);
    exit (1);
  }

  g_option_context_free (option_context);

  /* Some argument sanity checks*/
  if (application_to_delete && argc > 3) {
    g_print ("Cannot pass any other parameter when using --delete-application\n");
    exit (1);
  }

  if (private_instance && application_mode) {
    g_print ("Cannot use --private-instance and --application-mode at the same time\n");
    exit (1);
  }

  if (private_instance && profile_directory) {
    g_print ("Cannot use --private-instance and --profile at the same time\n");
    exit (1);
  }

  if (automation_mode && (private_instance || incognito_mode || application_mode || profile_directory)) {
    g_print ("Cannot use --automation-mode and --private-instance, --incognito-mode, --application-mode, or --profile at the same time\n");
    exit (1);
  }

  if (application_mode && profile_directory && !g_file_test (profile_directory, G_FILE_TEST_IS_DIR)) {
    g_print ("--profile must be an existing directory when --application-mode is requested\n");
    exit (1);
  }

  if (application_mode && !profile_directory) {
    if (desktop_file_basename) {
      if (ephy_is_running_inside_sandbox ()) {
        g_print ("In sandbox, no desktop file can be passed to --application-mode\n");
        exit (1);
      } else {
        desktop_info = g_desktop_app_info_new (desktop_file_basename);

        if (desktop_info)
          profile_directory = ephy_web_application_ensure_for_app_info (G_APP_INFO (desktop_info));

        if (!profile_directory) {
          g_print ("Invalid desktop file passed to --application-mode\n");
          exit (1);
        }
      }
    }
  }

  if (application_mode && !profile_directory) {
    g_print ("--profile must be used when --application-mode is requested without desktop file path\n");
    exit (1);
  }

  if (incognito_mode && !profile_directory)
    profile_directory = ephy_default_profile_dir ();

  /* Required for bubblewrap sandbox. */
  flags = EPHY_FILE_HELPERS_ENSURE_EXISTS;

  if (incognito_mode || private_instance || application_mode || automation_mode)
    flags |= EPHY_FILE_HELPERS_PRIVATE_PROFILE;
  if (incognito_mode)
    flags |= EPHY_FILE_HELPERS_STEAL_DATA;

  if (!ephy_file_helpers_init (profile_directory, flags, &error)) {
    g_error ("Fatal initialization error: %s", error->message);
  }

  if (g_settings_get_boolean (EPHY_SETTINGS_MAIN, EPHY_PREFS_START_IN_INCOGNITO_MODE) && !application_mode && !open_in_new_window) {
    incognito_mode = TRUE;
  }

  /* Run the migration in all cases, except when running a private
   *  instance without a given profile directory or running in
   *  incognito or automation mode. */
  if (!(private_instance && !profile_directory) && !incognito_mode && !automation_mode) {
    /* If the migration fails we don't really want to continue. */
    if (!ephy_profile_utils_do_migration ((const char *)profile_directory, -1, FALSE)) {
      g_print ("Failed to run the migrator process, Web will now abort.\n");
      exit (1);
    }
  }

  /* Ignore arguments in automation mode */
  if (automation_mode)
    g_clear_pointer (&arguments, g_strfreev);

  arbitrary_url = g_settings_get_boolean (EPHY_SETTINGS_LOCKDOWN,
                                          EPHY_PREFS_LOCKDOWN_ARBITRARY_URL);

  if (arguments && arbitrary_url) {
    g_print ("URL loading is locked down.\n");
    exit (1);
  }

  /* convert arguments to uris or at least to utf8 */
  if (arguments) {
    char **args = ephy_string_commandline_args_to_uris (arguments,
                                                        &error);
    if (error) {
      g_print ("Could not convert to UTF-8: %s!\n",
               error->message);
      g_error_free (error);
      exit (1);
    }
    g_strfreev (arguments);
    arguments = args;
  }

  /* Delete the requested web application, if any. Must happen after
   * ephy_file_helpers_init (). */
  if (application_to_delete) {
    ephy_web_application_delete (application_to_delete, NULL);
    exit (0);
  }

  /* Now create the shell */
  if (private_instance) {
    mode = EPHY_EMBED_SHELL_MODE_PRIVATE;
  } else if (incognito_mode) {
    mode = EPHY_EMBED_SHELL_MODE_INCOGNITO;
  } else if (automation_mode) {
    mode = EPHY_EMBED_SHELL_MODE_AUTOMATION;
  } else if (application_mode) {
    mode = EPHY_EMBED_SHELL_MODE_APPLICATION;

    if (desktop_info) {
      ephy_web_application_setup_from_desktop_file (desktop_info);
      g_object_unref (desktop_info);
    } else {
      ephy_web_application_setup_from_profile_directory (profile_directory);
    }
  } else if (profile_directory) {
    /* This mode exists purely for letting EphyShell know it should
     * not consider this instance part of the unique application
     * represented by the BROWSER mode.
     */
    mode = EPHY_EMBED_SHELL_MODE_STANDALONE;
  } else if (kiosk_mode) {
    mode = EPHY_EMBED_SHELL_MODE_KIOSK;
  } else {
    mode = EPHY_EMBED_SHELL_MODE_BROWSER;

    g_set_prgname ("epiphany");
    g_set_application_name (_("Web"));

    gtk_window_set_default_icon_name (APPLICATION_ID);
  }

  maximize_fd_limit ();

  _ephy_shell_create_instance (mode);

  if (search_term) {
    int current_len = arguments ? g_strv_length (arguments) : 0;
    arguments = g_realloc (arguments, current_len + 1 * sizeof (char *));
    arguments[current_len] = ephy_embed_utils_autosearch_address (search_term);
    g_clear_pointer (&search_term, g_free);
  }

  ctx = ephy_shell_startup_context_new (open_in_new_window ? EPHY_STARTUP_NEW_WINDOW : EPHY_STARTUP_NEW_TAB,
                                        session_filename,
                                        arguments);
  g_strfreev (arguments);
  ephy_shell = ephy_shell_get_default ();
  ephy_shell_set_startup_context (ephy_shell, ctx);

  g_unix_signal_add (SIGINT, (GSourceFunc)handle_shutdown_signal, GINT_TO_POINTER (SIGINT));
  g_unix_signal_add (SIGTERM, (GSourceFunc)handle_shutdown_signal, GINT_TO_POINTER (SIGTERM));

  status = g_application_run (G_APPLICATION (ephy_shell), argc, argv);

  /* Ensure the EphyShell really gets destroyed. This should be the final reference. */
  g_object_add_weak_pointer (G_OBJECT (ephy_shell), (gpointer *)&ephy_shell);
  g_object_unref (ephy_shell);
  g_assert (!ephy_shell);

  /* FIXME: should free this stuff even when exiting early */
  g_free (desktop_file_basename);
  g_free (profile_directory);

  ephy_settings_shutdown ();
  ephy_file_helpers_shutdown ();
  xmlCleanupParser ();

  if (shutdown_signum != 0)
    raise (shutdown_signum);

  return status;
}
