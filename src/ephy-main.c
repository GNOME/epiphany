/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2000-2002 Marco Pesenti Gritti
 *  Copyright © 2006, 2008 Christian Persch
 *  Copyright © 2011 Igalia S.L.
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

#include "ephy-debug.h"
#include "ephy-embed-prefs.h"
#include "ephy-file-helpers.h"
#include "ephy-private.h"
#include "ephy-session.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-state.h"
#include "ephy-string.h"
#include "ephy-web-app-utils.h"

#include <errno.h>
#include <gdk/gdkx.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libnotify/notify.h>
#include <libxml/xmlversion.h>
#include <string.h>

#ifdef ENABLE_INTROSPECTION
#include <girepository.h>
#endif

static GQuark startup_error_quark = 0;
#define STARTUP_ERROR_QUARK (startup_error_quark)

static gboolean open_in_new_tab = FALSE;
static gboolean open_in_new_window = FALSE;
static gboolean open_as_bookmarks_editor = FALSE;
/*static gboolean reload_plugins = FALSE;*/

static char *session_filename = NULL;
static char *bookmark_url = NULL;
static char *bookmarks_file = NULL;
static char **arguments = NULL;
static char *application_to_delete = NULL;

/* Only set from options in debug builds */
static gboolean private_instance = FALSE;
static gboolean application_mode = FALSE;
static gboolean keep_temp_directory = FALSE;
static char *profile_directory = NULL;

static gboolean
option_version_cb (const gchar *option_name,
                   const gchar *value,
                   gpointer     data,
                   GError     **error)
{
  g_print ("%s %s\n", _("Web"), VERSION);

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
  { "application-mode", 'a', 0, G_OPTION_ARG_NONE, &application_mode,
    N_("Start the browser in application mode"), NULL },
  { "profile", 0, 0, G_OPTION_ARG_STRING, &profile_directory,
    N_("Profile directory to use in the private instance"), N_("DIR") },
  { G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &arguments,
    "", N_("URL …")},
  { "version", 0, G_OPTION_FLAG_NO_ARG | G_OPTION_FLAG_HIDDEN, 
    G_OPTION_ARG_CALLBACK, option_version_cb, NULL, NULL },
  { "delete-application", 0, 0, G_OPTION_ARG_STRING | G_OPTION_FLAG_HIDDEN,
    &application_to_delete, NULL, NULL },
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
  if (time_str != NULL) {
    gulong value;
    gchar *end;
    errno = 0;
    
    /* Skip past the "_TIME" part */
    time_str += 5;
    
    value = strtoul (time_str, &end, 0);
    if (end != time_str && errno == 0)
      retval = (guint32) value;
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
show_error_message (GError **error)
{
  GtkWidget *dialog;

  /* FIXME better texts!!! */
  dialog = gtk_message_dialog_new (NULL,
                                   GTK_DIALOG_MODAL,
                                   GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_CLOSE,
                                   _("Could not start Web"));
  gtk_message_dialog_format_secondary_text
    (GTK_MESSAGE_DIALOG (dialog),
     _("Startup failed because of the following error:\n%s"),
     (*error)->message);

  g_clear_error (error);

  gtk_dialog_run (GTK_DIALOG (dialog));
}

static EphyStartupFlags
get_startup_flags (void)
{
  EphyStartupFlags flags = 0;

  if (open_in_new_tab)
    flags |= EPHY_STARTUP_NEW_TAB;
  if (open_in_new_window)
    flags |= EPHY_STARTUP_NEW_WINDOW;
  if (open_as_bookmarks_editor)
    flags |= EPHY_STARTUP_BOOKMARKS_EDITOR;

  return flags;
}

int
main (int argc,
      char *argv[])
{
  GOptionContext *option_context;
  GOptionGroup *option_group;
  GError *error = NULL;
  guint32 user_time;
  gboolean arbitrary_url;
  EphyShellStartupContext *ctx;
  EphyStartupFlags startup_flags;
  EphyEmbedShellMode mode;
  int status;

#ifdef ENABLE_NLS
  /* Initialize the i18n stuff */
  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
#endif

  /* check libxml2 API version epiphany was compiled with against the
   * version we're running with.
   */
  LIBXML_TEST_VERSION;

  notify_init (PACKAGE);

  /* If we're given -remote arguments, translate them */
  if (argc >= 2 && strcmp (argv[1], "-remote") == 0) {
    const char *opening, *closing;
    char *command, *argument;
    char **arguments;
    
    if (argc != 3) {
      g_print ("-remote allows exactly one argument\n");
      exit (1);
    }
    
    opening = strchr (argv[2], '(');
    closing = strchr (argv[2], ')');
    
    if (opening == NULL ||
        closing == NULL ||
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
    arguments = g_strsplit (argument, ",", -1);
    g_free (argument);
    if (arguments == NULL) {
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
                                     N_("Web"),
                                     N_("Web options"),
                                     NULL, NULL);

  g_option_group_set_translation_domain (option_group, GETTEXT_PACKAGE);

  g_option_group_add_entries (option_group, option_entries);

  g_option_context_set_main_group (option_context, option_group);

#ifdef ENABLE_INTROSPECTION
  g_option_context_add_group (option_context, g_irepository_get_option_group ());
#endif

  g_option_context_add_group (option_context, gtk_get_option_group (TRUE));

#ifdef GNOME_ENABLE_DEBUG
  option_group = g_option_group_new ("debug",
                                     "Epiphany debug options",
                                     "Epiphany debug options",
                                     NULL, NULL);
  g_option_group_add_entries (option_group, debug_option_entries);
  g_option_context_add_group (option_context, option_group);
#endif /* GNOME_ENABLE_DEBUG */

  if (!g_option_context_parse (option_context, &argc, &argv, &error)) {
    g_print ("Failed to parse arguments: %s\n", error->message);
    g_error_free (error);
    g_option_context_free (option_context);
    exit (1);
  }
        
  g_option_context_free (option_context);

  /* Some argument sanity checks*/
  if (application_to_delete != NULL && argc > 3) {
    g_print ("Cannot pass any other parameter when using --delete-application\n");
    exit (1);
  }

  if (arguments != NULL && (session_filename != NULL || open_as_bookmarks_editor)) {
    g_print ("Cannot use --bookmarks-editor or --load-session with URL arguments  \n");
    exit (1);
  }

  if (private_instance == TRUE && application_mode == TRUE) {
    g_print ("Cannot use --private-instance and --application-mode at the same time\n");
    exit (1);
  }

  if (profile_directory != NULL && private_instance == FALSE && application_mode == FALSE) {
    g_print ("--profile can only be used in combination with --private-instance or --application-mode\n");
    exit (1);
  }

  if (application_mode && profile_directory == NULL) {
    g_print ("--profile must be used when --application-mode is requested\n");
    exit (1);
  }

  if (application_mode && !g_file_test (profile_directory, G_FILE_TEST_IS_DIR)) {
      g_print ("--profile must be an existing directory when --application-mode is requested\n");
      exit (1);
  }

  /* Work-around Flash Player crash */
  g_setenv ("XLIB_SKIP_ARGB_VISUALS", "1", FALSE);

  /* Start our services */
  if (!ephy_file_helpers_init (profile_directory,
                               private_instance || application_mode,
                               keep_temp_directory || profile_directory,
                               &error)) {
    show_error_message (&error);
    exit (1);
  }

  arbitrary_url = g_settings_get_boolean (EPHY_SETTINGS_LOCKDOWN,
                                          EPHY_PREFS_LOCKDOWN_ARBITRARY_URL);

  if (arguments != NULL && arbitrary_url) {
    g_print ("URL loading is locked dow n\n");
    exit (1);
  }

  /* convert arguments to uris or at least to utf8 */
  if (arguments != NULL) {
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

  /* Get a timestamp manually if need be */
  if (user_time == 0)
      user_time = slowly_and_stupidly_obtain_timestamp (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()));

  startup_error_quark = g_quark_from_static_string ("epiphany-startup-error");

  /* Delete the requested web application, if any. Must happen after
   * ephy_file_helpers_init (). */
  if (application_to_delete) {
    ephy_web_application_delete (application_to_delete);
    exit (0);
  }

  /* Now create the shell */
  if (private_instance)
    mode = EPHY_EMBED_SHELL_MODE_PRIVATE;
  else if (application_mode) {
    char *app_name;
    char *app_icon;

    mode = EPHY_EMBED_SHELL_MODE_APPLICATION;

    app_name = g_strrstr (profile_directory, EPHY_WEB_APP_PREFIX);
    app_icon = g_build_filename (profile_directory, EPHY_WEB_APP_ICON_NAME, NULL);

    if (app_name) {
      /* Skip the 'app-' part */
      app_name += strlen (EPHY_WEB_APP_PREFIX);

      g_set_prgname (app_name);
      g_set_application_name (app_name);

      gtk_window_set_default_icon_from_file (app_icon, NULL);

      /* We need to re-set this because we have already parsed the
       * options, which inits GTK+ and sets this as a side effect. */
      gdk_set_program_class (app_name);
    }

    g_free (app_icon);
  } else {
    mode = EPHY_EMBED_SHELL_MODE_BROWSER;

    g_set_prgname ("epiphany");
    g_set_application_name (_("Web"));

    gtk_window_set_default_icon_name ("web-browser");
  }

  ephy_embed_prefs_init ();
  _ephy_shell_create_instance (mode);

  startup_flags = get_startup_flags ();
  ctx = ephy_shell_startup_context_new (startup_flags,
                                        bookmarks_file,
                                        session_filename,
                                        bookmark_url,
                                        arguments,
                                        user_time);
  g_strfreev (arguments);
  ephy_shell_set_startup_context (ephy_shell, ctx);
  status = g_application_run (G_APPLICATION (ephy_shell), argc, argv);

  /* Shutdown */
  g_object_unref (ephy_shell);

  if (notify_is_initted ())
    notify_uninit ();

  ephy_state_save ();
  ephy_embed_prefs_shutdown ();
  ephy_settings_shutdown ();
  ephy_file_helpers_shutdown ();
  xmlCleanupParser ();

  return status;
}
