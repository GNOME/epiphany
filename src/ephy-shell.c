/*
 *  Copyright (C) 2000-2003 Marco Pesenti Gritti
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
#include "ephy-debug.h"
#include "ephy-extensions-manager.h"
#include "toolbar.h"
#include "ephy-session.h"
#include "downloader-view.h"
#include "ephy-toolbars-model.h"
#include "ephy-automation.h"

#include <string.h>
#include <bonobo/bonobo-main.h>
#include <glib/gi18n.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkmessagedialog.h>
#include <dirent.h>
#include <unistd.h>

#ifdef ENABLE_NAUTILUS_VIEW
#include <bonobo/bonobo-generic-factory.h>
#include "ephy-nautilus-view.h"
#endif

#define EPHY_NAUTILUS_VIEW_IID "OAFIID:GNOME_Epiphany_NautilusView"
#define AUTOMATION_IID "OAFIID:GNOME_Epiphany_Automation"
#define SERVER_TIMEOUT 60000

#define EPHY_SHELL_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_SHELL, EphyShellPrivate))

struct EphyShellPrivate
{
	BonoboGenericFactory *automation_factory;
	EphySession *session;
	EphyBookmarks *bookmarks;
	EphyToolbarsModel *toolbars_model;
	EggToolbarsModel *fs_toolbars_model;
	EphyExtensionsManager *extensions_manager;
	GtkWidget *bme;
	GtkWidget *history_window;
	GList *del_on_exit;
	guint server_timeout;
};

static void
ephy_shell_class_init (EphyShellClass *klass);
static void
ephy_shell_init (EphyShell *gs);
static void
ephy_shell_finalize (GObject *object);

static GObjectClass *parent_class = NULL;

EphyShell *ephy_shell;

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
        static GType ephy_shell_type = 0;

        if (ephy_shell_type == 0)
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

		ephy_shell_type = g_type_register_static (EPHY_TYPE_EMBED_SHELL,
							  "EphyShell",
							  &our_info, 0);
        }

        return ephy_shell_type;

}

static void
ephy_shell_class_init (EphyShellClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = ephy_shell_finalize;

	g_type_class_add_private (object_class, sizeof(EphyShellPrivate));
}

#ifdef ENABLE_NAUTILUS_VIEW

static BonoboObject *
ephy_nautilus_view_new (EphyShell *gs)
{
	EphyNautilusView *view;

	view = EPHY_NAUTILUS_VIEW
		(ephy_nautilus_view_new_component (gs));

	return BONOBO_OBJECT (view);
}

#endif

static BonoboObject *
ephy_automation_factory_cb (BonoboGenericFactory *this_factory,
			    const char *iid,
			    EphyShell *es)
{
	if (strcmp (iid, AUTOMATION_IID) == 0)
	{
		return BONOBO_OBJECT (g_object_new (EPHY_TYPE_AUTOMATION, NULL));
	}
#ifdef ENABLE_NAUTILUS_VIEW
	else if (strcmp (iid, EPHY_NAUTILUS_VIEW_IID) == 0)
	{
		return ephy_nautilus_view_new (es);
	}
#endif

	g_assert_not_reached ();

	return NULL;
}


static BonoboGenericFactory *
ephy_automation_factory_new (EphyShell *es)
{
	BonoboGenericFactory *factory;
	GClosure *factory_closure;

	factory = g_object_new (bonobo_generic_factory_get_type (), NULL);

	factory_closure = g_cclosure_new
		(G_CALLBACK (ephy_automation_factory_cb), es, NULL);

	bonobo_generic_factory_construct_noreg
		(factory, AUTOMATION_FACTORY_IID, factory_closure);

	return factory;
}

static void
ephy_shell_init (EphyShell *gs)
{
	EphyShell **ptr = &ephy_shell;

	gs->priv = EPHY_SHELL_GET_PRIVATE (gs);

	gs->priv->session = NULL;
	gs->priv->bookmarks = NULL;
	gs->priv->bme = NULL;
	gs->priv->history_window = NULL;
	gs->priv->toolbars_model = NULL;
	gs->priv->fs_toolbars_model = NULL;
	gs->priv->extensions_manager = NULL;
	gs->priv->server_timeout = 0;

	ephy_shell = gs;
	g_object_add_weak_pointer (G_OBJECT(ephy_shell),
				   (gpointer *)ptr);

	/* Instantiate the automation factory */
	gs->priv->automation_factory = ephy_automation_factory_new (gs);
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
server_timeout (EphyShell *gs)
{
	g_object_unref (gs);

	return FALSE;
}

gboolean
ephy_shell_startup (EphyShell *gs,
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
		(AUTOMATION_FACTORY_IID, BONOBO_OBJREF (gs->priv->automation_factory), NULL);

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
		g_object_ref (gs);
		gs->priv->server_timeout = g_timeout_add
			(SERVER_TIMEOUT, (GSourceFunc)server_timeout, gs);
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
	}

	CORBA_exception_free (&ev);
	gdk_notify_startup_complete ();

	return !(result == Bonobo_ACTIVATION_REG_ALREADY_ACTIVE);
}

static void
delete_files (GList *l)
{
	for (; l != NULL; l = l->next)
	{
		unlink (l->data);
	}
}

static void
ephy_shell_finalize (GObject *object)
{
        EphyShell *gs = EPHY_SHELL (object);

	g_assert (ephy_shell == NULL);

	if (gs->priv->server_timeout > 0)
	{
		g_source_remove (gs->priv->server_timeout);
	}

	/* this will unload the extensions */
	LOG ("Unref extension manager")
	g_object_unref (gs->priv->extensions_manager);

	delete_files (gs->priv->del_on_exit);
	g_list_foreach (gs->priv->del_on_exit, (GFunc)g_free, NULL);
	g_list_free (gs->priv->del_on_exit);

	LOG ("Unref toolbars model")
	if (gs->priv->toolbars_model)
	{
		g_object_unref (G_OBJECT (gs->priv->toolbars_model));
	}

	LOG ("Unref fullscreen toolbars model")
	if (gs->priv->fs_toolbars_model)
	{
		g_object_unref (G_OBJECT (gs->priv->fs_toolbars_model));
	}

	LOG ("Unref Bookmarks Editor");
	if (gs->priv->bme)
	{
		gtk_widget_destroy (GTK_WIDGET (gs->priv->bme));
	}

	LOG ("Unref History Window");
	if (gs->priv->history_window)
	{
		gtk_widget_destroy (GTK_WIDGET (gs->priv->history_window));
	}

	LOG ("Unref bookmarks")
	if (gs->priv->bookmarks)
	{
		g_object_unref (gs->priv->bookmarks);
	}

        G_OBJECT_CLASS (parent_class)->finalize (object);

	if (gs->priv->automation_factory)
	{
		bonobo_activation_unregister_active_server
			(AUTOMATION_FACTORY_IID, BONOBO_OBJREF (gs->priv->automation_factory));

		bonobo_object_unref (gs->priv->automation_factory);
	}

	LOG ("Ephy shell finalized")
}

EphyShell *
ephy_shell_new (void)
{
	return EPHY_SHELL (g_object_new (EPHY_TYPE_SHELL, NULL));
}

static void
load_homepage (EphyEmbed *embed)
{
	char *home;

	home = eel_gconf_get_string(CONF_GENERAL_HOMEPAGE);

	if (home == NULL || home[0] == '\0')
	{
		g_free (home);

		home = g_strdup ("about:blank");
	}

	ephy_embed_load_url (embed, home);

	g_free (home);
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
	gboolean grouped;
	gboolean jump_to;
	EphyEmbed *previous_embed = NULL;
	GtkWidget *nb;
	gint position;
	Toolbar *toolbar;

	if (flags & EPHY_NEW_TAB_IN_NEW_WINDOW) in_new_window = TRUE;
	if (flags & EPHY_NEW_TAB_IN_EXISTING_WINDOW) in_new_window = FALSE;

	jump_to = (flags & EPHY_NEW_TAB_JUMP);

	if (!in_new_window && parent_window != NULL)
	{
		window = parent_window;
	}
	else
	{
		window = ephy_window_new ();
	}

	toolbar = ephy_window_get_toolbar (window);

	if (previous_tab)
	{
		previous_embed = ephy_tab_get_embed (previous_tab);
	}

	grouped = ((flags & EPHY_NEW_TAB_OPEN_PAGE ||
	            flags & EPHY_NEW_TAB_APPEND_GROUPED)) &&
		  !(flags & EPHY_NEW_TAB_APPEND_LAST);

	if ((flags & EPHY_NEW_TAB_APPEND_AFTER) && previous_embed != NULL)
	{
		nb = ephy_window_get_notebook (window);
		position = gtk_notebook_page_num (GTK_NOTEBOOK (nb),
						  GTK_WIDGET (previous_embed)) + 1;
	}
	else
	{
		position = grouped ? EPHY_NOTEBOOK_INSERT_GROUPED : EPHY_NOTEBOOK_INSERT_LAST;
	}

	tab = ephy_tab_new ();
	embed = ephy_tab_get_embed (tab);
	gtk_widget_show (GTK_WIDGET(embed));
	ephy_window_add_tab (window, tab,
			     position,
			     jump_to);
	gtk_widget_show (GTK_WIDGET(window));

	if (flags & EPHY_NEW_TAB_HOME_PAGE ||
	    flags & EPHY_NEW_TAB_NEW_PAGE)
	{
		ephy_tab_set_location (tab, "", TAB_ADDRESS_EXPIRE_NEXT);
		toolbar_activate_location (toolbar);
		load_homepage (embed);
	}
	else if (flags & EPHY_NEW_TAB_OPEN_PAGE)
	{
		g_assert (url != NULL);
		ephy_embed_load_url (embed, url);
	}

	if (flags & EPHY_NEW_TAB_FULLSCREEN_MODE)
	{
		gtk_window_fullscreen (GTK_WINDOW (window));
	}

        return tab;
}

/**
 * ephy_shell_get_session:
 * @gs: a #EphyShell
 *
 * Returns current session.
 *
 * Return value: the current session.
 **/
GObject *
ephy_shell_get_session (EphyShell *gs)
{
	g_return_val_if_fail (EPHY_IS_SHELL (gs), NULL);

	if (gs->priv->session == NULL)
	{
		EphyExtensionsManager *manager;

		manager = EPHY_EXTENSIONS_MANAGER
			(ephy_shell_get_extensions_manager (gs));

		/* Instantiate internal extensions */
		gs->priv->session =
			EPHY_SESSION (ephy_extensions_manager_add
				(manager, EPHY_TYPE_SESSION));
	}

	return G_OBJECT (gs->priv->session);
}

EphyBookmarks *
ephy_shell_get_bookmarks (EphyShell *gs)
{
	if (gs->priv->bookmarks == NULL)
	{
		gs->priv->bookmarks = ephy_bookmarks_new ();
	}

	return gs->priv->bookmarks;
}

GObject *
ephy_shell_get_toolbars_model (EphyShell *gs, gboolean fullscreen)
{
	if (fullscreen)
	{
		if (gs->priv->fs_toolbars_model == NULL)
		{
			const char *xml;

			gs->priv->fs_toolbars_model = egg_toolbars_model_new ();
			xml = ephy_file ("epiphany-fs-toolbar.xml");
			egg_toolbars_model_load (gs->priv->fs_toolbars_model, xml);
		}

		return G_OBJECT (gs->priv->fs_toolbars_model);
	}
	else
	{
		if (gs->priv->toolbars_model == NULL)
		{
			EphyBookmarks *bookmarks;

			bookmarks = ephy_shell_get_bookmarks (gs);

			gs->priv->toolbars_model = ephy_toolbars_model_new (bookmarks);

			g_object_set (bookmarks, "toolbars_model",
				      gs->priv->toolbars_model, NULL);
		}


		return G_OBJECT (gs->priv->toolbars_model);
	}
}

GObject *
ephy_shell_get_extensions_manager (EphyShell *es)
{
	g_return_val_if_fail (EPHY_IS_SHELL (es), NULL);

	if (es->priv->extensions_manager == NULL)
	{
		/* Instantiate extensions manager; this will load the extensions */
		es->priv->extensions_manager = ephy_extensions_manager_new ();
	}

	return G_OBJECT (es->priv->extensions_manager);
}

static void
toolwindow_show_cb (GtkWidget *widget)
{
	LOG ("Ref shell for %s", G_OBJECT_TYPE_NAME (widget))
	ephy_session_add_window (ephy_shell->priv->session, GTK_WINDOW (widget));
	g_object_ref (ephy_shell);
}

static void
toolwindow_hide_cb (GtkWidget *widget)
{
	LOG ("Unref shell for %s", G_OBJECT_TYPE_NAME (widget))
	ephy_session_remove_window (ephy_shell->priv->session, GTK_WINDOW (widget));
	g_object_unref (ephy_shell);
}

GtkWidget *
ephy_shell_get_bookmarks_editor (EphyShell *gs)
{
	EphyBookmarks *bookmarks;

	if (gs->priv->bme == NULL)
	{
		bookmarks = ephy_shell_get_bookmarks (ephy_shell);
		g_assert (bookmarks != NULL);
		gs->priv->bme = ephy_bookmarks_editor_new (bookmarks);

		g_signal_connect (gs->priv->bme, "show", 
				  G_CALLBACK (toolwindow_show_cb), NULL);
		g_signal_connect (gs->priv->bme, "hide", 
				  G_CALLBACK (toolwindow_hide_cb), NULL);
	}

	return gs->priv->bme;
}

GtkWidget *
ephy_shell_get_history_window (EphyShell *gs)
{
	EphyHistory *history;

	if (gs->priv->history_window == NULL)
	{
		history = ephy_embed_shell_get_global_history
			(EPHY_EMBED_SHELL (ephy_shell));
		g_assert (history != NULL);
		gs->priv->history_window = ephy_history_window_new (history);

		g_signal_connect (gs->priv->history_window, "show",
				  G_CALLBACK (toolwindow_show_cb), NULL);
		g_signal_connect (gs->priv->history_window, "hide",
				  G_CALLBACK (toolwindow_hide_cb), NULL);
	}

	return gs->priv->history_window;
}

void
ephy_shell_delete_on_exit (EphyShell *gs, const char *path)
{
	gs->priv->del_on_exit = g_list_append (gs->priv->del_on_exit,
					       g_strdup (path));
}

