/*
 *  Copyright (C) 2000, 2001, 2002 Marco Pesenti Gritti
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
#include <config.h>
#endif

#include "ephy-shell.h"
#include "ephy-state.h"
#include "ephy-embed-shell.h"
#include "eel-gconf-extensions.h"
#include "ephy-prefs.h"
#include "ephy-favicon-cache.h"
#include "ephy-stock-icons.h"
#include "ephy-window.h"
#include "ephy-file-helpers.h"
#include "ephy-thread-helpers.h"
#include "ephy-bookmarks-import.h"
#include "ephy-bookmarks-editor.h"
#include "ephy-history-window.h"
#include "ephy-debug.h"
#include "ephy-plugin.h"
#include "toolbar.h"
#include "session.h"
#include "downloader-view.h"
#include "ephy-toolbars-model.h"
#include "ephy-autocompletion.h"

#include <string.h>
#include <libgnomeui/gnome-client.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-i18n.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkmessagedialog.h>
#include <dirent.h>

#ifdef ENABLE_NAUTILUS_VIEW

#include <bonobo/bonobo-generic-factory.h>
#include "ephy-nautilus-view.h"

#define EPHY_NAUTILUS_VIEW_OAFIID "OAFIID:GNOME_Epiphany_NautilusViewFactory"

#endif

struct EphyShellPrivate
{
	Session *session;
	EphyAutocompletion *autocompletion;
	EphyBookmarks *bookmarks;
	EphyToolbarsModel *toolbars_model;
	GtkWidget *bme;
	GtkWidget *history_window;
	GList *plugins;
	gboolean server_mode;
};

static void
ephy_shell_class_init (EphyShellClass *klass);
static void
ephy_shell_init (EphyShell *gs);
static void
ephy_shell_finalize (GObject *object);
static void
ephy_init_services (EphyShell *gs);

#ifdef ENABLE_NAUTILUS_VIEW

static void
ephy_nautilus_view_init_factory (EphyShell *gs);
static BonoboObject *
ephy_nautilus_view_new (BonoboGenericFactory *factory,
			  const char *id,
			  EphyShell *gs);

#endif

static GObjectClass *parent_class = NULL;

EphyShell *ephy_shell;

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

                ephy_shell_type = g_type_register_static (EPHY_EMBED_SHELL_TYPE,
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
}

static void
ephy_shell_new_window_cb (EphyEmbedShell *shell,
			  EphyEmbed **new_embed,
                          EmbedChromeMask chromemask,
			  gpointer data)
{
	EphyTab *new_tab;
        EphyWindow *window;

	g_assert (new_embed != NULL);

	window = ephy_window_new ();
	ephy_window_set_chrome (window, chromemask);

	new_tab = ephy_tab_new ();
	ephy_window_add_tab (window, new_tab, EPHY_NOTEBOOK_INSERT_GROUPED, FALSE);
	*new_embed = ephy_tab_get_embed (new_tab);
}

static void
ephy_shell_load_plugins (EphyShell *es)
{
	DIR *d;
	struct dirent *e;

	d = opendir (PLUGINS_DIR);

	if (d == NULL)
	{
		return;
	}

	while ((e = readdir (d)) != NULL)
	{
		char *plugin;

		plugin = g_strconcat (PLUGINS_DIR"/", e->d_name, NULL);

		if (g_str_has_suffix (plugin, ".so"))
		{
			EphyPlugin *obj;

			obj = ephy_plugin_new (plugin);
			es->priv->plugins = g_list_append
				(es->priv->plugins, obj);
		}

		g_free (plugin);
	}
	closedir (d);
}

static void
ephy_shell_init (EphyShell *gs)
{
	EphyEmbedSingle *single;
	EphyShell **ptr = &ephy_shell;

        gs->priv = g_new0 (EphyShellPrivate, 1);
	gs->priv->session = NULL;
	gs->priv->bookmarks = NULL;
	gs->priv->bme = NULL;
	gs->priv->history_window = NULL;
	gs->priv->toolbars_model = NULL;
	gs->priv->plugins = NULL;
	gs->priv->server_mode = FALSE;

	ephy_shell = gs;
	g_object_add_weak_pointer (G_OBJECT(ephy_shell),
				   (gpointer *)ptr);

	ephy_debug_init ();
	ephy_thread_helpers_init ();
	ephy_file_helpers_init ();
	ephy_stock_icons_init ();
	ephy_ensure_dir_exists (ephy_dot_dir ());

	/* This ensures mozilla is fired up */
	single = ephy_embed_shell_get_embed_single (EPHY_EMBED_SHELL (gs));
	if (single != NULL)
	{
		g_signal_connect (G_OBJECT (single),
				  "new_window_orphan",
				  G_CALLBACK(ephy_shell_new_window_cb),
				  NULL);

		ephy_init_services (gs);
	}
	else
	{
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new
			(NULL,
                         GTK_DIALOG_MODAL,
                         GTK_MESSAGE_ERROR,
                         GTK_BUTTONS_CLOSE,
                         _("Epiphany can't be used now. "
                         "Mozilla initialization failed. Check your "
                         "MOZILLA_FIVE_HOME environmental variable."));
		gtk_dialog_run (GTK_DIALOG (dialog));

		exit (0);
	}

	ephy_shell_load_plugins (gs);
}

static void
ephy_shell_finalize (GObject *object)
{
        EphyShell *gs;

        g_return_if_fail (object != NULL);
        g_return_if_fail (IS_EPHY_SHELL (object));

	gs = EPHY_SHELL (object);

        g_return_if_fail (gs->priv != NULL);

	g_assert (ephy_shell == NULL);

	g_list_foreach (gs->priv->plugins, (GFunc)g_type_module_unuse, NULL);
	g_list_free (gs->priv->plugins);

	LOG ("Unref toolbars model")
	if (gs->priv->toolbars_model)
	{
		g_object_unref (G_OBJECT (gs->priv->toolbars_model));
	}

	LOG ("Unref session")
	if (gs->priv->session)
	{
		g_return_if_fail (IS_SESSION(gs->priv->session));
		g_object_remove_weak_pointer
			(G_OBJECT(gs->priv->session),
                         (gpointer *)&gs->priv->session);
		g_object_unref (G_OBJECT (gs->priv->session));
	}

	LOG ("Unref autocompletion")
	if (gs->priv->autocompletion)
	{
		g_object_unref (gs->priv->autocompletion);
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

        g_free (gs->priv);

	ephy_state_save ();
	ephy_file_helpers_shutdown ();

	LOG ("Ephy shell finalized")

	bonobo_main_quit ();

	LOG ("Bonobo quit done")
}

EphyShell *
ephy_shell_new (void)
{
	return EPHY_SHELL (g_object_new (EPHY_SHELL_TYPE, NULL));
}

static void
ephy_init_services (EphyShell *gs)
{
	/* preload the prefs */
	/* it also enables notifiers support */
	eel_gconf_monitor_add ("/apps/epiphany");
	eel_gconf_monitor_add ("/system/proxy");

#ifdef ENABLE_NAUTILUS_VIEW

	ephy_nautilus_view_init_factory (gs);

#endif

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
 * ephy_shell_get_active_window:
 * @gs: a #EphyShell
 *
 * Get the current active window. Use it when you
 * need to take an action (like opening an url) on
 * a window but you dont have a target window.
 * Ex. open a new tab from command line.
 *
 * Return value: the current active window
 **/
EphyWindow *
ephy_shell_get_active_window (EphyShell *gs)
{
	Session *session;
	const GList *windows;

	session = SESSION (ephy_shell_get_session (gs));
	windows = session_get_windows (session);

	if (windows == NULL) return NULL;

	return EPHY_WINDOW(windows->data);
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
	gboolean in_new_window;
	gboolean grouped;
	gboolean jump_to;
	EphyEmbed *previous_embed = NULL;
	GtkWidget *nb;
	gint position;
	Toolbar *toolbar;

	in_new_window = !eel_gconf_get_boolean (CONF_TABS_TABBED);

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
	            flags & EPHY_NEW_TAB_APPEND_GROUPED ||
		    flags & EPHY_NEW_TAB_CLONE_PAGE)) &&
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
	else if (flags & EPHY_NEW_TAB_CLONE_PAGE)
	{
		EmbedDisplayType display_type;

                display_type = (flags & EPHY_NEW_TAB_SOURCE_MODE) ?
                                DISPLAY_AS_SOURCE : DISPLAY_NORMAL;

		ephy_embed_load_url (embed, "about:blank");
		ephy_embed_copy_page (embed, previous_embed, display_type);
	}

	if (flags & EPHY_NEW_TAB_FULLSCREEN_MODE)
	{
		gtk_window_fullscreen (GTK_WINDOW (window));
	}

        return tab;
}

#ifdef ENABLE_NAUTILUS_VIEW

static void
ephy_nautilus_view_init_factory (EphyShell *gs)
{
	BonoboGenericFactory *ephy_nautilusview_factory;
	ephy_nautilusview_factory = bonobo_generic_factory_new
		(EPHY_NAUTILUS_VIEW_OAFIID,
		 (BonoboFactoryCallback) ephy_nautilus_view_new, gs);
	if (!BONOBO_IS_GENERIC_FACTORY (ephy_nautilusview_factory))
		g_warning ("Couldn't create the factory!");

}

static BonoboObject *
ephy_nautilus_view_new (BonoboGenericFactory *factory, const char *id,
			  EphyShell *gs)
{
	return ephy_nautilus_view_new_component (gs);
}

#endif

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
	if (!gs->priv->session)
	{
		gs->priv->session = session_new ();
		g_object_add_weak_pointer
			(G_OBJECT(gs->priv->session),
                         (gpointer *)&gs->priv->session);
	}

	return G_OBJECT (gs->priv->session);
}

GObject *
ephy_shell_get_autocompletion (EphyShell *gs)
{
	EphyShellPrivate *p = gs->priv;

	if (!p->autocompletion)
	{
		static const gchar *prefixes[] = {
			EPHY_AUTOCOMPLETION_USUAL_WEB_PREFIXES,
			NULL
		};

		EphyHistory *gh = ephy_embed_shell_get_global_history (EPHY_EMBED_SHELL (gs));
		EphyBookmarks *bmk = ephy_shell_get_bookmarks (gs);
		p->autocompletion = ephy_autocompletion_new ();
		ephy_autocompletion_set_prefixes (p->autocompletion, prefixes);

		ephy_autocompletion_add_source (p->autocompletion,
						EPHY_AUTOCOMPLETION_SOURCE (gh));
		ephy_autocompletion_add_source (p->autocompletion,
						EPHY_AUTOCOMPLETION_SOURCE (bmk));
	}
	return G_OBJECT (p->autocompletion);
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
ephy_shell_get_toolbars_model (EphyShell *gs)
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

static void
bookmarks_hide_cb (GtkWidget *widget, gpointer data)
{
	LOG ("Unref shell for bookmarks editor")
	g_object_unref (ephy_shell);
}

void
ephy_shell_show_bookmarks_editor (EphyShell *gs,
				  GtkWidget *parent)
{
	EphyBookmarks *bookmarks;

	if (gs->priv->bme == NULL)
	{
		bookmarks = ephy_shell_get_bookmarks (ephy_shell);
		g_assert (bookmarks != NULL);
		gs->priv->bme = ephy_bookmarks_editor_new (bookmarks);
		g_signal_connect (gs->priv->bme, "hide", 
				  G_CALLBACK (bookmarks_hide_cb), NULL);
	}

	if (!GTK_WIDGET_VISIBLE (gs->priv->bme))
	{
		LOG ("Ref shell for bookmarks editor")
		g_object_ref (ephy_shell);
	}

	if (parent)
	{
		ephy_bookmarks_editor_set_parent
			(EPHY_BOOKMARKS_EDITOR (gs->priv->bme), parent);
	}

	gtk_window_present (GTK_WINDOW (gs->priv->bme));
}

static void
history_window_hide_cb (GtkWidget *widget, gpointer data)
{
	LOG ("Unref shell for history window")
	g_object_unref (ephy_shell);
}

void
ephy_shell_show_history_window (EphyShell *gs,
				GtkWidget *parent)
{
	EphyHistory *history;

	if (gs->priv->history_window == NULL)
	{
		history = ephy_embed_shell_get_global_history
			(EPHY_EMBED_SHELL (ephy_shell));
		g_assert (history != NULL);
		gs->priv->history_window = ephy_history_window_new (history);
		g_signal_connect (gs->priv->history_window, "hide",
				  G_CALLBACK (history_window_hide_cb), NULL);
	}

	if (!GTK_WIDGET_VISIBLE (gs->priv->history_window))
	{
		LOG ("Ref shell for history window")
		g_object_ref (ephy_shell);
	}

	if (parent)
	{
		ephy_history_window_set_parent
			(EPHY_HISTORY_WINDOW (gs->priv->history_window), parent);
	}

	gtk_window_present (GTK_WINDOW (gs->priv->history_window));
}

gboolean
ephy_shell_get_server_mode (EphyShell *es)
{
	g_return_val_if_fail (IS_EPHY_SHELL (es), FALSE);

	return es->priv->server_mode;
}

void
ephy_shell_set_server_mode (EphyShell *es, gboolean as_server)
{
	g_return_if_fail (IS_EPHY_SHELL (es));

	if (es->priv->server_mode == FALSE && as_server == TRUE)
	{
		g_object_ref (G_OBJECT (es));
	}
	else if (es->priv->server_mode == TRUE && as_server == FALSE)
	{
		g_object_unref (G_OBJECT (es));
	}

	es->priv->server_mode = as_server;
}
