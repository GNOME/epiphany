/*
 *  Copyright (C) 2000-2004 Marco Pesenti Gritti
 *  Copyright (C) 2003, 2004 Christian Persch
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ephy-shell.h"
#include "ephy-embed-shell.h"
#include "eel-gconf-extensions.h"
#include "ephy-prefs.h"
#include "ephy-file-helpers.h"
#include "ephy-favicon-cache.h"
#include "ephy-window.h"
#include "ephy-bookmarks-import.h"
#include "ephy-bookmarks-editor.h"
#include "ephy-history-window.h"
#include "pdm-dialog.h"
#include "prefs-dialog.h"
#include "ephy-debug.h"
#include "ephy-extensions-manager.h"
#include "toolbar.h"
#include "ephy-session.h"
#include "downloader-view.h"
#include "egg-toolbars-model.h"
#include "ephy-toolbars-model.h"
#include "ephy-automation.h"
#include "print-dialog.h"

#include <string.h>
#include <bonobo/bonobo-main.h>
#include <glib/gi18n.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkmessagedialog.h>
#include <dirent.h>
#include <unistd.h>
#include <libgnomeui/gnome-client.h>

#define AUTOMATION_IID "OAFIID:GNOME_Epiphany_Automation"
#define SERVER_TIMEOUT 60000

#define EPHY_SHELL_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_SHELL, EphyShellPrivate))

struct EphyShellPrivate
{
	BonoboGenericFactory *automation_factory;
	EphySession *session;
	EphyBookmarks *bookmarks;
	EggToolbarsModel *toolbars_model;
	EggToolbarsModel *fs_toolbars_model;
	EphyExtensionsManager *extensions_manager;
	GtkWidget *bme;
	GtkWidget *history_window;
	GObject *pdm_dialog;
	GObject *prefs_dialog;
	GObject *print_setup_dialog;
	GList *del_on_exit;
	guint server_timeout;
};

EphyShell *ephy_shell = NULL;

static void ephy_shell_class_init	(EphyShellClass *klass);
static void ephy_shell_init		(EphyShell *shell);
static void ephy_shell_finalize		(GObject *object);

static GObjectClass *parent_class = NULL;

GQuark
ephy_shell_error_quark (void)
{
	static GQuark q = 0;

	if (q == 0)
	{
		q = g_quark_from_static_string ("ephy-shell-error-quark");
	}

	return q;
}

GType
ephy_shell_get_type (void)
{
	static GType type = 0;

	if (type == 0)
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyShellClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) ephy_shell_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (EphyShell),
			0, /* n_preallocs */
			(GInstanceInitFunc) ephy_shell_init
		};

		type = g_type_register_static (EPHY_TYPE_EMBED_SHELL,
					       "EphyShell",
					       &our_info, 0);
	}

	return type;
}

static void
ephy_shell_class_init (EphyShellClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = ephy_shell_finalize;

	g_type_class_add_private (object_class, sizeof(EphyShellPrivate));
}

static BonoboObject *
ephy_automation_factory_cb (BonoboGenericFactory *this_factory,
			    const char *iid,
			    EphyShell *shell)
{
	if (strcmp (iid, AUTOMATION_IID) == 0)
	{
		return BONOBO_OBJECT (g_object_new (EPHY_TYPE_AUTOMATION, NULL));
	}

	g_assert_not_reached ();

	return NULL;
}

static BonoboGenericFactory *
ephy_automation_factory_new (EphyShell *shell)
{
	BonoboGenericFactory *factory;
	GClosure *factory_closure;

	factory = g_object_new (bonobo_generic_factory_get_type (), NULL);

	factory_closure = g_cclosure_new
		(G_CALLBACK (ephy_automation_factory_cb), shell, NULL);

	bonobo_generic_factory_construct_noreg
		(factory, AUTOMATION_FACTORY_IID, factory_closure);

	return factory;
}

static void
ephy_shell_init (EphyShell *shell)
{
	EphyShell **ptr = &ephy_shell;

	shell->priv = EPHY_SHELL_GET_PRIVATE (shell);

	shell->priv->session = NULL;
	shell->priv->bookmarks = NULL;
	shell->priv->bme = NULL;
	shell->priv->history_window = NULL;
	shell->priv->pdm_dialog = NULL;
	shell->priv->print_setup_dialog = NULL;
	shell->priv->toolbars_model = NULL;
	shell->priv->fs_toolbars_model = NULL;
	shell->priv->extensions_manager = NULL;
	shell->priv->server_timeout = 0;

	/* globally accessible singleton */
	g_assert (ephy_shell == NULL);
	ephy_shell = shell;
	g_object_add_weak_pointer (G_OBJECT(ephy_shell),
				   (gpointer *)ptr);

	/* Instantiate the automation factory */
	shell->priv->automation_factory = ephy_automation_factory_new (shell);
}

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

static void
open_urls (GNOME_EphyAutomation automation,
	   const char **args, CORBA_Environment *ev,
	   gboolean new_tab, gboolean existing_window,
	   gboolean fullscreen)
{
	int i;

	if (args == NULL)
	{
		/* Homepage or resume */
		GNOME_EphyAutomation_loadurl
			(automation, "", fullscreen,
			 existing_window, new_tab, ev);
	}
	else
	{
		for (i = 0; args[i] != NULL; i++)
		{
			char *path;

			path = path_from_command_line_arg (args[i]);

			GNOME_EphyAutomation_loadurl
				(automation, path, fullscreen,
				 existing_window, new_tab, ev);

			g_free (path);
		}
	}
}

static gboolean
server_timeout (EphyShell *shell)
{
	g_object_unref (shell);

	return FALSE;
}

static gboolean
save_yourself_cb (GnomeClient *client,
		  gint phase,
		  GnomeSaveStyle save_style,
		  gboolean shutdown,
		  GnomeInteractStyle interact_style,
		  gboolean fast,
		  EphyShell *shell)
{
	char *argv[] = { "epiphany", "--load-session", NULL };
	char *discard_argv[] = { "rm", "-f", NULL };
	EphySession *session;
	char *tmp, *save_to;

	tmp = g_build_filename (ephy_dot_dir (),
				"session_gnome-XXXXXX",
				NULL);
	save_to = ephy_file_tmp_filename (tmp, "xml");
	g_free (tmp);

	session = EPHY_SESSION (ephy_shell_get_session (shell));

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
	EphyShell *shell)
{
	EphySession *session;

	session = EPHY_SESSION (ephy_shell_get_session (shell));
	ephy_session_close (session);
}

static void
gnome_session_init (EphyShell *shell)
{
	GnomeClient *client;

	client = gnome_master_client ();

	g_signal_connect (G_OBJECT (client),
			  "save_yourself",
			  G_CALLBACK (save_yourself_cb),
			  shell);
	g_signal_connect (G_OBJECT (client),
			  "die",
			  G_CALLBACK (die_cb),
			  shell);
}

gboolean
ephy_shell_startup (EphyShell *shell,
		    EphyShellStartupFlags flags,
		    const char **args,
		    const char *string_arg,
		    GError **error)
{
	CORBA_Environment ev;
	GNOME_EphyAutomation automation;
	Bonobo_RegistrationResult result;

	ephy_ensure_dir_exists (ephy_dot_dir ());

	CORBA_exception_init (&ev);

	result = bonobo_activation_register_active_server
		(AUTOMATION_FACTORY_IID, BONOBO_OBJREF (shell->priv->automation_factory), NULL);

	switch (result)
	{
		case Bonobo_ACTIVATION_REG_SUCCESS:
			break;
		case Bonobo_ACTIVATION_REG_ALREADY_ACTIVE:
			break;
		case Bonobo_ACTIVATION_REG_NOT_LISTED:
			g_set_error (error, EPHY_SHELL_ERROR,
				     EPHY_SHELL_ERROR_MISSING_SERVER,
				     _("Bonobo couldn't locate the GNOME_Epiphany_Automation.server "
				       "file. You can use bonobo-activation-sysconf to configure "
				       "the search path for bonobo server files."));
			break;
		case Bonobo_ACTIVATION_REG_ERROR:
			g_set_error (error, EPHY_SHELL_ERROR,
				     EPHY_SHELL_ERROR_FACTORY_REG_FAILED,
				     _("Epiphany can't be used now, due to an unexpected error "
				       "from Bonobo when attempting to register the automation "
				       "server"));
			break;
		default:
			g_assert_not_reached ();
	}

	if (flags & EPHY_SHELL_STARTUP_SERVER)
	{
		g_object_ref (shell);
		shell->priv->server_timeout = g_timeout_add
			(SERVER_TIMEOUT, (GSourceFunc)server_timeout, shell);
	}
	else if (result == Bonobo_ACTIVATION_REG_SUCCESS ||
		 result == Bonobo_ACTIVATION_REG_ALREADY_ACTIVE)
	{
		automation = bonobo_activation_activate_from_id (AUTOMATION_IID,
								 0, NULL, &ev);
		if (CORBA_Object_is_nil (automation, &ev))
		{
			g_set_error (error, EPHY_SHELL_ERROR,
				     EPHY_SHELL_ERROR_OBJECT_REG_FAILED,
				     _("Epiphany can't be used now, due to an unexpected error "
				       "from Bonobo when attempting to locate the automation "
				       "object."));
			automation = NULL;
		}
		else if (flags & EPHY_SHELL_STARTUP_BOOKMARKS_EDITOR)
		{
			GNOME_EphyAutomation_openBookmarksEditor
				(automation, &ev);
		}
		else if (flags & EPHY_SHELL_STARTUP_SESSION)
		{
			GNOME_EphyAutomation_loadSession
				(automation, string_arg, &ev);
		}
		else if (flags & EPHY_SHELL_STARTUP_IMPORT_BOOKMARKS)
		{
			GNOME_EphyAutomation_importBookmarks
				(automation, string_arg, &ev);
		}
		else if (flags & EPHY_SHELL_STARTUP_ADD_BOOKMARK)
		{
			GNOME_EphyAutomation_addBookmark
				(automation, string_arg, &ev);
		}
		else
		{
			open_urls (automation, args, &ev,
				   flags & EPHY_SHELL_STARTUP_TABS,
				   flags & EPHY_SHELL_STARTUP_EXISTING_WINDOW,
				   flags & EPHY_SHELL_STARTUP_FULLSCREEN);
		}

		if (automation)
		{
			bonobo_object_release_unref (automation, &ev);
		}

		gnome_session_init (shell);
	}

	CORBA_exception_free (&ev);
	gdk_notify_startup_complete ();

	return !(result == Bonobo_ACTIVATION_REG_ALREADY_ACTIVE);
}

static void
ephy_shell_finalize (GObject *object)
{
	EphyShell *shell = EPHY_SHELL (object);

	g_assert (ephy_shell == NULL);

	if (shell->priv->server_timeout > 0)
	{
		g_source_remove (shell->priv->server_timeout);
	}

	/* this will unload the extensions */
	LOG ("Unref extension manager")
	g_object_unref (shell->priv->extensions_manager);

	LOG ("Unref toolbars model")
	if (shell->priv->toolbars_model)
	{
		g_object_unref (G_OBJECT (shell->priv->toolbars_model));
	}

	LOG ("Unref fullscreen toolbars model")
	if (shell->priv->fs_toolbars_model)
	{
		g_object_unref (G_OBJECT (shell->priv->fs_toolbars_model));
	}

	LOG ("Unref Bookmarks Editor");
	if (shell->priv->bme)
	{
		gtk_widget_destroy (GTK_WIDGET (shell->priv->bme));
	}

	LOG ("Unref History Window");
	if (shell->priv->history_window)
	{
		gtk_widget_destroy (GTK_WIDGET (shell->priv->history_window));
	}

	LOG ("Unref PDM Dialog")
	if (shell->priv->pdm_dialog)
	{
		g_object_unref (shell->priv->pdm_dialog);
	}

	LOG ("Unref prefs dialog")
	if (shell->priv->prefs_dialog)
	{
		g_object_unref (shell->priv->prefs_dialog);
	}

	LOG ("Unref print setup dialog")
	if (shell->priv->print_setup_dialog)
	{
		g_object_unref (shell->priv->print_setup_dialog);
	}

	LOG ("Unref bookmarks")
	if (shell->priv->bookmarks)
	{
		g_object_unref (shell->priv->bookmarks);
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);

	if (shell->priv->automation_factory)
	{
		bonobo_activation_unregister_active_server
			(AUTOMATION_FACTORY_IID, BONOBO_OBJREF (shell->priv->automation_factory));

		bonobo_object_unref (shell->priv->automation_factory);
	}

	LOG ("Ephy shell finalized")
}

EphyShell *
ephy_shell_new (void)
{
	return EPHY_SHELL (g_object_new (EPHY_TYPE_SHELL, NULL));
}

static gboolean
url_is_empty (const char *location)
{
	gboolean is_empty = FALSE;

        if (location == NULL || location[0] == '\0' ||
            strcmp (location, "about:blank") == 0)
        {
                is_empty = TRUE;
        }

        return is_empty;
}

static gboolean
load_homepage (EphyEmbed *embed)
{
	char *home;
	gboolean is_empty;

	home = eel_gconf_get_string(CONF_GENERAL_HOMEPAGE);

	if (home == NULL || home[0] == '\0')
	{
		g_free (home);

		home = g_strdup ("about:blank");
	}

	is_empty = url_is_empty (home);

	ephy_embed_load_url (embed, home);

	g_free (home);

	return is_empty;
}

/**
 * ephy_shell_new_tab:
 * @shell: a #EphyShell
 * @parent_window: the target #EphyWindow or %NULL
 * @previous_tab: the referrer tab or %NULL
 * @url: an url to load or %NULL
 *
 * Create a new tab and the parent window when necessary.
 * Ever use this function to open urls in new window/tabs.
 *
 * ReturnValue: the created #EphyTab
 **/
EphyTab *
ephy_shell_new_tab (EphyShell *shell,
		    EphyWindow *parent_window,
		    EphyTab *previous_tab,
		    const char *url,
		    EphyNewTabFlags flags)
{
	EphyWindow *window;
	EphyTab *tab;
	EphyEmbed *embed;
	gboolean in_new_window = TRUE;
	gboolean jump_to;
	EphyEmbed *previous_embed = NULL;
	GtkWidget *nb;
	int position = -1;
	gboolean is_empty = FALSE;
	Toolbar *toolbar;

	if (flags & EPHY_NEW_TAB_IN_NEW_WINDOW) in_new_window = TRUE;
	if (flags & EPHY_NEW_TAB_IN_EXISTING_WINDOW) in_new_window = FALSE;

	in_new_window = in_new_window && !eel_gconf_get_boolean (CONF_LOCKDOWN_FULLSCREEN);

	jump_to = (flags & EPHY_NEW_TAB_JUMP) != 0;

	if (!in_new_window && parent_window != NULL)
	{
		window = parent_window;
	}
	else
	{
		window = ephy_window_new ();
	}

	toolbar = EPHY_TOOLBAR (ephy_window_get_toolbar (window));

	if (previous_tab != NULL)
	{
		previous_embed = ephy_tab_get_embed (previous_tab);
	}

	if ((flags & EPHY_NEW_TAB_APPEND_AFTER) && previous_tab != NULL)
	{
		nb = ephy_window_get_notebook (window);
		position = gtk_notebook_page_num (GTK_NOTEBOOK (nb),
						  GTK_WIDGET (previous_tab)) + 1;
	}

	tab = ephy_tab_new ();
	gtk_widget_show (GTK_WIDGET (tab));
	embed = ephy_tab_get_embed (tab);

	ephy_window_add_tab (window, tab, position, jump_to);
	gtk_widget_show (GTK_WIDGET (window));

	if (flags & EPHY_NEW_TAB_HOME_PAGE ||
	    flags & EPHY_NEW_TAB_NEW_PAGE)
	{
		ephy_tab_set_location (tab, "", TAB_ADDRESS_EXPIRE_NEXT);
		toolbar_activate_location (toolbar);
		is_empty = load_homepage (embed);
	}
	else if (flags & EPHY_NEW_TAB_OPEN_PAGE)
	{
		g_assert (url != NULL);
		ephy_embed_load_url (embed, url);
		is_empty = url_is_empty (url);
	}

	if (flags & EPHY_NEW_TAB_FULLSCREEN_MODE)
	{
		gtk_window_fullscreen (GTK_WINDOW (window));
	}

	/* Make sure the initial focus is somewhere sensible and not, for
	 * example, on the reload button.
	 */
	if (in_new_window || jump_to)
	{
		/* If the location entry is blank, focus that, except if the
		 * page was a copy */
		if (is_empty)
		{
			/* empty page, focus location entry */
			toolbar = EPHY_TOOLBAR (ephy_window_get_toolbar (window));
			toolbar_activate_location (toolbar);
		}
		else if (embed != NULL)
		{
			/* non-empty page, focus the page. but make sure the widget is realised first! */
			gtk_widget_realize (GTK_WIDGET (embed));
			ephy_embed_activate (embed);
		}
	}

	return tab;
}

/**
 * ephy_shell_get_session:
 * @shell: the #EphyShell
 *
 * Returns current session.
 *
 * Return value: the current session.
 **/
GObject *
ephy_shell_get_session (EphyShell *shell)
{
	g_return_val_if_fail (EPHY_IS_SHELL (shell), NULL);

	if (shell->priv->session == NULL)
	{
		EphyExtensionsManager *manager;

		manager = EPHY_EXTENSIONS_MANAGER
			(ephy_shell_get_extensions_manager (shell));

		/* Instantiate internal extensions */
		shell->priv->session =
			EPHY_SESSION (ephy_extensions_manager_add
				(manager, EPHY_TYPE_SESSION));
	}

	return G_OBJECT (shell->priv->session);
}

EphyBookmarks *
ephy_shell_get_bookmarks (EphyShell *shell)
{
	if (shell->priv->bookmarks == NULL)
	{
		shell->priv->bookmarks = ephy_bookmarks_new ();
	}

	return shell->priv->bookmarks;
}

GObject *
ephy_shell_get_toolbars_model (EphyShell *shell, gboolean fullscreen)
{
	LOG ("ephy_shell_get_toolbars_model fs=%d", fullscreen)

	if (fullscreen)
	{
		if (shell->priv->fs_toolbars_model == NULL)
		{
			gboolean success;
			const char *xml;

			shell->priv->fs_toolbars_model = egg_toolbars_model_new ();
			xml = ephy_file ("epiphany-fs-toolbar.xml");
			g_return_val_if_fail (xml != NULL, NULL);

			success = egg_toolbars_model_load
				(shell->priv->fs_toolbars_model, xml);
			g_return_val_if_fail (success, NULL);
		}

		return G_OBJECT (shell->priv->fs_toolbars_model);
	}
	else
	{
		if (shell->priv->toolbars_model == NULL)
		{
			EphyBookmarks *bookmarks;
			GObject *bookmarksbar_model;

			shell->priv->toolbars_model = ephy_toolbars_model_new ();

			/* get the bookmarks toolbars model. we have to do this
			 * before loading the toolbars model from disk, since
			 * this will connect the get_item_* signals
			 */
			bookmarks = ephy_shell_get_bookmarks (shell);
			bookmarksbar_model = ephy_bookmarks_get_toolbars_model (bookmarks);

			/* ok, now we can load the model */
			ephy_toolbars_model_load
				(EPHY_TOOLBARS_MODEL (shell->priv->toolbars_model));
		}

		return G_OBJECT (shell->priv->toolbars_model);
	}
}

GObject *
ephy_shell_get_extensions_manager (EphyShell *es)
{
	g_return_val_if_fail (EPHY_IS_SHELL (es), NULL);

	if (es->priv->extensions_manager == NULL)
	{
		char *path;

		/* Instantiate extensions manager */
		es->priv->extensions_manager = ephy_extensions_manager_new ();

		/* load the extensions */
		ephy_extensions_manager_load_dir (es->priv->extensions_manager,
						  EXTENSIONS_DIR);

		path = g_build_filename (ephy_dot_dir (), "extensions", NULL);
		ephy_extensions_manager_load_dir (es->priv->extensions_manager,
						  path);
		g_free (path);
	}

	return G_OBJECT (es->priv->extensions_manager);
}

static void
toolwindow_show_cb (GtkWidget *widget, EphyShell *es)
{
	EphySession *session;

	LOG ("Ref shell for %s", G_OBJECT_TYPE_NAME (widget))

	session = EPHY_SESSION (ephy_shell_get_session (es));
	ephy_session_add_window (ephy_shell->priv->session, GTK_WINDOW (widget));
	g_object_ref (ephy_shell);
}

static void
toolwindow_hide_cb (GtkWidget *widget, EphyShell *es)
{
	EphySession *session;

	LOG ("Unref shell for %s", G_OBJECT_TYPE_NAME (widget))

	session = EPHY_SESSION (ephy_shell_get_session (es));
	ephy_session_remove_window (ephy_shell->priv->session, GTK_WINDOW (widget));
	g_object_unref (ephy_shell);
}

GtkWidget *
ephy_shell_get_bookmarks_editor (EphyShell *shell)
{
	EphyBookmarks *bookmarks;

	if (shell->priv->bme == NULL)
	{
		bookmarks = ephy_shell_get_bookmarks (ephy_shell);
		g_assert (bookmarks != NULL);
		shell->priv->bme = ephy_bookmarks_editor_new (bookmarks);

		g_signal_connect (shell->priv->bme, "show", 
				  G_CALLBACK (toolwindow_show_cb), shell);
		g_signal_connect (shell->priv->bme, "hide", 
				  G_CALLBACK (toolwindow_hide_cb), shell);
	}

	return shell->priv->bme;
}

GtkWidget *
ephy_shell_get_history_window (EphyShell *shell)
{
	EphyHistory *history;

	if (shell->priv->history_window == NULL)
	{
		history = EPHY_HISTORY
			(ephy_embed_shell_get_global_history (embed_shell));
		g_assert (history != NULL);
		shell->priv->history_window = ephy_history_window_new (history);

		g_signal_connect (shell->priv->history_window, "show",
				  G_CALLBACK (toolwindow_show_cb), shell);
		g_signal_connect (shell->priv->history_window, "hide",
				  G_CALLBACK (toolwindow_hide_cb), shell);
	}

	return shell->priv->history_window;
}

GObject *
ephy_shell_get_pdm_dialog (EphyShell *shell)
{
	if (shell->priv->pdm_dialog == NULL)
	{
		shell->priv->pdm_dialog = g_object_new (EPHY_TYPE_PDM_DIALOG, NULL);

		g_object_add_weak_pointer (shell->priv->pdm_dialog,
					   (gpointer *) &shell->priv->pdm_dialog);
	}

	return shell->priv->pdm_dialog;
}

GObject *
ephy_shell_get_prefs_dialog (EphyShell *shell)
{
	if (shell->priv->prefs_dialog == NULL)
	{
		shell->priv->prefs_dialog = g_object_new (EPHY_TYPE_PREFS_DIALOG, NULL);

		g_object_add_weak_pointer (shell->priv->prefs_dialog,
					   (gpointer *) &shell->priv->prefs_dialog);
	}

	return shell->priv->prefs_dialog;
}

GObject *
ephy_shell_get_print_setup_dialog (EphyShell *shell)
{
	if (shell->priv->print_setup_dialog == NULL)
	{
		shell->priv->print_setup_dialog = G_OBJECT (ephy_print_setup_dialog_new ());

		g_object_add_weak_pointer (shell->priv->print_setup_dialog,
					   (gpointer *) &shell->priv->print_setup_dialog);
	}

	return shell->priv->print_setup_dialog;
}
